#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
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

static bool             pointinarea(v2 p, area a);
static v2               cursorpos(bool* success);
static area             winarea(xcb_window_t win, bool* success);
static float            getoverlaparea(area a, area b);

static void             setbordercolor(client* cl, uint32_t color);
static void             setborderwidth(client* cl, uint32_t width);
static void             moveclient(client* cl, v2 pos);
static void             resizeclient(client* cl, v2 size);
static void             moveresizeclient(client* cl, area a);
static void             raiseclient(client* cl);
static bool             clienthasdeleteatom(client *c);
static char*            getclientname(client* cl);
static void             killclient(client* cl);
static void             focusclient(client* cl);
static void             setxfocus(client* cl);
static void             frameclient(client* cl);
static void             unframeclient(client* cl);
static void             unfocusclient(client* cl);
static void             configclient(client* cl);
static void             hideclient(client* cl);
static void             showclient(client* cl);
static bool             raiseevent(client* cl, xcb_atom_t protocol);
static void             setwintype(client* cl);
static void             seturgent(client* cl, bool urgent);
static xcb_atom_t       getclientprop(client* cl, xcb_atom_t prop);
static void             setfullscreen(client* cl, bool fullscreen);
static void             switchclientdesktop(client* cl, int32_t desktop);

static void             uploaddesktopnames();
static void             createdesktop(uint32_t idx, monitor* mon);

static void             setupatoms();
static void             grabkeybinds();
static void             loaddefaultcursor();

static void             setuptitlebar(client* cl);
static void             rendertitlebar(client* cl);
static void             updatetitlebar(client* cl);
static void             hidetitlebar(client* cl);
static void             showtitlebar(client* cl);

static void             evmaprequest(xcb_generic_event_t* ev);
static void             evunmapnotify(xcb_generic_event_t* ev);
static void             evdestroynotify(xcb_generic_event_t* ev);
static void             eventernotify(xcb_generic_event_t* ev);
static void             evkeypress(xcb_generic_event_t* ev);
static void             evbuttonpress(xcb_generic_event_t* ev);
static void             evbuttonrelease(xcb_generic_event_t* ev);
static void             evmotionnotify(xcb_generic_event_t* ev);
static void             evconfigrequest(xcb_generic_event_t* ev);
static void             evconfignotify(xcb_generic_event_t* ev);
static void             evpropertynotify(xcb_generic_event_t* ev);
static void             evclientmessage(xcb_generic_event_t* ev);
static void             evexpose(xcb_generic_event_t* ev);

static client*          addclient(xcb_window_t win);
static void             releaseclient(xcb_window_t win);
static client*          clientfromwin(xcb_window_t win);
static client*          clientfromtitlebar(xcb_window_t titlebar);

static monitor*         addmon(area a, uint32_t idx); 
static monitor*         monbyarea(area a);
static monitor*         clientmon(client* cl);
static monitor*         cursormon();
static uint32_t         updatemons();

static xcb_keysym_t     getkeysym(xcb_keycode_t keycode);
static xcb_keycode_t*   getkeycodes(xcb_keysym_t keysym);

static xcb_atom_t       getatom(const char* atomstr);

static void             ewmh_updateclients();

static void             sigchld_handler(int32_t signum);

static bool             strinarr(char* array[], int count, const char* target);
static int32_t          compstrs(const void* a, const void* b);

static void             initglcontext();
static void             setglcontext(xcb_window_t win);
static bool             isglextsupported(const char* extlist, const char* ext);

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

// Define the function pointer for glXSwapIntervalEXT
typedef void (*glXSwapIntervalEXTProc)(Display *, GLXDrawable, int);

