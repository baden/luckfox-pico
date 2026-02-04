#include <stdio.h>
// #include "ssd1306.h"
// #include "ssd1306_fonts.h"
// #include "nano_gfx.h"

// #include "ssd1306_i2c.h"

#define U8G2_USE_LINUX_I2C 0
#define U8G2_USE_LINUX_FB 1

// #include "linux-i2c.h"
#include "u8g2.h"

#if defined(U8G2_USE_LINUX_I2C) && U8G2_USE_LINUX_I2C == 1
#define SSD1306_ADDR 0x3C
#endif

#if defined(U8G2_USE_LINUX_FB) && U8G2_USE_LINUX_FB == 1
const char *fb_dev = "/dev/fb0";
#endif

u8g2_t u8g2;

int oled_init(void)
{
    printf("OLED: Initializing SSD1306 display...\n");

    #if defined(U8G2_USE_LINUX_I2C) && U8G2_USE_LINUX_I2C == 1
    u8g2_Setup_ssd1306_i2c_128x32_univision_1(&u8g2, U8G2_R0, u8x8_byte_linux_i2c, u8x8_linux_i2c_delay);
    u8g2_SetI2CAddress(&u8g2, SSD1306_ADDR /*<< 1*/); // Shift address for 7-bit ?
    #endif

    #if defined(U8G2_USE_LINUX_FB) && U8G2_USE_LINUX_FB == 1
    u8g2_SetupLinuxFb(&u8g2, U8G2_R0, fb_dev);
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