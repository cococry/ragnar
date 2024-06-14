#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier winmod   = Super; 

#define TERMINAL_CMD "alacritty &"
#define MENU_CMD "~/.config/rofi/launchers/type-3/launcher.sh &"
#define BROWSER_CMD "brave &"

// Number of keybinds
static const uint32_t numkeybinds = 7;

// Window manger keybinds
static const keybind keybinds[] = {
/* Modifier | Key       | Callback        | Command */ 
  {Super,     KeyEscape,  terminate,        NULL},
  {Super,     KeyTab,     cyclefocus,       NULL},
  {Super,     KeyQ,       killfocus,        NULL},
  {Super,     KeyF,       togglefullscreen, NULL},
  /* Application shortcuts */
  {Super,     KeyReturn,  runcmd,           TERMINAL_CMD},
  {Super,     KeyS,       runcmd,           MENU_CMD},
  {Super,     KeyW,       runcmd,           BROWSER_CMD}
};
