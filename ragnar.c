#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_cursor.h>
#include <xcb/randr.h>
#include <xcb/composite.h>

#include <X11/keysym.h>
#include <X11/Xlib-xcb.h>
#include <X11/Xlib.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include "structs.h"


static void             setup();
static void             loop();

static bool             pointinarea(v2_t p, area_t a);
static v2_t             cursorpos(bool* success);
static area_t           winarea(xcb_window_t win, bool* success);
static xcb_window_t 	truecolorwindow(area_t a, uint32_t bw);
static float            getoverlaparea(area_t a, area_t b);

static void             setbordercolor(client_t* cl, uint32_t color);
static void             setborderwidth(client_t* cl, uint32_t width);
static void             moveclient(client_t* cl, v2_t pos);
static void             resizeclient(client_t* cl, v2_t size);
static void             moveresizeclient(client_t* cl, area_t a);
static void             raiseclient(client_t* cl);
static bool             clienthasdeleteatom(client_t* cl);
static bool             clientonscreen(client_t* cl, monitor_t* mon);
static char*            getclientname(client_t* cl);
static void             killclient(client_t* cl);
static void             focusclient(client_t* cl);
static void             setxfocus(client_t* cl);
static void             frameclient(client_t* cl);
static void             unframeclient(client_t* cl);
static void             unfocusclient(client_t* cl);
static void             configclient(client_t* cl);
static void             hideclient(client_t* cl);
static void             showclient(client_t* cl);
static bool             raiseevent(client_t* cl, xcb_atom_t protocol);
static void             setwintype(client_t* cl);
static void             seturgent(client_t* cl, bool urgent);
static client_t*	nextvisible(bool tiled);
static client_t*	prevvisible(bool tiled);
static xcb_atom_t       getclientprop(client_t* cl, xcb_atom_t prop);
static void             setfullscreen(client_t* cl, bool fullscreen);
static void             switchclientdesktop(client_t* cl, int32_t desktop);
static uint32_t 	numinlayout(monitor_t* mon);

static void             makelayout(monitor_t* mon);
static void             tiledmaster(monitor_t* mon);
static void 		swapclients(client_t* c1, client_t* c2);

static void             uploaddesktopnames();
static void             createdesktop(uint32_t idx, monitor_t* mon);

static void             setupatoms();
static void             grabkeybinds();
static void             loaddefaultcursor();

static void             setuptitlebar(client_t* cl);
static void             rendertitlebar(client_t* cl);
static void             updatetitlebar(client_t* cl);
static void             hidetitlebar(client_t* cl);
static void             showtitlebar(client_t* cl);

static void             evmaprequest(xcb_generic_event_t* ev);
static void             evunmapnotify(xcb_generic_event_t* ev);
static void             evdestroynotify(xcb_generic_event_t* ev);
static void             eventernotify(xcb_generic_event_t* ev);
static void             evleavenotify(xcb_generic_event_t* ev);
static void             evkeypress(xcb_generic_event_t* ev);
static void             evbuttonpress(xcb_generic_event_t* ev);
static void             evbuttonrelease(xcb_generic_event_t* ev);
static void             evmotionnotify(xcb_generic_event_t* ev);
static void             evconfigrequest(xcb_generic_event_t* ev);
static void             evconfignotify(xcb_generic_event_t* ev);
static void             evpropertynotify(xcb_generic_event_t* ev);
static void             evclientmessage(xcb_generic_event_t* ev);
static void             evexpose(xcb_generic_event_t* ev);

static client_t*        addclient(xcb_window_t win);
static void             releaseclient(xcb_window_t win);
static client_t*        clientfromwin(xcb_window_t win);
static client_t*        clientfromtitlebar(xcb_window_t titlebar);

static monitor_t*       addmon(area_t a, uint32_t idx); 
static monitor_t*       monbyarea(area_t a);
static monitor_t*       clientmon(client_t* cl);
static desktop_t*       mondesktop(monitor_t* mon);
static monitor_t*       cursormon();
static uint32_t         updatemons();

static xcb_keysym_t     getkeysym(xcb_keycode_t keycode);
static xcb_keycode_t*   getkeycodes(xcb_keysym_t keysym);
static strut_t          readstrut(xcb_window_t win);
static void             getwinstruts();

static xcb_atom_t       getatom(const char* atomstr);

static void             ewmh_updateclients();

static void             sigchld_handler(int32_t signum);

static bool             strinarr(char* array[], int count, const char* target);
static int32_t          compstrs(const void* a, const void* b);
static void             strtoascii(char* str); 

static void             initglcontext();
static void             setglcontext(xcb_window_t win);

static void 		logtofile(const char* fmt, ...);
static char* 		cmdoutput(const char* cmd);


// This needs to be included after the function definitions
#include "config.h"


/* Evaluates to the length (count of elements) in a given array */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
/* Evaluates to the minium of two given numbers */
#define MIN(a, b) (((a)<(b))?(a):(b))
/* Evaluates to the maximum of two given numbers */
#define MAX(a, b) (((a)>(b))?(a):(b))

/* */
static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]         = evmaprequest,
  [XCB_UNMAP_NOTIFY]        = evunmapnotify,
  [XCB_DESTROY_NOTIFY]      = evdestroynotify,
  [XCB_ENTER_NOTIFY]        = eventernotify,
  [XCB_LEAVE_NOTIFY]        = evleavenotify,
  [XCB_KEY_PRESS]           = evkeypress,
  [XCB_BUTTON_PRESS]        = evbuttonpress,
  [XCB_BUTTON_RELEASE]      = evbuttonrelease,
  [XCB_MOTION_NOTIFY]       = evmotionnotify,
  [XCB_CONFIGURE_REQUEST]   = evconfigrequest,
  [XCB_CONFIGURE_NOTIFY]    = evconfignotify,
  [XCB_PROPERTY_NOTIFY]     = evpropertynotify,
  [XCB_CLIENT_MESSAGE]      = evclientmessage,
  [XCB_EXPOSE]              = evexpose,
};


static state_t s;

/**
 * @brief Sets up the WM state and the X server 
 *
 * This function establishes a s.conection to the X server,
 * sets up the root window and window manager keybindings.
 * The event mask of the root window is being cofigured to
 * listen to necessary events. 
 * After the configuration of the root window, all the specified
 * keybinds in config.h are grabbed by the window manager.
 */
void
setup() {
  // Setup SIGCHLD handler
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  s.clients = NULL;

  s.dsp = XOpenDisplay(NULL);
  if(!s.dsp) {
    fprintf(stderr, "rangar: cannot open X Display.\n");
    terminate();
  }
  // s.conecting to the X server
  s.con = XGetXCBConnection(s.dsp);
  // Checking for errors
  if (xcb_connection_has_error(s.con) || !s.con) {
    fprintf(stderr, "ragnar: cannot open display.\n");
    terminate();
  }

  if(!usedecoration) {
    titlebarheight = 0;
  }

  s.lastexposetime = 0;
  s.lastmotiontime = 0;

  s.curlayout = initlayout;

  // Lock the display to prevent concurrency issues
  XSetEventQueueOwner(s.dsp, XCBOwnsEventQueue);

  s.initgl = false;

  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  s.root = screen->root;
  s.screen = screen;

  /* Setting event mask for root window */
  uint32_t evmask[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
      XCB_EVENT_MASK_ENTER_WINDOW |
      XCB_EVENT_MASK_FOCUS_CHANGE | 
      XCB_EVENT_MASK_POINTER_MOTION
  };
  xcb_change_window_attributes_checked(s.con, s.root, XCB_CW_EVENT_MASK, evmask);

  // Load the default root cursor image
  loaddefaultcursor();

  // Grab the window manager's keybinds
  grabkeybinds();


  // Handle monitor setup 
  uint32_t registered_monitors = updatemons();

  // Setup atoms for EWMH and so on
  setupatoms();

  // Gather strut information 
  s.nwinstruts = 0;
  getwinstruts(s.root);

  s.curdesktop = malloc(sizeof(*s.curdesktop) * registered_monitors);
  if(s.curdesktop) {
    for(uint32_t i = 0; i < registered_monitors; i++) {
      s.curdesktop[i].idx = desktopinit;
    }
  }
}

/**
 * @brief Event loop of the window manager 
 *
 * This function polls for X server events and 
 * handles them accoringly by calling the associated event handler.
 */
void
loop() {
  xcb_generic_event_t *ev;

  while (1) {
    // Poll for events without blocking
    while ((ev = xcb_wait_for_event(s.con))) {
      uint8_t evcode = ev->response_type & ~0x80;
      /* If the event we receive is listened for by our 
       * event listeners, call the callback for the event. */
      if (evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
	evhandlers[evcode](ev);
      }
      free(ev);
    }
  }
}

/**
 * @brief Terminates the window manager 
 *
 * This function terminates the window manager by
 * diss.conecting the s.conection to the X server and
 * exiting the program.
 */
void 
terminate() {
  {
    client_t* cl = s.clients;
    client_t* next;
    while(cl != NULL) {
      next = cl->next;
      releaseclient(cl->win);
      cl = next; 
    }
  }
  {
    monitor_t* mon = s.monitors;
    monitor_t* next;
    while (mon != NULL) {
      next = mon->next;
      free(mon);
      mon = next;
    }
  }
  xcb_disconnect(s.con);
  exit(EXIT_SUCCESS);
}

/**
 * @brief Evaluates if a given point is inside a given area 
 *
 * @param p The point to check if it is inside the given area
 * @param area The area to check
 *
 * @return True if the point p is in the area, false if it is not in the area 
 */
bool
pointinarea(v2_t p, area_t area) {
  return (p.x >= area.pos.x &&
      p.x < (area.pos.x + area.size.x) &&
      p.x >= area.pos.y &&
      p.y < (area.pos.y + area.size.y));
}

/**
 * @brief Returns the current cursor position on the X display 
 *
 * @param success Gets assigned whether or not the pointer query
 * was successfull.
 *
 * @return The cursor position as a two dimensional vector 
 */
v2_t 
cursorpos(bool* success) {
  // Query the pointer position
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s.con, xcb_query_pointer(s.con, s.root), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve cursor position."); 
    free(reply);
    return (v2_t){0};
  }
  // Create a v2_t for to store the position 
  v2_t cursor = (v2_t){.x = reply->root_x, .y = reply->root_y};

  // Check for errors
  free(reply);
  return cursor;
}

/**
 * @brief Returns the area (position and size) of a given window 
 *
 * @param win The window to get the area from 
 * @param success Gets assigned whether or not the geometry query
 * was successfull.
 *
 * @return The area of the given window 
 */
area_t
winarea(xcb_window_t win, bool* success) {
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve cursor position."); 
    free(reply);
    return (area_t){0};
  }
  // Creating the area structure to store the geometry in
  area_t a = (area_t){.pos = (v2_t){reply->x, reply->y}, .size = (v2_t){reply->width, reply->height}};

  free(reply);
  return a;
}

xcb_window_t
truecolorwindow(area_t a, uint32_t bw) {
  xcb_window_t win;
  XVisualInfo vinfo;
  if (!XMatchVisualInfo(s.dsp, DefaultScreen(s.dsp), 32, TrueColor, &vinfo)) {
    fprintf(stderr, "ragnar: no true-color visual found.\n");
    terminate();
  }

  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(s.dsp, DefaultRootWindow(s.dsp), vinfo.visual, AllocNone);
  attr.border_pixel = bw;
  attr.background_pixel = 0;
  attr.override_redirect = True; 

  unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect;

  win = XCreateWindow(s.dsp, DefaultRootWindow(s.dsp),
      a.pos.x, a.pos.y, a.size.x, a.size.y,
      winborderwidth, vinfo.depth, InputOutput, vinfo.visual,
      mask, &attr);

  return win;
}

