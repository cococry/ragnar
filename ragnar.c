#include <assert.h>
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
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_cursor.h>
#include <xcb/randr.h>

#include "structs.h"

static void             setup();
static void             loop();
static void             terminate();

static bool             pointinarea(v2 p, area a);
static bool             areainarea(area a, area b);
static v2               cursorpos(bool* success);
static area             winarea(xcb_window_t win, bool* success);

static void             setbordercolor(client* cl, uint32_t color);
static void             setborderwidth(client* cl, uint32_t width);
static void             moveclient(client* cl, v2 pos);
static void             resizeclient(client* cl, v2 size);
static void             moveresizeclient(client* cl, area a);
static void             raiseclient(client* cl);
static void             killclient(client* cl);
static void             killfocus();
static void             focusclient(client* cl);
static void             setxfocus(client* cl);
static void             unfocusclient(client* cl);
static void             configclient(client* cl);
static void             cyclefocus();
static bool             raiseevent(client* cl, xcb_atom_t protocol);
static void             setwintype(client* cl);
static void             seturgent(client* cl, bool urgent);
static xcb_atom_t       getclientprop(client* cl, xcb_atom_t prop);
static void             setfullscreen(client* cl, bool fullscreen);
static void             togglefullscreen();

static void             setupatoms();
static void             grabkeybinds();
static void             loaddefaultcursor();

static void             evmaprequest(xcb_generic_event_t* ev);
static void             evunmapnotify(xcb_generic_event_t* ev);
static void             eventernotify(xcb_generic_event_t* ev);
static void             evkeypress(xcb_generic_event_t* ev);
static void             evbuttonpress(xcb_generic_event_t* ev);
static void             evmotionnotify(xcb_generic_event_t* ev);
static void             evconfigrequest(xcb_generic_event_t* ev);
static void             evconfignotify(xcb_generic_event_t* ev);
static void             evpropertynotify(xcb_generic_event_t* ev);
static void             evclientmessage(xcb_generic_event_t* ev);

static client*          addclient(xcb_window_t win);
static void             releaseclient(xcb_window_t win);
static client*          clientfromwin(xcb_window_t win);


static monitor*         addmon(area a);
static monitor*         monbyarea(area a);
static monitor*         clientmon(client* cl);
static monitor*         cursormon();
static void             updatemons();

static xcb_keysym_t     getkeysym(xcb_keycode_t keycode);
static xcb_keycode_t*   getkeycodes(xcb_keysym_t keysym);

static void             runcmd(const char* cmd);
static xcb_atom_t       getatom(const char* atomstr);

static void             ewmh_updateclients();

static void             sigchld_handler(int32_t signum);

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
  [XCB_ENTER_NOTIFY]        = eventernotify,
  [XCB_KEY_PRESS]           = evkeypress,
  [XCB_BUTTON_PRESS]        = evbuttonpress,
  [XCB_MOTION_NOTIFY]       = evmotionnotify,
  [XCB_CONFIGURE_REQUEST]   = evconfigrequest,
  [XCB_CONFIGURE_NOTIFY]    = evconfignotify,
  [XCB_PROPERTY_NOTIFY]     = evpropertynotify,
  [XCB_CLIENT_MESSAGE]      = evclientmessage,
};

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

  // s.conecting to the X server
  s.con = xcb_connect(NULL, NULL);
  // Checking for errors
  if (xcb_connection_has_error(s.con)) {
    fprintf(stderr, "ragnar: cannot open display.\n");
    exit(EXIT_FAILURE);
  }
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  s.root = screen->root;
  s.screen = screen;

  /* Setting event mask for root window */
  uint32_t evmask[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_PROPERTY_CHANGE |
    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_FOCUS_CHANGE | 
    XCB_EVENT_MASK_POINTER_MOTION
  };
  xcb_change_window_attributes_checked(s.con, s.root, XCB_CW_EVENT_MASK, evmask);

  loaddefaultcursor();

  // Grab the window manager's keybinds
  grabkeybinds();
  // Setup atoms for EWMH and so on
  setupatoms();

  // Handle monitor setup 
  updatemons();
  s.monfocus = cursormon();
}

