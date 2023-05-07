#pragma once
#include <stdint.h>

typedef struct {
    uint32_t width, height;
} Monitor;

/* Commands */
#define TERMINAL_CMD "alacritty &"
#define WEB_BROWSER_CMD "chromium &"

/* Keybindings */
#define MASTER_KEY Mod4Mask 

#define TERMINAL_OPEN_KEY XK_Return
#define WEB_BROWSER_OPEN_KEY XK_W

#define WM_TERMINATE_KEY XK_C

#define WINDOW_CLOSE_KEY XK_Q
#define WINDOW_FULLSCREEN_KEY XK_F

#define WINDOW_ADD_TO_LAYOUT_KEY XK_space
#define WINDOW_LAYOUT_CYCLE_UP_KEY XK_Up
#define WINDOW_LAYOUT_CYCLE_DOWN_KEY XK_Down
#define WINDOW_LAYOUT_INCREASE_MASTER_X_KEY XK_L
#define WINDOW_LAYOUT_DECREASE_MASTER_X_KEY XK_H
#define WINDOW_LAYOUT_INCREASE_SLAVE_Y_KEY XK_K
#define WINDOW_LAYOUT_DECREASE_SLAVE_Y_KEY XK_J
#define WINDOW_LAYOUT_FLOATING_KEY XK_R
#define WINDOW_LAYOUT_TILED_MASTER_KEY XK_T

#define WINDOW_GAP_INCREASE_KEY XK_plus
#define WINDOW_GAP_DECREASE_KEY XK_minus

/* Desktops */
#define DESKTOP_CYCLE_UP_KEY XK_A
#define DESKTOP_CYCLE_DOWN_KEY XK_D
#define DESKTOP_CLIENT_CYCLE_UP_KEY XK_O
#define DESKTOP_CLIENT_CYCLE_DOWN_KEY XK_P
#define DESKTOP_COUNT 10

/* Window properties */
#define WINDOW_BG_COLOR 0x32302f // black
#define WINDOW_BORDER_WIDTH 3 
#define WINDOW_BORDER_COLOR 0x242424 //  gray
#define WINDOW_MIN_SIZE_Y_LAYOUT 100
#define WINDOW_MAX_GAP 400

/* Bar */
#define BAR_SIZE 10

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 1
extern Monitor Monitors[MONITOR_COUNT]; // [!] Define and configure in xragnar.c
