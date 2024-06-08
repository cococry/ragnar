#pragma once
#include "structs.h"

static const int32_t winborderwidth           = 1;
static const int32_t winbordercolor           = 0x5e5e5e;
static const int32_t winbordercolor_selected  = 0xcccccc;

static const kb_modifier winmod       = Alt; 

static const keybind exitkeybind      = {Alt | Shift, KeyEscape};
static const keybind terminalkeybind  = {Alt, KeyReturn};
static const keybind wincyclekeybind  = {Alt, KeyTab};

static const char* terminalcmd        = "alacritty &";
