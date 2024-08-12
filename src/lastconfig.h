#pragma once
/* ======= ragnarwm config ======== 
 * NOTE: For a detailed look at all values that can be used
 * for configuration see the structs.h header. 
 * =============================== */ 


// The maximum number of struts that are noticed by the layouts
// (Struts define the area that a window takes up) This is usefull for 
// docks, bars etc. as the layout keeps space for them.
#define MAX_STRUTS 8

// Specifies how many desktops are allocated
#define MAX_DESKTOPS 9

#include "structs.h"

/* Window properties */
// Specifies the width of the border around frame windows
static const int32_t        winborderwidth = 3; // (px)
                                                // Specifies the color of the border around frame windows
static const int32_t        winbordercolor = 0x555555;
// Specifies the color of the border around frame windows when the client
// window is focused.
static const int32_t        winbordercolor_selected = 0x777777;

// Specifies the main modifier (key) that is used to execute all window manager keybindings
// Options are:
// 	- Shift  
//	- Control 
//	- Alt      
//	- Super 
static const kb_modifier_t modkey   = Super; 

/* Window controls */
// Specifies the modifier (key) that is used to interact with windows (move/resize)
// Options are:
// 	- Shift  
//	- Control 
//	- Alt      
//	- Super 
static const kb_modifier_t winmod   = modkey;
// Specifies the mouse button that needs to be held in combination with pressing the 
// winmod modifier key in order to move a window 
// Options are:
// 	- LeftMouse
// 	- MiddleMouse
//	- RightMouse 
static const mousebtn_t movebtn     = LeftMouse; 
// Specifies the mouse button that needs to be held in combination with pressing the 
// winmod modifier key in order to resize a window 
// Options are:
// 	- LeftMouse
// 	- MiddleMouse
//	- RightMouse 
static const mousebtn_t resizebtn   = RightMouse;

/* Desktops */
// Specifies the desktop index that is selected on startup
static const int32_t desktopinit   = 0;

// Specifies the name of every allocated dekstop (ordered)
static const char* desktopnames[MAX_DESKTOPS] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

/* Decoration */

// Specifies wheter or not window decorations should be enabled. Window decorations 
// in ragnarwm are rendered by using OpenGL 3D acceleration. So naturally, enabling them
// will result in higher memory usage of the window manager as OpenGL context and resources need
// to be created. If you want to squeeze out the most performance, it is recommended to disable decorations.
// Known problems are that NVIDIA drivers on linux don't mix well with OpenGL contexts and drawing to them. 
// If you are expierencing lag, mostly when resizing windows with decoration, its recommended to either turn off
// decoration or set dynamic_rerender_on_resize = false further down in the config 
static const bool     usedecoration       = false;
static const bool     showtitlebars_init  = false;
static uint32_t       titlebarheight  = 30;
static const int32_t  titlebarcolor   = 0x282828;
static const int32_t  fontcolor       = 0xeeeeee;
static const char*    fontpath        = "/usr/share/ragnarwm/fonts/LilexNerdFont-Medium.ttf";
static const char*    closeiconpath   = "/usr/share/ragnarwm/icons/close.png";
static const char*    layouticonpath  = "/usr/share/ragnarwm/icons/layout.png";
static const uint32_t iconcolor       = 0xeeeeee;

/* Layout */
static const float layoutmasterarea      = 0.5f;
static const float layoutmasterarea_min  = 0.1f;
static const float layoutmasterarea_max  = 0.9f;
static const float layoutmasterarea_step  = 0.1f;

static const float layoutsize_step      = 100;   // px
static const float layoutsize_min       = 150;  // px


static const int32_t keywinmove_step = 100;
// Specifies the gap between windows within a tiling layout 
static const int32_t        winlayoutgap = 5; // (px)
static const int32_t        winlayoutgap_max = 150; // (px)
static const int32_t        winlayoutgap_step = 5; // (px)


// Specifies the initial layout type that is used when the window manager 
// initializes.
// Options are:
// 	- LayoutTiledMaster
// 	- LayoutFloating
static const layout_type_t  initlayout   = LayoutTiledMaster;

#define TERMINAL_CMD    "kitty &"
#define MENU_CMD        "~/.config/rofi/launchers/type-3/launcher.sh &"
#define BROWSER_CMD     "firefox &"
#define SCREENSHOT_CMD  "flameshot gui &"