/**
 * @brief Event loop of the window manager 
 *
 * This function waits for X server events and 
 * handles them accoringly by calling the associated event handler.
 */
void
loop() {
  xcb_generic_event_t *ev;
  while ((ev = xcb_wait_for_event(s.con))) {
    uint8_t evcode = ev->response_type & ~0x80;
    /* If the event we receive is listened for by our 
     * event listeners, call the callback for the event. */
    if(evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
      evhandlers[evcode](ev);
    }
    free(ev);
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
 * @brief Evaluates if a given area is contained within another 
 * given area
 *
 * @param a The area to check if it's contained within the other area.
 * @param b The area to check if a is contained within it.
 *
 * @return If the area 'a' is within the area 'b'  
 */
bool
areainarea(area a, area b) {
  // Calculate the corners of area a
  float a_left = a.pos.x;
  float a_right = a.pos.x + a.size.x;

  // Calculate the corners of area b
  float b_left = b.pos.x;
  float b_right = b.pos.x + b.size.x;

  // Check if all corners of area a are within area b
  return (a_left >= b_left && a_right <= b_right);
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
  xcb_change_window_attributes_checked(s.con, cl->win, XCB_CW_BORDER_PIXEL, &color);
  xcb_flush(s.con);
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
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){width});
  // Update the border width of the client
  cl->borderwidth = width;
  xcb_flush(s.con);
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
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);
  xcb_flush(s.con);

  cl->area.pos = pos;


  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
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

  // Resize the window by configuring it's width and height property
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);
  xcb_flush(s.con);

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

  // Move and resize the window by configuring its x, y, width, and height properties
  xcb_configure_window(s.con, cl->win, 
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | 
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
  xcb_flush(s.con);

  cl->area = a;

  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
  s.monfocus = cl->mon; 
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
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_STACK_MODE, config);
  xcb_flush(s.con);
}

/**
 * @brief Kills a given client by destroying the associated window and 
 * removing it from the linked list.
 *
 * @param cl The client to kill 
 */