/**
 * @brief Gets the area (in float) of the overlap between two areas 
 *
 * @param a The inner area 
 * @param a The outer area 
 *
 * @return The overlap between the two areas 
 */
float
getoverlaparea(area_t a, area_t b) {
  float x = MAX(0, MIN(a.pos.x + a.size.x, b.pos.x + b.size.x) - MAX(a.pos.x, b.pos.x));
  float y = MAX(0, MIN(a.pos.y + a.size.y, b.pos.y + b.size.y) - MAX(a.pos.y, b.pos.y));
  return x * y;
}

/**
 * @brief Sets the border color of a given clients window 
 *
 * @param cl The client to set the border color of 
 * @param color The border color 
 */
void
setbordercolor(client_t* cl, uint32_t color) {
  // Return if the client is NULL
  if(!cl) {
    return;
  }
  // Change the configuration for the border color of the clients window
  xcb_change_window_attributes_checked(s.con, cl->frame, XCB_CW_BORDER_PIXEL, &color);
}

/**
 * @brief Sets the border width of a given clients window 
 * and updates its 'borderwidth' variable 
 *
 * @param cl The client to set the border width of 
 * @param width The border width 
 */
void
setborderwidth(client_t* cl, uint32_t width) {
  if(!cl) {
    return;
  }
  // Change the configuration for the border width of the clients window
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){width});
  // Update the border width of the client
  cl->borderwidth = width;
}

/**
 * @brief Moves the window of a given client and updates its area.
 *
 * @param cl The client to move 
 * @param pos The position to move the client to 
 */
void
moveclient(client_t* cl, v2_t pos) {
  if(!cl) {
    return;
  }
  int32_t posval[2] = {
    (uint32_t)pos.x, (uint32_t)pos.y
  };

  // Move the window by configuring it's x and y position property
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);

  cl->area.pos = pos;

  configclient(cl);
  updatetitlebar(cl);

  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
  uploaddesktopnames();
  s.monfocus = cl->mon;
}

/**
 * @brief Resizes the window of a given client and updates its area.
 *
 * @param cl The client to resize 
 * @param pos The new size of the clients window 
 */
void
resizeclient(client_t* cl, v2_t size) {
  if (!cl) {
    return;
  }

  // Retrieve size hints
  xcb_size_hints_t hints;
  if (xcb_icccm_get_wm_normal_hints_reply(s.con, xcb_icccm_get_wm_normal_hints(s.con, cl->win), &hints, NULL)) {
    // Enforce minimum size
    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
      if (size.x < hints.min_width) size.x = hints.min_width;
      if (size.y < hints.min_height) size.y = hints.min_height;
    }

    // Enforce maximum size
    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
      if (size.x > hints.max_width) size.x = hints.max_width;
      if (size.y > hints.max_height) size.y = hints.max_height;
    }
  }

  uint32_t sizeval[2] = { (uint32_t)size.x, (uint32_t)size.y };
  uint32_t sizeval_content[2] = { (uint32_t)size.x, (uint32_t)size.y - ((cl->showtitlebar) ? titlebarheight : 0.0f)};

  // Resize the window by configuring its width and height property
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval_content);
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);

  updatetitlebar(cl);

  cl->area.size = size;
}

/**
 * @brief Moves and resizes the window of a given client and updates its area.
 *
 * @param cl The client to move and resize
 * @param a The new area (position and size) for the client's window
 */
void
moveresizeclient(client_t* cl, area_t a) {
  if (!cl) {
    return;
  }
  uint32_t values[4] = {
    (uint32_t)a.pos.x, 
    (uint32_t)a.pos.y,
    (uint32_t)a.size.x,
    (uint32_t)a.size.y
  };
  uint32_t values_content[4] = {
    (uint32_t)a.size.x,
    (uint32_t)a.size.y - ((cl->showtitlebar) ? titlebarheight : 0.0f)
  };

  // Move and resize the window by configuring its x, y, width, and height properties
  xcb_configure_window(s.con, cl->frame, 
      XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
  xcb_configure_window(s.con, cl->win, 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values_content);

  // Update clients area
  cl->area = a;
  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
  uploaddesktopnames();

  s.monfocus = cl->mon; 
  updatetitlebar(cl);
}

/**
 * @brief Raises the window of a given client to the top of the stack
 *
 * @param cl The client to raise 
 */
void
raiseclient(client_t* cl) {
  if(!cl) {
    return;
  }
  uint32_t config[] = { XCB_STACK_MODE_ABOVE };
  // Change the configuration of the window to be above 
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_STACK_MODE, config);
}


/**
 * @brief Checks if a client has a WM_DELETE atom set 
 *
 * @param cl The client to check delete atom for
 *
 * @return Whether or not the given client has a WM_DELETE atom 
 */
bool 
clienthasdeleteatom(client_t* cl) {
  bool ret = false;
  xcb_icccm_get_wm_protocols_reply_t reply;

  if(xcb_icccm_get_wm_protocols_reply(s.con, xcb_icccm_get_wm_protocols_unchecked(s.con, cl->win, s.wm_atoms[WMprotocols]), &reply, NULL)) {
    for(uint32_t i = 0; !ret && i < reply.atoms_len; i++) {
      if(reply.atoms[i] == s.wm_atoms[WMdelete]) {
	ret = true;
      }
    }
    xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  }

  return ret;
}

/**
 * @brief Checks if a client is on a given monitor and if it is 
 * currently visible (on the current desktop) 
 *
 * @param cl The client to check visibility for 
 *
 * @return Whether or not the given client is visible and on the given monitor 
 */
bool 
clientonscreen(client_t* cl, monitor_t* mon) {
  return cl->mon == mon && cl->desktop == mondesktop(cl->mon)->idx;
}

/**
 * @brief Retrieves the name of a given client (allocates memory) 
 *
 * @param cl The client to retrieve a name from  
 *
 * @return The name of the given client  
 */
char* 
getclientname(client_t* cl) {
  xcb_icccm_get_text_property_reply_t prop;
  // Try to get _NET_WM_NAME
  xcb_get_property_cookie_t cookie = xcb_icccm_get_text_property(s.con, cl->win, XCB_ATOM_WM_NAME);
  if (xcb_icccm_get_text_property_reply(s.con, cookie, &prop, NULL)) {
    char* name = strndup(prop.name, prop.name_len);
    xcb_icccm_get_text_property_reply_wipe(&prop);
    return name;
  }

  // If _NET_WM_NAME is not available, try WM_NAME
  cookie = xcb_icccm_get_text_property(s.con, cl->win, XCB_ATOM_WM_NAME);
  if (xcb_icccm_get_text_property_reply(s.con, cookie, &prop, NULL)) {
    char *name = strndup(prop.name, prop.name_len);
    xcb_icccm_get_text_property_reply_wipe(&prop);
    return name;
  }

  return NULL;
}

/**
 * @brief Kills a given client by destroying the associated window and 
 * removing it from the linked list.
 *
 * @param cl The client to kill 
 */
void
killclient(client_t* cl) {
  // If the client specifically has a delete atom set, send a delete event  
  if(clienthasdeleteatom(cl)) {
    xcb_client_message_event_t ev;
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = cl->win;
    ev.format = 32;
    ev.data.data32[0] = s.wm_atoms[WMdelete];
    ev.data.data32[1] = XCB_TIME_CURRENT_TIME;
    ev.type = s.wm_atoms[WMprotocols];
    xcb_send_event(s.con, false, cl->win, XCB_EVENT_MASK_NO_EVENT, (const char*)&ev);
  } else {
    // Force kill the client without sending an event
    xcb_grab_server(s.con);
    xcb_set_close_down_mode(s.con, XCB_CLOSE_DOWN_DESTROY_ALL);
    xcb_kill_client(s.con, cl->win);
    xcb_ungrab_server(s.con);
  }
  makelayout(s.monfocus);
  xcb_flush(s.con);
}

/**
 * @brief Kills the currently focused window 
 */
void
killfocus() {
  if(!s.focus) {
    return;
  }
  killclient(s.focus);
}

/**
 * @brief Sets the current focus to a given client
 *
 * @param cl The client to focus
 */
void
focusclient(client_t* cl) {
  if(!cl || cl->win == s.root) {
    return;
  }

  s.ignore_enter_layout = false;

  // Unfocus the previously focused window to ensure that there is only
  // one focused (highlighted) window at a time.
  if(s.focus) {
    unfocusclient(s.focus);
  }

  // Set input focus 
  setxfocus(cl);

  if(!cl->fullscreen) {
    // Change border color to indicate selection
    setbordercolor(cl, winbordercolor_selected);
    setborderwidth(cl, winborderwidth);
  }

  // Set the focused client
  s.focus = cl;
  uploaddesktopnames();
  s.monfocus = cursormon();
}

/**
 * @brief Sets the X input focus to a given client and handles EWMH atoms & 
 * focus event.
 *
 * @param cl The client to set the input focus to
 */

void
setxfocus(client_t* cl) {
  // Set input focus to client
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);

  // Set active window hint
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHactiveWindow],
      XCB_ATOM_WINDOW, 32, 1, &cl->win);

  // Raise take-focus event
  raiseevent(cl, s.wm_atoms[WMtakeFocus]);
}

/**
 * @brief Creates a frame window for the content of a client window and decoration to live in.
 * The frame window encapsulates the client's window and decoration like titlebar.
 * focus event.
 *
 * @param cl The client to create a frame window for 
 */
void
frameclient(client_t* cl) {
  {
    cl->frame = truecolorwindow(cl->area, winborderwidth);

    // Select input events 
    unsigned long event_mask = StructureNotifyMask | SubstructureNotifyMask | SubstructureRedirectMask |
      PropertyChangeMask | EnterWindowMask | FocusChangeMask | ButtonPressMask;
    XSelectInput(s.dsp, cl->frame, event_mask); 
  }
  setuptitlebar(cl);

  // Reparent the client's content to the newly created frame
  xcb_reparent_window(s.con, cl->win, cl->frame, 0, titlebarheight);

  {
    int32_t posval[2] = {(int32_t)cl->area.pos.x, (int32_t)cl->area.pos.y};
    xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);
    uint32_t sizeval[2] = { (uint32_t)cl->area.size.x, (uint32_t)cl->area.size.y };
    uint32_t sizeval_content[2] = { (uint32_t)cl->area.size.x, (uint32_t)cl->area.size.y - titlebarheight};
    // Resize the window by configuring it's width and height property
    xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval_content);
    xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);
  }
  configclient(cl);

}


/**
 * @brief Destroys and unmaps the frame window of a given client 
 * which consequently removes the client's window from the display
 *
 * @param cl The client to unframe 
 */
void
unframeclient(client_t* cl) {
  xcb_unmap_window(s.con, cl->titlebar);
  xcb_unmap_window(s.con, cl->frame);
  xcb_reparent_window(s.con, cl->win, s.root, 0, 0);
  xcb_destroy_window(s.con, cl->titlebar);
  xcb_destroy_window(s.con, cl->frame);
  xcb_flush(s.con);
}
void
unfocusclient(client_t* cl) {
  if (!cl) {
    return;
  }
  setbordercolor(cl, winbordercolor);
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHactiveWindow]);

  cl->ignoreexpose = false;
}

/**
 * @brief Configures a given client by sending a X configure event 
 *
 * @param cl The client to configure 
 */
