#pragma once
#include "structs.h"

static int32_t winborderwidth = 1;
static int32_t winbordercolor = 0xff0000;

static keybind exitkeybind = {Super | Alt, KeyEscape};
static keybind terminalkeybind = {Super, KeyReturn};

static const char* terminalcmd = "alacritty &";
