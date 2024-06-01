#pragma once

#define ALT   XCB_MOD_MASK_1
#define SUPER XCB_MOD_MASK_4

typedef struct {
  uint16_t modmask;
  xcb_keysym_t key;
} keybind;

static int32_t winborderwidth = 1;
static int32_t winbordercolor = 0xff0000;

static keybind exitkeybind = {SUPER | ALT, XK_Escape};
static keybind terminalkeybind = {SUPER, XK_Return};

static const char* terminalcmd = "alacritty &";
