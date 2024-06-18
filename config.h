#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier modkey   = Super; 
static const kb_modifier winmod   = modkey; 
static const mousebtn movebtn     = LeftMouse; 
static const mousebtn resizebtn   = RightMouse; 

#define TERMINAL_CMD  "alacritty &"
#define MENU_CMD      "~/.config/rofi/launchers/type-3/launcher.sh &"
#define BROWSER_CMD   "brave &"

// Window manger keybinds
static const keybind keybinds[] = {
/* Modifier | Key       | Callback        | Command */ 
  {modkey,     KeyEscape,  terminate,        NULL},
  {modkey,     KeyTab,     cyclefocus,       NULL},
  {modkey,     KeyQ,       killfocus,        NULL},
  {modkey,     KeyF,       togglefullscreen, NULL},
  /* Application shortcuts */
  {modkey,     KeyReturn,  runcmd,           TERMINAL_CMD},
  {modkey,     KeyS,       runcmd,           MENU_CMD},
  {modkey,     KeyW,       runcmd,           BROWSER_CMD}
};
