#pragma once

typedef enum {
  Shift     = XCB_MOD_MASK_SHIFT,
  Control   = XCB_MOD_MASK_CONTROL,
  Alt       = XCB_MOD_MASK_1,
  Super     = XCB_MOD_MASK_4
} kb_modifier;

typedef struct {
  uint16_t modmask;
  xcb_keysym_t key;
} keybind;

static int32_t winborderwidth = 1;
static int32_t winbordercolor = 0xff0000;

static keybind exitkeybind = {Super | Alt, XK_Escape};
static keybind terminalkeybind = {Super, XK_Return};

static const char* terminalcmd = "alacritty &";