void
configclient(client_t* cl) {
  if(!cl) {
    return;
  }
  xcb_configure_notify_event_t event;
  memset(&event, 0, sizeof(event));

  event.response_type = XCB_CONFIGURE_NOTIFY;
  event.window = cl->win;
  event.event = cl->win;
  // New X position of the window
  event.x = cl->area.pos.x;                
  // New Y position of the window
  event.y = cl->area.pos.y + ((cl->showtitlebar) ? titlebarheight : 0.0f);
  // New width of the window
  event.width = cl->area.size.x;            
  // New height of the window
  event.height = cl->area.size.y - ((cl->showtitlebar) ? titlebarheight : 0.0f);
  // Border width
  event.border_width = cl->borderwidth;
  // Above sibling window (None in this case)
  event.above_sibling = XCB_NONE;
  // Override-redirect flag
  event.override_redirect = 0;
  // Send the event
  xcb_send_event(s.con, 0, cl->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *)&event);
}

/*
 * @brief Hides a given client by unframing it's window
 * @param cl The client to hide 
 */
void
hideclient(client_t* cl) {
  cl->ignoreunmap = true;
  xcb_unmap_window(s.con, cl->frame);
}

/*
 * @brief Shows a given client by unframing it's window
 * @param cl The client to show 
 */
void
showclient(client_t* cl) {
  xcb_map_window(s.con, cl->frame);
}

/**
 * @brief Handles a map request event on the X server by 
 * adding the mapped window to the linked list of clients and 
 * setting up necessary stuff.
 *
 * @param ev The generic event 
 */
void 
evmaprequest(xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

  // Retrieving attributes of the mapped window
  xcb_get_window_attributes_cookie_t wa_cookie = xcb_get_window_attributes(s.con, map_ev->window);
  xcb_get_window_attributes_reply_t *wa_reply = xcb_get_window_attributes_reply(s.con, wa_cookie, NULL);
  // Return if attributes could not be retrieved or if the window uses override_redirect
  if (!wa_reply || wa_reply->override_redirect) {
    free(wa_reply);
    return;
  }
  free(wa_reply);
  // Don't handle already managed clients 
  if(clientfromwin(map_ev->window) != NULL) {
    return;
  }

  // Setup listened events for the mapped window
  {
    uint32_t evmask[] = { XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes_checked(s.con, map_ev->window, XCB_CW_EVENT_MASK, evmask);
  }

  // Grabbing mouse events for interactive moves/resizes 
  {
    uint16_t evmask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION;
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
	s.root, XCB_NONE, 1, winmod);
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
	s.root, XCB_NONE, 3, winmod);
  }

  // Retrieving cursor position
  bool cursor_success;
  v2_t cursor = cursorpos(&cursor_success);
  if(!cursor_success) return;

  // Adding the mapped client to our linked list
  client_t* cl = addclient(map_ev->window);

  // Set initial border 
  setbordercolor(cl, winbordercolor);
  setborderwidth(cl, winborderwidth);


  // Set window type of client (e.g dialog)
  setwintype(cl);

  // Set client's monitor
  cl->mon = clientmon(cl);
  cl->desktop = mondesktop(s.monfocus)->idx;

  // Update the EWMH client list
  ewmh_updateclients();

  bool success;
  cl->area = winarea(cl->frame, &success);
  if(!success) return;

  if(s.monfocus) {
    // Spawn the window in the center of the focused monitor
    moveclient(cl, (v2_t){
	s.monfocus->area.pos.x + (s.monfocus->area.size.x - cl->area.size.x) / 2.0f, 
	s.monfocus->area.pos.y + (s.monfocus->area.size.y - cl->area.size.y) / 2.0f});
  }

  makelayout(s.monfocus);

  // Map the window
  xcb_map_window(s.con, map_ev->window);

  // Map the window on the screen
  xcb_map_window(s.con, cl->frame);

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(cl);
  }

  // Raise all floating clients
  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(cl, s.monfocus) && cl->floating) {
      raiseclient(cl);
    }
  }
  xcb_flush(s.con);
}
/**
 * @brief Handles a X unmap event by unmapping the window 
 * and removing the associated client from the linked list.
 *
 * @param ev The generic event 
 */
void 
evunmapnotify(xcb_generic_event_t* ev) {
  // Retrieve the event
  xcb_unmap_notify_event_t* unmap_ev = (xcb_unmap_notify_event_t*)ev;

  client_t* cl = clientfromwin(unmap_ev->window);
  if(cl && cl->ignoreunmap) {
    cl->ignoreunmap = false;
    return;
  }
  if(cl) {
    unframeclient(cl);
  } else {
    xcb_unmap_window(s.con, unmap_ev->window);
  }

  // Remove the client from the list
  releaseclient(unmap_ev->window);

  // Update the EWMH client list
  ewmh_updateclients();

  // Re-establish the window layout
  makelayout(s.monfocus);

  xcb_flush(s.con);
}

void 
evdestroynotify(xcb_generic_event_t* ev) {
  xcb_destroy_notify_event_t* destroy_ev = (xcb_destroy_notify_event_t*)ev;
  client_t* cl = clientfromwin(destroy_ev->window);
  if(!cl) {
    return;
  }
  unframeclient(cl);
  releaseclient(destroy_ev->window);
}

/**
 * @brief Handles a X enter-window event by focusing the window
 * that's associated with the enter event.
 *
 * @param ev The generic event 
 */
void 
eventernotify(xcb_generic_event_t* ev) {
  xcb_enter_notify_event_t *enter_ev = (xcb_enter_notify_event_t*)ev;
  if(s.ignore_enter_layout) return;

  if((enter_ev->mode != XCB_NOTIFY_MODE_NORMAL || enter_ev->detail == XCB_NOTIFY_DETAIL_INFERIOR)
      && enter_ev->event != s.root) {
    return;
  }

  client_t* cl = clientfromwin(enter_ev->event);
  bool istitlebar = false;
  if(!cl) {
    cl = clientfromtitlebar(enter_ev->event);
    istitlebar = cl != NULL;

    if(!cl && enter_ev->event != s.root) return;
  }

  // Focus entered client
  if(cl) {
    if(istitlebar) {
      cl->titlebar_render_additional = true;
      rendertitlebar(cl);
    }
    focusclient(cl);
  }
  else if(enter_ev->event == s.root) {
    // Set Input focus to root
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client_t* cl;
    for (cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->fullscreen) {
	continue;
      }
      setbordercolor(cl, winbordercolor);
      setborderwidth(cl, winborderwidth);
    }
  }

  xcb_flush(s.con);
}

void 
evleavenotify(xcb_generic_event_t* ev) {
  xcb_leave_notify_event_t* leave_ev = (xcb_leave_notify_event_t*)ev;
  client_t* cl = clientfromtitlebar(leave_ev->event);
  if(!cl) return;
  cl->titlebar_render_additional = false;
  rendertitlebar(cl);

  xcb_flush(s.con);
}

/**
 * @brief Handles a X key press event by checking if the pressed 
 * key (and modifiers) match any window manager keybind and then executing
 * that keybinds function.
 *
 * @param ev The generic event 
 */
void
evkeypress(xcb_generic_event_t* ev) {
  xcb_key_press_event_t *e = ( xcb_key_press_event_t *) ev;
  // Get associated keysym for the keycode of the event
  xcb_keysym_t keysym = getkeysym(e->detail);

  /* Iterate throguh the keybinds and check if one of them was pressed. */
  size_t numkeybinds = sizeof(keybinds) / sizeof(keybinds[0]);
  for (uint32_t i = 0; i < numkeybinds; ++i) {
    // If it was pressed, call the callback of the keybind
    if ((keysym == keybinds[i].key) && (e->state == keybinds[i].modmask)) {
      if(keybinds[i].cb) {
	keybinds[i].cb(keybinds[i].data);
      }
    }
  }
  xcb_flush(s.con);
}

/**
 * @brief Handles a X button press event by focusing the client 
 * associated with the pressed window and setting cursor and window grab positions.
 *
 * @param ev The generic event 
 */
void
evbuttonpress(xcb_generic_event_t* ev) {
  xcb_button_press_event_t* button_ev = (xcb_button_press_event_t*)ev;

  // Get the window attributes
  xcb_get_window_attributes_cookie_t attr_cookie = xcb_get_window_attributes(s.con, button_ev->event);
  xcb_get_window_attributes_reply_t* attr_reply = xcb_get_window_attributes_reply(s.con, attr_cookie, NULL);

  if (attr_reply) {
    if (attr_reply->override_redirect) {
      free(attr_reply);
      return; // Do nothing if override_redirect is set
    }
    free(attr_reply);
  } else {
    // Unable to get window attributes
    return;
  }

  client_t* cl = clientfromwin(button_ev->event);
  if (!cl) {
    cl = clientfromtitlebar(button_ev->event);
    if (!cl) return;
    v2_t cursorpos = (v2_t){.x = (float)button_ev->root_x - cl->area.pos.x, .y = (float)button_ev->root_y - cl->area.pos.y};
    area_t closebtnarea = (area_t){
      .pos = cl->closebutton,
	.size = (v2_t){30, titlebarheight}
    };
    area_t layoutbtnarea = (area_t){
      .pos = cl->layoutbutton,
	.size = (v2_t){30, titlebarheight}
    };
    if (pointinarea(cursorpos, closebtnarea)) {
      killclient(cl);
      return;
    }
    if (pointinarea(cursorpos, layoutbtnarea) &&
	s.curlayout != LayoutFloating && cl->floating &&
	cl->titlebar_render_additional) {
      cl->floating = false;
      makelayout(cl->mon);
      return;
    }
    focusclient(cl);
  } else {
    // Focusing client 
    if (cl != s.focus) {
      // Unfocus the previously focused window to ensure that there is only
      // one focused (highlighted) window at a time.
      if (s.focus) {
	unfocusclient(s.focus);
      }
      focusclient(cl);
    }
  }
  bool success;
  area_t area = winarea(cl->frame, &success);
  if(!success) return;

  cl->area = area;
  // Setting grab position
  s.grabwin = cl->area;
  s.grabcursor = (v2_t){.x = (float)button_ev->root_x, .y = (float)button_ev->root_y};

  cl->ignoreexpose = false;

  // Raising the client to the top of the stack
  raiseclient(cl);
  xcb_flush(s.con);
}

void
evbuttonrelease(xcb_generic_event_t* ev) {
  xcb_button_release_event_t* button_ev = (xcb_button_release_event_t*)ev;
  client_t* cl = clientfromwin(button_ev->event);

  if(cl && usedecoration) {
    for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->showtitlebar) {
	rendertitlebar(cl);
      }
    }
    return;
  }

  cl = clientfromtitlebar(button_ev->event);
  if(!cl) return;

  if(button_ev->root_y <= 0) {
    setfullscreen(cl, true);
  }
  cl->ignoreexpose = false;
  rendertitlebar(cl);
  xcb_flush(s.con);
}

/**
 * @brief Handles a X motion notify event by moving the clients window if left mouse 
 * button is held and resizing the clients window if right mouse is held. 
 *
 * @param ev The generic event 
 */
