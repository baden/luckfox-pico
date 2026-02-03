#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
// #include "nano_gfx.h"

// #include "ssd1306_i2c.h"

int oled_init(void)
{
    printf("OLED: Initializing SSD1306 display...\n");

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

    return 0;
}