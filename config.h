/*
 * 
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
            __                                                  
           / /_  __  __   _________  _________  ____________  __
          / __ \/ / / /  / ___/ __ \/ ___/ __ \/ ___/ ___/ / / /
         / /_/ / /_/ /  / /__/ /_/ / /__/ /_/ / /__/ /  / /_/ / 
        /_.___/\__, /   \___/\____/\___/\____/\___/_/   \__, /  
              /____/                                   /____/   
*
*/
#pragma once
#include "structs.h"

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 2
static const Monitor Monitors[MONITOR_COUNT] = {(Monitor){.width = 1920, .height = 1080}, (Monitor){.width = 2560, .height = 1440}};

/* Commands */
#define TERMINAL_CMD                            "alacritty &"
#define WEB_BROWSER_CMD                         "brave &"
#define APPLICATION_LAUNCHER_CMD                "rofi-app-launcher &"

#define WM_REFRESH_SPEED                        1.0f // In seconds
#define WM_UPDATE_LAG                           10 
/* Font*/
#define FONT                                    "JetBrains Mono Nerd Font:size=13:style=bold"
#define FONT_SIZE                               13
#define FONT_COLOR                              "#000000"
#define DECORATION_FONT_COLOR                   "#ffffff"
/* Keybindings */
#define MASTER_KEY                              Mod4Mask 

#define TERMINAL_OPEN_KEY                       XK_Return 
#define WEB_BROWSER_OPEN_KEY                    XK_W
#define APPLICATION_LAUNCHER_OPEN_KEY           XK_S

#define WM_TERMINATE_KEY                        XK_C

#define WINDOW_CLOSE_KEY                        XK_Q
#define WINDOW_FULLSCREEN_KEY                   XK_F
#define WINDOW_CYCLE_KEY                        XK_Tab

#define WINDOW_ADD_TO_LAYOUT_KEY XK_space
#define WINDOW_LAYOUT_CYCLE_UP_KEY XK_Up
#define WINDOW_LAYOUT_CYCLE_DOWN_KEY            XK_Down
#define WINDOW_LAYOUT_INCREASE_MASTER_KEY       XK_L
#define WINDOW_LAYOUT_DECREASE_MASTER_KEY       XK_H
#define WINDOW_LAYOUT_INCREASE_SLAVE_KEY        XK_K
#define WINDOW_LAYOUT_DECREASE_SLAVE_KEY        XK_J
#define WINDOW_LAYOUT_SET_MASTER_KEY            XK_G
#define WINDOW_LAYOUT_FLOATING_KEY              XK_R

#define WINDOW_LAYOUT_TILED_MASTER_KEY          XK_T
#define WINDOW_LAYOUT_HORIZONTAL_MASTER_KEY     XK_V
#define WINDOW_LAYOUT_HORIZONTAL_STRIPES_KEY    XK_X
#define WINDOW_LAYOUT_VERTICAL_STRIPES_KEY      XK_M

#define WINDOW_GAP_INCREASE_KEY                 XK_plus
#define WINDOW_GAP_DECREASE_KEY                 XK_minus

#define BAR_TOGGLE_KEY                          XK_I 
#define BAR_CYCLE_MONITOR_UP_KEY                XK_N
#define BAR_CYCLE_MONITOR_DOWN_KEY              XK_B

#define DECORATION_TOGGLE_KEY                   XK_U

/* Desktops */
#define DESKTOP_CYCLE_UP_KEY                    XK_D
#define DESKTOP_CYCLE_DOWN_KEY                  XK_A
#define DESKTOP_CLIENT_CYCLE_UP_KEY             XK_P
#define DESKTOP_CLIENT_CYCLE_DOWN_KEY           XK_O
#define DESKTOP_COUNT                           6

/* Window properties */
#define WINDOW_BG_COLOR                         0x32302f 
#define WINDOW_BORDER_WIDTH                     3 // In pixles
#define WINDOW_BORDER_COLOR                     0xa6719a
#define WINDOW_BORDER_COLOR_ACTIVE              0xf2c9e9
#define WINDOW_BORDER_COLOR_HARD_SELECTED       0xed1329
#define WINDOW_MAX_COUNT_LAYOUT                 5    
#define WINDOW_MIN_SIZE_LAYOUT_HORIZONTAL       300 
#define WINDOW_MIN_SIZE_LAYOUT_VERTICAL         100 // In pixels
#define WINDOW_MAX_GAP                          100 // In pixels
#define WINDOW_TRANSPARENT_FRAME                true
#define WINDOW_SELECT_HOVERED                   true
#define WINDOW_LAYOUT_DEFAULT                   WINDOW_LAYOUT_TILED_MASTER

