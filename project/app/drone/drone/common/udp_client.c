#include "udp_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <math.h>

double udp_get_time_seconds(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int udp_client_init(udp_client_t* client) {
    if (!client) return -1;
    memset(client, 0, sizeof(udp_client_t));
    client->sockfd = -1;
    return udp_client_connect(client);
}

int udp_client_connect(udp_client_t* client) {
    if (client->sockfd >= 0) close(client->sockfd);

    client->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client->sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }

    // Set non-blocking/timeout
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000; // 10ms timeout
    setsockopt(client->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&client->server_addr, 0, sizeof(client->server_addr));
    client->server_addr.sin_family = AF_INET;
    client->server_addr.sin_port = htons(UDP_SERVER_PORT);
    if (inet_pton(AF_INET, UDP_SERVER_HOST, &client->server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        return -1;
    }

    client->connected = true;
    printf("UDP: MAVLink client initialized for %s:%d\n", UDP_SERVER_HOST, UDP_SERVER_PORT);
    return 0;
}

void udp_client_cleanup(udp_client_t* client) {
    if (client && client->sockfd >= 0) {
        close(client->sockfd);
        client->sockfd = -1;
    }
}

int udp_client_reconnect(udp_client_t* client) {
    return udp_client_connect(client);
}

bool udp_client_is_connected(const udp_client_t* client) {
    return client && client->connected;
}

static int send_mavlink_message(udp_client_t* client, mavlink_message_t* msg) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buffer, msg);
    
    ssize_t sent = sendto(client->sockfd, buffer, len, 0, 
                          (struct sockaddr*)&client->server_addr, sizeof(client->server_addr));
    if (sent < 0) {
        // perror("UDP send failed");
        return -1;
    }
    return 0;
}

static int send_command_ack(udp_client_t* client, uint16_t command, uint8_t result) {
    mavlink_message_t msg;
    mavlink_msg_command_ack_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                                 command, result, 0, 0, 0, 0);
    return send_mavlink_message(client, &msg);
}

int udp_client_send_heartbeat(udp_client_t* client, bool armed, uint8_t base_mode, uint32_t custom_mode) {
    mavlink_message_t msg;
    
    // Ensure safety armed flag is set correctly in base_mode based on armed state
    if (armed) {
        base_mode |= MAV_MODE_FLAG_SAFETY_ARMED;
    } else {
        base_mode &= ~MAV_MODE_FLAG_SAFETY_ARMED;
    }
    
    // MAV_STATE_ACTIVE if armed, else STANDBY
    uint8_t system_status = armed ? MAV_STATE_ACTIVE : MAV_STATE_STANDBY;

    mavlink_msg_heartbeat_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg, 
                               MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, 
                               base_mode, custom_mode, system_status);

    client->last_heartbeat_time = udp_get_time_seconds();
    
    printf("MAVLink: Sending heartbeat - ARM=%d, base_mode=%d, custom_mode=%d, status=%d\n", 
           armed ? 1 : 0, base_mode, custom_mode, system_status);
    
    return send_mavlink_message(client, &msg);
}

int udp_client_send_telemetry(udp_client_t* client, float axis0, float axis1, 
                              int lebidka_state, int aktuator_state) {
    
    mavlink_message_t msg;
    
    // Send ATTITUDE
    // Stick positions mapped to Roll/Pitch for visualization
    float roll = axis0; 
    float pitch = axis1;
    float yaw = 0.0f;
    uint32_t boot_ms = (uint32_t)(udp_get_time_seconds() * 1000);
    
    mavlink_msg_attitude_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                              boot_ms, roll, pitch, yaw, 0, 0, 0);
    send_mavlink_message(client, &msg);

    // Send named values for custom states
    mavlink_msg_named_value_int_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                                     boot_ms, "LEBIDKA", lebidka_state);
    send_mavlink_message(client, &msg);

    mavlink_msg_named_value_int_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                                     boot_ms, "AKTUATOR", aktuator_state);
    send_mavlink_message(client, &msg);
    
    // Also send VFR_HUD for Compass (Yaw as Heading)
    int16_t heading = (int16_t)(yaw * 180.0f / M_PI);
    if(heading < 0) heading += 360;
    mavlink_msg_vfr_hud_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                             0, 0, heading, 0, 0, 0);
    send_mavlink_message(client, &msg);
    
    // Send GPS position to satisfy QGC requirements
    // Fake GPS coordinates for Kyiv, Ukraine
    double lat = 50.4501;   // Kyiv coordinates
    double lon = 30.5234;
    float alt = 100.0f;     // 100m altitude
    uint64_t time_usec = (uint64_t)(udp_get_time_seconds() * 1000000);
    
    mavlink_msg_global_position_int_pack(MAV_SYSTEM_ID, MAV_COMPONENT_ID, &msg,
                                      boot_ms, (int32_t)(lat * 1e7), (int32_t)(lon * 1e7), 
                                      (int32_t)(alt * 1000), 0, 0, 0, 0, 0);
    send_mavlink_message(client, &msg);
    
    return 0;
}

