#pragma once

#define _XCB_EV_LAST 36 

typedef enum {
  Shift     = XCB_MOD_MASK_SHIFT,
  Control   = XCB_MOD_MASK_CONTROL,
  Alt       = XCB_MOD_MASK_1,
  Super     = XCB_MOD_MASK_4
} kb_modifier;

typedef enum {
  LeftMouse   = XCB_BUTTON_MASK_1,
  MiddleMouse = XCB_BUTTON_MASK_2,
  RightMouse  = XCB_BUTTON_MASK_3,
} mousebtn;

typedef struct {
  const char* cmd;
  int32_t i;
} passthrough_data;

typedef struct {
  uint16_t modmask;
  xcb_keysym_t key;
  void (*cb)(passthrough_data data);
  passthrough_data data;
} keybind;

typedef enum {
    KeyVoidSymbol = XK_VoidSymbol,
    KeyBackSpace = XK_BackSpace,
    KeyTab = XK_Tab,
    KeyLinefeed = XK_Linefeed,
    KeyClear = XK_Clear,
    KeyReturn = XK_Return,
    KeyPause = XK_Pause,
    KeyScroll_Lock = XK_Scroll_Lock,
    KeySys_Req = XK_Sys_Req,
    KeyEscape = XK_Escape,
    KeyDelete = XK_Delete,
    KeyHome = XK_Home,
    KeyLeft = XK_Left,
    KeyUp = XK_Up,
    KeyRight = XK_Right,
    KeyDown = XK_Down,
    KeyPage_Up = XK_Page_Up,
    KeyPage_Down = XK_Page_Down,
    KeyEnd = XK_End,
    KeyBegin = XK_Begin,
    KeySpace = XK_space,
    KeyExclam = XK_exclam,
    KeyQuotedbl = XK_quotedbl,
    KeyNumberSign = XK_numbersign,
    KeyDollar = XK_dollar,
    KeyPercent = XK_percent,
    KeyAmpersand = XK_ampersand,
    KeyApostrophe = XK_apostrophe,
    KeyParenleft = XK_parenleft,
    KeyParenright = XK_parenright,
    KeyAsterisk = XK_asterisk,
    KeyPlus = XK_plus,
    KeyComma = XK_comma,
    KeyMinus = XK_minus,
    KeyPeriod = XK_period,
    KeySlash = XK_slash,
    Key0 = XK_0,
    Key1 = XK_1,
    Key2 = XK_2,
    Key3 = XK_3,
    Key4 = XK_4,
    Key5 = XK_5,
    Key6 = XK_6,
    Key7 = XK_7,
    Key8 = XK_8,
    Key9 = XK_9,
    KeyColon = XK_colon,
    KeySemicolon = XK_semicolon,
    KeyLess = XK_less,
    KeyEqual = XK_equal,
    KeyGreater = XK_greater,
    KeyQuestion = XK_question,
    KeyAt = XK_at,
    KeyBracketLeft = XK_bracketleft,
    KeyBackslash = XK_backslash,
    KeyBracketRight = XK_bracketright,
    KeyAsciiCircum = XK_asciicircum,
    KeyUnderscore = XK_underscore,
    KeyGrave = XK_grave,
    KeyA = XK_a,
    KeyB = XK_b,
    KeyC = XK_c,
    KeyD = XK_d,
    KeyE = XK_e,
    KeyF = XK_f,
    KeyG = XK_g,
    KeyH = XK_h,
    KeyI = XK_i,
    KeyJ = XK_j,
    KeyK = XK_k,
    KeyL = XK_l,
    KeyM = XK_m,
    KeyN = XK_n,
    KeyO = XK_o,
    KeyP = XK_p,
    KeyQ = XK_q,
    KeyR = XK_r,
    KeyS = XK_s,
    KeyT = XK_t,
    KeyU = XK_u,
    KeyV = XK_v,
    KeyW = XK_w,
    KeyX = XK_x,
    KeyY = XK_y,
    KeyZ = XK_z,
    KeyBraceLeft = XK_braceleft,
    KeyBar = XK_bar,
    KeyBraceRight = XK_braceright,
    KeyAsciiTilde = XK_asciitilde,
    KeyNum_Lock = XK_Num_Lock,
    KeyKP_0 = XK_KP_0,
    KeyKP_1 = XK_KP_1,
    KeyKP_2 = XK_KP_2,
    KeyKP_3 = XK_KP_3,
    KeyKP_4 = XK_KP_4,
    KeyKP_5 = XK_KP_5,
    KeyKP_6 = XK_KP_6,
    KeyKP_7 = XK_KP_7,
    KeyKP_8 = XK_KP_8,
    KeyKP_9 = XK_KP_9,
    KeyKP_Decimal = XK_KP_Decimal,
    KeyKP_Divide = XK_KP_Divide,
    KeyKP_Multiply = XK_KP_Multiply,
    KeyKP_Subtract = XK_KP_Subtract,
    KeyKP_Add = XK_KP_Add,
    KeyKP_Enter = XK_KP_Enter,
    KeyKP_Equal = XK_KP_Equal,
    KeyF1 = XK_F1,
    KeyF2 = XK_F2,
    KeyF3 = XK_F3,
    KeyF4 = XK_F4,
    KeyF5 = XK_F5,
    KeyF6 = XK_F6,
    KeyF7 = XK_F7,
    KeyF8 = XK_F8,
    KeyF9 = XK_F9,
    KeyF10 = XK_F10,
    KeyF11 = XK_F11,
    KeyF12 = XK_F12,
    KeyF13 = XK_F13,
    KeyF14 = XK_F14,
    KeyF15 = XK_F15,
    KeyF16 = XK_F16,
    KeyF17 = XK_F17,
    KeyF18 = XK_F18,
    KeyF19 = XK_F19,
    KeyF20 = XK_F20,
    KeyF21 = XK_F21,
    KeyF22 = XK_F22,
    KeyF23 = XK_F23,
    KeyF24 = XK_F24,
    KeyF25 = XK_F25,
    KeyF26 = XK_F26,
    KeyF27 = XK_F27,
    KeyF28 = XK_F28,
    KeyF29 = XK_F29,
    KeyF30 = XK_F30,
    KeyF31 = XK_F31,
    KeyF32 = XK_F32,
    KeyF33 = XK_F33,
    KeyF34 = XK_F34,
    KeyF35 = XK_F35,
    KeyShift_L = XK_Shift_L,
    KeyShift_R = XK_Shift_R,
    KeyControl_L = XK_Control_L,
    KeyControl_R = XK_Control_R,
    KeyCaps_Lock = XK_Caps_Lock,
    KeyShift_Lock = XK_Shift_Lock,
    KeyMeta_L = XK_Meta_L,
    KeyMeta_R = XK_Meta_R,
    KeyAlt_L = XK_Alt_L,
    KeyAlt_R = XK_Alt_R,
    KeySuper_L = XK_Super_L,
    KeySuper_R = XK_Super_R,
    KeyHyper_L = XK_Hyper_L,
    KeyHyper_R = XK_Hyper_R,
} keycode_t;

