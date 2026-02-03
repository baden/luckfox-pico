#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include "drone_types.h"

// Input structure from Web Client (similar to UDP)
typedef struct {
    float axes[4];      // Pitch, Roll, Throttle, Yaw (-1.0 to 1.0)
    bool cmd_arm;
    bool cmd_disarm;
    bool valid;
    double timestamp;   // Time of last valid message
    
    // Aux controls
    int lebidka_val;    // -1 (up), 0, 1 (down)
    int aktuator_val;   // -1, 0, 1
} web_control_input_t;

typedef struct {
    int server_fd;
    int client_fd;
    bool connected;
    int port;
    const char* www_root;
} web_server_t;

// Initialize the web server
int web_server_init(web_server_t* server, int port, const char* www_root);

// Process requests (should be called in a loop)
// Returns 0 on success, -1 on fatal error
void web_server_run_step(web_server_t* server, web_control_input_t* input);

// Send telemetry to connected websocket client
void web_server_send_telemetry(web_server_t* server, float r, float p, float y, float t, bool armed);

// Cleanup
void web_server_cleanup(web_server_t* server);

#endif
