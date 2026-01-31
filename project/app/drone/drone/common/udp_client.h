#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define UDP_MAX_PACKET_SIZE 4096
#define UDP_SERVER_HOST "s.navi.cc"
#define UDP_SERVER_PORT 8766
#define UDP_KEEP_ALIVE_INTERVAL 15

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    bool connected;
    char device_id[32];
    double last_send_time;
    double last_receive_time;
} udp_client_t;

typedef struct {
    float axes[4];      // axes[0] - horizontal, axes[1] - vertical, axes[2] - switch B, axes[3] - switch C
    int buttons[4];      // buttons[0] - ARM button
    bool valid;
    double timestamp;
} udp_joystick_data_t;

typedef enum {
    UDP_COMMAND_UNKNOWN = 0,
    UDP_COMMAND_JOY_UPDATE,
    UDP_COMMAND_KEEP_ALIVE,
    UDP_COMMAND_REGISTER,
    UDP_COMMAND_RESTART
} udp_command_type_t;

typedef struct {
    udp_command_type_t type;
    union {
        udp_joystick_data_t joystick;
        struct {
            char device_id[32];
        } register_data;
    } data;
} udp_packet_t;

// Initialize UDP client
int udp_client_init(udp_client_t* client, const char* device_id);

// Cleanup UDP client
void udp_client_cleanup(udp_client_t* client);

// Connect to server
int udp_client_connect(udp_client_t* client);

// Send keep-alive packet
int udp_client_send_keep_alive(udp_client_t* client);

// Send registration packet
int udp_client_send_register(udp_client_t* client);

// Receive data from server
int udp_client_receive(udp_client_t* client, udp_packet_t* packet, double timeout_sec);

// Check if connected
bool udp_client_is_connected(const udp_client_t* client);

// Reconnect if disconnected
int udp_client_reconnect(udp_client_t* client);

// Simple JSON parser for specific format
int udp_parse_json_packet(const char* json_data, udp_packet_t* packet);

// Get current time in seconds
double udp_get_time_seconds(void);

#endif // UDP_CLIENT_H