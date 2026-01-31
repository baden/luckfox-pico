#include "udp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>

// Function to generate device ID from MAC (simplified version)
static void generate_device_id(char* device_id, size_t size) {
    // For now, use a simple timestamp-based ID
    // In production, this should be based on actual MAC address
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(device_id, size, "DRONE_%ld", tv.tv_sec % 1000000);
}

double udp_get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// Very simple JSON parser - only parses specific format we need
int udp_parse_json_packet(const char* json_data, udp_packet_t* packet) {
    if (!json_data || !packet) {
        return -1;
    }
    
    memset(packet, 0, sizeof(udp_packet_t));
    
    // Look for "command" field
    const char* cmd_start = strstr(json_data, "\"command\"");
    if (!cmd_start) {
        return -1;
    }
    
    cmd_start = strchr(cmd_start, ':');
    if (!cmd_start) {
        return -1;
    }
    cmd_start++; // Skip ':'
    
    // Skip whitespace
    while (*cmd_start == ' ' || *cmd_start == '\t' || *cmd_start == '\n') {
        cmd_start++;
    }
    
    // Parse command string
    if (strncmp(cmd_start, "\"joy_update\"", 13) == 0) {
        packet->type = UDP_COMMAND_JOY_UPDATE;
        
        // Parse data section
        const char* data_start = strstr(json_data, "\"data\"");
        if (!data_start) {
            return -1;
        }
        
        // Parse axes array
        const char* axes_start = strstr(data_start, "\"axes\"");
        if (axes_start) {
            const char* bracket_start = strchr(axes_start, '[');
            if (bracket_start) {
                bracket_start++; // Skip '['
                for (int i = 0; i < 4; i++) {
                    char* end;
                    packet->data.joystick.axes[i] = strtof(bracket_start, &end);
                    if (bracket_start == end) {
                        break; // No number found
                    }
                    bracket_start = end;
                    // Skip to next number
                    while (*bracket_start && (*bracket_start == ',' || *bracket_start == ' ' || *bracket_start == '\t')) {
                        bracket_start++;
                    }
                }
            }
        }
        
        // Parse buttons array
        const char* buttons_start = strstr(data_start, "\"buttons\"");
        if (buttons_start) {
            const char* bracket_start = strchr(buttons_start, '[');
            if (bracket_start) {
                bracket_start++; // Skip '['
                for (int i = 0; i < 4; i++) {
                    char* end;
                    packet->data.joystick.buttons[i] = strtol(bracket_start, &end, 10);
                    if (bracket_start == end) {
                        break; // No number found
                    }
                    bracket_start = end;
                    // Skip to next number
                    while (*bracket_start && (*bracket_start == ',' || *bracket_start == ' ' || *bracket_start == '\t')) {
                        bracket_start++;
                    }
                }
            }
        }
        
        packet->data.joystick.valid = true;
        packet->data.joystick.timestamp = udp_get_time_seconds();
        
    } else if (strncmp(cmd_start, "\"restart\"", 9) == 0) {
        packet->type = UDP_COMMAND_RESTART;
        
    } else {
        packet->type = UDP_COMMAND_UNKNOWN;
    }
    
    return 0;
}

