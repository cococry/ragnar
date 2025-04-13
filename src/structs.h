#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <xcb/xcb_ewmh.h>

#include <GL/gl.h>
#include <GL/glx.h>
#include <xcb/xproto.h>

typedef struct state_t state_t;
typedef struct passthrough_data_t passthrough_data_t;

void terminate_successfully(state_t* s, passthrough_data_t data);
void cyclefocusdown(state_t* s, passthrough_data_t data);
void cyclefocusup(state_t* s, passthrough_data_t data);
void killfocus(state_t* s, passthrough_data_t data);
void togglefullscreen(state_t* s, passthrough_data_t data);
void raisefocus(state_t* s, passthrough_data_t data);
void cycledesktopup(state_t* s, passthrough_data_t data); 
void cycledesktopdown(state_t* s, passthrough_data_t data);
void cyclefocusdesktopup(state_t* s, passthrough_data_t data);
void cyclefocusdesktopdown(state_t* s, passthrough_data_t data);
void switchdesktop(state_t* s, passthrough_data_t data); 
void switchfocusdesktop(state_t* s, passthrough_data_t data);
void runcmd(state_t* s, passthrough_data_t data);
void addfocustolayout(state_t* s, passthrough_data_t data);
void settiledmaster(state_t* s, passthrough_data_t data);
void setverticalstripes(state_t* s, passthrough_data_t data);
void sethorizontalstripes(state_t* s, passthrough_data_t data); 
void setfloatingmode(state_t* s, passthrough_data_t data);
void updatebarslayout(state_t* s, passthrough_data_t data);
void cycledownlayout(state_t* s, passthrough_data_t data);
void cycleuplayout(state_t* s, passthrough_data_t data);
void addmasterlayout(state_t* s, passthrough_data_t data);
void removemasterlayout(state_t* s, passthrough_data_t data);
void incmasterarealayout(state_t* s, passthrough_data_t data);
void decmasterarealayout(state_t* s, passthrough_data_t data);
void incgapsizelayout(state_t* s, passthrough_data_t data);
void decgapsizelayout(state_t* s, passthrough_data_t data);
void inclayoutsizefocus(state_t* s, passthrough_data_t data);
void declayoutsizefocus(state_t* s, passthrough_data_t data);
void movefocusup(state_t* s, passthrough_data_t data);
void movefocusdown(state_t* s, passthrough_data_t data);
void movefocusleft(state_t* s, passthrough_data_t data);
void movefocusright(state_t* s, passthrough_data_t data);
void cyclefocusmonitordown(state_t* s, passthrough_data_t data);
void cyclefocusmonitorup(state_t* s, passthrough_data_t data);
void togglescratchpad(state_t* s, passthrough_data_t data);
void reloadconfigfile(state_t* s, passthrough_data_t data);

#define _XCB_EV_LAST 36 

/* Evaluates to the length (count of elements) in a given array */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
/* Evaluates to the minium of two given numbers */
#define MIN(a, b) (((a)<(b))?(a):(b))
/* Evaluates to the maximum of two given numbers */
#define MAX(a, b) (((a)>(b))?(a):(b))

#define MWM_HINTS_DECORATIONS   2
#define MWM_DECOR_ALL           1

typedef enum {
  Shift     = XCB_MOD_MASK_SHIFT,
  Control   = XCB_MOD_MASK_CONTROL,
  Alt       = XCB_MOD_MASK_1,
  Super     = XCB_MOD_MASK_4
} kb_modifier_t;

typedef enum {
  LeftMouse   = XCB_BUTTON_MASK_1,
  MiddleMouse = XCB_BUTTON_MASK_2,
  RightMouse  = XCB_BUTTON_MASK_3,
} mousebtn_t;

typedef enum {
  LayoutFloating = 0,
  LayoutTiledMaster,
  LayoutVerticalStripes,
  LayoutHorizontalStripes
} layout_type_t;

typedef enum {
  LayeringOrderNormal = 0,
  LayeringOrderBelow,
  LayeringOrderAbove,
} layering_order_t;

typedef enum {
  EWMHsupported = 0, 
  EWMHname, 
  EWMHstate, 
  EWMHstateHidden,
  EWMHstateAbove,
  EWMHstateBelow,
  EWMHcheck,
  EWMHfullscreen, 
  EWMHactiveWindow, 
  EWMHwindowType,
  EWMHwindowTypeDialog, 
  EWMHclientList,
  EWMHcurrentDesktop,
  EWMHnumberOfDesktops,
  EWMHdesktopNames,
  EWMHcount
} ewmh_atom_t;

