#include <stdio.h>
#include "oled.h"
// #include "ssd1306.h"
// #include "ssd1306_fonts.h"
// #include "nano_gfx.h"

// #include "ssd1306_i2c.h"

#define U8G2_USE_LINUX_I2C 0
#define U8G2_USE_LINUX_FB 1

// #include "linux-i2c.h"
#include "u8g2/csrc/u8g2.h"

#if defined(U8G2_USE_LINUX_I2C) && U8G2_USE_LINUX_I2C == 1
#define SSD1306_ADDR 0x3C
#endif

#if defined(U8G2_USE_LINUX_FB) && U8G2_USE_LINUX_FB == 1
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
const char *fb_dev = "/dev/fb0";

// Глобальні змінні для вашого "контролера" оновлень
int my_fb_fd = -1;
struct fb_var_screeninfo my_vinfo;
void *my_fb_ptr = NULL;
size_t my_fb_size = 0;
#endif

u8g2_t u8g2;

#if defined(U8G2_USE_LINUX_FB) && U8G2_USE_LINUX_FB == 1
static void flush_fb(void) {
    // 2. ПРИМУСОВЕ ОНОВЛЕННЯ (якщо відкрили дескриптор)
    if (my_fb_fd != -1) {
        // Метод А: msync (кажемо ядру, що пам'ять змінена)
        msync(my_fb_ptr, my_fb_size, MS_SYNC);

        // Метод Б: PAN_DISPLAY (найефективніший "пінок" для драйверів Luckfox/Rockchip)
        // Ми просто кажемо відобразити ту саму область з нульовим зміщенням
        my_vinfo.yoffset = 0;
        // my_vinfo.activate = FB_ACTIVATE_NOW; // Додайте це для впевненості
        ioctl(my_fb_fd, FBIOPAN_DISPLAY, &my_vinfo);
    }

}
#else
#define flush_fb()
#endif

int oled_init(void)
{
    printf("OLED: Initializing SSD1306 display...\n");

    #if defined(U8G2_USE_LINUX_I2C) && U8G2_USE_LINUX_I2C == 1
    u8g2_Setup_ssd1306_i2c_128x32_univision_1(&u8g2, U8G2_R0, u8x8_byte_linux_i2c, u8x8_linux_i2c_delay);
    u8g2_SetI2CAddress(&u8g2, SSD1306_ADDR /*<< 1*/); // Shift address for 7-bit ?
    #endif

    #if defined(U8G2_USE_LINUX_FB) && U8G2_USE_LINUX_FB == 1
    u8g2_SetupLinuxFb(&u8g2, U8G2_R0, fb_dev);

    // ВЛАСНИЙ КОД для керування оновленням:
    my_fb_fd = open(fb_dev, O_RDWR);
    if (my_fb_fd != -1) {
        struct fb_fix_screeninfo finfo;
        if (ioctl(my_fb_fd, FBIOGET_FSCREENINFO, &finfo) == 0 &&
            ioctl(my_fb_fd, FBIOGET_VSCREENINFO, &my_vinfo) == 0) {

            my_fb_size = finfo.smem_len;
            // Відображаємо ту саму пам'ять. Оскільки MAP_SHARED, зміни від u8g2 будуть тут видні
            my_fb_ptr = mmap(0, my_fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, my_fb_fd, 0);
        }
    }
    #endif

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // Wake up display
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_maniac_tf);
    u8g2_SetFontRefHeightText(&u8g2);
    u8g2_SetFontPosTop(&u8g2);

    u8g2_DrawStr(&u8g2, 0, 0, "Loading...");

    // u8g2_SetFont(&u8g2, u8g2_font_sticker100complete_tr);
    // u8g2_SetFontRefHeightText(&u8g2);
    // u8g2_SetFontPosTop(&u8g2);
    // u8g2_DrawStr(&u8g2, 0, 0, ".");


    u8g2_SendBuffer(&u8g2);
    flush_fb();


    #if 0
    // ssd1306_128x32_i2c_init();
    ssd1306_platform_i2cConfig_t config;
    // config.busId = 3;       // номер шини (якщо /dev/i2c-3)
    // config.devAddr = 0x3C;  // адреса дисплея
    // ssd1306_i2cInitEx(config.busId, config.devAddr);

    #define OLED_BUS_ID 3
    #define OLED_ADDR 0x3C
    ssd1306_platform_i2cInit(OLED_BUS_ID, OLED_ADDR, &config);

    ssd1306_128x32_i2c_init();

    ssd1306_clearScreen();
    ssd1306_setFixedFont(ssd1306xled_font6x8);
    ssd1306_printFixed(0, 8, "Luckfox RV1106", STYLE_NORMAL);

    #endif

    return 0;
}


void oled_display(const oled_status_t* status)
{
    u8g2_ClearBuffer(&u8g2);
    
    // --- 1. ARM State (Top Left) ---
    u8g2_SetFont(&u8g2, u8g2_font_profont12_tf); // Small clear font
    if (status->armed) {
        u8g2_DrawStr(&u8g2, 0, 8, "ARM");
    } else {
        u8g2_DrawStr(&u8g2, 0, 8, "DIS");
    }

    // --- 2. Connection Status (Below ARM) ---
    // M = MAVLink/UDP, W = Web, R = CRSF
    int y_status = 20;
    int x_status = 0;
    
    if (status->udp_connected) {
        u8g2_DrawStr(&u8g2, x_status, y_status, "M");
        x_status += 10;
    }
    if (status->web_connected) {
        u8g2_DrawStr(&u8g2, x_status, y_status, "W");
        x_status += 10;
    }
    if (status->crsf_connected) {
        u8g2_DrawStr(&u8g2, x_status, y_status, "R");
    }

    // --- 3. Animation (Moving Dot) ---
    static int anim_x = 0;
    static int anim_dir = 1;
    
    // Animate in a small area below status, e.g., line 28-30
    u8g2_DrawPixel(&u8g2, 10 + anim_x, 30);
    
    anim_x += anim_dir;
    if (anim_x > 20) anim_dir = -1;
    if (anim_x < 0) anim_dir = 1;

    // --- 4. Stick Visualizer (Right Side) ---
    // Circle d=32 -> r=16. Center around x=96 (128-32), y=16
    int cx = 96;
    int cy = 16;
    int r = 15; // Slightly smaller to fit
    
    u8g2_DrawCircle(&u8g2, cx, cy, r, U8G2_DRAW_ALL);
    u8g2_DrawLine(&u8g2, cx - r, cy, cx + r, cy); // Horizontal axis
    u8g2_DrawLine(&u8g2, cx, cy - r, cx, cy + r); // Vertical axis
    
    // Stick Position dot
    // Map -1.0..1.0 to -r..r
    // axis0 is Roll (X), axis1 is Pitch (Y)
    int dot_x = cx + (int)(status->axis0 * r);
    int dot_y = cy - (int)(status->axis1 * r); // Invert Y because screen Y+ is down
    
    // Draw filled circle for dot (r=2)
    u8g2_DrawDisc(&u8g2, dot_x, dot_y, 2, U8G2_DRAW_ALL);

    u8g2_SendBuffer(&u8g2);
    flush_fb();
}
