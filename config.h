#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier movewinmod   = Super; 

static const keybind exitkeybind      = {Super | Shift, KeyEscape};
static const keybind terminalkeybind  = {Super, KeyReturn};

static const char* terminalcmd        = "alacritty &";
