#include "web_server.h"
#include "cJSON.h"
#include "settings.h"
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
        
        // Parse JSON
        cJSON *json = cJSON_Parse(buf);
        if (!json) return;

        // 1. Handle Settings Requests
        cJSON *get_settings = cJSON_GetObjectItem(json, "get_settings");
        if (cJSON_IsBool(get_settings) && cJSON_IsTrue(get_settings)) {
            char *settings_str = settings_get_json_string();
            if (settings_str) {
                // Wrap in "settings" key for client context if needed, or send flat
                // sending flat {"udp_host":...}
                mg_ws_send(c, settings_str, strlen(settings_str), WEBSOCKET_OP_TEXT);
                free(settings_str);
            }
        }

        cJSON *set_settings = cJSON_GetObjectItem(json, "settings");
        if (cJSON_IsObject(set_settings)) {
            char *settings_str = cJSON_PrintUnformatted(set_settings);
            if (settings_str) {
                settings_apply_json_string(settings_str);
                free(settings_str);
                // Optionally Ack?
            }
        }

        // 2. Handle Control Inputs
        // Expected: {"arm":true,"a0":0.5,"a1":0.2,"a2":0.0,"a3":0.0,...}
        
        cJSON *item;
        
        // Axes
        item = cJSON_GetObjectItem(json, "a0");
        if (cJSON_IsNumber(item)) server->temp_input.axes[0] = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(json, "a1");
        if (cJSON_IsNumber(item)) server->temp_input.axes[1] = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(json, "a2");
        if (cJSON_IsNumber(item)) server->temp_input.axes[2] = (float)item->valuedouble;
        
        item = cJSON_GetObjectItem(json, "a3");
        if (cJSON_IsNumber(item)) server->temp_input.axes[3] = (float)item->valuedouble;

        // Arming
        bool armed_cmd = false;
        bool disarmed_cmd = false;

        item = cJSON_GetObjectItem(json, "arm");
        if (cJSON_IsBool(item)) {
            if (cJSON_IsTrue(item)) armed_cmd = true;
            else disarmed_cmd = true; // "arm": false means disarm? 
            // Usually UI sends "arm":true to arm, "arm":false to disarm.
            // Existing code had "arm":false -> disarm = 1.
        }
        
        // Legacy/Alternative checks
        if (cJSON_IsTrue(cJSON_GetObjectItem(json, "cmd_arm"))) armed_cmd = true;
        if (cJSON_IsTrue(cJSON_GetObjectItem(json, "cmd_disarm"))) disarmed_cmd = true;

        server->temp_input.cmd_arm = armed_cmd;
        server->temp_input.cmd_disarm = disarmed_cmd;

        // Aux
        item = cJSON_GetObjectItem(json, "lb");
        if (cJSON_IsNumber(item)) server->temp_input.lebidka_val = item->valueint;
        
        item = cJSON_GetObjectItem(json, "ak");
        if (cJSON_IsNumber(item)) server->temp_input.aktuator_val = item->valueint;

        // Command: restart
        item = cJSON_GetObjectItem(json, "command");
        if (cJSON_IsString(item) && strcmp(item->valuestring, "restart") == 0) {
            server->temp_input.cmd_restart = true;
        }

        server->temp_input.valid = true;
        server->temp_input.timestamp = 0; // Main loop will set time
        server->has_new_input = true;

        cJSON_Delete(json);
        
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
    mg_log_set(MG_LL_ERROR); // Disable debug logs to reduce spam
    
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
        
        // Clear one-shot commands
        server->temp_input.cmd_restart = false;
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
