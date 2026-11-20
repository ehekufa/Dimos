#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Color depth: 32 (XRGB8888 for Limine Framebuffer) */
#define LV_COLOR_DEPTH 32
#define LV_COLOR_16_SWAP 0

/* Memory management: custom DimaOS kernel pool */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM == 1
    #define LV_MEM_CUSTOM_INCLUDE "mm.h"
    #define LV_MEM_CUSTOM_ALLOC   ExAllocatePool
    #define LV_MEM_CUSTOM_FREE    ExFreePool
    #define LV_MEM_CUSTOM_REALLOC ExReallocatePool
#endif

#define LV_MEMCPY_MEMSET_STD 1
#define LV_STDLIB_INCLUDE "rtl.h"

/* HAL / Tick */
#define LV_TICK_CUSTOM 0
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 16

/* Drawing */
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4
#define LV_LAYER_SIMPLE_BUF_SIZE (24 * 1024)

/* Logging */
#define LV_USE_LOG 0

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      1
#define LV_USE_WIN        1
#define LV_USE_LIST       1
#define LV_USE_MSGBOX     1

/* Themes */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 0

/* Layouts */
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Examples / Demos off */
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
