#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier winmod   = Alt; 

#define TERMINAL_CMD "alacritty &"
#define MENU_CMD "~/.config/rofi/launchers/type-3/launcher.sh &"
#define BROWSER_CMD "brave &"

// Number of keybinds
static const uint32_t numkeybinds = 6;

// Window manger keybinds
static const keybind keybinds[] = {
/* Modifier | Key        | Callback  | Command */ 
  {Alt,     KeyEscape,  terminate,  NULL},
  {Alt,     KeyTab,     cyclefocus, NULL},
  {Alt,     KeyQ,       killfocus,  NULL},
  /* Application shortcuts */
  {Alt,     KeyReturn,  runcmd,     TERMINAL_CMD},
  {Alt,     KeyS,       runcmd,     MENU_CMD},
  {Alt,     KeyW,       runcmd,     BROWSER_CMD}
};
