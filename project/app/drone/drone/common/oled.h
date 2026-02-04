#pragma once
#include <stdbool.h>

typedef struct {
    bool armed;
    float axis0;        // Roll (-1.0 to 1.0)
    float axis1;        // Pitch (-1.0 to 1.0)
    bool udp_connected; // MAVLink active
    bool web_connected; // WebSocket active
    bool crsf_connected;// CRSF active
} oled_status_t;

// Initialize OLED display
int oled_init(void);
void oled_clear(void);
void oled_display(const oled_status_t* status);
void oled_draw_pixel(int x, int y, bool on);
