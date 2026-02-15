#pragma once
#include <stdbool.h>

typedef struct {
    bool armed;
    float axis0;        // Roll (-1.0 to 1.0)
    float axis1;        // Pitch (-1.0 to 1.0)
    bool udp_connected; // MAVLink active
    bool web_connected; // WebSocket active
    bool crsf_connected;// CRSF active

    // Network Status
    int eth_status;     // 0=Down, 1=Up, 2=Ping
    int wg_status;      // 0=Down, 1=Up, 2=Ping
    bool op_connected;  // Operator Ping
    bool dev1_ping;     // Device 1 (10.0.7.101)
    bool dev2_ping;     // Device 2 (10.0.7.102)
    bool dev3_ping;     // Device 3 (10.0.7.103)
} oled_status_t;

// Initialize OLED display
int oled_init(bool show_loading);
void oled_deinit(void);
void oled_clear(void);
int oled_display(const oled_status_t* status);
void oled_draw_pixel(int x, int y, bool on);
void oled_print_reboot(void); // New function prototype