static State s;

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

  s.lastexposetime = (struct timespec){0, 0};

  // Lock the display to prevent concurrency issues
  XSetEventQueueOwner(s.dsp, XCBOwnsEventQueue);
  if(usedecoration) {
    // Initialize OpenGL
    initglcontext();
  }
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

  loaddefaultcursor();

  // Grab the window manager's keybinds
  grabkeybinds();

  // Handle monitor setup 
  uint32_t registered_monitors = updatemons();

  // Setup atoms for EWMH and so on
  setupatoms();

  s.curdesktop = malloc(sizeof(*s.curdesktop) * registered_monitors);
  if(s.curdesktop) {
    for(uint32_t i = 0; i < registered_monitors; i++) {
      s.curdesktop[i] = desktopinit;
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
    while ((ev = xcb_poll_for_event(s.con))) {
      uint8_t evcode = ev->response_type & ~0x80;
      /* If the event we receive is listened for by our 
       * event listeners, call the callback for the event. */
      if (evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
        evhandlers[evcode](ev);
      }
      free(ev);
    }
    
    // Sleep for a short period to prevent high CPU usage
    usleep(event_polling_rate_ms * 1000); 
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
    client* cl = s.clients;
    client* next;
    while(cl != NULL) {
      next = cl->next;
      releaseclient(cl->win);
      cl = next; 
    }
  }
  {
    monitor* mon = s.monitors;
    monitor* next;
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
 * @return If the point p is in the area, false if it is not in the area 
 */
bool
pointinarea(v2 p, area area) {
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
v2 
cursorpos(bool* success) {
  // Query the pointer position
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s.con, xcb_query_pointer(s.con, s.root), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve cursor position."); 
    free(reply);
    return (v2){0};
  }
  // Create a v2 for to store the position 
  v2 cursor = (v2){.x = reply->root_x, .y = reply->root_y};

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
area 
winarea(xcb_window_t win, bool* success) {
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve cursor position."); 
    free(reply);
    return (area){0};
  }
  // Creating the area structure to store the geometry in
  area a = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height}};

  // Error checking
  free(reply);
  return a;
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
getoverlaparea(area a, area b) {
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
setbordercolor(client* cl, uint32_t color) {
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
setborderwidth(client* cl, uint32_t width) {
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
moveclient(client* cl, v2 pos) {
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
resizeclient(client* cl, v2 size) {
  if(!cl) {
    return;
  }
  uint32_t sizeval[2] = { (uint32_t)size.x, (uint32_t)size.y };
  uint32_t sizeval_content[2] = { (uint32_t)size.x, (uint32_t)size.y - ((cl->showtitlebar) ? titlebarheight : 0.0f)};

  // Resize the window by configuring it's width and height property
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
moveresizeclient(client* cl, area a) {
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
raiseclient(client* cl) {
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
clienthasdeleteatom(client *c) {
	bool ret = false;
	xcb_icccm_get_wm_protocols_reply_t reply;

	if(xcb_icccm_get_wm_protocols_reply(s.con, xcb_icccm_get_wm_protocols_unchecked(s.con, c->win, s.wm_atoms[WMprotocols]), &reply, NULL)) {
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
 * @brief Retrieves the name of a given client (allocates memory) 
 *
 * @param cl The client to retrieve a name from  
 *
 * @return The name of the given client  
 */
char* 
getclientname(client* cl) {
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
killclient(client* cl) {
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
focusclient(client* cl) {
  if(!cl || cl->win == s.root) {
    return;
  }

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
setxfocus(client* cl) {
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
frameclient(client* cl) {
  {
    cl->frame = xcb_generate_id(s.con);
    uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
    uint32_t values[2] = {1, 
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
      XCB_EVENT_MASK_PROPERTY_CHANGE |
      XCB_EVENT_MASK_ENTER_WINDOW |
      XCB_EVENT_MASK_FOCUS_CHANGE |
      XCB_EVENT_MASK_BUTTON_PRESS};

    xcb_create_window(s.con, XCB_COPY_FROM_PARENT, cl->frame, s.root,
                      cl->area.pos.x, cl->area.pos.y, cl->area.size.x, cl->area.size.y,
                      1, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                      mask, values);

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

  // Map the window on the screen
  xcb_map_window(s.con, cl->frame);
}


/**
 * @brief Destroys and unmaps the frame window of a given client 
 * which consequently removes the client's window from the display
 *
 * @param cl The client to unframe 
 */
void
unframeclient(client* cl) {
  xcb_unmap_window(s.con, cl->titlebar);
  xcb_unmap_window(s.con, cl->frame);
  xcb_reparent_window(s.con, cl->win, s.root, 0, 0);
  xcb_destroy_window(s.con, cl->titlebar);
  xcb_destroy_window(s.con, cl->frame);
}
void
unfocusclient(client* cl) {
  if (!cl) {
    return;
  }
  setbordercolor(cl, winbordercolor);
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHactiveWindow]);
}

/**
 * @brief Configures a given client by sending a X configure event 
 *
 * @param cl The client to configure 
 */
void
configclient(client* cl) {
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
hideclient(client* cl) {
  cl->ignoreunmap = true;
  xcb_unmap_window(s.con, cl->frame);
}

/*
 * @brief Shows a given client by unframing it's window
 * @param cl The client to show 
 */
void
showclient(client* cl) {
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
  v2 cursor = cursorpos(&cursor_success);
  if(!cursor_success) return;

  // Adding the mapped client to our linked list
  client* cl = addclient(map_ev->window);

  // Set initial border 
  setbordercolor(cl, winbordercolor);
  setborderwidth(cl, winborderwidth);


  // Set window type of client (e.g dialog)
  setwintype(cl);

  // Set OpenGL context to the titlebar of the Window
  setglcontext(cl->titlebar);

  // Set client's monitor
  cl->mon = clientmon(cl);
  cl->desktop = s.curdesktop[s.monfocus->idx];

  // Set all clients floating for now (TODO)
  cl->floating = true;

  // Update the EWMH client list
  ewmh_updateclients();

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(cl);
  }
  bool success;
  cl->area = winarea(cl->frame, &success);
  if(!success) return;
  if(s.monfocus) {
    // Spawn the window in the center of the focused monitor
    moveclient(cl, (v2){
      s.monfocus->area.pos.x + (s.monfocus->area.size.x - cl->area.size.x) / 2.0f, 
      s.monfocus->area.pos.y + (s.monfocus->area.size.y - cl->area.size.y) / 2.0f});
  }

  // Map the window
  xcb_map_window(s.con, map_ev->window);
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

  client* cl = clientfromwin(unmap_ev->window);
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

  xcb_flush(s.con);
}

void 
evdestroynotify(xcb_generic_event_t* ev) {
  xcb_destroy_notify_event_t* destroy_ev = (xcb_destroy_notify_event_t*)ev;
  client* cl = clientfromwin(destroy_ev->window);
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

  if((enter_ev->mode != XCB_NOTIFY_MODE_NORMAL || enter_ev->detail == XCB_NOTIFY_DETAIL_INFERIOR)
    && enter_ev->event != s.root) {
    return;
  }

  client* cl = clientfromwin(enter_ev->event);

  if(!cl) {
    if(!(cl = clientfromtitlebar(enter_ev->event)) && enter_ev->event != s.root) return;
  }

  // Focus entered client
  if(cl) { 
    focusclient(cl);
  }
  else if(enter_ev->event == s.root) {
    // Set Input focus to root
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client* cl;
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
        // Handle error, unable to get window attributes
        return;
    }

    client* cl = clientfromwin(button_ev->event);
    if (!cl) {
        cl = clientfromtitlebar(button_ev->event);
        if (!cl) return;
        v2 cursorpos = (v2){.x = (float)button_ev->root_x - cl->area.pos.x, .y = (float)button_ev->root_y - cl->area.pos.y};
        area closebtnarea = (area){
            .pos = cl->closebutton,
            .size = (v2){30, titlebarheight}
        };
        if (pointinarea(cursorpos, closebtnarea)) {
            killclient(cl);
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
    area area = winarea(cl->frame, &success);
    if(!success) return;

    cl->area = area;
    // Setting grab position
    s.grabwin = cl->area;
    s.grabcursor = (v2){.x = (float)button_ev->root_x, .y = (float)button_ev->root_y};

    // Raising the client to the top of the stack
    raiseclient(cl);
    xcb_flush(s.con);
}

void
evbuttonrelease(xcb_generic_event_t* ev) {
  xcb_button_release_event_t* button_ev = (xcb_button_release_event_t*)ev;
  client* cl = clientfromwin(button_ev->event);

  if(cl && usedecoration) {
    for(client* cl = s.clients; cl != NULL; cl = cl->next) {
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
  // high polling-rate mouses. We are only doing this if OpenGL decoration is enabled as 
  // rerendering the decoration too many times is the main bottleneck.
  if(usedecoration) {
    // Get the current time
    struct timespec curtime;
    clock_gettime(CLOCK_MONOTONIC, &curtime);

    // Calculate the time difference between the current and last expose events
    size_t diff = (curtime.tv_sec - s.lastexposetime.tv_sec) * 1000 +
      (curtime.tv_nsec - s.lastexposetime.tv_nsec) / 1000000;

    const uint32_t debounce = 1000 / motion_notify_debounce_fps;  
    if (diff < debounce) {
      // Skip handling this expose event
      return;
    }

    // Update the last time
    s.lastexposetime = curtime;
  }

  if(motion_ev->event == s.root) {
    // Update the focused monitor to the monitor under the cursor
    monitor* mon = cursormon();
    uploaddesktopnames();
    s.monfocus = mon;
    return;
  }
  if(!(motion_ev->state & XCB_BUTTON_MASK_1 || motion_ev->state & XCB_BUTTON_MASK_3)) return;

  // Position of the cursor in the drag event
  v2 dragpos    = (v2){.x = (float)motion_ev->root_x, .y = (float)motion_ev->root_y};
  // Drag difference from the current drag event to the initial grab 
  v2 dragdelta  = (v2){.x = dragpos.x - s.grabcursor.x, .y = dragpos.y - s.grabcursor.y};
  // New position of the window
  v2 movedest   = (v2){.x = (float)(s.grabwin.pos.x + dragdelta.x), .y = (float)(s.grabwin.pos.y + dragdelta.y)};


  client* cl = clientfromwin(motion_ev->event);
  if(!cl) {
    if(!(cl = clientfromtitlebar(motion_ev->event))) return;
    if(cl->fullscreen) {
      setfullscreen(cl, false);
      s.grabwin = cl->area;
      movedest = (v2){.x = (float)(s.grabwin.pos.x + dragdelta.x), .y = (float)(s.grabwin.pos.y + dragdelta.y)};
    }
    moveclient(cl, movedest);
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
  } 
  // Resize the window
  else if(motion_ev->state & resizebtn) {
    // Resize delta (clamped)
    v2 resizedelta  = (v2){.x = MAX(dragdelta.x, -s.grabwin.size.x), .y = MAX(dragdelta.y, -s.grabwin.size.y)};
    // New window size
    v2 sizedest = (v2){.x = s.grabwin.size.x + resizedelta.x, .y = s.grabwin.size.y + resizedelta.y};

    resizeclient(cl, sizedest);
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

  client* cl = clientfromwin(config_ev->window);
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
    if(s.monfocus) {
      // Spawn the window in the center of the focused monitor
      moveclient(cl, (v2){
        s.monfocus->area.pos.x + (s.monfocus->area.size.x - cl->area.size.x) / 2.0f, 
        s.monfocus->area.pos.y + (s.monfocus->area.size.y - cl->area.size.y) / 2.0f});
    }
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
  client* cl = clientfromwin(config_ev->window);
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
  client* cl = clientfromwin(prop_ev->window);

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
  client* cl = clientfromwin(msg_ev->window);

  if(!cl) {
    return;
  }

  if(msg_ev->type == s.ewmh_atoms[EWMHstate]) {
    // If client requested fullscreen toggle
    if(msg_ev->data.data32[1] == s.ewmh_atoms[EWMHfullscreen] ||
       msg_ev->data.data32[2] == s.ewmh_atoms[EWMHfullscreen]) {
      // Set/unset client fullscreen 
      setfullscreen(cl, (msg_ev->data.data32[0] == 1 || (msg_ev->data.data32[0] == 2 && !cl->fullscreen)));
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
  client* cl = clientfromtitlebar(expose_ev->window);
  if(!cl) return;
  rendertitlebar(cl);
}

/**
 * @brief Cycles the currently focused client 
 */
void
cyclefocus() {
  if (!s.clients || !s.focus)
    return;
  client* next = NULL;
  // Find the next client on the current monitor & desktop 
  for(client* cl = s.focus->next; cl != NULL; cl = cl->next) {
    if(cl->mon == s.monfocus && cl->desktop == s.curdesktop[s.monfocus->idx]) {
      next = cl;
      break;
    }
  }

  // If there is a next client, just focus it
  if (next != NULL) {
    focusclient(next);
    raiseclient(next);
  }
  // If there is no next client, cycle back to the first client on the 
  // current monitor & desktop
  else {
    for(client* cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->mon == s.monfocus && cl->desktop == s.curdesktop[s.monfocus->idx]) {
        next = cl;
        break;
      }
    }
    // If there is one, focus it
    if (next != NULL) {
      focusclient(next);
      raiseclient(next);
    }
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
raiseevent(client* cl, xcb_atom_t protocol) {
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
setwintype(client* cl) {
  xcb_atom_t state = getclientprop(cl, s.ewmh_atoms[EWMHstate]);
  xcb_atom_t wintype = getclientprop(cl, s.ewmh_atoms[EWMHwindowType]);

  if(state == s.ewmh_atoms[EWMHfullscreen]) {
    setfullscreen(cl, true);
  } 
  if(wintype == s.ewmh_atoms[EWMHwindowTypeDialog]) {
    cl->floating = true;
  }
}

void
seturgent(client* cl, bool urgent) {
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
getclientprop(client* cl, xcb_atom_t prop) {
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
setfullscreen(client* cl, bool fullscreen) {
  if(!s.monfocus || !cl) return;

  cl->fullscreen = fullscreen;
  if(cl->fullscreen) {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 1, &s.ewmh_atoms[EWMHfullscreen]);
    // Store previous position of client
    cl->area_prev = cl->area;
    // Set the client's area to the focused monitors area, effictivly
    // making the client as large as the monitor screen
    cl->area = s.monfocus->area;

    // Unset border of client if it's fullscreen
    cl->borderwidth = 0;
  } else {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    // Set the client's area to the area before the last fullscreen occured 
    cl->area = cl->area_prev;
    cl->borderwidth = winborderwidth;
  }
  // Update client's border width
  setborderwidth(cl, cl->borderwidth);

  // Update the clients geometry 
  moveresizeclient(cl, cl->area);

  configclient(cl);

  raiseclient(cl);

  if(cl->area.size.x >= cl->mon->area.size.x && cl->area.size.y >= cl->mon->area.size.y && !cl->fullscreen) {
    setfullscreen(cl, true);
  }
}

/**
 * @brief Switches the desktop of a given client and hides that client.
 * @param cl The client to switch the desktop of
 * @param desktop The desktop to set the client of 
 */
void
switchclientdesktop(client* cl, int32_t desktop) {
  cl->desktop = desktop;
  if(cl == s.focus) {
    unfocusclient(cl);
  }
  hideclient(cl);
}

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
createdesktop(uint32_t idx, monitor* mon) {
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
  int32_t newdesktop = s.curdesktop[s.monfocus->idx];
  if(newdesktop + 1 < MAX_DESKTOPS) {
    newdesktop++;
  } else {
    newdesktop = 0;
  }
  switchdesktop((passthrough_data){.i = newdesktop});
}

/**
 * @brief Cycles the currently selected desktop index one desktop down 
 */
void
cycledesktopdown() {
  int32_t newdesktop = s.curdesktop[s.monfocus->idx];
  if(newdesktop - 1 >= 0) {
    newdesktop--;
  } else {
    newdesktop = MAX_DESKTOPS - 1;
  }
  switchdesktop((passthrough_data){.i = newdesktop});
}


/**
 * @brief Switches the currently selected desktop index to the given 
 * index and notifies EWMH that there was a desktop change
 *
 * @param data The .i member is used as the desktop to switch to
 * */
void
switchdesktop(passthrough_data data) {
  if(!s.monfocus) return;
  if(data.i == s.curdesktop[s.monfocus->idx]) return;

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



  for (client* cl = s.clients; cl != NULL; cl = cl->next) {
    if(cl->mon != s.monfocus) continue;
    // Hide the clients on the current desktop
    if(cl->desktop == s.curdesktop[s.monfocus->idx]) {
      hideclient(cl);
    // Show the clients on the desktop we want to switch to
    } else if(cl->desktop == data.i) {
      showclient(cl);
    }
  }

  // Unfocus all selected clients
  for(client* cl = s.clients; cl != NULL; cl = cl->next) {
    unfocusclient(cl);
  }

  s.curdesktop[s.monfocus->idx] = data.i;
}

/**
 * @brief Switches the desktop of the currently selected client to a given index 
 *
 * @param data The .i member is used as the desktop to switch to
 * */
void
switchfocusdesktop(passthrough_data data) {
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
  for(monitor* mon = s.monitors; mon != NULL; mon = mon->next) {
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
setuptitlebar(client* cl) {
 /* This function is written with Xlib not xcb, as the decoration window 
   * needs to be created with Xlib to get a working OpenGL context on it. */
  if(!usedecoration) return;

  Window root = DefaultRootWindow(s.dsp);
  XSetWindowAttributes attribs;
  attribs.colormap = XCreateColormap(s.dsp, s.root, s.glvis->visual, AllocNone);
  attribs.event_mask = EnterWindowMask | ExposureMask | StructureNotifyMask;
  cl->titlebar = (xcb_window_t)XCreateWindow(s.dsp, root, 0, 0, cl->area.size.x,
                                             titlebarheight, 0, s.glvis->depth, InputOutput, s.glvis->visual,
                                             CWColormap | CWEventMask, &attribs);
  XReparentWindow(s.dsp, cl->titlebar, cl->frame, 0, 0);
  XMapWindow(s.dsp, cl->titlebar);
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

  {
    setglcontext(cl->titlebar);

    const char* exts = glXQueryExtensionsString(s.dsp, DefaultScreen(s.dsp));
    if (!isglextsupported(exts, "GLX_EXT_swap_control")) {
      fprintf(stderr, "ragnar: GLX_EXT_swap_control is not supported.\n");
      terminate();
    }

    // Load the glXSwapIntervalEXT function
    glXSwapIntervalEXTProc glXSwapIntervalEXT = (glXSwapIntervalEXTProc)
      glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
    if (!glXSwapIntervalEXT) {
      fprintf(stderr, "ragnar: failed to get glXSwapIntervalEXT function address.\n");
      terminate();
    }
    // Disable VSync
    glXSwapIntervalEXT(s.dsp, cl->titlebar, 0);

    if(!s.ui.init) {
      s.ui = lf_init_x11(cl->area.pos.x, titlebarheight);
      lf_free_font(&s.ui.theme.font);
      s.ui.theme.font = lf_load_font(fontpath, 24);
      s.ui.theme.text_props.text_color = lf_color_from_hex(fontcolor);
      s.closeicon = lf_load_texture(closeiconpath, LF_TEX_FILTER_LINEAR, false);
    }
    rendertitlebar(cl);
  }
}

void
rendertitlebar(client* cl) {
  if(!usedecoration) return;
  if(cl->desktop != s.curdesktop[s.monfocus->idx]) return;
  setglcontext(cl->titlebar);

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
  {
    // Get client name
    bool namevalid = (cl->name && strlen(cl->name));
    char* displayname = (namevalid ? cl->name : "No name"); 
    // Center text
    float textwidth = lf_text_dimension(&s.ui, displayname).x;
    lf_set_ptr_x_absolute(&s.ui, (cl->area.size.x - textwidth) / 2.0f);

    // Display text and remove margin
    LfUIElementProps props = s.ui.theme.text_props;
    props.margin_left = 0.0f;
    lf_push_style_props(&s.ui, props);
    lf_text(&s.ui, displayname); 
    lf_pop_style_props(&s.ui);
  }
  {
    uint32_t width = 12;
    float margin = 15;
    LfUIElementProps props = s.ui.theme.button_props;
    lf_set_ptr_x_absolute(&s.ui, cl->area.size.x - width - margin);

    props.color = LF_NO_COLOR;
    props.padding = 0;
    props.margin_top = (titlebarheight - width) / 2.0f;
    props.border_width = 0;
    props.margin_left = 0;
    props.margin_right = 0;
    lf_push_style_props(&s.ui, props);
    cl->closebutton = (v2){lf_get_ptr_x(&s.ui), lf_get_ptr_y(&s.ui)};
    lf_set_image_color(&s.ui, lf_color_from_hex(iconcolor));
    lf_image_button(&s.ui, ((LfTexture){.id = s.closeicon.id, .width = width, .height = width}));
    lf_unset_image_color(&s.ui);
    lf_pop_style_props(&s.ui);
  }
  lf_end(&s.ui);

  glXSwapBuffers(s.dsp, cl->titlebar);
}

void
updatetitlebar(client* cl) {
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
hidetitlebar(client* cl) {
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
showtitlebar(client* cl) {
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
client*
addclient(xcb_window_t win) {
  // Allocate client structure
  client* cl = (client*)malloc(sizeof(*cl));
  cl->win = win;

  /* Get the window area */
  bool success;
  area area = winarea(win, &success);
  if(!success) return cl;

  cl->area = area;
  cl->borderwidth = winborderwidth;
  cl->fullscreen = false;
  cl->floating = false;
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
  client** prev = &s.clients;
  client* cl = s.clients;
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
client*
clientfromwin(xcb_window_t win) {
  client* cl;
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
client*
clientfromtitlebar(xcb_window_t titlebar) {
  if(!usedecoration) return NULL;
  client* cl;
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
monitor* addmon(area a, uint32_t idx) {
  monitor* mon  = (monitor*)malloc(sizeof(*mon));
  mon->area     = a;
  mon->next     = s.monitors;
  mon->idx      = idx;
  mon->desktopcount = 0;
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
        area monarea = (area){
          .pos = (v2){
            crtc_reply->x, crtc_reply->y
          },
          .size = (v2){
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
monitor*
monbyarea(area a) {
  monitor* mon;
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
monitor*
clientmon(client* cl) {
  monitor* ret = s.monitors;
  if (!cl) {
    return ret;
  }

  monitor* mon;
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

/**
 * @brief Returns the monitor under the cursor 
 *
 * @return The monitor that is under the cursor.
 * Returns the first monitor if there is no monitor under the cursor. 
 */
monitor*
cursormon() {
  bool success;
  v2 cursor = cursorpos(&success);
  if(!success) {
    return s.monitors;
  }
  monitor* mon;
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
 * @brief Runs a given command by forking the process and using execl.
 *
 * @param cmd The command to run 
 */
void
runcmd(passthrough_data data) {
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

  client* cl;
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

bool 
strinarr(char* array[], int count, const char* target) {
  for (int i = 0; i < count; ++i) {
    if (array[i] != NULL && strcmp(array[i], target) == 0) {
      return true; // String found
    }
  }
  return false; // String not found
}

int32_t 
compstrs(const void* a, const void* b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * @brief Creates an GLX Context and sets up OpenGL visual. 
 * */
void 
initglcontext() {
  static int32_t visattribs[] = {
    GLX_X_RENDERABLE, True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    GLX_DOUBLEBUFFER, True,
    None
  };

  int fbcount;
  GLXFBConfig* fbconfs = glXChooseFBConfig(s.dsp, DefaultScreen(s.dsp), visattribs, &fbcount);
  if(!fbconfs) {
    fprintf(stderr, "rangar: cannot retrieve an OpenGL framebuffer config.\n");
    terminate();
  }

  GLXFBConfig glfbconf = fbconfs[0];
  XFree(fbconfs);

  s.glvis = glXGetVisualFromFBConfig(s.dsp, glfbconf);
  if(!s.glvis) {
    fprintf(stderr, "ragnar: no appropriate OpenGL visual found.\n");
    terminate();
  }

  s.glcontext = glXCreateContext(s.dsp, s.glvis, NULL, GL_TRUE);
  if(!s.glvis) {
    fprintf(stderr, "ragnar: failed to create an OpenGL context.\n");
    terminate();
  }
}

void
setglcontext(xcb_window_t win) {
  if(!usedecoration) return;
  glXMakeCurrent(s.dsp, (GLXDrawable)win, s.glcontext);
}

bool 
isglextsupported(const char* extlist, const char* ext) {
  const char* start;
  const char* where, *terminator;

  where = strchr(ext, ' ');
  if (where || *ext == '\0')
    return 0;

  for (start = extlist;;) {
    where = strstr(start, ext);
    if (!where)
      break;

    terminator = where + strlen(ext);
    if (where == start || *(where - 1) == ' ') {
      if (*terminator == ' ' || *terminator == '\0') {
        return 1;
      }
    }
    start = terminator;
  }
  return 0;
}

int 
main() {
  // Run the startup script
  runcmd((passthrough_data){.cmd = "ragnarstart"});
  // Setup the window manager
  setup();
  // Enter the event loop
  loop();
  // Terminate after the loop
  terminate();
  return EXIT_SUCCESS;
}

