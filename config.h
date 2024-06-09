#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier winmod   = Super; 

#define TERMINAL_CMD "alacritty &"

static const uint32_t numkeybinds = 4;
static const keybind keybinds[] = {
  {Super,     KeyEscape,  terminate,  NULL},
  {Super,     KeyTab,     cyclefocus, NULL},
  {Super,     Keyq,       killfocus,  NULL},
  {Super,     KeyReturn,  NULL,       TERMINAL_CMD}
};