/* Window manger keybinds */
static const keybind_t keybinds[] = {
  /* Modifier   | Key       | Callback              | Command */ 
  {modkey,          KeyEscape,  terminate_successfully, { NULL }},
  /* Clients */
  {modkey,          KeyTab,     cyclefocusdown,     { NULL }},
  {modkey | Shift,  KeyTab,     cyclefocusup,       { NULL }},
  {modkey,          KeyQ,       killfocus,          { NULL }},
  {modkey,          KeyF,       togglefullscreen,   { NULL }},
  {modkey,          KeyR,       raisefocus,         { NULL }},
  {modkey | Shift,  KeyI,       toggletitlebars,    { NULL }},
  {modkey,          KeyI,       togglefocustitlebar,{ NULL }},
  {modkey,          KeyR,       raisefocus,         { NULL }},

  {modkey | Shift,  KeyW,       movefocusup,            { NULL }},
  {modkey | Shift,  KeyA,       movefocusleft,          { NULL }},
  {modkey | Shift,  KeyS,       movefocusdown,          { NULL }},
  {modkey | Shift,  KeyD,       movefocusright,         { NULL }},
  {modkey,          KeyRight,   cyclefocusmonitorup,    { NULL }},
  {modkey,          KeyLeft,    cyclefocusmonitordown,  { NULL }},
  /* Desktops */
  {modkey,          KeyD,       cycledesktopup,          { NULL }},
  {modkey,          KeyA,       cycledesktopdown,        { NULL }},
  {modkey,          KeyP,       cyclefocusdesktopup,     { NULL }},
  {modkey,          KeyO,       cyclefocusdesktopdown,   { NULL }},

  /* Switch desktop hotkeys */
  {modkey,     Key1,       switchdesktop,               { .i = 0 }},
  {modkey,     Key2,       switchdesktop,               { .i = 1 }},
  {modkey,     Key3,       switchdesktop,               { .i = 2 }},
  {modkey,     Key4,       switchdesktop,               { .i = 3 }},
  {modkey,     Key5,       switchdesktop,               { .i = 4 }},
  {modkey,     Key6,       switchdesktop,               { .i = 5 }},
  {modkey,     Key7,       switchdesktop,               { .i = 6 }},
  {modkey,     Key8,       switchdesktop,               { .i = 7 }},
  {modkey,     Key9,       switchdesktop,               { .i = 8 }},

  /* Move focus to desktop hotkeys */
  {modkey | Shift,     Key1,       switchfocusdesktop,  { .i = 0 }},
  {modkey | Shift,     Key2,       switchfocusdesktop,  { .i = 1 }},
  {modkey | Shift,     Key3,       switchfocusdesktop,  { .i = 2 }},
  {modkey | Shift,     Key4,       switchfocusdesktop,  { .i = 3 }},
  {modkey | Shift,     Key5,       switchfocusdesktop,  { .i = 4 }},
  {modkey | Shift,     Key6,       switchfocusdesktop,  { .i = 5 }},
  {modkey | Shift,     Key7,       switchfocusdesktop,  { .i = 6 }},
  {modkey | Shift,     Key8,       switchfocusdesktop,  { .i = 7 }},
  {modkey | Shift,     Key9,       switchfocusdesktop,  { .i = 8 }},

  /* Layout shortcuts */
  {modkey | Shift,  KeyT,       settiledmaster,       { NULL }},
  {modkey | Shift,  KeyR,       setfloatingmode,      { NULL }},
  {modkey | Shift,  KeyH,       sethorizontalstripes, { NULL }},
  {modkey | Shift,  KeyV,       setverticalstripes,   { NULL }},
  {modkey,          KeySpace,   addfocustolayout,     { NULL }},
  {modkey | Shift,  KeyB,   	  updatebarslayout,     { NULL }},
  {modkey,          KeyJ,   	  cycledownlayout,      { NULL }},
  {modkey,  	      KeyK,   	  cycleuplayout,        { NULL }},
  {modkey | Shift,  KeyJ,   	  inclayoutsizefocus, { NULL }},
  {modkey | Shift,  KeyK,   	  declayoutsizefocus, { NULL }},
  {modkey,  	      KeyM,   	  addmasterlayout,      { NULL }},
  {modkey | Shift,  KeyM,   	  removemasterlayout,   { NULL }},
  {modkey,          KeyH,   	  decmasterarealayout,  { NULL }},
  {modkey,          KeyL,   	  incmasterarealayout,  { NULL }},
  {modkey,          KeyMinus,  	decgapsizelayout,     { NULL }},
  {modkey,          KeyPlus,   	incgapsizelayout,     { NULL }},

  /* Application shortcuts */
  {modkey,     KeyReturn,  runcmd, { .cmd = TERMINAL_CMD }},
  {modkey,     KeyS,       runcmd, { .cmd = MENU_CMD }},
  {modkey,     KeyW,       runcmd, { .cmd = BROWSER_CMD }},
  {modkey,     KeyE,       runcmd, { .cmd = SCREENSHOT_CMD }}
};

/* Advanced Configuration */

// The FPS rate at which motion notify events are processed
static const size_t motion_notify_debounce_fps = 60;
// Specifies if non ASCII characters should be rendered on the titlebar 
// Turning this on can lead to unexpected behaviour in some clients.
static const uint32_t decoration_render_non_ascii = false;
// Specifies whether or not vsync should be enabled on OpenGL render windows
static bool glvsync = false;
// The character that is rendered instead of a non-ASCII character 
static const char non_ascii_replacement = ' ';
// Specifies if clients that are interactivly resized should re-render their
// decoration dynamically for every resize or just rerender once the interactive 
// resize operation has finished. Setting this to false can help with performance on 
// either low end system or some versions of propriatary NVIDIA drivers as they cannot 
// handle multiple OpenGL contexts well.
static const bool dynamic_rerender_on_resize = true;


// Specifies the file to log debug messages to 
static const char* logfile = "/home/cococry/ragnarwm.log";
// Specifies the default cursor image on the root window
static const char* cursorimage = "arrow"; 
// Specifies whether or not logging should be enabled
static bool logmessages = false;
// Specifies whether or not messages should be logged to the logfile
static bool shouldlogtofile = true;

