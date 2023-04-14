#pragma once
#include <stdint.h>

typedef struct {
    uint32_t width, height;
} Monitor;

/* Commands */
#define TERMINAL_CMD "alacritty &"
#define WEB_BROWSER_CMD "chromium &"

/* Keybindings */
#define MASTER_KEY Mod1Mask // super key
#define TERMINAL_OPEN_KEY XK_Return // enter key
#define WM_TERMINATE_KEY XK_C
#define WEB_BROWSER_KEY XK_W
#define WINDOW_CLOSE_KEY XK_Q
#define WINDOW_CYCLE_KEY XK_Tab

/* Window properties */
#define WINDOW_BG_COLOR 0x000000 // black
#define WINDOW_BORDER_WIDTH 3 
#define WINDOW_BORDER_COLOR 0x2b34d9 // darkblue

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 2
const Monitor Monitors[MONITOR_COUNT] = { (Monitor){ .width = 1920, .height = 1080 },
                                          (Monitor){ .width = 2560, .height = 1440 }};