void
evmotionnotify(xcb_generic_event_t* ev) {
  xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)ev;
  // Throttle motiton notify events for performance and to avoid jiterring on certain 
  // high polling-rate mouses.   
  uint32_t curtime = motion_ev->time;
  if((curtime - s.lastmotiontime) <= (1000 / motion_notify_debounce_fps)) {
    return;
  }
  s.lastmotiontime = curtime;

  if(s.ignore_enter_layout) {
    s.ignore_enter_layout = false;
  }

  if(motion_ev->event == s.root) {
    // Update the focused monitor to the monitor under the cursor
    monitor_t* mon = cursormon();
    uploaddesktopnames();
    s.monfocus = mon;
    return;
  }
  if(!(motion_ev->state & resizebtn || motion_ev->state & movebtn)) return;

  // Position of the cursor in the drag event
  v2_t dragpos    = (v2_t){.x = (float)motion_ev->root_x, .y = (float)motion_ev->root_y};
  // Drag difference from the current drag event to the initial grab 
  v2_t dragdelta  = (v2_t){.x = dragpos.x - s.grabcursor.x, .y = dragpos.y - s.grabcursor.y};
  // New position of the window
  v2_t movedest   = (v2_t){.x = (float)(s.grabwin.pos.x + dragdelta.x), .y = (float)(s.grabwin.pos.y + dragdelta.y)};


  client_t* cl = clientfromwin(motion_ev->event);
  if(!cl) {
    if(!(cl = clientfromtitlebar(motion_ev->event))) return;
    if(cl->fullscreen) {
      setfullscreen(cl, false);
      s.grabwin = cl->area;
      movedest = (v2_t){.x = (float)(s.grabwin.pos.x + dragdelta.x), .y = (float)(s.grabwin.pos.y + dragdelta.y)};
    }
    moveclient(cl, movedest);

    if(!cl->floating) {
      // Remove the client from the layout when the user moved it 
      cl->floating = true;
      makelayout(s.monfocus);
    }
    xcb_flush(s.con);
    return;
  }
  if(cl->fullscreen) {
    // Unset fullscreen
    cl->fullscreen = false;
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    cl->borderwidth = winborderwidth;
    setborderwidth(cl, cl->borderwidth);
    showtitlebar(cl);
  }

  // Move the window
  if(motion_ev->state & movebtn) {
    moveclient(cl, movedest);
    cl->desktop = mondesktop(cl->mon)->idx;
  } 
  // Resize the window
  else if(motion_ev->state & resizebtn) {
    // Resize delta (clamped)
    v2_t resizedelta  = (v2_t){.x = MAX(dragdelta.x, -s.grabwin.size.x), .y = MAX(dragdelta.y, -s.grabwin.size.y)};
    // New window size
    v2_t sizedest = (v2_t){.x = s.grabwin.size.x + resizedelta.x, .y = s.grabwin.size.y + resizedelta.y};

    resizeclient(cl, sizedest);
    cl->ignoreexpose = true;
  }

  if(!cl->floating) {
    // Remove the client from the layout when the user moved it 
    cl->floating = true;
    makelayout(s.monfocus);
  }

  xcb_flush(s.con);
}

/**
 * @brief Handles a Xorg configure request by configuring the client that 
 * is associated with the window how the event requested it.
 *
 * @param ev The generic event 
 */
void 
evconfigrequest(xcb_generic_event_t* ev) {
  xcb_configure_request_event_t *config_ev = (xcb_configure_request_event_t *)ev;

  client_t* cl = clientfromwin(config_ev->window);
  if(!cl) {
    uint16_t mask = 0;
    uint32_t values[7];
    uint32_t i = 0;

    if (config_ev->value_mask & XCB_CONFIG_WINDOW_X) {
      mask |= XCB_CONFIG_WINDOW_X;
      values[i++] = config_ev->x;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_Y) {
      mask |= XCB_CONFIG_WINDOW_Y;
      values[i++] = config_ev->y;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
      mask |= XCB_CONFIG_WINDOW_WIDTH;
      values[i++] = config_ev->width;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
      mask |= XCB_CONFIG_WINDOW_HEIGHT;
      values[i++] = config_ev->height;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
      mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
      values[i++] = config_ev->border_width;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
      mask |= XCB_CONFIG_WINDOW_SIBLING;
      values[i++] = config_ev->sibling;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
      mask |= XCB_CONFIG_WINDOW_STACK_MODE;
      values[i++] = config_ev->stack_mode;
    }

    // Configure the window with the specified values
    xcb_configure_window(s.con, config_ev->window, mask, values);
  } else {
    if(s.curlayout != LayoutFloating && !cl->floating) return;
    {
      uint16_t mask = 0;
      uint32_t values[5];
      uint32_t i = 0;
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
	mask |= XCB_CONFIG_WINDOW_WIDTH;
	values[i++] = config_ev->width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
	mask |= XCB_CONFIG_WINDOW_HEIGHT;
	values[i++] = config_ev->height;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
	mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
	values[i++] = config_ev->border_width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
	mask |= XCB_CONFIG_WINDOW_SIBLING;
	values[i++] = config_ev->sibling;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
	mask |= XCB_CONFIG_WINDOW_STACK_MODE;
	values[i++] = config_ev->stack_mode;
      }
      xcb_configure_window(s.con, cl->frame, mask, values);
    }
    {
      uint16_t mask = 0;
      uint32_t values[7];
      uint32_t i = 0;
      mask |= XCB_CONFIG_WINDOW_X;
      mask |= XCB_CONFIG_WINDOW_Y;
      values[i++] = 0;
      values[i++] = ((cl->showtitlebar) ?  titlebarheight : 0.0f);
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
	mask |= XCB_CONFIG_WINDOW_WIDTH;
	values[i++] = config_ev->width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
	mask |= XCB_CONFIG_WINDOW_HEIGHT;
	values[i++] = config_ev->height - ((cl->showtitlebar ? titlebarheight : 0.0f));
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
	mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
	values[i++] = config_ev->border_width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
	mask |= XCB_CONFIG_WINDOW_SIBLING;
	values[i++] = config_ev->sibling;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
	mask |= XCB_CONFIG_WINDOW_STACK_MODE;
	values[i++] = config_ev->stack_mode;
      }
      xcb_configure_window(s.con, cl->win, mask, values);
    }
    bool success;
    cl->area = winarea(cl->frame, &success);
    if(!success) return;

    updatetitlebar(cl);
    configclient(cl);
  }

  xcb_flush(s.con);
}


/*
 * @brief Handles a X configure notify event. If the root window 
 * recieved a configure notify event, the list of monitors is refreshed.
 * */
void
evconfignotify(xcb_generic_event_t* ev) {
  xcb_configure_notify_event_t* config_ev = (xcb_configure_notify_event_t*)ev;

  if(config_ev->window == s.root) {
    updatemons();
  }
  client_t* cl = clientfromwin(config_ev->window);
  if(!cl) return;
  updatetitlebar(cl);

  xcb_flush(s.con);
}
/**
 * @brief Handles a X property notify event for window properties by handling various Extended Window Manager Hints (EWMH). 
 *
 * @param ev The generic event 
 */
void
evpropertynotify(xcb_generic_event_t* ev) {
  xcb_property_notify_event_t* prop_ev = (xcb_property_notify_event_t*)ev;
  client_t* cl = clientfromwin(prop_ev->window);

  if(cl) {
    // Updating the window type if we receive a window type change event.
    if(prop_ev->atom == s.ewmh_atoms[EWMHwindowType]) {
      setwintype(cl);
    }
    if(usedecoration) {
      if(prop_ev->atom == s.ewmh_atoms[EWMHname]) {
	if(cl->name)
	  free(cl->name);
	cl->name = getclientname(cl);
	rendertitlebar(cl);
      }
    }
  }
  xcb_flush(s.con);
}
/**
 * @brief Handles a X client message event by setting fullscreen or 
 * urgency (based on the event request).
 *
 * @param ev The generic event 
 */
void
evclientmessage(xcb_generic_event_t* ev) {
  xcb_client_message_event_t* msg_ev = (xcb_client_message_event_t*)ev;
  client_t* cl = clientfromwin(msg_ev->window);

  if(!cl) {
    return;
  }

  if(msg_ev->type == s.ewmh_atoms[EWMHstate]) {
    // If client requested fullscreen toggle
    if(msg_ev->data.data32[1] == s.ewmh_atoms[EWMHfullscreen] ||
	msg_ev->data.data32[2] == s.ewmh_atoms[EWMHfullscreen]) {
      // Set/unset client fullscreen 
      bool fs =  (msg_ev->data.data32[0] == 1 || (msg_ev->data.data32[0] == 2 && !cl->fullscreen));
      setfullscreen(cl, fs);
      if(fs) {
	hidetitlebar(cl);
      } else {
	showtitlebar(cl);
      }
    }
  } else if(msg_ev->type == s.ewmh_atoms[EWMHactiveWindow]) {
    if(s.focus != cl && !cl->urgent) {
      seturgent(cl, true);
    }
  }
  xcb_flush(s.con);
}

void
evexpose(xcb_generic_event_t* ev) {
  xcb_expose_event_t* expose_ev = (xcb_expose_event_t*)ev;
  client_t* cl = clientfromtitlebar(expose_ev->window);
  if(cl->ignoreexpose && !dynamic_rerender_on_resize) return;
  rendertitlebar(cl);
  xcb_flush(s.con);
}

/**
 * @brief Cycles the currently focused client one down 
 */
void
cyclefocusdown() {
  if (!s.clients || !s.focus)
    return;
 
  client_t* next = nextvisible(false);

  // If there is a next client, just focus it
  if (next) {
    focusclient(next);
    raiseclient(next);
  }
}

/**
 * @brief Cycles the currently focused client one up
 */
void
cyclefocusup() {
  // Return if there are no clients or no focus
  if (!s.clients || !s.focus)
    return;
  
  client_t* next = prevvisible(false);

  if(next) {
    focusclient(next);
    raiseclient(next);
  }
}
/**
 * @brief Raises the currently focused client
 */
void
raisefocus() {
  if(!s.focus) return;
  raiseclient(s.focus);
}


/**
 * @brief Sends a given X event by atom to the window of a given client
 *
 * @param cl The client to send the event to
 * @param protocol The atom to store the event
 */
bool
raiseevent(client_t* cl, xcb_atom_t protocol) {
  bool exists = false;
  xcb_icccm_get_wm_protocols_reply_t reply;

  // Checking if the event protocol exists
  if (xcb_icccm_get_wm_protocols_reply(s.con, xcb_icccm_get_wm_protocols(s.con, cl->win, s.wm_atoms[WMprotocols]), &reply, NULL)) {
    for (uint32_t i = 0; i < reply.atoms_len && !exists; i++) {
      exists = (reply.atoms[i] == protocol);
    }
    xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  }

  if(exists) {
    /* Creating and sending the event structure if the event protocol is 
     * valid. */
    xcb_client_message_event_t event;
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = cl->win;
    event.type = s.wm_atoms[WMprotocols];
    event.format = 32;
    event.data.data32[0] = protocol;
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(s.con, 0, cl->win, XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
  }
  return exists;
}

/**
 * @brief Checks for window type of clients by EWMH and acts 
 * accoringly.
 *
 * @param cl The client to set/update the window type of
 */
void
setwintype(client_t* cl) {
  xcb_atom_t state = getclientprop(cl, s.ewmh_atoms[EWMHstate]);
  xcb_atom_t wintype = getclientprop(cl, s.ewmh_atoms[EWMHwindowType]);

  if(state == s.ewmh_atoms[EWMHfullscreen]) {
    setfullscreen(cl, true);
    hidetitlebar(cl);
  } 
  if(wintype == s.ewmh_atoms[EWMHwindowTypeDialog]) {
    cl->floating = true;
  }
}

void
seturgent(client_t* cl, bool urgent) {
  xcb_icccm_wm_hints_t wmh;
  cl->urgent = urgent;

  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints(s.con, cl->win);
  if (!xcb_icccm_get_wm_hints_reply(s.con, cookie, &wmh, NULL)) {
    return;
  }

  if (urgent) {
    wmh.flags |= XCB_ICCCM_WM_HINT_X_URGENCY;
  } else {
    wmh.flags &= ~XCB_ICCCM_WM_HINT_X_URGENCY;
  }
  xcb_icccm_set_wm_hints(s.con, cl->win, &wmh);
}

client_t* 
nextvisible(bool tiled) {
  client_t* next = NULL;
  // Find the next client on the current monitor & desktop 
  for(client_t* cl = s.focus->next; cl != NULL; cl = cl->next) {
    bool checktiled = (tiled) ? !cl->floating : true;
    if(checktiled && clientonscreen(cl, s.monfocus)) {
      next = cl;
      break;
    }
  }

  // If there is no next client, cycle back to the first client on the 
  // current monitor & desktop
  if(!next) {
    for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    bool checktiled = (tiled) ? !cl->floating : true;
      if(checktiled && clientonscreen(cl, s.monfocus)) {
	next = cl;
	break;
      }
    }
  }
  return next;
}

