#pragma once
#include "structs.h"

/* Window properties */
static const int32_t winborderwidth           = 3;
static const int32_t winbordercolor           = 0x000000;
static const int32_t winbordercolor_selected  = 0x555555;


// Main modifier that is used to execute all window manager keybindings
static const kb_modifier modkey   = Alt; 

/* Window controls */
static const kb_modifier winmod   = modkey; 
static const mousebtn movebtn     = LeftMouse; 
static const mousebtn resizebtn   = RightMouse;

/* Desktops */
// The desktop index that is selected on startup
static const int32_t desktopinit   = 0;

#define MAX_DESKTOPS 9
static const char* desktopnames[MAX_DESKTOPS] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* Decoration */
static const bool     usedecoration   = true;
static uint32_t       titlebarheight  = 30;
static const int32_t  titlebarcolor   = 0xffffff;
static const int32_t  fontcolor       = 0x000000;
static const char*    fontpath        = "/usr/share/ragnarwm/fonts/LilexNerdFont-Bold.ttf";
static const char*    closeiconpath   = "/usr/share/ragnarwm/icons/close.png";
static const uint32_t iconcolor       = 0x00000;

#define TERMINAL_CMD    "alacritty &"
#define MENU_CMD        "~/.config/rofi/launchers/type-3/launcher.sh &"
#define BROWSER_CMD     "brave &"
#define SCREENSHOT_CMD  "flameshot gui &"

/* Window manger keybinds */
static const keybind keybinds[] = {
/* Modifier   | Key       | Callback              | Command */ 
  {modkey,     KeyEscape,  terminate,               { NULL }},
  /* Clients */
  {modkey,     KeyTab,     cyclefocus,              { NULL }},
  {modkey,     KeyQ,       killfocus,               { NULL }},
  {modkey,     KeyF,       togglefullscreen,        { NULL }},
  {modkey,     KeyR,       raisefocus,              { NULL }},
  /* Desktops */
  {modkey,     KeyD,       cycledesktopup,          { NULL }},
  {modkey,     KeyA,       cycledesktopdown,        { NULL }},
  {modkey,     KeyP,       cyclefocusdesktopup,     { NULL }},
  {modkey,     KeyO,       cyclefocusdesktopdown,   { NULL }},
  /* Switch desktop hotkeys */
  {modkey,     Key1,       switchdesktop,           { .i = 0 }},
  {modkey,     Key2,       switchdesktop,           { .i = 1 }},
  {modkey,     Key3,       switchdesktop,           { .i = 2 }},
  {modkey,     Key4,       switchdesktop,           { .i = 3 }},
  {modkey,     Key5,       switchdesktop,           { .i = 4 }},
  {modkey,     Key6,       switchdesktop,           { .i = 5 }},
  {modkey,     Key7,       switchdesktop,           { .i = 6 }},
  {modkey,     Key8,       switchdesktop,           { .i = 7 }},
  {modkey,     Key9,       switchdesktop,           { .i = 8 }},
  /* Move focus to desktop hotkeys */
  {modkey | Shift,     Key1,       switchfocusdesktop,           { .i = 0 }},
  {modkey | Shift,     Key2,       switchfocusdesktop,           { .i = 1 }},
  {modkey | Shift,     Key3,       switchfocusdesktop,           { .i = 2 }},
  {modkey | Shift,     Key4,       switchfocusdesktop,           { .i = 3 }},
  {modkey | Shift,     Key5,       switchfocusdesktop,           { .i = 4 }},
  {modkey | Shift,     Key6,       switchfocusdesktop,           { .i = 5 }},
  {modkey | Shift,     Key7,       switchfocusdesktop,           { .i = 6 }},
  {modkey | Shift,     Key8,       switchfocusdesktop,           { .i = 7 }},
  {modkey | Shift,     Key9,       switchfocusdesktop,           { .i = 8 }},

  /* Application shortcuts */
  {modkey,     KeyReturn,  runcmd,                  { .cmd = TERMINAL_CMD }},
  {modkey,     KeyS,       runcmd,                  { .cmd = MENU_CMD }},
  {modkey,     KeyW,       runcmd,                  { .cmd = BROWSER_CMD }},
  {modkey,     KeyE,       runcmd,                  { .cmd = SCREENSHOT_CMD }}
};

/* Advanced Configuration */

// The FPS rate at which motion notify events are processed
static const size_t motion_notify_debounce_fps = 60;
// The delay between event polls (used to avoid high CPU usage)
static const uint32_t event_polling_rate_ms = 10;
