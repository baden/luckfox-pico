#include "web_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Event handler for Mongoose
static void fn(struct mg_connection *c, int ev, void *ev_data) {
    web_server_t *server = (web_server_t *)c->fn_data;

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        
        // Check for WebSocket upgrade request
        if (mg_http_get_header(hm, "Upgrade") != NULL) {
            mg_ws_upgrade(c, hm, NULL);
        } else {
            // Serve files from the configured root
            struct mg_http_serve_opts opts = {
                .root_dir = server->www_root,
                .extra_headers = "Access-Control-Allow-Origin: *\r\n"
            };
            mg_http_serve_dir(c, hm, &opts);
        }
        
    } else if (ev == MG_EV_WS_OPEN) {
        server->connected = true;
        // printf("Web: WS connected\n");
        
    } else if (ev == MG_EV_WS_MSG) {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        
        // Ensure null-termination for parsing
        char buf[1024];
        int len = wm->data.len;
        if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
        memcpy(buf, wm->data.buf, len);
        buf[len] = '\0';
        
        // Parse JSON control input using standard string search
        // Expected: {"arm":true,"a0":0.5,"a1":0.2,"a2":0.0,"a3":0.0,...}
        
        float a0=0, a1=0, a2=0, a3=0;
        int arm=0, disarm=0, lb=0, ak=0;
        
        // A0 (Roll)
        char* p = strstr(buf, "\"a0\":");
        if (p) a0 = strtof(p + 5, NULL);
        
        // A1 (Pitch)
        p = strstr(buf, "\"a1\":");
        if (p) a1 = strtof(p + 5, NULL);
        
        // A2 (Throttle)
        p = strstr(buf, "\"a2\":");
        if (p) a2 = strtof(p + 5, NULL);
        
        // A3 (Yaw)
        p = strstr(buf, "\"a3\":");
        if (p) a3 = strtof(p + 5, NULL);
        
        // ARM/DISARM
        // Check "arm":true or "arm":false (new frontend format)
        p = strstr(buf, "\"arm\":true");
        if (p) arm = 1;
        p = strstr(buf, "\"arm\":false");
        if (p) disarm = 1; 
        
        // Legacy "cmd_arm" checks just in case
        p = strstr(buf, "\"cmd_arm\":true");
        if (p) arm = 1;
        p = strstr(buf, "\"cmd_disarm\":true");
        if (p) disarm = 1;
        
        // Lebidka
        p = strstr(buf, "\"lb\":");
        if (p) lb = atoi(p + 5);
        
        // Aktuator
        p = strstr(buf, "\"ak\":");
        if (p) ak = atoi(p + 5);
        
        // Update temporary input storage
        server->temp_input.axes[0] = a0;
        server->temp_input.axes[1] = a1;
        server->temp_input.axes[2] = a2;
        server->temp_input.axes[3] = a3;
        server->temp_input.cmd_arm = (arm == 1);
        server->temp_input.cmd_disarm = (disarm == 1);
        server->temp_input.lebidka_val = lb;
        server->temp_input.aktuator_val = ak;
        server->temp_input.valid = true;
        server->temp_input.timestamp = 0; // Main loop will set time
        
        server->has_new_input = true;
        
    } else if (ev == MG_EV_CLOSE) {
        // Check if any other connection is still open?
        // For simplicity, we just leave connected=true until we specifically check later,
        // or toggle it here. But iterating lists is safer in the main loop if needed.
        // Actually, we can just check if connection list is empty in run_step.
    }
}

int web_server_init(web_server_t* server, int port, const char* www_root) {
    memset(server, 0, sizeof(web_server_t));
    server->port = port;
    server->www_root = www_root;
    
    mg_mgr_init(&server->mgr);
    
    char url[32];
    snprintf(url, sizeof(url), "0.0.0.0:%d", port);
    
    if (mg_http_listen(&server->mgr, url, fn, server) == NULL) {
        printf("Web: Failed to listen on %s\n", url);
        return -1;
    }
    
    printf("Web server listening on %s, serving %s\n", url, www_root);
    return 0;
}

void web_server_cleanup(web_server_t* server) {
    mg_mgr_free(&server->mgr);
}

void web_server_run_step(web_server_t* server, web_control_input_t* input) {
    // Poll Mongoose (non-blocking)
    mg_mgr_poll(&server->mgr, 0);
    
    // Check connection status
    bool any_ws = false;
    for (struct mg_connection *c = server->mgr.conns; c != NULL; c = c->next) {
        if (c->is_websocket) {
            any_ws = true;
            break;
        }
    }
    server->connected = any_ws;
    
    // Transfer input if new data arrived
    if (server->has_new_input && input != NULL) {
        *input = server->temp_input;
        server->has_new_input = false;
    }
}

void web_server_send_telemetry(web_server_t* server, float r, float p, float y, float t, bool armed) {
    if (!server->connected) return;
    
    char json[256];
    int len = snprintf(json, sizeof(json), 
             "{\"r\":%.2f,\"p\":%.2f,\"y\":%.2f,\"t\":%.2f,\"armed\":%s}", 
             r, p, y, t, armed ? "true" : "false");
             
    for (struct mg_connection *c = server->mgr.conns; c != NULL; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, json, len, WEBSOCKET_OP_TEXT);
        }
    }
}