client_t*
prevvisible(bool tiled) {
  client_t* prev = NULL;
  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    bool checktiled = (tiled) ? !cl->floating : true;
    // Check if the client after the iterating client is the focused
    // client, making it the client after the focus.
    if(checktiled && clientonscreen(cl, s.monfocus) && cl->next == s.focus) {
      prev = cl;
      break;
    }
    // Check if the current client is the focused client 
    // and cycling to the last client that is onscreen to wrap 
    // around.
    if(checktiled && clientonscreen(cl, s.monfocus) && cl == s.focus) {
      for(client_t* cl2 = s.clients; cl2 != NULL; cl2 = cl2->next) {
	bool checktiled2 = (tiled) ? !cl2->floating : true;
	if(checktiled2 && clientonscreen(cl2, s.monfocus) &&
	    (!cl2->next || (cl2->next && 
			    cl2->next->desktop != mondesktop(s.monfocus)->idx))) {
	  prev = cl2;
	  break;
	}
      }
      break;
    }
  }
  return prev;
}

/**
 * @brief Gets the value of a given property on a 
 * window of a given client.
 *
 * @param cl The client to retrieve the property value from
 * @param prop The property to retrieve
 *
 * @return The value of the given property on the given window
 */
xcb_atom_t
getclientprop(client_t* cl, xcb_atom_t prop) {
  xcb_generic_error_t *error;
  xcb_atom_t atom = XCB_NONE;
  xcb_get_property_cookie_t cookie;
  xcb_get_property_reply_t *reply;

  // Get the property from the window
  cookie = xcb_get_property(s.con, 0, cl->win, prop, XCB_ATOM_ATOM, 0, sizeof(xcb_atom_t));
  // Get the reply
  reply = xcb_get_property_reply(s.con, cookie, &error);

  if(reply) { 
    // Check if the property type matches the expected type and has the right format
    if(reply->type == XCB_ATOM_ATOM && reply->format == 32 && reply->value_len > 0) {
      atom = *(xcb_atom_t*)xcb_get_property_value(reply);
    }
    free(reply);
  }
  if(error) {
    free(error);
  }
  return atom;
}

/**
 * @brief Puts a given client in or out of fullscreen
 * mode based on the input.
 *
 * @param cl The client to toggle fullscreen of 
 * @param fullscreen Whether the client should be set 
 * fullscreen or not
 */
void
setfullscreen(client_t* cl, bool fullscreen) {
  if(!s.monfocus || !cl) return;

  cl->fullscreen = fullscreen;
  if(cl->fullscreen) {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 1, &s.ewmh_atoms[EWMHfullscreen]);
    // Store previous position of client
    cl->area_prev = cl->area;
    // Store previous floating state of client
    cl->floating_prev = cl->floating;
    cl->floating = true;
    // Set the client's area to the focused monitors area, effictivly
    // making the client as large as the monitor screen
    cl->area = s.monfocus->area;

    // Unset border of client if it's fullscreen
    cl->borderwidth = 0;
  } else {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    // Set the client's area to the area before the last fullscreen occured 
    cl->area = cl->area_prev;
    cl->floating = cl->floating_prev;
    cl->borderwidth = winborderwidth;
  }
  // Update client's border width
  setborderwidth(cl, cl->borderwidth);

  // Update the clients geometry 
  moveresizeclient(cl, cl->area);

  // Send configure event to the client
  configclient(cl);

  // Raise fullscreened clients
  raiseclient(cl);

  // If the client is 'sudo fullscreen', fullscreen it on the WM
  if(cl->area.size.x >= cl->mon->area.size.x && cl->area.size.y >= cl->mon->area.size.y && !cl->fullscreen) {
    setfullscreen(cl, true);
  }

  makelayout(s.monfocus);
}

/**
 * @brief Switches the desktop of a given client and hides that client.
 * @param cl The client to switch the desktop of
 * @param desktop The desktop to set the client of 
 */
void
switchclientdesktop(client_t* cl, int32_t desktop) {
  // Create the desktop if it was not created yet
  if(!strinarr(s.monfocus->activedesktops, s.monfocus->desktopcount, desktopnames[desktop])) {
    createdesktop(desktop, s.monfocus);
  }

  cl->desktop = desktop;
  if(cl == s.focus) {
    unfocusclient(cl);
  }
  hideclient(cl);
  makelayout(s.monfocus);
}

/**
 * Returns how many client are currently in the layout on a 
 * given monitor.
 * @param mon The monitor to count clients on 
 * */
uint32_t
numinlayout(monitor_t* mon) {
  uint32_t nlayout = 0;
  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(cl, mon) && !cl->floating) {
      nlayout++;
    }
  }
  return nlayout;
}

/**
 * @brief Establishes the current tiling layout for the windows
 * @param mon The monitor to use as the frame of the layout 
 */
void
makelayout(monitor_t* mon) {
  if(s.curlayout == LayoutFloating) return;

  /* Make sure that there is always at least one slave window */
  uint32_t nlayout = numinlayout(s.monfocus);
  uint32_t deskidx = mondesktop(mon)->idx;
  while(nlayout - mon->layouts[deskidx].nmaster == 0 && nlayout != 1) {
    mon->layouts[deskidx].nmaster--;
  }

  switch(s.curlayout) {
    case LayoutTiledMaster: 
      {
	tiledmaster(mon);
	break;
      }
    default: 
      {
	break;
      }
  }
}

/**
 * @brief Cycles the focused window up in the layout list by
 * swapping it with the client before the it.
 */
void 
cycleuplayout() {
  client_t* prev = prevvisible(true);
  s.ignore_enter_layout = true;
  swapclients(s.focus, prev);
  makelayout(s.monfocus);
}

/**
 * @brief Cycles the focused window down in the layout list by
 * swapping it with the client after the it.
 */
void
cycledownlayout() {
  client_t* next = nextvisible(true);
  s.ignore_enter_layout = true;
  swapclients(s.focus, next);
  makelayout(s.monfocus);
}

/**
 * @brief Increments the number of windows that are seen as master windows
 * in master-slave layouts.
 */
void 
addmasterlayout() {
  uint32_t nlayout = numinlayout(s.monfocus);
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(nlayout - (layout->nmaster + 1) >= 1 && nlayout > 1) {
    layout->nmaster++;
  }
  makelayout(s.monfocus);
}

/**
 * @brief Decrements the number of windows that are seen as master windows
 * in master-slave layouts.
 */
void 
removemasterlayout() {
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(layout->nmaster - 1 >= 1) {
    layout->nmaster--;
  }
  makelayout(s.monfocus);
}

void 
incmasterarealayout() {
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(layout->masterarea 
      + layoutmasterarea_step <= layoutmasterarea_max) {
    layout->masterarea += layoutmasterarea_step;
  }
  makelayout(s.monfocus);
}

void 
decmasterarealayout() {
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(layout->masterarea 
      - layoutmasterarea_step >= layoutmasterarea_min) {
    layout->masterarea -= layoutmasterarea_step;
  }
  makelayout(s.monfocus);
}

void 
incgapsizelayout() {
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(layout->gapsize + winlayoutgap_step <= winlayoutgap_max) {
    layout->gapsize += winlayoutgap_step;
  }
  makelayout(s.monfocus);
}

void 
decgapsizelayout() {
  uint32_t deskidx = mondesktop(s.monfocus)->idx;
  layout_props_t* layout = &s.monfocus->layouts[deskidx];

  if(layout->gapsize - winlayoutgap_step >= 0) {
    layout->gapsize -= winlayoutgap_step;
  }
  makelayout(s.monfocus);
}

/**
 * @brief Establishes a tiled master layout for the windows that are 
 * currently visible.
 * @param mon The monitor to use as the frame of the layout 
 */
void 
tiledmaster(monitor_t* mon) {
  if(!mon) return;

  uint32_t nslaves    = 0;
  uint32_t deskidx    = mondesktop(mon)->idx;
  uint32_t nmaster    = mon->layouts[deskidx].nmaster;
  int32_t gapsize     = mon->layouts[deskidx].gapsize;
  float   masterarea  = mon->layouts[deskidx].masterarea;

  {
    uint32_t i = 0;
    for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->floating || cl->desktop != mondesktop(cl->mon)->idx
	  || cl->mon != mon) continue;
      if(i >= nmaster) {
	nslaves++;
      }
      i++;
    }
  }

  uint32_t i = 0;

  uint32_t w = mon->area.size.x;
  uint32_t h = mon->area.size.y;
  int32_t x = mon->area.pos.x;
  int32_t y = mon->area.pos.y;

  // Apply strut information to the layout
  for(uint32_t i = 0; i < s.nwinstruts; i++) {
    bool onmonitor = 
      mon->area.pos.x >= s.winstruts[i].startx 
      && s.winstruts[i].endx <= mon->area.pos.x + mon->area.size.x;

    if(!onmonitor) continue;

    if(s.winstruts[i].left != 0) {
      x += s.winstruts[i].left;
      w -= s.winstruts[i].left;
    }
    if(s.winstruts[i].right != 0) {
      w -= s.winstruts[i].right;
    }
    if(s.winstruts[i].top != 0) {
      y += s.winstruts[i].top;
      h -= s.winstruts[i].top;
    }
    if(s.winstruts[i].bottom != 0) {
      h -= s.winstruts[i].bottom;
    }
  }

  int32_t ymaster = y;
  int32_t wmaster = w * masterarea;

  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(cl->mon)->idx || cl->mon != mon) continue;

    bool ismaster = (i < nmaster);

    float height = (h / (ismaster ? nmaster : nslaves));
    bool singleclient = !nslaves;

    moveclient(cl, (v2_t){
	(ismaster ? x : (int32_t)(x + wmaster)) + gapsize,
	(ismaster ? ymaster : y) + gapsize});
    resizeclient(cl, (v2_t){
	((singleclient ? w : (ismaster ? (uint32_t)wmaster : (uint32_t)w - wmaster)) 
	 - cl->borderwidth * 2) - gapsize * 2,
	(height - cl->borderwidth * 2) - gapsize * 2});

    if(!ismaster) {
      y += height;
    } else {
      ymaster += height;
    }
    i++;
  }
}

/**
 * @brief Swaps two clients within the linked list of clients 
 * @param c1 The client to swap with c2 
 * @param c2 The client to swap with c1
 */
void 
swapclients(client_t* c1, client_t* c2) {
  if (c1 == c2) {
    return;
  }

  client_t *prev1 = NULL, 
	   *prev2 = NULL, 
	   *tmp = s.clients;

  while (tmp && tmp != c1) {
    prev1 = tmp;
    tmp = tmp->next;
  }

  if (tmp == NULL) return;

  tmp = s.clients;

  while (tmp && tmp != c2) {
    prev2 = tmp;
    tmp = tmp->next;
  }

  if (tmp== NULL) return;

  if (prev1) {
    prev1->next = c2;
  } else { 
    s.clients = c2;
  }

  if (prev2) {
    prev2->next = c1;
  } else { 
    s.clients = c1;
  }

  tmp = c1->next;
  c1->next = c2->next;
  c2->next = tmp;
}

/**
 * @brief Uploads the active desktop names and sends them to EWMH 
 */
