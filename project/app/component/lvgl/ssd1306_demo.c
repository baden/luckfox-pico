#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl.h"

/* Forward declarations from lv_port_ssd1306.c */
void lv_port_disp_init(void);
void lv_port_disp_deinit(void);

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

int main(void)
{
    /* Setup signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize LVGL */
    lv_init();
    
    /* Initialize the display driver */
    lv_port_disp_init();
    
    printf("LVGL SSD1306 demo started for 128x32 display\n");
    
    /* Create a simple demo optimized for small display */
    
    /* Create title label - fits in 128x32 */
    lv_obj_t * title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "SSD1306 Test");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
    
    /* Create status label */
    lv_obj_t * status = lv_label_create(lv_scr_act());
    lv_label_set_text(status, "Running...");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_8, 0);
    lv_obj_align(status, LV_ALIGN_TOP_LEFT, 2, 14);
    
    /* Create a small progress bar */
    lv_obj_t * bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(bar, 80, 8);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    
    /* Animation counter */
    int counter = 0;
    
    printf("Press Ctrl+C to exit\n");
    
    /* Main loop */
    while(running) {
        /* Update progress bar */
        counter = (counter + 2) % 100;
        lv_bar_set_value(bar, counter, LV_ANIM_OFF);
        
        /* Update status text every 50 cycles */
        if(counter % 50 == 0) {
            static char status_text[32];
            snprintf(status_text, sizeof(status_text), "Count: %d", counter);
            lv_label_set_text(status, status_text);
        }
        
        /* Handle LVGL tasks */
        lv_timer_handler();
        usleep(10000); /* 10ms delay */
    }
    
    printf("\nCleaning up...\n");
    
    /* Cleanup */
    lv_port_disp_deinit();
    return 0;
}