void
killclient(client* cl) {
  if(!cl) {
    return;
  }
  // Destroy the window on the X server
  if(!raiseevent(s.focus, s.wm_atoms[WMdelete])) {
    xcb_destroy_window(s.con, cl->win);
  }
  // Remove the client from the linked list
  releaseclient(cl->win);
  // Unset focus if the client was focused
  if(s.focus == cl) {
    s.focus = NULL;
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
  killclient(s.focus);;
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
  s.monfocus = cl->mon;

  xcb_flush(s.con);
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

void
unfocusclient(client* cl) {
  if (!cl) {
    return;
  }
  setbordercolor(cl, winbordercolor);
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHactiveWindow]);
  xcb_flush(s.con);
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
  // New X position of the window
  event.x = cl->area.pos.x;                
  // New Y position of the window
  event.y = cl->area.pos.y;
  // New width of the window
  event.width = cl->area.size.x;            
  // New height of the window
  event.height = cl->area.size.y;
  // Border width
  event.border_width = cl->borderwidth;
  // Above sibling window (None in this case)
  event.above_sibling = XCB_NONE;
  // Override-redirect flag
  event.override_redirect = 0;
  // Send the event
  xcb_send_event(s.con, 0, cl->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *)&event);
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
    uint32_t evmask[] = {  XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_POINTER_MOTION }; 
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

  // Set client's monitor
  cl->mon = clientmon(cl);

  // Set all clients floating for now (TODO)
  cl->floating = true;

  if(s.monfocus) {
    // Spawn the window in the center of the focused monitor
    moveclient(cl, (v2){
      s.monfocus->area.pos.x + (s.monfocus->area.size.x - cl->area.size.x) / 2.0f, 
      s.monfocus->area.pos.y + (s.monfocus->area.size.y - cl->area.size.y) / 2.0f});
  }


  // Send configure event to the client
  configclient(cl);

  // Update the EWMH client list
  ewmh_updateclients();

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(cl);
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

  // Remove the client from the list
  releaseclient(unmap_ev->window);

  // Update the EWMH client list
  ewmh_updateclients();

  // Unmap the window
  xcb_unmap_window(s.con, unmap_ev->window);
  xcb_flush(s.con);
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
  // Focus entered client
  if(cl) { 
    focusclient(cl);
  }
  else if(enter_ev->event == s.root) {
    // Set Input focus to root
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client* cl;
    for(cl = s.clients; cl != NULL; cl = cl->next) {
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
  for (uint32_t i = 0; i < numkeybinds; ++i) {
    // If it was pressed, call the callback of the keybind
    if ((keysym == keybinds[i].key) && (e->state == keybinds[i].modmask)) {
      if(keybinds[i].cb) {
        keybinds[i].cb(keybinds[i].cmd);
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

  client* cl = clientfromwin(button_ev->event);
  if(!cl) {
    return;
  }
  // Focusing client 
  if(cl != s.focus) {
    // Unfocus the previously focused window to ensure that there is only
    // one focused (highlighted) window at a time.
    if(s.focus) {
      unfocusclient(s.focus);
    }
    focusclient(cl);
  }

  // Setting grab position
  s.grabwin = cl->area;
  s.grabcursor = (v2){.x = (float)button_ev->root_x, (float)button_ev->root_y};

  // Raising the client to the top of the stack
  raiseclient(cl);
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

  if(motion_ev->event == s.root) {
    // Update the focused monitor to the monitor under the cursor
    s.monfocus = cursormon();
    return;
  }
  client* cl = clientfromwin(motion_ev->event);
  if(!cl) {
    return;
  }
  s.monfocus = cursormon();

  // Position of the cursor in the drag event
  v2 dragpos    = (v2){.x = (float)motion_ev->root_x, .y = (float)motion_ev->root_y};
  // Drag difference from the current drag event to the initial grab 
  v2 dragdelta  = (v2){.x = dragpos.x - s.grabcursor.x, .y = dragpos.y - s.grabcursor.y};

  // On left click, move the window
  if(motion_ev->state & XCB_BUTTON_MASK_1) {
    // New position of the window
    v2 movedest = (v2){.x = (float)(s.grabwin.pos.x + dragdelta.x), .y = (float)(s.grabwin.pos.y + dragdelta.y)};

    moveclient(cl, movedest);
  } 
  // On right click resize the window
  else if(motion_ev->state & XCB_BUTTON_MASK_3) {
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
  /* ===== Taken from DWM and rewriten for XCB ==== */
  xcb_configure_request_event_t* config_ev = (xcb_configure_request_event_t*)ev;

  client* cl = clientfromwin(config_ev->window);

  // Variables for configuring the window
  uint16_t mask = 0;
  uint32_t values[7];
  int i = 0;

  if (cl) {
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
      cl->borderwidth = config_ev->border_width;
    } else {
      // If the client is not floating, don't use any values of the event, instead
      // configure the client window by the tiling layout later
      configclient(cl);
    }
  } else {
    // If the configure event did not occur on a client window, 
    // just configure it.
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_X) {
      values[i++] = config_ev->x;
      mask |= XCB_CONFIG_WINDOW_X;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_Y) {
      values[i++] = config_ev->y;
      mask |= XCB_CONFIG_WINDOW_Y;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
      values[i++] = config_ev->width;
      mask |= XCB_CONFIG_WINDOW_WIDTH;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
      values[i++] = config_ev->height;
      mask |= XCB_CONFIG_WINDOW_HEIGHT;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
      values[i++] = config_ev->border_width;
      mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
      values[i++] = config_ev->sibling;
      mask |= XCB_CONFIG_WINDOW_SIBLING;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
      values[i++] = config_ev->stack_mode;
      mask |= XCB_CONFIG_WINDOW_STACK_MODE;
    }
    xcb_configure_window(s.con, config_ev->window, mask, values);
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
}

/**
 * @brief Cycles the currently focused client 
 */
void
cyclefocus() {
  if (!s.clients || !s.focus)
    return;
  // Find the next client to focus
  client *next = s.focus->next ? s.focus->next : s.clients;
  focusclient(next);
  raiseclient(next);
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
    for (unsigned int i = 0; i < reply.atoms_len && !exists; i++) {
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
void setfullscreen(client* cl, bool fullscreen) {
  cl->fullscreen = fullscreen;
  if(cl->fullscreen) {
    // Store previous position of client
    cl->area_prev = cl->area;
    // Set the client's area to the focused monitors area, effictivly
    // making the client as large as the monitor screen
    cl->area = s.monfocus->area;

    // Unset border of client if it's fullscreen
    cl->borderwidth = 0;
  } else {
    // Set the client's area to the area before the last fullscreen occured 
    cl->area = cl->area_prev;
    cl->borderwidth = winborderwidth;
  }
  // Update client's border width
  setborderwidth(cl, cl->borderwidth);
  // Update client's geometry
  moveresizeclient(cl, cl->area);
}

/**
 * @brief Toggles fullscreen mode on the currently focused client
 */
void togglefullscreen() {
  bool fs = !(s.focus->fullscreen);
  setfullscreen(s.focus, fs); 
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
  s.ewmh_atoms[EWMHcheck]             = getatom("_NET_SUPPORTING_WM_CHECK");
  s.ewmh_atoms[EWMHfullscreen]        = getatom("_NET_WM_STATE_FULLSCREEN");
  s.ewmh_atoms[EWMHwindowType]        = getatom("_NET_WM_WINDOW_TYPE");
  s.ewmh_atoms[EWMHwindowTypeDialog]  = getatom("_NET_WM_WINDOW_TYPE_DIALOG");
  s.ewmh_atoms[EWMHclientList]        = getatom("_NET_CLIENT_LIST");

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

  // Set _NET_SUPPORTED property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHsupported],
                      XCB_ATOM_ATOM, 32, EWMHcount, s.ewmh_atoms);

  // Delete _NET_CLIENT_LIST property from the root window
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

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
	for (uint32_t i = 0; i < numkeybinds; ++i) {
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
  // Assert that it worked
  assert(success && "Failed to get window area.");

  cl->area = area;
  cl->borderwidth = winborderwidth;
  cl->fullscreen = false;
  cl->floating = false;


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
  for(cl = s.clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->win == win) {
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
monitor* addmon(area a) {
  monitor* mon = (monitor*)malloc(sizeof(*mon));
  mon->area = a;

  mon->next = s.monitors;
  s.monitors = mon;

  return mon;
}

void
updatemons() {
  // Get xrandr screen resources
  xcb_randr_get_screen_resources_current_cookie_t res_cookie = xcb_randr_get_screen_resources_current(s.con, s.screen->root);
  xcb_randr_get_screen_resources_current_reply_t *res_reply = xcb_randr_get_screen_resources_current_reply(s.con, res_cookie, NULL);

  if (!res_reply) {
    fprintf(stderr, "ragnar: not get screen resources.\n");
    terminate();
  }

  // Get number of connected monitors
  int num_outputs = xcb_randr_get_screen_resources_current_outputs_length(res_reply);
  // Get list of monitors
  xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(res_reply);

  for (int i = 0; i < num_outputs; i++) {
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
          addmon(monarea);
        }
        free(crtc_reply);
      }
    }
    if (info_reply) {
      free(info_reply);
    }
  }
  free(res_reply);

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
monitor* monbyarea(area a) {
  monitor* mon;
  for(mon = s.monitors; mon != NULL; mon = mon->next) {
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
  if(!cl) {
    return s.monitors;
  }
  monitor* mon;
  for(mon = s.monitors; mon != NULL; mon = mon->next) {
    if(areainarea(cl->area, mon->area)) {
      return mon;
    }
  }
  return s.monitors;
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
  for(mon = s.monitors; mon != NULL; mon = mon->next) {
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
runcmd(const char* cmd) {
  if (cmd == NULL) {
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    // If execl fails
    fprintf(stderr, "ragnar: failed to execute command.\n");
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    // Parent process
    int status;
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
 * @brief Updates the client list EWMH atom tothe current list of clients.
 * */
void
ewmh_updateclients() {
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

  client* cl;
  for(cl = s.clients; cl != NULL; cl = cl->next) {
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

int 
main() {
  // Run the startup script
  runcmd("ragnarstart");
  // Setup the window manager
  setup();
  // Enter the event loop
  loop();
  // Terminate after the loop
  terminate();
  return EXIT_SUCCESS;
}

