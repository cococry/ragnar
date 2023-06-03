/*
 
 ██▀███   ▄▄▄        ▄████  ███▄    █  ▄▄▄       ██▀███   █     █░ ███▄ ▄███▓
▓██ ▒ ██▒▒████▄     ██▒ ▀█▒ ██ ▀█   █ ▒████▄    ▓██ ▒ ██▒▓█░ █ ░█░▓██▒▀█▀ ██▒
▓██ ░▄█ ▒▒██  ▀█▄  ▒██░▄▄▄░▓██  ▀█ ██▒▒██  ▀█▄  ▓██ ░▄█ ▒▒█░ █ ░█ ▓██    ▓██░
▒██▀▀█▄  ░██▄▄▄▄██ ░▓█  ██▓▓██▒  ▐▌██▒░██▄▄▄▄██ ▒██▀▀█▄  ░█░ █ ░█ ▒██    ▒██ 
░██▓ ▒██▒ ▓█   ▓██▒░▒▓███▀▒▒██░   ▓██░ ▓█   ▓██▒░██▓ ▒██▒░░██▒██▓ ▒██▒   ░██▒
░ ▒▓ ░▒▓░ ▒▒   ▓▒█░ ░▒   ▒ ░ ▒░   ▒ ▒  ▒▒   ▓▒█░░ ▒▓ ░▒▓░░ ▓░▒ ▒  ░ ▒░   ░  ░
  ░▒ ░ ▒░  ▒   ▒▒ ░  ░   ░ ░ ░░   ░ ▒░  ▒   ▒▒ ░  ░▒ ░ ▒░  ▒ ░ ░  ░  ░      ░
  ░░   ░   ░   ▒   ░ ░   ░    ░   ░ ░   ░   ▒     ░░   ░   ░   ░  ░      ░   
   ░           ░  ░      ░          ░       ░  ░   ░         ░           ░   
                                                                                                            
                      _________  _  _______________
                     / ___/ __ \/ |/ / __/  _/ ___/
                    / /__/ /_/ /    / _/_/ // (_ / 
                    \___/\____/_/|_/_/ /___/\___/                                   
*/
#pragma once
#include "structs.h"

/* Commands */
#define TERMINAL_CMD                            "alacritty &"
#define WEB_BROWSER_CMD                         "chromium &"
#define APPLICATION_LAUNCHER_CMD                "dmenu_run &"

#define WM_REFRESH_SPEED                        1.0 // In seconds

/* Font */
/*
 * IMPORTANT:
 * Unicode characters often need some tweaking with positioning.
 * If you use a unicode character as an icon and it is not correctly centered,
 * just add spaces to the back or front of the icon to center it. */

#define FONT                                    "monospace:size=11"
#define FONT_SIZE                               11 // Should be same as size of given font 
#define FONT_COLOR                              "#ffffff" // Color for the font of the bar
#define DECORATION_FONT_COLOR                   "#ffffff" // Color for the font of the window decoration

/* Keybindings */
#define MASTER_KEY                              Mod4Mask // All WM Keybindings are using the Master-Key as a modifier

/* Application keybinds*/
#define TERMINAL_OPEN_KEY                       XK_Return
#define WEB_BROWSER_OPEN_KEY                    XK_W
#define APPLICATION_LAUNCHER_OPEN_KEY           XK_S

/* WM keybinds */
#define WM_TERMINATE_KEY                        XK_C

/* Window keybinds */
#define WINDOW_CLOSE_KEY                        XK_Q
#define WINDOW_FULLSCREEN_KEY                   XK_F
#define WINDOW_CYCLE_KEY                        XK_Tab

/* Window-Layout keybinds */
#define WINDOW_ADD_TO_LAYOUT_KEY                XK_space
#define WINDOW_LAYOUT_CYCLE_UP_KEY              XK_Up
#define WINDOW_LAYOUT_CYCLE_DOWN_KEY            XK_Down
#define WINDOW_LAYOUT_INCREASE_MASTER_KEY       XK_L
#define WINDOW_LAYOUT_DECREASE_MASTER_KEY       XK_H
#define WINDOW_LAYOUT_INCREASE_SLAVE_KEY        XK_K
#define WINDOW_LAYOUT_DECREASE_SLAVE_KEY        XK_J
#define WINDOW_LAYOUT_FLOATING_KEY              XK_R
#define WINDOW_LAYOUT_TILED_MASTER_KEY          XK_T


