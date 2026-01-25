#define LV_CONF_H_SIMPLE

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH 1

/*Swap the 2 bytes of RGB565 color. Useful if the display has an 8-bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP 0

/*Enable more chroma keying modes. Requires >16bit color depth. Might increase performance.*/
#define LV_COLOR_SCREEN_TRANSP 0

/*Enable anti-aliasing, lines will be smoother. Requires >16 bit color depth*/
#define LV_ANTIALIAS 0

/*====================
   MEMORY SETTINGS
 *====================*/

/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()`*/
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#else
    #define LV_MEM_SIZE (32U * 1024U)          /*Size memory used by `lv_mem_alloc()` in bytes (>= 2kB)*/
    #define LV_MEM_ADR 0                       /*Set an address for memory pool instead of allocation it. 0: not used*/
#endif

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh period. LVD will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD 30        /*[ms]*/

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD 30     /*[ms]*/

/*Use a custom tick source. Tells LVGL how many milliseconds elapsed where the
 *custom tick source elapsed*/
#define LV_TICK_CUSTOM 0

/*===================
   FEATURE CONFIGURATION
 *==================*/

/*-------------
 * Drawing
 *-----------*/

/*Enable the built in fonts*/
#define LV_FONT_MONTSERRAT_8  1
#define LV_FONT_MONTSERRAT_10 1

/*Always set a default font from the built-in fonts even if LV_FONT_CUSTOM is set*/
#define LV_FONT_DEFAULT &lv_font_montserrat_8

/*Enable drawing very contiguous letters with their kerning tables.*/
#define LV_USE_FONT_KERNING 0

/*-------------
 * Logging
 *-----------*/
/*Enable the log module*/
#define LV_USE_LOG 0
#if LV_USE_LOG
    /*How important log should be added:*/
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
    /*1: Print log with 'printf'*/
    #define LV_LOG_PRINTF   1
#endif

/*-------------
 * Asserts
 *-----------*/

/*Enable asserts if an operation is failed or an invalid data is found.*/
#define LV_USE_ASSERT_NULL          1   /*Check if the parameter is NULL. Very recommended. Bespoke rendering is still possible with NULL.*/
#define LV_USE_ASSERT_MALLOC        1   /*Checks is the memory is successfully allocated or no. Very Recommended.*/
#define LV_USE_ASSERT_STYLE         0   /*Check if the styles are properly initialized. Very time consuming.*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*Check the integrity of `lv_mem` after critical operations. Very time consuming.*/
#define LV_USE_ASSERT_OBJ           0   /*Check the object's type and existence (e.g. not deleted). Very time consuming.*/

/*-------------
 * Others
 *-----------*/

/*1: Enable API to take snapshots for objects*/
#define LV_USE_SNAPSHOT 0

/*1: Enable Monochrome theme for 1-bit displays*/
#define LV_USE_THEME_MONO 1

/*===================
* LVGL SETTINGS
*===================*/

/*Default Dot Per Inch. Used to initialize default sizes such as widget sized, style paddings.
 *(Not so important, you can adjust it to modify default sizes and spaces)*/
#define LV_DPI_DEF 130     /*[px/inch]*/

/*=================
   DEVICE SETTINGS
 *=================*/

/*Maximum buffer size to allocate for rotation. Only used if LV_DISP_ROT_MAX_BUF != 0*/
#define LV_DISP_ROT_MAX_BUF 0

#define LV_ENABLE_GLOBAL_CUSTOM 0
#if LV_ENABLE_GLOBAL_CUSTOM
    /*Header to include for the custom 'lv_global' function*/
    #define LV_GLOBAL_CUSTOM_INCLUDE "something.h"
#endif

/*==================
   SPECIAL SETTINGS
 *==================*/

/*==================
   TESTING SETTINGS
 *==================*/

/*==================
   EXAMPLE SETTINGS
 *==================*/

/*==================
   DEMO SETTINGS
 *==================*/

/*Enable the built-in Benchmark Demo*/
#define LV_USE_DEMO_BENCHMARK 0

/*Enable the built-in Music Player Demo*/
#define LV_USE_DEMO_MUSIC 0

/*Enable the built-in Recorder Demo*/
#define LV_USE_DEMO_RECORDER 0

/*Enable the built-in Stress Demo*/
#define LV_USE_DEMO_STRESS 0

/*Enable the built-in Widgets Demo*/
#define LV_USE_DEMO_WIDGETS 0

/*==================
   PORT CONFIGURATION
 *==================*/

/*Resolution of the display*/
#define LV_HOR_RES_MAX          (128)
#define LV_VER_RES_MAX          (32)