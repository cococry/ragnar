#pragma once
#include "structs.h"

static const int32_t winborderwidth = 1;
static const int32_t winbordercolor = 0xff0000ff;

static const keybind exitkeybind = {Super | Alt, KeyEscape};
static const keybind terminalkeybind = {Super, KeyReturn};

static const char* terminalcmd = "alacritty &";