#define WINDOW_LAYOUT_TILED_MASTER_KEY          XK_T
#define WINDOW_LAYOUT_HORIZONTAL_MASTER_KEY     XK_V
#define WINDOW_LAYOUT_HORIZONTAL_STRIPES_KEY    XK_X
#define WINDOW_LAYOUT_VERTICAL_STRIPES_KEY      XK_M

/* Window-Gap keybinds */
#define WINDOW_GAP_INCREASE_KEY                 XK_plus
#define WINDOW_GAP_DECREASE_KEY                 XK_minus

/* Bar keybinds*/
#define BAR_TOGGLE_KEY                          XK_I
#define BAR_CYCLE_MONITOR_UP_KEY                XK_N
#define BAR_CYCLE_MONITOR_DOWN_KEY              XK_B

/* Decoration keyinds */
#define DECORATION_TOGGLE_KEY                   XK_U

/* Desktops */
/* In Ragnar, you can specify a unlimeted amount of desktops. 
 * At the moment, you can only cycle desktops and windows through desktops
 * one by one. */
#define DESKTOP_CYCLE_UP_KEY                    XK_D
#define DESKTOP_CYCLE_DOWN_KEY                  XK_A
#define DESKTOP_CLIENT_CYCLE_UP_KEY             XK_P
#define DESKTOP_CLIENT_CYCLE_DOWN_KEY           XK_O
#define DESKTOP_COUNT                           10

/* Window properties */

#define WINDOW_BG_COLOR                         0x32302f // Background color of the window frame (effectless when WINDOW_TRANSPARENT_FRAME=true)
#define WINDOW_BORDER_WIDTH                     3 
#define WINDOW_BORDER_COLOR                     0x242424 // Color of window border for inactive windows
#define WINDOW_BORDER_COLOR_ACTIVE              0x454545 // Color of window border for active windows
#define WINDOW_MIN_SIZE_LAYOUT_VERTICAL         300 // Minimum height of a window in a layout 
#define WINDOW_MIN_SIZE_LAYOUT_HORIZONTAL       100 // Minimum width of a window in a layout 
#define WINDOW_MAX_COUNT_LAYOUT                 5 // Maximum number of windows in a layout
#define WINDOW_MAX_GAP                          400 // Maximum amount of pixel-gap between windows in layout
#define WINDOW_TRANSPARENT_FRAME                true // If transparency with a compositer is used, enable this

/* Window decoration */

/*  
 *       ======================================
 *  1 -> | htop >                     < [=] X | 
 *       ================================^==^==
 *        ^                              |  |
 *        |                              3  4
 *        2 
 *  1: Decoration-title 
 *          This label displays the title of the window.
 *  2: Decoration-Titlebar
 *          The bar that is added to the top of each window 
 *          if decoration is on. Window decorations are drawn
 *          in the titlebar.
 *  3: 
 *      Fullscreen Button/Icon:
 *          When left-clicked, fullscreen of the window gets toggled.
 *  4:
 *      Close Button/Icon
 *          When left-clicked, window closes.
 * 
 * */
#define SHOW_DECORATION                         true
#define DECORATION_TITLEBAR_SIZE                30 
#define DECORATION_COLOR                        0x2d2d2d // Color of the titlebar
#define DECORATION_TITLE_COLOR                  0x212121 // Color of the Decoration-title lable 

/* Decoration - Close Icon */
#define DECORATION_SHOW_CLOSE_ICON              true 
#define DECORATION_CLOSE_ICON                   "X"
#define DECORATION_CLOSE_ICON_COLOR             0x212121 
#define DECORATION_CLOSE_ICON_SIZE              50

/* Decoration - Maximize Icon */
#define DECORATION_SHOW_MAXIMIZE_ICON           true
#define DECORATION_MAXIMIZE_ICON                "Max"
#define DECORATION_MAXIMIZE_ICON_COLOR          0x212121
#define DECORATION_MAXIMIZE_ICON_SIZE           50

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
#define SHOW_BAR                                true
#define BAR_SHOW_INFO_LABEL                     true
#define BAR_SHOW_VERSION_LABEL                  true