typedef enum {
  WMprotocols = 0,
  WMdelete,
  WMstate,
  WMtakeFocus,
  WMcount
} wm_atom_t;

typedef enum {
  LogLevelTrace = 0,
  LogLevelWarn,
  LogLevelError 
} log_level_t;

struct passthrough_data_t {
  const char* cmd;
  int32_t i;
};

typedef struct state_t state_t;

typedef void (*keycallback_t)(state_t* s, passthrough_data_t data);


typedef struct {
  uint16_t modmask;
  xcb_keysym_t key;
  keycallback_t cb;
  passthrough_data_t data;
} keybind_t;

typedef struct {
  uint32_t flags;
  uint32_t functions;
  uint32_t decorations;
  int32_t  input_mode;
  uint32_t status;
} motif_wm_hints_t;

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
} v2_t;

typedef struct {
  v2_t pos, size;
} area_t;

typedef struct {
  uint32_t left, right, top, bottom;
  int32_t startx, endx;
  int32_t starty, endy;
} strut_t;

typedef struct monitor_t monitor_t;

typedef struct {
  uint32_t nmaster;
  float masterarea;
  int32_t gapsize;
  layout_type_t curlayout;
  bool mastermaxed;
} layout_props_t; 


typedef struct client_t client_t;

struct client_t {
  area_t area, area_prev;
  bool fullscreen, floating, floating_prev,
       fixed, hidden;
  
  bool is_scratchpad;

  int32_t scratchpad_index;

  xcb_window_t win, frame;

  v2_t closebutton, layoutbutton;

  client_t* next;

  size_t borderwidth;

  monitor_t* mon;
  uint32_t desktop;


  bool urgent, ignoreunmap, ignoreexpose, decorated, neverfocus; 

  v2_t minsize;
  v2_t maxsize;

  float layoutsizeadd;

  char* name;
};

typedef struct {
  uint32_t idx;
} desktop_t;
typedef struct {
  char* name;
  bool init;
} named_desktop_t;

struct monitor_t {
  area_t area;
  monitor_t* next;
  uint32_t idx;

  named_desktop_t* activedesktops;
  uint32_t desktopcount;
  layout_props_t* layouts;

  client_t* clients;
};

typedef struct {
  uint32_t maxstruts;
  uint32_t maxdesktops;
  uint32_t maxscratchpads;

  uint32_t winborderwidth;
  uint32_t winbordercolor;
  uint32_t winbordercolor_selected;

  kb_modifier_t modkey; 
  kb_modifier_t winmod;

  mousebtn_t movebtn;
  mousebtn_t resizebtn; 

  uint32_t desktopinit;

  char** desktopnames;

  bool usedecoration;

  double layoutmasterarea;
  double layoutmasterarea_min;
  double layoutmasterarea_max;
  double layoutmasterarea_step;

  double layoutsize_step;
  double layoutsize_min;

  double keywinmove_step;

  int32_t winlayoutgap; 
  int32_t winlayoutgap_max; 
  int32_t winlayoutgap_step;

  layout_type_t initlayout;

  keybind_t* keybinds;
  uint32_t numkeybinds;

  uint32_t motion_notify_debounce_fps;

  bool glvsync;

  char* logfile;
  bool logmessages;
  bool shouldlogtofile;

  char* cursorimage;
} config_data_t;

typedef struct {
  xcb_window_t win;
  bool hidden, needs_restart;
} scratchpad_t;

struct state_t {
  xcb_connection_t* con;
  xcb_ewmh_connection_t ewmh;
  xcb_window_t root;
  xcb_screen_t* screen; 

  float lastexposetime, lastmotiontime; 

  Display* dsp; 

  bool ignore_enter_layout;

  client_t* focus;

  v2_t grabcursor;
  area_t grabwin;

  monitor_t* monitors;
  monitor_t* monfocus;

  xcb_atom_t wm_atoms[WMcount]; 
  xcb_atom_t ewmh_atoms[EWMHcount];

  desktop_t* curdesktop;

  strut_t* winstruts;
  uint32_t nwinstruts;

  config_data_t config;

  scratchpad_t* scratchpads;
  int32_t mapping_scratchpad_index;
};


typedef void (*event_handler_t)(state_t* s, xcb_generic_event_t* ev);

