#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>
#include <stdint.h>

#include "lv_conf_ssd1306.h"
#include "lvgl.h"

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static long int screensize = 0;
static char *fbp = 0;
static int fbfd = -1;

/* Function to reverse bits in a byte - MSB first mapping */
static inline uint8_t reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

/* Flush the content of the internal buffer the specific area on the display
 * You can use DMA or any hardware acceleration to do this operation in the background but
 * 'lv_disp_flush_ready()' has to be called when it's finished.
 */
static void ssd1306_fb_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    if(fbp == 0 || fbfd == -1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    /* For monochrome 1-bit display */
    int32_t x, y;
    for(y = area->y1; y <= area->y2; y++) {
        for(x = area->x1; x <= area->x2; x++) {
            if(x < vinfo.xres && y < vinfo.yres) {
                /* Calculate byte position for 1-bit framebuffer */
                int byte_offset = (x / 8) + (y * finfo.line_length);
                int bit_in_byte = x % 8;
                
                /* Get pixel value from LVGL color */
                uint8_t pixel_val = color_p->full;
                
                /* Read current byte, reverse it, modify, reverse back */
                uint8_t current_byte = fbp[byte_offset];
                uint8_t reversed_byte = reverse_bits(current_byte);
                
                if(pixel_val & 0x01) {
                    reversed_byte |= (1 << bit_in_byte);  // Set bit in reversed order
                } else {
                    reversed_byte &= ~(1 << bit_in_byte); // Clear bit in reversed order
                }
                
                /* Reverse back and write to framebuffer */
                fbp[byte_offset] = reverse_bits(reversed_byte);
                color_p++;
            }
        }
    }

    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    /* Open the framebuffer device */
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        return;
    }

    /* Get variable screen information */
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        close(fbfd);
        return;
    }

    /* Get fixed screen information */
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        close(fbfd);
        return;
    }

    /* Figure out the size of the screen in bytes */
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

    /* Map the device to memory */
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        close(fbfd);
        return;
    }

    printf("Framebuffer info: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

    /* Initialize LVGL display driver */
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf_1[128 * 10]; /* Buffer size for 128x32 display */
    
    lv_disp_draw_buf_init(&draw_buf, buf_1, NULL, 128 * 10);

    /*Initialize the display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    
    disp_drv.hor_res = vinfo.xres;
    disp_drv.ver_res = vinfo.yres;
    disp_drv.flush_cb = ssd1306_fb_flush;
    disp_drv.draw_buf = &draw_buf;
    
    lv_disp_drv_register(&disp_drv);
}

void lv_port_disp_deinit(void)
{
    if (fbp != 0 && (int)fbp != -1) {
        munmap(fbp, screensize);
    }
    
    if (fbfd != -1) {
        close(fbfd);
    }
    
    fbfd = -1;
    fbp = 0;
}