void
uploaddesktopnames() {
  qsort(s.monfocus->activedesktops, s.monfocus->desktopcount, sizeof(const char*), compstrs);
  // Calculate the total length of the property value
  size_t total_length = 0;
  for (uint32_t i = 0; i < s.monfocus->desktopcount; i++) {
    total_length += strlen(s.monfocus->activedesktops[i]) + 1; // +1 for the null byte
  }

  // Allocate memory for the data
  char* data = malloc(total_length);

  // Concatenate the desktop names into the data buffer
  char* ptr = data;
  for (uint32_t i = 0; i < s.monfocus->desktopcount; i++) {
    strcpy(ptr, s.monfocus->activedesktops[i]);
    ptr += strlen(s.monfocus->activedesktops[i]) + 1; 
  }

  // Get the root window
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  xcb_window_t root_window = screen->root;

  // Set the _NET_DESKTOP_NAMES property
  xcb_change_property(s.con,
      XCB_PROP_MODE_REPLACE,
      root_window,
      s.ewmh_atoms[EWMHdesktopNames],
      XCB_ATOM_STRING,
      8,
      total_length,
      data);

  free(data);
}
void
createdesktop(uint32_t idx, monitor_t* mon) {
  mon->activedesktops[mon->desktopcount] = malloc(strlen(desktopnames[idx]) + 1);
  if(mon->activedesktops[mon->desktopcount]) {
    strcpy(mon->activedesktops[mon->desktopcount], desktopnames[idx]);
  }

  mon->desktopcount++;
  // Set number of desktops (_NET_NUMBER_OF_DESKTOPS)
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHnumberOfDesktops],
      XCB_ATOM_CARDINAL, 32, 1, &mon->desktopcount);
  uploaddesktopnames();
}

/*
 * Cycles the desktop, that the focused client is on, up
 * */
void
cyclefocusdesktopup() {
  if(!s.focus) return;
  int32_t new_desktop = s.focus->desktop;
  if(new_desktop + 1 < MAX_DESKTOPS) {
    new_desktop++;
  } else {
    new_desktop = 0;
  }
  switchclientdesktop(s.focus, new_desktop);
}

/*
 * Cycles the desktop, that the focused client is on, down 
 * */
void
cyclefocusdesktopdown() {
  if(!s.focus) return;
  int32_t new_desktop = s.focus->desktop;
  if(new_desktop - 1 >= 0) {
    new_desktop--;
  } else {
    new_desktop = MAX_DESKTOPS - 1;
  }
  switchclientdesktop(s.focus, new_desktop);
}

/**
 * @brief Toggles fullscreen mode on the currently focused client
 */
void togglefullscreen() {
  if(!s.focus) return;
  bool fs = !(s.focus->fullscreen);
  if(fs) {
    hidetitlebar(s.focus);
  } else {
    showtitlebar(s.focus);
  }
  setfullscreen(s.focus, fs); 
}

/**
 * @brief Cycles the currently selected desktop index one desktop up 
 */
void
cycledesktopup() {
  int32_t newdesktop = mondesktop(s.monfocus)->idx;
  if(newdesktop + 1 < MAX_DESKTOPS) {
    newdesktop++;
  } else {
    newdesktop = 0;
  }
  switchdesktop((passthrough_data_t){.i = newdesktop});
}

/**
 * @brief Cycles the currently selected desktop index one desktop down 
 */
void
cycledesktopdown() {
  int32_t newdesktop = mondesktop(s.monfocus)->idx;
  if(newdesktop - 1 >= 0) {
    newdesktop--;
  } else {
    newdesktop = MAX_DESKTOPS - 1;
  }
  switchdesktop((passthrough_data_t){.i = newdesktop});
}


/**
 * @brief Switches the currently selected desktop index to the given 
 * index and notifies EWMH that there was a desktop change
 *
 * @param data The .i member is used as the desktop to switch to
 * */
void
switchdesktop(passthrough_data_t data) {
  if(!s.monfocus) return;
  if(data.i == (int32_t)mondesktop(s.monfocus)->idx) return;

  // Create the desktop if it was not created yet
  if(!strinarr(s.monfocus->activedesktops, s.monfocus->desktopcount, desktopnames[data.i])) {
    createdesktop(data.i, s.monfocus);
  }

  uint32_t desktopidx = 0;
  for(uint32_t i = 0; i < s.monfocus->desktopcount; i++) {
    if(strcmp(s.monfocus->activedesktops[i], desktopnames[data.i]) == 0) {
      desktopidx = i;
      break;
    }
  }
  // Notify EWMH for desktop change
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHcurrentDesktop],
      XCB_ATOM_CARDINAL, 32, 1, &desktopidx);



  for (client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    if(cl->mon != s.monfocus) continue;
    // Hide the clients on the current desktop
    if(cl->desktop == mondesktop(s.monfocus)->idx) {
      hideclient(cl);
      // Show the clients on the desktop we want to switch to
    } else if((int32_t)cl->desktop == data.i) {
      showclient(cl);
    }
  }

  // Unfocus all selected clients
  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    unfocusclient(cl);
  }

  mondesktop(s.monfocus)->idx = data.i;
  makelayout(s.monfocus);
}

/**
 * @brief Switches the desktop of the currently selected client to a given index 
 *
 * @param data The .i member is used as the desktop to switch to
 * */
void
switchfocusdesktop(passthrough_data_t data) {
  if(!s.focus) return;
  switchclientdesktop(s.focus, data.i);
}

/**
 * @brief Creates WM atoms & sets up properties for EWMH. 
 * */
void
setupatoms() {
  s.wm_atoms[WMprotocols]             = getatom("WM_PROTOCOLS");
  s.wm_atoms[WMdelete]                = getatom("WM_DELETE_WINDOW");
  s.wm_atoms[WMstate]                 = getatom("WM_STATE");
  s.wm_atoms[WMtakeFocus]             = getatom("WM_TAKE_FOCUS");
  s.ewmh_atoms[EWMHactiveWindow]      = getatom("_NET_ACTIVE_WINDOW");
  s.ewmh_atoms[EWMHsupported]         = getatom("_NET_SUPPORTED");
  s.ewmh_atoms[EWMHname]              = getatom("_NET_WM_NAME");
  s.ewmh_atoms[EWMHstate]             = getatom("_NET_WM_STATE");
  s.ewmh_atoms[EWMHstateHidden]       = getatom("_NET_WM_STATE_HIDDEN");
  s.ewmh_atoms[EWMHcheck]             = getatom("_NET_SUPPORTING_WM_CHECK");
  s.ewmh_atoms[EWMHfullscreen]        = getatom("_NET_WM_STATE_FULLSCREEN");
  s.ewmh_atoms[EWMHwindowType]        = getatom("_NET_WM_WINDOW_TYPE");
  s.ewmh_atoms[EWMHwindowTypeDialog]  = getatom("_NET_WM_WINDOW_TYPE_DIALOG");
  s.ewmh_atoms[EWMHclientList]        = getatom("_NET_CLIENT_LIST");
  s.ewmh_atoms[EWMHcurrentDesktop]    = getatom("_NET_CURRENT_DESKTOP");
  s.ewmh_atoms[EWMHnumberOfDesktops]  = getatom("_NET_NUMBER_OF_DESKTOPS");
  s.ewmh_atoms[EWMHdesktopNames]      = getatom("_NET_DESKTOP_NAMES");

  xcb_atom_t utf8str = getatom("UTF8_STRING");

  xcb_window_t wmcheckwin = xcb_generate_id(s.con);
  xcb_create_window(s.con, XCB_COPY_FROM_PARENT, wmcheckwin, s.root, 
      0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 
      XCB_COPY_FROM_PARENT, 0, NULL);

  // Set _NET_WM_CHECK property on the wmcheckwin
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, wmcheckwin, s.ewmh_atoms[EWMHcheck],
      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_WM_NAME property on the wmcheckwin
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, wmcheckwin, s.ewmh_atoms[EWMHname],
      utf8str, 8, strlen("ragnar"), "ragnar");

  // Set _NET_WM_CHECK property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHcheck],
      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_CURRENT_DESKTOP property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHcurrentDesktop],
      XCB_ATOM_CARDINAL, 32, 1, &desktopinit);

  // Set _NET_SUPPORTED property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHsupported],
      XCB_ATOM_ATOM, 32, EWMHcount, s.ewmh_atoms);

  // Delete _NET_CLIENT_LIST property from the root window
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

  s.monfocus = cursormon();
  // Create initial desktop for all monitors 
  for(monitor_t* mon = s.monitors; mon != NULL; mon = mon->next) {
    createdesktop(desktopinit, mon);
  }

  xcb_flush(s.con);
}


/**
 * @brief Grabs all the keybinds specified in config.h for the window 
 * manager. The function also ungrabs all previously grabbed keys.
 * */