#define BAR_SIZE                                30
#define BAR_PADDING_Y                           10 // Amount of space left between most left point of the bar-monitor to bar & most right point of the bar-monitor
#define BAR_PADDING_X                           10 // Y offset from the uppest point of the bar-monitor to the bar 
#define BAR_START_MONITOR                       1 // Monitor on which the bar is on. (0 is most left)
#define BAR_COLOR                               0x202020
#define BAR_BORDER_COLOR                        0x303030
#define BAR_LABEL_PADDING                       100 // Space between bar main label and info label

#define BAR_MAIN_LABEL_COLOR                    0x1e404f
#define BAR_INFO_LABEL_COLOR                    0x1e404f
#define BAR_VERSION_LABEL_COLOR                 0x1e404f

#define BAR_INFO_PROGRAM_ICON                   "Program: "
#define BAR_INFO_MONITOR_ICON                   "Monitor: "
#define BAR_INFO_DESKTOP_ICON                   "Desktop: "
#define BAR_INFO_WINDOW_LAYOUT_ICON             "Layout: "

#define BAR_SLICES_COUNT                        1 // Amount of commands mapped to the bar
#define BAR_BUTTON_COUNT                        3 // Amount of buttons on the bar-button-label

/* Bar Commands */
static BarCommand  BarCommands[BAR_SLICES_COUNT] =
{
                                                (BarCommand){.cmd = "echo \"$(date +%R)  \"", .refresh_time = 1.0f},
};

/* Bar buttons */
#define BAR_BUTTON_PADDING                      20
static BarButton BarButtons[BAR_BUTTON_COUNT] =
{
                                                (BarButton){.cmd = APPLICATION_LAUNCHER_CMD, .icon = "Search", .color = 0x1e404f},
                                                (BarButton){.cmd = TERMINAL_CMD, .icon = "Terminal", .color = 0x1e404f},
                                                (BarButton){.cmd = WEB_BROWSER_CMD, .icon = "Browser", .color = 0x1e404f},
};

/* Designs */

/* --- DESIGNS --- */
/* STAIGHT:
 *  |
 * SHARP_LEFT_UP:
 *      /-|
 *     /  |
 *    /---|
 * SHARP_RIGHT_UP
 *    |-\
 *    |  \
 *    |---\
 * SHARP_LEFT_DOWN
 *    \---|
 *     \  |
 *      \-|
 * SHARP_RIGHT_DOWN
 *    |---/
 *    |  /
 *    |-/
 * ARROW_LEFT:
 *    <
 * ARROW_RIGHT
 *    >
 * ROUND_LEFT:
 *   (|
 * ROUND_RIGHT
 *   |)
 */

#define DECORATION_DESIGN_WIDTH                 20 // Amount of pixels to the most outer part of the decoration design
#define DECORATION_ICONS_LABEL_DESIGN           DESIGN_ARROW_LEFT
#define DECORATION_TITLE_LABEL_DESIGN           DESIGN_ARROW_RIGHT

#define BAR_LABEL_DESIGN_WIDTH                  20 // Amount of pixels to the most outer part of the bar design
#define BAR_MAIN_LABEL_DESIGN                   DESIGN_ARROW_RIGHT
#define BAR_INFO_LABEL_DESIGN_FRONT             DESIGN_ARROW_LEFT
#define BAR_INFO_LABEL_DESIGN_BACK              DESIGN_ARROW_RIGHT
#define BAR_VERSION_LABEL_DESIGN                DESIGN_ARROW_LEFT

/* Scratchpads */
#define SCRATCH_PAD_COUNT                       2
static ScratchpadDef ScratchpadDefs[SCRATCH_PAD_COUNT] =
{
                                                (ScratchpadDef){.cmd = "alacritty &", .key = XK_1},
                                                (ScratchpadDef){.cmd = "alacritty -e vim &", .key = XK_2},
};

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT                           2
static const Monitor Monitors[MONITOR_COUNT] =  {(Monitor){.width = 1920, .height = 1080}, (Monitor){.width = 1920, .height = 1080}};

static const uint32_t BarButtonLabelPos[MONITOR_COUNT] = { 1350, 1350 };
