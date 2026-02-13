#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Path to the persistent settings file
#define SETTINGS_FILE_PATH "/oem/usr/share/drone/settings.json"

// Settings structure
typedef struct {
    float steering_damping;        // 0.0 to 1.0
    float steering_damping_curve;  // -1.0 to 1.0
    char udp_host[64];             // IP address or hostname
    int udp_port;                  // Port number
} app_settings_t;

// Initialize settings (load from file -> env -> defaults)
void settings_init(void);

// Save current settings to file
void settings_save(void);

// Get a copy of all settings (thread-safe)
app_settings_t settings_get(void);

// Set all settings (thread-safe)
void settings_set(const app_settings_t *new_settings);

// Get specific values
float settings_get_steering_damping(void);
float settings_get_steering_damping_curve(void);
void settings_get_udp_host(char *buffer, size_t size);
int settings_get_udp_port(void);

// Set specific values
void settings_set_steering_damping(float value);
void settings_set_steering_damping_curve(float value);
void settings_set_udp_host(const char *host);
void settings_set_udp_port(int port);

// Get settings as JSON string (must be freed by caller)
char *settings_get_json_string(void);

// Apply settings from JSON string
// Returns 0 on success, -1 on failure
int settings_apply_json_string(const char *json_str);

#endif // SETTINGS_H