typedef struct {
  float x, y;
} v2;

typedef struct {
  v2 pos, size;
} area;

typedef void (*event_handler_t)(xcb_generic_event_t* ev);

typedef struct monitor monitor;

struct monitor {
  area area;
  monitor* next;
  uint32_t idx;
};

typedef struct client client;

struct client {
  area area, area_prev;
  bool fullscreen, floating;
  xcb_window_t win;

  client* next;

  size_t borderwidth;

  monitor* mon;
  int32_t desktop;

  bool urgent, ignoreunmap;
};

typedef enum {
  EWMHsupported, 
  EWMHname, 
  EWMHstate, 
  EWMHstateHidden, 
  EWMHcheck,
  EWMHfullscreen, 
  EWMHactiveWindow, 
  EWMHwindowType,
  EWMHwindowTypeDialog, 
  EWMHclientList,
  EWMHcurrentDesktop,
  EWMHcount
} ewmh_atom;

typedef enum {
  WMprotocols,
  WMdelete,
  WMstate,
  WMtakeFocus,
  WMcount
} wm_atom;


typedef struct {
  xcb_connection_t* con;
  xcb_window_t root;
  xcb_screen_t* screen; 

  client* clients;
  client* focus;

  v2 grabcursor;
  area grabwin;

  monitor* monitors;
  monitor* monfocus;

  xcb_atom_t wm_atoms[WMcount]; 
  xcb_atom_t ewmh_atoms[EWMHcount];

  int32_t* curdesktop;
} State;
