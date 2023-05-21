#pragma once
#include <X11/X.h>
#include <X11/Xft/Xft.h>
#include <stdint.h>

// INTERNAL -- IGNORE
typedef struct {
    XftFont* font;
    XftColor color;
    XftDraw* draw;
} FontStruct;

typedef struct {
    uint32_t width, height;
} Monitor;
typedef struct {
    const char* cmd;
    float refresh_time;
    float timer;
    char text[512];
    bool init;
} BarCommand;

typedef struct {
    const char* cmd;
    const char* icon;
    Window win;
    FontStruct font;
} BarButton;

typedef struct {
    const char* cmd;
    uint8_t key;
    bool spawned, hidden;
    Window win, frame;
} ScratchpadDef;

/* Commands */
#define TERMINAL_CMD "alacritty &"
#define WEB_BROWSER_CMD "brave &"
#define APPLICATION_LAUNCHER_CMD "dmenu_run &"

#define WM_REFRESH_SPEED 1.0f // In seconds
/* Font*/
/*
 * IMPORTANT:
 * Unicode characters often need some tweaking with positioning.
 * If you use a unicode character as an icon and it is not correctly centered,
 * just add spaces to the back or front of the icon to center it. */
#define FONT "JetBrains Mono Nerd Font:size=15:style=bold"
#define FONT_SIZE 15
#define FONT_COLOR "#ffffff"

/* Keybindings */
#define MASTER_KEY Mod4Mask 

#define TERMINAL_OPEN_KEY XK_Return
#define WEB_BROWSER_OPEN_KEY XK_W
#define APPLICATION_LAUNCHER_OPEN_KEY XK_S

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
#define WINDOW_CYCLE_KEY XK_Tab

#define WINDOW_GAP_INCREASE_KEY XK_plus
#define WINDOW_GAP_DECREASE_KEY XK_minus

#define BAR_TOGGLE_KEY XK_I 
#define BAR_CYCLE_MONITOR_UP_KEY XK_N
#define BAR_CYCLE_MONITOR_DOWN_KEY XK_B

#define DECORATION_TOGGLE_KEY XK_U

/* Desktops */
#define DESKTOP_CYCLE_UP_KEY XK_D
#define DESKTOP_CYCLE_DOWN_KEY XK_A
#define DESKTOP_CLIENT_CYCLE_UP_KEY XK_P
#define DESKTOP_CLIENT_CYCLE_DOWN_KEY XK_O
#define DESKTOP_COUNT 10

/* Window properties */
#define WINDOW_BG_COLOR 0x32302f 
#define WINDOW_BORDER_WIDTH 3 // In pixles
#define WINDOW_BORDER_COLOR 0x282828
#define WINDOW_BORDER_COLOR_ACTIVE 0x7c6f64
#define WINDOW_MIN_SIZE_Y_LAYOUT 100 // In pixels
#define WINDOW_MAX_GAP 400 // In pixels

/* Window decoration */
#define SHOW_DECORATION true
#define DECORATION_TITLEBAR_SIZE 40
#define DECORATION_COLOR 0x1d2021 
#define DECORATION_TITLE_COLOR 0x32302f 
#define DECORATION_TRIANGLES true

#define DECORATION_SHOW_CLOSE_ICON true
#define DECORATION_CLOSE_ICON ""
#define DECORATION_CLOSE_ICON_COLOR 0x32302f 
#define DECORATION_CLOSE_ICON_SIZE 50

#define DECORATION_SHOW_MAXIMIZE_ICON true
#define DECORATION_MAXIMIZE_ICON " "
#define DECORATION_MAXIMIZE_ICON_COLOR 0x32302f 
#define DECORATION_MAXIMIZE_ICON_SIZE 50

/* Bar */

/*
 *  ===============================================================================================
 *  |------------------------------\      \--------------\            |=| |=| |=|       \---------|
 *  ===============================================================================================
 *    ^                                   ^                                ^                    ^
 *    |                                   |                                |                    |
 *    Main Label. Content of this       Info Label. Conent of this       Button Label.        Version Label.
 *    label is the output of            label is information             Icons which, when    This shows the version of
 *    the BarCommands                   about the WM like the current    clicked execute      Ragnar WM that is running
 *                                      monitor or the current program.  their given command.
 * */
#define SHOW_BAR true

#define BAR_SHOW_INFO_LABEL true
#define BAR_SHOW_VERSION_LABEL true

#define BAR_SIZE 45 // In pixels
#define BAR_START_MONITOR 1 // Monitor on which the bar is on. (0 is most left)
#define BAR_REFRESH_SPEED 1.0 // In seconds
#define BAR_COLOR 0x1d2021 
#define BAR_LABEL_PADDING 100 // In pixels
                            
#define BAR_MAIN_LABEL_COLOR 0x32302f
#define BAR_INFO_LABEL_COLOR 0x32302f
#define BAR_BUTTON_LABEL_COLOR 0x32302f 
#define BAR_VERSION_LABEL_COLOR 0x32302f 

#define BAR_INFO_PROGRAM_ICON ""
#define BAR_INFO_MONITOR_ICON "󰍹"
#define BAR_INFO_DESKTOP_ICON ""
#define BAR_INFO_WINDOW_LAYOUT_ICON ""

#define BAR_SLICES_COUNT 4
#define BAR_BUTTON_COUNT 5

static BarCommand  BarCommands[BAR_SLICES_COUNT] = 
{ 
    (BarCommand){.cmd = "echo \"  󰣇\"", .refresh_time = 300.0f},
    (BarCommand){.cmd = "echo \"  $(date +%R) \"", .refresh_time = 1.0f},
    (BarCommand){.cmd = "ram-ragbar", .refresh_time = 1.0f,},
    //(BarCommand){.cmd = "kernel-ragbar", .refresh_time = 300.0f},
    (BarCommand){.cmd = "uptime-ragbar", .refresh_time = 1.0f},
    //(BarCommand){.cmd = "packages-ragbar", .refresh_time = 60.0f},
    //(BarCommand){.cmd = "updates-ragbar", .refresh_time = 120.0f},
};

static BarButton BarButtons[BAR_BUTTON_COUNT] =
{
    (BarButton){.cmd = APPLICATION_LAUNCHER_CMD, .icon = " "},
    (BarButton){.cmd = TERMINAL_CMD, .icon = " "},
    (BarButton){.cmd = WEB_BROWSER_CMD, .icon = " "},
    (BarButton){.cmd = "qtfm &", .icon = " "},
    (BarButton){.cmd = "nitrogen &", .icon = " "},
};

/* Scratchpads */
#define SCRATCH_PAD_COUNT 5
static ScratchpadDef ScratchpadDefs[SCRATCH_PAD_COUNT] = 
{
    (ScratchpadDef){.cmd = "alacritty &", .key = XK_1},
    (ScratchpadDef){.cmd = "alacritty -e mocp &", .key = XK_2},
    (ScratchpadDef){.cmd = "alacritty -o \"window.dimensions.lines=9\" -o \"window.dimensions.columns=37\" -e tty-clock -C 3 &", .key = XK_3},
    (ScratchpadDef){.cmd = "alacritty -e cava &", .key = XK_4},
    (ScratchpadDef){.cmd = "alacritty -e nvim ~/dev/Ragnar/config.h &", .key = XK_5}
};

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 2
static const Monitor Monitors[MONITOR_COUNT] = {(Monitor){.width = 1920, .height = 1080}, (Monitor){.width = 2560, .height = 1440}};

static const uint32_t BarButtonLabelPos[MONITOR_COUNT] = { 1350, 1600 };