int udp_client_receive(udp_client_t* client, udp_control_input_t* input) {
    uint8_t buffer[2048];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    
    // Reset command flags each time we check for new data
    input->cmd_arm = false;
    input->cmd_disarm = false;
    input->cmd_takeoff = false;
    input->cmd_set_mode = false;
    input->valid = false;
    
    ssize_t received = recvfrom(client->sockfd, buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&src_addr, &addr_len);
    
    if (received > 0) {
        mavlink_message_t msg;
        mavlink_status_t status;
        
        bool input_updated = false;
        
        for (int i = 0; i < received; ++i) {
            if (mavlink_parse_char(MAVLINK_COMM_0, buffer[i], &msg, &status)) {
                
                switch (msg.msgid) {
                    case MAVLINK_MSG_ID_MANUAL_CONTROL: {
                        mavlink_manual_control_t packet;
                        mavlink_msg_manual_control_decode(&msg, &packet);
                        
                        // MAVLink MANUAL_CONTROL is -1000..1000
                        input->axes[0] = packet.x / 1000.0f; // Pitch
                        input->axes[1] = packet.y / 1000.0f; // Roll
                        input->axes[2] = packet.z / 1000.0f; // Throttle
                        input->axes[3] = packet.r / 1000.0f; // Yaw
                        input->buttons = packet.buttons;
                        input->valid = true;
                        input->timestamp = udp_get_time_seconds();
                        input_updated = true;
                        
                        printf("MAVLink: MANUAL_CONTROL R=%.2f P=%.2f T=%.2f Y=%.2f Btn=%d\n",
                               input->axes[1], input->axes[0], input->axes[2], input->axes[3], input->buttons);
                        break;
                    }
                    
                    case MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE: {
                        mavlink_rc_channels_override_t packet;
                        mavlink_msg_rc_channels_override_decode(&msg, &packet);
                        
                        // Map PWM 1000..2000 to -1.0..1.0
                        // Using block as simple lambda replacement
                        // Channel 1: Roll, 2: Pitch, 3: Throttle, 4: Yaw (AETR or TAER?)
                        // Usually: 1=Roll, 2=Pitch, 3=Throttle, 4=Yaw
                        
                        float ch1 = (packet.chan1_raw == 0 || packet.chan1_raw == 65535) ? 0.0f : (packet.chan1_raw - 1500) / 500.0f;
                        float ch2 = (packet.chan2_raw == 0 || packet.chan2_raw == 65535) ? 0.0f : (packet.chan2_raw - 1500) / 500.0f;
                        float ch3 = (packet.chan3_raw == 0 || packet.chan3_raw == 65535) ? 0.0f : (packet.chan3_raw - 1500) / 500.0f;
                        float ch4 = (packet.chan4_raw == 0 || packet.chan4_raw == 65535) ? 0.0f : (packet.chan4_raw - 1500) / 500.0f;

                        input->axes[1] = ch1; // Roll
                        input->axes[0] = ch2; // Pitch
                        input->axes[2] = ch3; // Throttle
                        input->axes[3] = ch4; // Yaw
                        input->valid = true;
                        input->timestamp = udp_get_time_seconds();
                        input_updated = true;
                        
                        printf("MAVLink: RC_OVERRIDE Ch1=%d Ch2=%d Ch3=%d Ch4=%d\n",
                               packet.chan1_raw, packet.chan2_raw, packet.chan3_raw, packet.chan4_raw);
                        break;
                    }
                    
                    case MAVLINK_MSG_ID_COMMAND_LONG: {
                        mavlink_command_long_t packet;
                        mavlink_msg_command_long_decode(&msg, &packet);
                        
                        // Check if command is for us
                        if (packet.target_system == MAV_SYSTEM_ID || packet.target_system == 0) {
                            if (packet.command == MAV_CMD_COMPONENT_ARM_DISARM) {
                                if (packet.param1 == 1.0f) {
                                    input->cmd_arm = true;
                                    printf("MAVLink CMD: ARM\n");
                                } else {
                                    input->cmd_disarm = true;
                                    printf("MAVLink CMD: DISARM\n");
                                }
                                send_command_ack(client, packet.command, MAV_RESULT_ACCEPTED);
                                input_updated = true;
                            } else if (packet.command == MAV_CMD_NAV_TAKEOFF) {
                                input->cmd_takeoff = true;
                                printf("MAVLink CMD: TAKEOFF\n");
                                send_command_ack(client, packet.command, MAV_RESULT_ACCEPTED);
                                input_updated = true;
                            } else if (packet.command == 176) { // MAV_CMD_DO_SET_MODE
                                printf("MAVLink CMD: SET_MODE (Mode=%.0f, Custom=%.0f)\n", packet.param1, packet.param2);
                                
                                // Capture the requested mode for the main loop to handle
                                input->cmd_set_mode = true;
                                input->target_mode = (uint8_t)packet.param1;
                                input->target_custom_mode = (uint32_t)packet.param2;
                                input_updated = true;

                                // Helper logging
                                if (input->target_mode == 16) { 
                                    printf("MAVLink: Requesting GUIDED mode\n");
                                } else if (input->target_mode == 4) { 
                                    printf("MAVLink: Requesting CUSTOM mode\n");
                                } else if (input->target_mode == 1) { 
                                    printf("MAVLink: Requesting MANUAL mode\n");
                                }
                                
                                send_command_ack(client, packet.command, MAV_RESULT_ACCEPTED);
                            } else if (packet.command == 511 || packet.command == 512 || packet.command == 521) {
                                // These are non-standard commands, likely from custom GCS implementations
                                // Silently acknowledge but don't process to prevent spam
                                printf("MAVLink CMD: Custom command %d (ignored)\n", packet.command);
                                send_command_ack(client, packet.command, MAV_RESULT_ACCEPTED);
                            } else {
                                printf("MAVLink CMD: %d (unknown)\n", packet.command);
                                // Send UNSUPPORTED for truly unknown commands
                                send_command_ack(client, packet.command, MAV_RESULT_UNSUPPORTED);
                            }
                        }
                        break;
                    }
                    
                    case MAVLINK_MSG_ID_HEARTBEAT:
                        // printf("MAVLink: Heartbeat from %d/%d\n", msg.sysid, msg.compid);
                        break;
                }
            }
        }
        return input_updated ? 1 : 0;
    }
    
    return 0;
}