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

#include "structs.h"

static void             setup();
static void             loop();
static void             terminate();

static bool             pointinarea(v2 p, area a);
static v2               cursorpos(bool* success);
static area             winarea(xcb_window_t win, bool* success);

static void             setbordercolor(client* cl, uint32_t color);
static void             setborderwidth(client* cl, uint32_t width);
static void             moveclient(client* cl, v2 pos);
static void             resizeclient(client* cl, v2 size);
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
static void             evpropertynotify(xcb_generic_event_t* ev);
static void             evclientmessage(xcb_generic_event_t* ev);

static client*          addclient(xcb_window_t win);
static void             releaseclient(xcb_window_t win);
static client*          clientfromwin(xcb_window_t win);

static xcb_keysym_t     getkeysym(xcb_keycode_t keycode);
static xcb_keycode_t*   getkeycodes(xcb_keysym_t keysym);

static void             runcmd(const char* cmd);
static xcb_atom_t       getatom(const char* atomstr);

static void             ewmh_updateclients();


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
    XCB_EVENT_MASK_FOCUS_CHANGE
  };
  xcb_change_window_attributes_checked(s.con, s.root, XCB_CW_EVENT_MASK, evmask);

  loaddefaultcursor();

  // Grab the window manager's keybinds
  grabkeybinds();
  // Setup atoms for EWMH and so on
  setupatoms();
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

  /* Referesh window area aafter moving it on X server. */
  bool success;
  area a = winarea(cl->win, &success);
  if(!success) return;

  cl->area = a;
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

  /* Referesh window area aafter resizing it on X server. */
  bool success;
  area a = winarea(cl->win, &success);
  if(!success) return;

  cl->area = a;
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

  // Change border color to indicate selection
  setbordercolor(cl, winbordercolor_selected);
  setborderwidth(cl, winborderwidth);

  // Set the focused client
  s.focus = cl;

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
    uint32_t evmask[] = {  XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE }; 
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

  client* cl = clientfromwin(motion_ev->event);
  if(!cl) {
    return;
  }

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
 * @brief Handles a X configure request by configuring the client that 
 * is associated with the window how the event requested it.
 *
 * @param ev The generic event 
 */
void 
evconfigrequest(xcb_generic_event_t* ev) {
  xcb_configure_request_event_t* req = (xcb_configure_request_event_t*)ev;
  xcb_window_t win = req->window;

  client* cl = clientfromwin(win);
  if(!cl) {
    return;
  }

  // Configuration mask
  uint16_t mask = 0;
  uint32_t values[7];
  int i = 0;
  // If the events wants to configure x, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_X) {
    cl->area.pos.y = req->y;
    mask |= XCB_CONFIG_WINDOW_X;
    values[i++] = req->x;
  }
  // If the events wants to configure y, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_Y) {
    cl->area.pos.x = req->x;
    mask |= XCB_CONFIG_WINDOW_Y;
    values[i++] = req->y;
  }
  // If the events wants to configure width, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
    cl->area.size.x = req->width;
    mask |= XCB_CONFIG_WINDOW_WIDTH;
    values[i++] = req->width;
  }
  if (req->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
    cl->area.size.y = req->height;
    mask |= XCB_CONFIG_WINDOW_HEIGHT;
    values[i++] = req->height;
  }
  // If the events wants to configure height, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
    cl->borderwidth = req->border_width;
    mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
    values[i++] = req->border_width;
  }
  // If the events wants to configure a sibling, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
    mask |= XCB_CONFIG_WINDOW_SIBLING;
    values[i++] = req->sibling;
  }
  // If the events wants to configure the stack mode, add it to the config
  if (req->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
    mask |= XCB_CONFIG_WINDOW_STACK_MODE;
    values[i++] = req->stack_mode;
  }

  // Update the clients geometry and border width
  setborderwidth(cl, cl->borderwidth);
  moveclient(cl, cl->area.pos);
  resizeclient(cl, cl->area.size);

  // Configure the window with the accumulated mask and values
  xcb_configure_window(s.con, win, mask, values);
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
    if(msg_ev->data.data32[1] == s.ewmh_atoms[EWMHfullscreen] ||
       msg_ev->data.data32[2] == s.ewmh_atoms[EWMHfullscreen]) {
      // TODO: Set client fullscreen
      printf("Client went fullscreen.\n");
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
    // TODO: Set client fullscreen
    printf("Client went fullscreen.\n");
  } 
  if(wintype == s.ewmh_atoms[EWMHwindowTypeDialog]) {
    // TODO: Mark client as floating
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
 * by window.
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

