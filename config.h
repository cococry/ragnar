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
*/
#pragma once
#include "structs.h"

/* Monitors */
// Ordered From left to right (0 is most left)
#define MONITOR_COUNT 1
static const Monitor Monitors[MONITOR_COUNT] = {(Monitor){.width = 1920, .height = 1080}};

#define UI_REFRESH_RATE                         1.0f // In seconds
/* Commands */
#define TERMINAL_CMD                            "alacritty &"
#define WEB_BROWSER_CMD                         "firefox &"
#define APPLICATION_LAUNCHER_CMD                "dmenu_run &"

/* Font*/
#define FONT                                    "monnospace:size=12"
#define FONT_SIZE                               12
#define FONT_COLOR                              "#ffffff"
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
#define WINDOW_BG_COLOR                         0x434343 
#define WINDOW_BORDER_WIDTH                     3 // In pixles
#define WINDOW_BORDER_COLOR                     0x7a8478
#define WINDOW_BORDER_COLOR_ACTIVE              0xd3c6aa
#define WINDOW_BORDER_COLOR_HARD_SELECTED       0xa7c080
#define WINDOW_MAX_COUNT_LAYOUT                 5    
#define WINDOW_MIN_SIZE_LAYOUT_HORIZONTAL       300 
#define WINDOW_MIN_SIZE_LAYOUT_VERTICAL         100 // In pixels
#define WINDOW_MAX_GAP                          100 // In pixels
#define WINDOW_INITIAL_GAP                      10 // In pixels
#define WINDOW_START_GAP                        16 // In pixles
#define WINDOW_TRANSPARENT_FRAME                false
#define WINDOW_SELECT_HOVERED                   true
#define WINDOW_LAYOUT_DEFAULT                   WINDOW_LAYOUT_TILED_MASTER

/* Window decoration */
#define SHOW_DECORATION                         false
#define DECORATION_TITLEBAR_SIZE                35
#define DECORATION_COLOR                        0x131020 
#define DECORATION_TITLE_COLOR                  0x1a1823  

#define DECORATION_SHOW_CLOSE_ICON              true
#define DECORATION_CLOSE_ICON                   "X"
#define DECORATION_CLOSE_ICON_COLOR             0x1a1823
#define DECORATION_CLOSE_ICON_SIZE              50

#define DECORATION_SHOW_MAXIMIZE_ICON           true
#define DECORATION_MAXIMIZE_ICON                " |=|"
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
#define SHOW_BAR                              true
#define BAR_INSTANT_UPDATE                    false // Turning this on leads to higher cpu usage

#define BAR_SHOW_DESKTOP_LABEL                true
#define BAR_SHOW_VERSION_LABEL                true

#define BAR_SIZE                              35 // In pixels
#define BAR_PADDING_Y                         0
#define BAR_PADDING_X                         0
#define BAR_START_MONITOR                     0 // Monitor on which the bar is on startup. (0 is most left)
#define BAR_REFRESH_SPEED                     1.0 // In seconds
#define BAR_COLOR                             0x1e2326
#define BAR_BORDER_COLOR                      0x4f5b58
#define BAR_BORDER_WIDTH                      0

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
#define BAR_LABEL_DESIGN_WIDTH                20
#define BAR_MAIN_LABEL_DESIGN                 DESIGN_ROUND_RIGHT
#define BAR_DESKTOP_LABEL_DESIGN_FRONT        DESIGN_ROUND_LEFT
#define BAR_DESKTOP_LABEL_DESIGN_BACK         DESIGN_ROUND_RIGHT
#define BAR_VERSION_LABEL_DESIGN              DESIGN_ROUND_LEFT


#define BAR_MAIN_LABEL_COLOR                  0x2e383c
#define BAR_DESKTOP_LABEL_COLOR               0x2e383c
#define BAR_DESKTOP_LABEL_SELECTED_COLOR      0x1e2326
#define BAR_VERSION_LABEL_COLOR               0x2e383c 

#define BAR_COMMANDS_COUNT                      2

#define BAR_DESKTOP_LABEL_ICON_SIZE           50

static const uint32_t BarInfoLabelPos[MONITOR_COUNT] = {810};

#define BAR_COMMAND_SEPERATOR "|"

static BarDesktopIcon DesktopIcons[DESKTOP_COUNT] = 
{ 
    (BarDesktopIcon){.icon = "1",  .color = "#f55151"  }, 
    (BarDesktopIcon){.icon = "2",  .color = "#f0528b"  }, 
    (BarDesktopIcon){.icon = "3",  .color = "#d267ba"  }, 
    (BarDesktopIcon){.icon = "4",  .color = "#a07fd7"  }, 
    (BarDesktopIcon){.icon = "5",  .color = "#6791dd"  }, 
    (BarDesktopIcon){.icon = "6",  .color = "#379bd0"  }, 
};

static BarCommand  BarCommands[BAR_COMMANDS_COUNT] = 
{ 
    (BarCommand){.cmd = "echo \" RGNR\"", .color = "#ffffff", .bg_color = -1},
    (BarCommand){.cmd = "echo \" $(date +%R)\"", .color ="#5eb5eb", .bg_color = -1},
};

#define BAR_BUTTON_PADDING                  20
#define BAR_BUTTON_SIZE                     120
#define BAR_BUTTON_COUNT                    3
static const uint32_t BarButtonLabelPos[MONITOR_COUNT] = { 1300 };
static BarButton BarButtons[BAR_BUTTON_COUNT] =
{
    (BarButton){.cmd = APPLICATION_LAUNCHER_CMD, .icon = "Search", .color = 0x2e383c},
    (BarButton){.cmd = TERMINAL_CMD, .icon = "Terminal", .color = 0x2e383c},
    (BarButton){.cmd = WEB_BROWSER_CMD, .icon = "Web", .color = 0x2e383c},
};


/* Scratchpads */
#define SCRATCH_PAD_COUNT 2
static ScratchpadDef ScratchpadDefs[SCRATCH_PAD_COUNT] =
{
    (ScratchpadDef){.cmd = "alacritty &", .key = XK_1},
    (ScratchpadDef){.cmd = "alacritty -e mocp &", .key = XK_2},
};
#define CUSTOM_KEYBIND_COUNT 1

static Keybind CustomKeybinds[CUSTOM_KEYBIND_COUNT] =
{
    (Keybind){.cmd = "flameshot gui &", .key = XK_E}
};
