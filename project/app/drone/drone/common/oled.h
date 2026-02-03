#pragma once

// Initialize OLED display
int oled_init(void);
void oled_clear(void);
void oled_display(void);
void oled_draw_pixel(int x, int y, bool on);
