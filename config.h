#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier winmod   = Alt; 

#define TERMINAL_CMD "alacritty &"

// Number of keybinds
static const uint32_t numkeybinds = 4;

// Window manger keybinds
static const keybind keybinds[] = {
/* Modifier | Key        | Callback  | Command */ 
  {Alt,       KeyEscape,  terminate,  NULL},
  {Alt,       KeyTab,     cyclefocus, NULL},
  {Alt,       Keyq,       killfocus,  NULL},
  {Alt,       KeyReturn,  execsafe,   TERMINAL_CMD}
};