int udp_client_init(udp_client_t* client, const char* device_id) {
    if (!client) {
        return -1;
    }
    
    memset(client, 0, sizeof(udp_client_t));
    client->sockfd = -1;
    
    if (device_id) {
        strncpy(client->device_id, device_id, sizeof(client->device_id) - 1);
    } else {
        generate_device_id(client->device_id, sizeof(client->device_id));
    }
    
    // Set up server address
    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(UDP_SERVER_PORT);
    
    // Resolve hostname
    struct hostent* he = gethostbyname(UDP_SERVER_HOST);
    if (!he) {
        herror("gethostbyname");
        return -1;
    }
    
    memcpy(&client->server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    
    return udp_client_connect(client);
}

void udp_client_cleanup(udp_client_t* client) {
    if (!client) {
        return;
    }
    
    if (client->sockfd >= 0) {
        close(client->sockfd);
        client->sockfd = -1;
    }
    
    client->connected = false;
}

int udp_client_connect(udp_client_t* client) {
    if (!client) {
        return -1;
    }
    
    // Close existing socket
    if (client->sockfd >= 0) {
        close(client->sockfd);
    }
    
    // Create UDP socket
    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) {
        perror("socket creation failed");
        client->connected = false;
        return -1;
    }
    
    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt timeout");
    }
    
    // Bind to any local port
    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = 0; // Any port
    
    if (bind(client->sockfd, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        perror("bind failed");
        close(client->sockfd);
        client->sockfd = -1;
        client->connected = false;
        return -1;
    }
    
    client->connected = true;
    printf("UDP: Connected to server %s:%d\n", UDP_SERVER_HOST, UDP_SERVER_PORT);
    
    // Send initial registration
    return udp_client_send_register(client);
}

int udp_client_send_keep_alive(udp_client_t* client) {
    if (!client || !client->connected) {
        return -1;
    }
    
    double current_time = udp_get_time_seconds();
    if (current_time - client->last_send_time < UDP_KEEP_ALIVE_INTERVAL) {
        return 0; // Not time to send yet
    }
    
    char packet[256];
    snprintf(packet, sizeof(packet), 
        "{\"command\": \"keep_alive\", \"id\": \"%s\"}", 
        client->device_id);
    
    ssize_t sent = sendto(client->sockfd, packet, strlen(packet), 0,
                         (struct sockaddr*)&client->server_addr, sizeof(client->server_addr));
    
    if (sent < 0) {
        perror("sendto keep_alive");
        client->connected = false;
        return -1;
    }
    
    client->last_send_time = current_time;
    printf("UDP: Sent keep-alive packet\n");
    return 0;
}

int udp_client_send_register(udp_client_t* client) {
    if (!client || !client->connected) {
        return -1;
    }
    
    char packet[256];
    snprintf(packet, sizeof(packet), 
        "{\"command\": \"register\", \"id\": \"%s\"}", 
        client->device_id);
    
    ssize_t sent = sendto(client->sockfd, packet, strlen(packet), 0,
                         (struct sockaddr*)&client->server_addr, sizeof(client->server_addr));
    
    if (sent < 0) {
        perror("sendto register");
        client->connected = false;
        return -1;
    }
    
    client->last_send_time = udp_get_time_seconds();
    printf("UDP: Sent registration packet\n");
    return 0;
}

int udp_client_receive(udp_client_t* client, udp_packet_t* packet, double timeout_sec) {
    if (!client || !client->connected || !packet) {
        return -1;
    }
    
    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = (int)timeout_sec;
    tv.tv_usec = (int)((timeout_sec - tv.tv_sec) * 1000000);
    if (setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt timeout");
    }
    
    char buffer[UDP_MAX_PACKET_SIZE];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(client->sockfd, buffer, sizeof(buffer) - 1, 0,
                               (struct sockaddr*)&from_addr, &from_len);
    
    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("recvfrom");
            client->connected = false;
            return -1;
        }
        return 0; // Timeout
    }
    
    buffer[received] = '\0'; // Null-terminate
    client->last_receive_time = udp_get_time_seconds();
    
    // Parse the JSON packet
    if (udp_parse_json_packet(buffer, packet) != 0) {
        printf("UDP: Failed to parse packet: %s\n", buffer);
        return -1;
    }
    
    return received;
}

bool udp_client_is_connected(const udp_client_t* client) {
    return client ? client->connected : false;
}

int udp_client_reconnect(udp_client_t* client) {
    if (!client) {
        return -1;
    }
    
    printf("UDP: Attempting to reconnect...\n");
    
    // Close existing connection
    if (client->sockfd >= 0) {
        close(client->sockfd);
        client->sockfd = -1;
    }
    
    client->connected = false;
    
    // Wait a bit before reconnecting
    sleep(2);
    
    return udp_client_connect(client);
}