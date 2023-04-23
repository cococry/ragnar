#pragma once
#include <stdint.h>

typedef struct {
    uint32_t width, height;
} Monitor;

/* Commands */
#define TERMINAL_CMD "alacritty &"
#define WEB_BROWSER_CMD "brave &"

/* Keybindings */
#define MASTER_KEY Mod1Mask 
#define TERMINAL_OPEN_KEY XK_Return
#define WEB_BROWSER_OPEN_KEY XK_W
#define WM_TERMINATE_KEY XK_C
#define WINDOW_CLOSE_KEY XK_Q
#define WINDOW_CYCLE_KEY XK_Tab
#define WINDOW_ADD_TO_LAYOUT_KEY XK_space
#define WINDOW_LAYOUT_MOVE_UP_KEY XK_Up
#define WINDOW_LAYOUT_MOVE_DOWN_KEY XK_Down
#define WINDOW_LAYOUT_INCREASE_MASTER_X_KEY XK_L
#define WINDOW_LAYOUT_DECREASE_MASTER_X_KEY XK_H
#define WINDOW_LAYOUT_INCREASE_SLAVE_Y_KEY XK_K
#define WINDOW_LAYOUT_DECREASE_SLAVE_Y_KEY XK_J
#define WINDOW_GAP_INCREASE_KEY XK_plus
#define WINDOW_GAP_DECREASE_KEY XK_minus
#define WINDOW_FULLSCREEN_KEY XK_F
#define WINDOW_LAYOUT_TILED_MASTER_KEY XK_T
#define WINDOW_LAYOUT_FLOATING_KEY XK_R

/* Window properties */
#define WINDOW_BG_COLOR 0x32302f // black
#define WINDOW_BORDER_WIDTH 3 
#define WINDOW_BORDER_COLOR 0x242424 //  gray

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 2
const Monitor Monitors[MONITOR_COUNT] = { (Monitor){ .width = 1920, .height = 1080 }, (Monitor){ .width = 2560, .height = 1440 }};