void
grabkeybinds() {
  // Ungrab any grabbed keys
  xcb_ungrab_key(s.con, XCB_GRAB_ANY, s.root, XCB_MOD_MASK_ANY);

  // Grab every keybind
  size_t numkeybinds = sizeof(keybinds) / sizeof(keybinds[0]);
  for (size_t i = 0; i < numkeybinds; ++i) {
    // Get the keycode for the keysym of the keybind
    xcb_keycode_t *keycode = getkeycodes(keybinds[i].key);
    // Grab the key if it is valid 
    if (keycode != NULL) {
      xcb_grab_key(s.con, 1, s.root, keybinds[i].modmask, *keycode,
	  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
  }
  xcb_flush(s.con);
}
/**
 * @brief Loads and sets the default cursor image of the window manager.
 * The default image is the left facing pointer.
 * */
void
loaddefaultcursor() {
  xcb_cursor_context_t* context;
  // Create the cursor context
  if (xcb_cursor_context_new(s.con, s.screen, &context) < 0) {
    fprintf(stderr, "ragnar: cannot create cursor context.\n");
    terminate();
  }
  // Load the context
  xcb_cursor_t cursor = xcb_cursor_load_cursor(context, "arrow");

  // Set the cursor to the root window
  xcb_change_window_attributes(s.con, s.root, XCB_CW_CURSOR, &cursor);

  // Flush the requests to the X server
  xcb_flush(s.con);

  // Free allocated resources
  xcb_cursor_context_free(context);
}

void
setuptitlebar(client_t* cl) {
  if(!usedecoration) return;

  // Initialize OpenGL
  if(!s.initgl) {
    initglcontext();
    s.initgl = true;
  }


  Colormap colormap = DefaultColormap(s.dsp, DefaultScreen(s.dsp));

  // Initialize background color
  XColor color;
  color.red   = ((titlebarcolor >> 16) & 0xFF) * 256; 
  color.green = ((titlebarcolor >> 8) & 0xFF) * 256;  
  color.blue  = (titlebarcolor & 0xFF) * 256;
  color.flags = DoRed | DoGreen | DoBlue;

  // Allocate the color in the colormap
  if (!XAllocColor(s.dsp, colormap, &color)) {
    fprintf(stderr, "ragnar: unable to allocate X color.\n");
    return;
  }


  Window root = DefaultRootWindow(s.dsp);
  XSetWindowAttributes attribs;
  attribs.colormap = XCreateColormap(s.dsp, s.root, s.glvis->visual, AllocNone); 
  attribs.event_mask = EnterWindowMask | LeaveWindowMask | ExposureMask | StructureNotifyMask;
  attribs.background_pixel = color.pixel;

  cl->titlebar = (xcb_window_t)XCreateWindow(s.dsp, root, 0, 0, cl->area.size.x,
      titlebarheight, 0, s.glvis->depth, InputOutput, s.glvis->visual,
      CWColormap | CWEventMask | CWBackPixel, &attribs);
  XReparentWindow(s.dsp, cl->titlebar, cl->frame, 0, 0);
  XMapWindow(s.dsp, cl->titlebar);
  XSync(s.dsp, false);
 
  // Grab Buttons
  {
    unsigned int event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    // Grab Button 1
    XGrabButton(s.dsp, Button1, AnyModifier, cl->titlebar, 
	False, event_mask, GrabModeAsync, GrabModeAsync, None, None);

    // Grab Button 3
    XGrabButton(s.dsp, Button3, AnyModifier, cl->titlebar, 
	False, event_mask, GrabModeAsync, GrabModeAsync, None, None);
  }

  // Set OpenGL context
  setglcontext(cl->titlebar);

  // Set GL swap interval which disables or enables vsync 
  PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = 
    (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
  if (glXSwapIntervalEXT) {
    glXSwapIntervalEXT(s.dsp, cl->titlebar, 0); // Set swap interval to 0 to disable V-Sync
  } else {
    fprintf(stderr, "ragnar: GLX_EXT_swap_control not supported.\n");
  }
  if(!s.ui.init) {
    s.ui = lf_init_x11(cl->area.pos.x, titlebarheight);
    lf_free_font(&s.ui.theme.font);
    s.ui.theme.font = lf_load_font(fontpath, 24);
    s.ui.theme.text_props.text_color = lf_color_from_hex(fontcolor);
    s.closeicon = lf_load_texture(closeiconpath, LF_TEX_FILTER_LINEAR, false);
    s.layouticon = lf_load_texture(layouticonpath, LF_TEX_FILTER_LINEAR, false);
  }
  rendertitlebar(cl);
}

void
rendertitlebar(client_t* cl) {
  if(!usedecoration) return;
  if(!cl) return;
  if(cl->desktop != mondesktop(s.monfocus)->idx) return;
  setglcontext(cl->titlebar);

  uint32_t btnsize = 12;
  float btnmargin = 15;

  // Clear background
  {
    LfColor color = lf_color_from_hex(titlebarcolor);
    vec4s zto = lf_color_to_zto(color);
    glClearColor(zto.r, zto.g, zto.b, zto.a);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  lf_resize_display(&s.ui, cl->area.size.x, titlebarheight);
  glViewport(0, 0, cl->area.size.x, titlebarheight);

  lf_begin(&s.ui);
  lf_set_line_should_overflow(&s.ui, false);
  // Render layout button

  if(cl->titlebar_render_additional && s.curlayout != LayoutFloating && cl->floating)
  {
    LfUIElementProps props = s.ui.theme.button_props;
    props.color = LF_NO_COLOR;
    props.padding = 0;
    props.margin_top = (titlebarheight - btnsize * 1.25) / 2.0f;
    props.border_width = 0;
    props.margin_left = btnmargin;
    props.margin_right = 0;
    lf_push_style_props(&s.ui, props);
    cl->layoutbutton = (v2_t){lf_get_ptr_x(&s.ui), lf_get_ptr_y(&s.ui)};
    lf_set_image_color(&s.ui, lf_color_from_hex(iconcolor));
    lf_image_button(&s.ui, ((LfTexture){.id = s.layouticon.id, .width = btnsize * 1.25, .height = btnsize * 1.25}));
    lf_unset_image_color(&s.ui);
    lf_pop_style_props(&s.ui);
  }

  // Render client name
  {
    // Get client name
    bool namevalid = (cl->name && strlen(cl->name));
    char* displayname = (namevalid ? cl->name : "No name"); 
    // Remove non-ASCII characters if configured 
    if(!decoration_render_non_ascii) {
      strtoascii(displayname);
    }
    // Center text
    float textwidth = lf_text_dimension(&s.ui, displayname).x;
    lf_set_ptr_x_absolute(&s.ui, (cl->area.size.x - textwidth) / 2.0f);

    lf_set_cull_end_x(&s.ui, cl->area.size.x - btnsize * 2 - btnmargin);

    // Display text and remove margin
    LfUIElementProps props = s.ui.theme.text_props;
    props.margin_left = 0.0f;
    lf_push_style_props(&s.ui, props);
    lf_text(&s.ui, displayname); 
    lf_pop_style_props(&s.ui);

    lf_unset_cull_end_x(&s.ui);
  }

  // Render close button
  {
    LfUIElementProps props = s.ui.theme.button_props;
    lf_set_ptr_x_absolute(&s.ui, cl->area.size.x - btnsize - btnmargin);

    props.color = LF_NO_COLOR;
    props.padding = 0;
    props.margin_top = (titlebarheight - btnsize) / 2.0f;
    props.border_width = 0;
    props.margin_left = 0;
    props.margin_right = 0;
    lf_push_style_props(&s.ui, props);
    cl->closebutton = (v2_t){lf_get_ptr_x(&s.ui), lf_get_ptr_y(&s.ui)};
    lf_set_image_color(&s.ui, lf_color_from_hex(iconcolor));
    lf_image_button(&s.ui, ((LfTexture){.id = s.closeicon.id, .width = btnsize, .height = btnsize}));
    lf_unset_image_color(&s.ui);
    lf_pop_style_props(&s.ui);
  }

  lf_set_line_should_overflow(&s.ui, true);
  lf_end(&s.ui);

  glXSwapBuffers(s.dsp, cl->titlebar);
}

void
updatetitlebar(client_t* cl) {
  if(!usedecoration) return;

  bool success;
  // Update client geometry 
  cl->area = winarea(cl->frame, &success);
  if(!success) return;

  // Update decoration geometry
  uint32_t vals[2];
  vals[0] = cl->area.size.x;
  vals[1] = titlebarheight;
  xcb_configure_window(s.con, cl->titlebar, 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
}
void
hidetitlebar(client_t* cl) {
  xcb_unmap_window(s.con, cl->titlebar);
  {
    uint32_t vals[2] = {0, 0};
    xcb_configure_window(s.con, cl->win, 
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
  }
  {
    uint32_t vals[2] = {cl->area.size.x, cl->area.size.y};
    xcb_configure_window(s.con, cl->win, 
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
  }
  cl->showtitlebar = false;
}

void
showtitlebar(client_t* cl) {
  xcb_map_window(s.con, cl->titlebar);
  {
    uint32_t vals[2] = {0, titlebarheight};
    xcb_configure_window(s.con, cl->win, 
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
  }
  {
    uint32_t vals[2] = {cl->area.size.x, cl->area.size.y - titlebarheight};
    xcb_configure_window(s.con, cl->win, 
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
  }
  cl->showtitlebar = true;
}

/**
 * @brief Adds a client window to the linked list of clients.
 *
 * @param win The window to create a client from and add it 
 * to the clients
 *
 * @return The newly created client
 */
client_t*
addclient(xcb_window_t win) {
  // Allocate client structure
  client_t* cl = (client_t*)malloc(sizeof(*cl));
  cl->win = win;

  /* Get the window area */
  bool success;
  area_t area = winarea(win, &success);
  if(!success) return cl;

  cl->area = area;
  cl->borderwidth = winborderwidth;
  cl->fullscreen = false;
  cl->floating = false;
  cl->titlebar_render_additional = false;
  if(usedecoration)
    cl->name = getclientname(cl);
  cl->showtitlebar = usedecoration;

  // Create frame window for the client
  frameclient(cl);

  // Updating the linked list of clients
  cl->next = s.clients;
  s.clients = cl;
  return cl;
}

/**
 * @brief Removes a given client from the list of clients
 * by window.
 *
 * @param win The window of the client to remove
 */
void 
releaseclient(xcb_window_t win) {
  client_t** prev = &s.clients;
  client_t* cl = s.clients;
  /* Iterate throguh the clients to find the client thats 
   * associated with the window */
  while(cl) {
    if(cl->win == win) {
      /* Setting the pointer to previous client to the next client 
       * after the client we want to release, effectivly removing it
       * from our list of clients*/ 
      *prev = cl->next;
      // Freeing memory allocated for client
      free(cl);
      return;
    }
    // Advancing the client
    prev = &cl->next;
    cl = cl->next;
  }
}

/**
 * @brief Returns the associated client from a given window.
 * Returns NULL if there is no client associated with the window.
 *
 * @param win The window to get the client from
 *
 * @return The client associated with the given window (NULL if no associated client)
 */
client_t*
clientfromwin(xcb_window_t win) {
  client_t* cl;
  for (cl = s.clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->win == win) {
      return cl;
    }
  }
  return NULL;
}

/**
 * @brief Returns the associated client from a given titlebar window.
 * Returns NULL if there is no client associated with the titlebar window.
 *
 * @param win The titlebar window to get the client from
 *
 * @return The client associated with the given titlbar window (NULL if no associated client)
 */
client_t*
clientfromtitlebar(xcb_window_t titlebar) {
  if(!usedecoration) return NULL;
  client_t* cl;
  for (cl = s.clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->titlebar == titlebar) {
      return cl;
    }
  }
  return NULL;
}

/**
 * @brief Adds a monitor area to the linked list of monitors.
 *
 * @param a The area of the monitor to create 
 *
 * @return The newly created monitor 
 */
monitor_t* addmon(area_t a, uint32_t idx) {
  monitor_t* mon  = (monitor_t*)malloc(sizeof(*mon));
  mon->area     = a;
  mon->next     = s.monitors;
  mon->idx      = idx;
  mon->desktopcount = 0;

  mon->layouts = malloc(sizeof(*mon->layouts) * MAX_DESKTOPS);
  for(uint32_t i = 0; i < MAX_DESKTOPS; i++) {
    mon->layouts[i].nmaster = 1;
    mon->layouts[i].gapsize = winlayoutgap;
    mon->layouts[i].masterarea = MIN(MAX(layoutmasterarea, 0.0), 1.0);
  }

  s.monitors    = mon;

  return mon;
}

uint32_t
updatemons() {
  // Get xrandr screen resources
  xcb_randr_get_screen_resources_current_cookie_t res_cookie = xcb_randr_get_screen_resources_current(s.con, s.screen->root);
  xcb_randr_get_screen_resources_current_reply_t *res_reply = xcb_randr_get_screen_resources_current_reply(s.con, res_cookie, NULL);

  if (!res_reply) {
    fprintf(stderr, "ragnar: not get screen resources.\n");
    terminate();
  }

  // Get number of connected monitors
  int32_t num_outputs = xcb_randr_get_screen_resources_current_outputs_length(res_reply);
  // Get list of monitors
  xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(res_reply);

  uint32_t registered_count = 0; 
  for (int32_t i = 0; i < num_outputs; i++) {
    // Get the monitor info
    xcb_randr_get_output_info_cookie_t info_cookie = xcb_randr_get_output_info(s.con, outputs[i], XCB_TIME_CURRENT_TIME);
    xcb_randr_get_output_info_reply_t *info_reply = xcb_randr_get_output_info_reply(s.con, info_cookie, NULL);

    // Check if montior has CRTC
    if (info_reply && info_reply->crtc != XCB_NONE) {
      // Get CRTC info
      xcb_randr_get_crtc_info_cookie_t crtc_cookie = xcb_randr_get_crtc_info(s.con, info_reply->crtc, XCB_TIME_CURRENT_TIME);
      xcb_randr_get_crtc_info_reply_t *crtc_reply = xcb_randr_get_crtc_info_reply(s.con, crtc_cookie, NULL);

      // If that worked, register the monitor if it isn't registered yet 
      if (crtc_reply) {
	area_t monarea = (area_t){
	  .pos = (v2_t){
	    crtc_reply->x, crtc_reply->y
	  },
	    .size = (v2_t){
	      crtc_reply->width, crtc_reply->height
	    }
	};
	if(!monbyarea(monarea)) {
	  addmon(monarea, registered_count++);
	}
	free(crtc_reply);
      }
    }
    if (info_reply) {
      free(info_reply);
    }
  }
  free(res_reply);

  return registered_count;
}

/**
 * @brief Returns the monitor within the list of monitors 
 * that matches the given area. 
 * Returns NULL if there is no monitor associated with the area.
 *
 * @param a The area to find within the monitors 
 *
 * @return The monitor associated with the given area (NULL if no associated monitor)
 */
monitor_t*
monbyarea(area_t a) {
  monitor_t* mon;
  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    if((mon->area.pos.x == a.pos.x && mon->area.pos.y == a.pos.y) &&
	(mon->area.size.x == a.size.x && mon->area.size.y == a.size.y)) {
      return mon;
    }
  }
  return NULL;
}

/**
 * @brief Returns the monitor in which a given client is contained in. 
 * any registered monitor.
 *
 * @param cl The client to get the monitor of 
 *
 * @return The monitor that the given client is contained in.
 * Returns the first monitor if the given client is not contained within
 */
monitor_t*
clientmon(client_t* cl) {
  monitor_t* ret = s.monitors;
  if (!cl) {
    return ret;
  }

  monitor_t* mon;
  float biggest_overlap_area = -1.0f;

  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    float overlap = getoverlaparea(cl->area, mon->area);

    // Assign the monitor on which the client has the biggest overlap area
    if (overlap > biggest_overlap_area) {
      biggest_overlap_area = overlap;
      ret = mon;
    }
  }
  return ret;
}

desktop_t* 
mondesktop(monitor_t* mon) {
  return &s.curdesktop[mon->idx];
}

/**
 * @brief Returns the monitor under the cursor 
 *
 * @return The monitor that is under the cursor.
 * Returns the first monitor if there is no monitor under the cursor. 
 */
monitor_t*
cursormon() {
  bool success;
  v2_t cursor = cursorpos(&success);
  if(!success) {
    return s.monitors;
  }
  monitor_t* mon;
  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    if(pointinarea(cursor, mon->area)) {
      return mon;
    }
  }
  return s.monitors;
}

/**
 * @brief Returns the keysym of a given key code. 
 * Returns 0 if there is no keysym for the given keycode.
 *
 * @param keycode The keycode to get the keysym from 
 *
 * @return The keysym of the given keycode (0 if no keysym associated) 
 */
xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
  // Allocate key symbols 
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
  // Get the keysym
  xcb_keysym_t keysym = (!(keysyms) ? 0 : xcb_key_symbols_get_keysym(keysyms, keycode, 0));
  // Free allocated resources
  xcb_key_symbols_free(keysyms);
  return keysym;
}

/**
 * @brief Returns the keycode of a given keysym. 
 * Returns NULL if there is no keycode for the given keysym.
 *
 * @param keysym The keysym to get the keycode from 
 *
 * @return The keycode of the given keysym (NULL if no keysym associated) 
 */
xcb_keycode_t*
getkeycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
  xcb_keycode_t *keycode = (!(keysyms) ? NULL : xcb_key_symbols_get_keycode(keysyms, keysym));
  xcb_key_symbols_free(keysyms);
  return keycode;

}

/**
 * @brief Reads the _NET_WM_STRUT_PARTIAL atom of a given window and 
 * uses the gathered strut information to populate a strut_t.
 *
 * @param win The window to retrieve the strut of 
 *
 * @return The strut of the window 
 */
strut_t
readstrut(xcb_window_t win) {
  strut_t strut = {0};
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(s.con, 0, strlen("_NET_WM_STRUT_PARTIAL"), "_NET_WM_STRUT_PARTIAL");
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(s.con, cookie, NULL);

  if (!reply) {
    fprintf(stderr, "ragnar: failed to get _NET_WM_STRUT_PARTIAL atom.\n");
    return strut;
  }

  xcb_get_property_cookie_t propcookie = xcb_get_property(s.con, 0, win, reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 16);
  xcb_get_property_reply_t* propreply = xcb_get_property_reply(s.con, propcookie, NULL);

  if (reply && xcb_get_property_value_length(propreply) >= 16) {
    uint32_t* data = (uint32_t*)xcb_get_property_value(propreply);
    strut.left    = data[0];
    strut.right   = data[1];
    strut.top     = data[2];
    strut.bottom  = data[3];
    strut.startx  = data[8];
    strut.endx	  = data[9];
    strut.starty  = data[4];
    strut.endy 	  = data[5];
    logtofile("Read strut data of window %i: L: %i, R: %i, T: %i B: %i SX: %i EX: %i SY: %i EY: %i", win,
	strut.left, strut.right, strut.top, strut.bottom, strut.startx, strut.endx, 
	strut.starty, strut.endy); 
  } 

  free(reply);
  free(propreply);

  return strut;
}

/**
 * @brief Gathers the struts of all subwindows of a given window
 *
 * @param win The window to use as a root 
 */
void 
getwinstruts(xcb_window_t win) {
  xcb_query_tree_cookie_t cookie = xcb_query_tree(s.con, win);
  xcb_query_tree_reply_t* reply = xcb_query_tree_reply(s.con, cookie, NULL);

  if (!reply) {
    fprintf(stderr, "ragnar: failed to get the query tree for window 0x%08x.\n", win);
    return;
  }

  xcb_window_t* childs = xcb_query_tree_children(reply);
  uint32_t nchilds = xcb_query_tree_children_length(reply);

  for (uint32_t i = 0; i < nchilds; i++) {
    xcb_window_t child = childs[i];
    strut_t strut = readstrut(child);
    if(!(strut.left == 0 && strut.right == 0 && 
	  strut.top == 0 && strut.bottom == 0) &&
	s.nwinstruts + 1 <= MAX_STRUTS) {
      s.winstruts[s.nwinstruts++] = strut;
    }
    getwinstruts(child);
  }

  free(reply);
}

/**
 * @brief Runs a given command by forking the process and using execl.
 *
 * @param cmd The command to run 
 */
void
runcmd(passthrough_data_t data) {
  if (data.cmd == NULL) {
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    execl("/bin/sh", "sh", "-c", data.cmd, (char *)NULL);
    // If execl fails
    fprintf(stderr, "ragnar: failed to execute command.\n");
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    // Parent process
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    // Fork failed
    perror("fork");
    fprintf(stderr, "ragnar: failed to execute command.\n");
    return;
  }
}

void 
addfocustolayout() {
  s.focus->floating = false;
  makelayout(s.monfocus);
}

void 
settiledmaster() {
  for(client_t* cl = s.clients; cl != NULL; cl = cl->next) {
    cl->floating = false;
  }
  s.curlayout = LayoutTiledMaster;
  makelayout(s.monfocus);
}

void 
setfloatingmode() {
  s.curlayout = LayoutFloating;
  makelayout(s.monfocus);
}

void
updatebarslayout() {
  // Gather strut information 
  s.nwinstruts = 0;
  getwinstruts(s.root);
  makelayout(s.monfocus);
}

/**
 * @brief Retrieves an intern X atom by name.
 *
 * @param atomstr The string (name) of the atom to retrieve
 *
 * @return The XCB atom associated with the given atom name.
 * Returns XCB_ATOM_NONE if the given name is not associated with 
 * any atom.
 * */
xcb_atom_t
getatom(const char* atomstr) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(s.con, 0, strlen(atomstr), atomstr);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(s.con, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

/**
 * 
 * @brief Updates the client list EWMH atom tothe current list of clients.
 * */
void
ewmh_updateclients() {
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

  client_t* cl;
  for (cl = s.clients; cl != NULL; cl = cl->next) {
    xcb_change_property(s.con, XCB_PROP_MODE_APPEND, s.root, s.ewmh_atoms[EWMHclientList],
	XCB_ATOM_WINDOW, 32, 1, &cl->win);
  }
}

/**
 * @brief Signal handler for SIGCHLD to avoid zombie processes
 * */
void
sigchld_handler(int32_t signum) {
  (void)signum;
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * @brief Checks if a given string is within a given 
 * array of strings
 * @param array Haystack
 * @param count Number of items within the haystack
 * @param target Needle
 *
 * @return Wether or not the given string was found in the 
 * given array
 * */
bool 
strinarr(char* array[], int count, const char* target) {
  for (int i = 0; i < count; ++i) {
    if (array[i] != NULL && strcmp(array[i], target) == 0) {
      return true; // String found
    }
  }
  return false; // String not found
}

/*
 * @brief Converts a given string to ASCII by removing all non-ASCII
 * characters within it and replacing them with a replacement character.
 * @param str The string to un-ASCII-fy 
 * */
void 
strtoascii(char* str) {
  while(*str) {
    if((unsigned char)*str >= 127) {
      *str = non_ascii_replacement;
    }
    str++;
  }
}

/*
 * @brief Compares two strings 
 * @param a The first string to compare
 * @param b The second string to compare
 * @return See 'man strcmp' */
int32_t 
compstrs(const void* a, const void* b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * @brief Creates an GLX Context and sets up OpenGL visual. 
 * */
void 
initglcontext() {
  /* Create an OpenGL context */
  GLint attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, 
     None };

  int screen_num = DefaultScreen(s.dsp);
  s.glvis = glXChooseVisual(s.dsp, screen_num, attribs);
  if (!s.glvis) {
    fprintf(stderr, "ragnar: no appropriate OpenGL visual found\n");
    return;
  }

  s.glcontext = glXCreateContext(s.dsp, s.glvis, NULL, GL_TRUE); 

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * @brief Set the OpenGL context and drawable to a given window
 * @param win The window to draw on 
 * */
void
setglcontext(xcb_window_t win) {
  // Get the current drawable and context
  GLXDrawable curwin = glXGetCurrentDrawable();
  GLXContext curcontext = glXGetCurrentContext();

  // Check if the current drawable and context match the desired ones
  if (curwin != win || curcontext != s.glcontext) {
    // Make the given window and context the current drawable
    if (!glXMakeCurrent(s.dsp, win, s.glcontext)) {
      fprintf(stderr, "ragnar: failed to make OpenGL window context.\n");
    }
  }
}

/**
 * @brief Logs a given formatted message to the log file specified in the
 * config. 
 * @param fmt The format string
 * @param ... The variadic arguments */
void 
logtofile(const char* fmt, ...) {
  if(!logdebug) return;

  // Open the file in append mode
  FILE *file = fopen(logfile, "a");

  // Check if the file was opened successfully
  if (file == NULL) {
    perror("ragnar: error opening log file.");
    return;
  }

  // Write the date to the file 
  char* date = cmdoutput("date +\"%d.%m.%y %H:%M:%S\"");
  fprintf(file, "%s | ", date);

  // Initialize the variable argument list
  va_list args;
  va_start(args, fmt);

  // Write the formatted string to the file
  vfprintf(file, fmt, args);

  // End the variable argument list
  va_end(args);

  fprintf(file, "\n");

  // Close the file
  fclose(file);
}

char* 
cmdoutput(const char* cmd) {
  FILE *fp;
  char buffer[512];
  char *result = NULL;
  size_t result_len = 0;

  // Open a pipe to the command
  fp = popen(cmd, "r");
  if (fp == NULL) {
    perror("popen");
    return NULL;
  }

  // Read the command's output
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t buffer_len = strlen(buffer);
    char *new_result = realloc(result, result_len + buffer_len + 1);
    if (new_result == NULL) {
      perror("realloc");
      free(result);
      pclose(fp);
      return NULL;
    }
    result = new_result;
    memcpy(result + result_len, buffer, buffer_len);
    result_len += buffer_len;
    result[result_len] = '\0'; 
  }

  // Close the pipe
  if (pclose(fp) == -1) {
    perror("pclose");
    free(result);
    return NULL;
  }

  return result;
}


int
main() {
  // Run the startup script
  runcmd((passthrough_data_t){.cmd = "ragnarstart"});
  // Setup the window manager
  setup();
  // Enter the event loop
  loop();
  // Terminate after the loop
  terminate();
  return EXIT_SUCCESS;
}
