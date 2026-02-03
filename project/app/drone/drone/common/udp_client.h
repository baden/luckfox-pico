#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <common/mavlink.h>

// MAVLink connection settings
#define UDP_SERVER_HOST "10.8.0.11"
#define UDP_SERVER_PORT 14550
#define MAV_SYSTEM_ID 1
#define MAV_COMPONENT_ID MAV_COMP_ID_AUTOPILOT1

// Telemetry/Heartbeat intervals
#define HEARTBEAT_INTERVAL_MS 1000
#define TELEMETRY_INTERVAL_MS 100 // 10Hz

typedef struct {
    int sockfd;
    struct sockaddr_in server_addr;
    bool connected;
    double last_heartbeat_time;
    double last_telemetry_time;
} udp_client_t;

// Simplified control input structure (mapped from MANUAL_CONTROL or RC_CHANNELS)
typedef struct {
    float axes[4];      // 0:Pitch, 1:Roll, 2:Throt, 3:Yaw (Normalized -1.0 to 1.0)
    uint16_t buttons;   // Button mask
    bool valid;
    double timestamp;
    
    // Command flags (one-shot)
    bool cmd_arm;
    bool cmd_disarm;
    bool cmd_takeoff;
    bool cmd_set_mode;
    uint8_t target_mode;      // MAVLink base_mode
    uint32_t target_custom_mode;
} udp_control_input_t;

// Initialize UDP client
int udp_client_init(udp_client_t* client);

// Cleanup UDP client
void udp_client_cleanup(udp_client_t* client);

// Connect to server (setup address)
int udp_client_connect(udp_client_t* client);

// Reconnect
int udp_client_reconnect(udp_client_t* client);

// Check status
bool udp_client_is_connected(const udp_client_t* client);

// Send Heartbeat
int udp_client_send_heartbeat(udp_client_t* client, bool armed, uint8_t base_mode, uint32_t custom_mode);

// Send Telemetry (Attitude/Status/GPS)
int udp_client_send_telemetry(udp_client_t* client, float axis0, float axis1, 
                              int lebidka_state, int aktuator_state);

// Receive and process MAVLink messages
// Returns 1 if new control input available, 0 otherwise
int udp_client_receive(udp_client_t* client, udp_control_input_t* input);

// Utils
double udp_get_time_seconds(void);

#endif // UDP_CLIENT_H