/* Window decoration */
#define SHOW_DECORATION                         false
#define DECORATION_TITLEBAR_SIZE                30
#define DECORATION_COLOR                        0x131020 
#define DECORATION_TITLE_COLOR                  0x1a1823  

#define DECORATION_SHOW_CLOSE_ICON              true
#define DECORATION_CLOSE_ICON                   ""
#define DECORATION_CLOSE_ICON_COLOR             0x1a1823
#define DECORATION_CLOSE_ICON_SIZE              50

#define DECORATION_SHOW_MAXIMIZE_ICON           true
#define DECORATION_MAXIMIZE_ICON                " "
#define DECORATION_MAXIMIZE_ICON_COLOR          0x1a1823 
#define DECORATION_MAXIMIZE_ICON_SIZE           50

#define DECORATION_DESIGN_WIDTH                 20
#define DECORATION_ICONS_LABEL_DESIGN           DESIGN_ROUND_LEFT
#define DECORATION_TITLE_LABEL_DESIGN           DESIGN_ROUND_RIGHT

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
#define SHOW_BAR                            true
#define BAR_INSTANT_UPDATE                  false // Turning this on leads to 100% cpu usage

#define BAR_SHOW_INFO_LABEL                 true
#define BAR_SHOW_VERSION_LABEL              true

#define BAR_SIZE                            40 // In pixels
#define BAR_PADDING_Y                       20
#define BAR_PADDING_X                       20
#define BAR_START_MONITOR                   1 // Monitor on which the bar is on startup. (0 is most left)
#define BAR_REFRESH_SPEED                   1.0 // In seconds
#define BAR_COLOR                           0x0f0f12
#define BAR_BORDER_COLOR                    0x24273a

/* --- BAR DESIGNS --- */
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
#define BAR_LABEL_DESIGN_WIDTH              20
#define BAR_MAIN_LABEL_DESIGN               DESIGN_ROUND_RIGHT
#define BAR_INFO_LABEL_DESIGN_FRONT         DESIGN_ROUND_LEFT
#define BAR_INFO_LABEL_DESIGN_BACK          DESIGN_ROUND_RIGHT
#define BAR_VERSION_LABEL_DESIGN            DESIGN_ROUND_LEFT


#define BAR_MAIN_LABEL_COLOR                0xdd7890
#define BAR_INFO_LABEL_COLOR                0xdd7890
#define BAR_INFO_LABEL_SELECTED_COLOR       0xbf677c
#define BAR_VERSION_LABEL_COLOR             0xdd7890 

#define BAR_INFO_PROGRAM_ICON               ""
#define BAR_INFO_MONITOR_ICON               "󰍹"
#define BAR_INFO_DESKTOP_ICON               ""
#define BAR_INFO_WINDOW_LAYOUT_ICON         ""

#define BAR_SLICES_COUNT                    5

#define BAR_INFO_LABEL_DESKTOP_ICON_SIZE    50
static const char* BarInfoLabelDesktopIcons[DESKTOP_COUNT] = { "", "", "", "", "", "" };
static const uint32_t BarInfoLabelPos[MONITOR_COUNT] = {900, 700};
static BarCommand  BarCommands[BAR_SLICES_COUNT] = 
{ 
    (BarCommand){.cmd = "echo \"  󰣇\"", .refresh_time = 300.0f},
    (BarCommand){.cmd = "echo \"  $(date +%R) \"", .refresh_time = 1.0f},
    (BarCommand){.cmd = "ram-ragbar", .refresh_time = 1.0f,},
    (BarCommand){.cmd = "uptime-ragbar", .refresh_time = 1.0f},
    (BarCommand){.cmd = "packages-ragbar", .refresh_time = 60.0f},
};

#define BAR_BUTTON_PADDING                  20
#define BAR_BUTTON_SIZE                     50
#define BAR_BUTTON_COUNT                    4
static const uint32_t BarButtonLabelPos[MONITOR_COUNT] = { 1350, 1600 };
static BarButton BarButtons[BAR_BUTTON_COUNT] =
{
    (BarButton){.cmd = APPLICATION_LAUNCHER_CMD, .icon = "", .color = 0xdd7890},
    (BarButton){.cmd = TERMINAL_CMD, .icon = "", .color = 0xdd7890},
    (BarButton){.cmd = WEB_BROWSER_CMD, .icon = "", .color = 0xdd7890},
    (BarButton){.cmd = "nitrogen &", .icon = "", .color = 0xdd7890},
};


/* Scratchpads */
#define SCRATCH_PAD_COUNT 2
static ScratchpadDef ScratchpadDefs[SCRATCH_PAD_COUNT] =
{
    (ScratchpadDef){.cmd = "alacritty &", .key = XK_1},
    (ScratchpadDef){.cmd = "alacritty -e mocp &", .key = XK_2},
};

