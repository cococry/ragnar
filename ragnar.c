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

#include "structs.h"

static void     setup();
static void     loop();
static void     terminate();

static bool     pointinarea(v2 p, area a);
static v2       cursorpos(bool* success);
static area     winarea(xcb_window_t win, bool* success);

static void     setbordercolor(client* cl, uint32_t color);
static void     setborderwidth(client* cl, uint32_t width);
static void     moveclient(client* cl, v2 pos);
static void     resizeclient(client* cl, v2 size);
static void     raiseclient(client* cl);
static void     killclient(client* cl);
static void     killfocus();
static void     focusclient(client* cl);
static void     configclient(client* cl);
static void     cyclefocus();

static void     evmaprequest(xcb_generic_event_t* ev);
static void     evunmapnotify(xcb_generic_event_t* ev);
static void     eventernotify(xcb_generic_event_t* ev);
static void     evfocusin(xcb_generic_event_t* ev);
static void     evfocusout(xcb_generic_event_t* ev);
static void     evkeypress(xcb_generic_event_t* ev);
static void     evbuttonpress(xcb_generic_event_t* ev);
static void     evmotionnotify(xcb_generic_event_t* ev);
static void     evconfigrequest(xcb_generic_event_t* ev);

static client*  addclient(xcb_window_t win);
static void     releaseclient(xcb_window_t win);
static client*  clientfromwin(xcb_window_t win);

static xcb_keysym_t     getkeysym(xcb_keycode_t keycode);
static xcb_keycode_t*   getkeycodes(xcb_keysym_t keysym);

// This needs to be included after the function definitionss
#include "config.h"


#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]         = evmaprequest,
  [XCB_UNMAP_NOTIFY]        = evunmapnotify,
  [XCB_ENTER_NOTIFY]        = eventernotify,
  [XCB_KEY_PRESS]           = evkeypress,
  [XCB_FOCUS_IN]            = evfocusin,
  [XCB_FOCUS_OUT]           = evfocusout,
  [XCB_BUTTON_PRESS]        = evbuttonpress,
  [XCB_MOTION_NOTIFY]       = evmotionnotify,
  [XCB_CONFIGURE_REQUEST]   = evconfigrequest,
};

static State s;

/**
 * @brief Sets up the WM state and the X server 
 *
 * This function establishes a connection to the x server,
 * sets up the root window and window manager keybindings.
 * The event mask of the root window is being cofigured to
 * listen to necessary events. 
 * After the configuration of the root window, all the specified
 * keybinds in config.h are grabbed by the window manager.
 *
 */
void
setup() {
  s.clients = NULL;

  // Connecting to the X server
  s.con = xcb_connect(NULL, NULL);
  // Checking for errors
  if (xcb_connection_has_error(s.con)) {
    fprintf(stderr, "ragnar: cannot open display\n");
    exit(1);
  }
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  s.root = screen->root;


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

void
loop() {
  xcb_generic_event_t *ev;
  while ((ev= xcb_wait_for_event(s.con))) {
    uint8_t evcode = ev->response_type & ~0x80;
    /* If the event we receive is listened for by our 
     * event listeners, call the callback for the event. */
    if(evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
      evhandlers[evcode](ev);
    }
    free(ev);
  }
}

void 
terminate() {
  xcb_disconnect(s.con);
  exit(0);
}

bool
pointinarea(v2 p, area area) {
  return (p.x >= area.pos.x &&
  p.x < (area.pos.x + area.size.x) &&
  p.x >= area.pos.y &&
  p.y < (area.pos.y + area.size.y));
}

v2 
cursorpos(bool* success) {
  // Query the pointer position
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s.con, xcb_query_pointer(s.con, s.root), NULL);
  // Create a v2 for to store it
  v2 cursor = (v2){.x = reply->root_x, .y = reply->root_y};

  // Check for errors
  if((*success = (reply != NULL))) { 
    free(reply);
  } else {
    fprintf(stderr, "ragnar: failed to retrieve cursor position"); 
  }
  return cursor;
}

area 
winarea(xcb_window_t win, bool* success) {
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  // Creating the area structure to store the geometry in
  area a = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height}};

  // Error checking
  if((*success = (reply != NULL))) {
    free(reply);
  } else {
    fprintf(stderr, "ragnar: failed to retrieve cursor position"); 
  }
  return a;
}

void
setbordercolor(client* cl, uint32_t color) {
  if(!cl) return;
  xcb_change_window_attributes_checked(s.con, cl->win, XCB_CW_BORDER_PIXEL, &color);
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){cl->borderwidth});
  xcb_flush(s.con);
}

void
setborderwidth(client* cl, uint32_t width) {
  if(!cl) return;
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){width});
  xcb_flush(s.con);
}

void
moveclient(client* cl, v2 pos) {
  if(!cl) return;
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

void
resizeclient(client* cl, v2 size) {
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

void
raiseclient(client* cl) {
  if(!cl) return;
  uint32_t evmask[] = { XCB_STACK_MODE_ABOVE };
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_STACK_MODE, evmask);
  xcb_flush(s.con);
}

void
killclient(client* cl) {
  if(!cl) return;
  // Destroy the window on the X server
  xcb_destroy_window(s.con, cl->win);
  // Remove the client from the linked list
  releaseclient(cl->win);
  // Unset focus if the client was focused
  if(s.focus == cl) {
    s.focus = NULL;
  }
  xcb_flush(s.con);
}

void
killfocus() {
  killclient(s.focus);;
}

void
focusclient(client* cl) {
  if(!cl) return;
  if(!cl->win || cl->win == s.root) return;
  // Set input focus to client
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);

  // Change border color to indicate selection
  setbordercolor(cl, winbordercolor_selected);

  // Set the focused client
  s.focus = cl;
}

void
configclient(client* cl) {
  if(!cl) return;
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
}

void 
evmaprequest(xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

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


  // Map the window
  xcb_map_window(s.con, map_ev->window);

  // Retrieving cursor position
  bool cursor_success;
  v2 cursor = cursorpos(&cursor_success);
  if(!cursor_success) goto flush;

  // Adding the mapped client to our linked list
  client* cl = addclient(map_ev->window);

  // Set initial border color
  setbordercolor(cl, winbordercolor);

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(cl);
  }

flush:
  xcb_flush(s.con);
}

void 
evunmapnotify(xcb_generic_event_t* ev) {
  // Retrieve the event
  xcb_unmap_notify_event_t* unmap_ev = (xcb_unmap_notify_event_t*)ev;

  // Remove the client from the list
  releaseclient(unmap_ev->window);

  // Unmap the window
  xcb_unmap_window(s.con, unmap_ev->window);
}

void 
eventernotify(xcb_generic_event_t* ev) {
  xcb_enter_notify_event_t *enter_ev = (xcb_enter_notify_event_t*)ev;

  client* cl = clientfromwin(enter_ev->event);
  if(cl) 
    focusclient(cl);
  else if(enter_ev->event == s.root) {
    // Set Input focus to root
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client* cl;
    for(cl = s.clients; cl != NULL; cl = cl->next) {
      setbordercolor(cl, winbordercolor);
    }
  }

  xcb_flush(s.con);
}

void
evfocusin(xcb_generic_event_t* ev) {
  xcb_focus_in_event_t* focus_ev = (xcb_focus_in_event_t*)ev;
  // Retrieving associated client
  client* cl = clientfromwin(focus_ev->event);
  if(!cl) return;
  // If a client gained focus, set selected border color.
  setbordercolor(cl, winbordercolor_selected);
}

void
evfocusout(xcb_generic_event_t* ev) {
  xcb_focus_out_event_t* focus_ev = (xcb_focus_out_event_t*)ev;
  // Retrieving associated client
  client* cl = clientfromwin(focus_ev->event);
  if(!cl) return;
  // If a client lost focus, set unselected border color.
  setbordercolor(cl, winbordercolor);
}

void
evkeypress(xcb_generic_event_t* ev) {
  xcb_key_press_event_t *e = ( xcb_key_press_event_t *) ev;
  // Get associated keysym for the keycode of the event
  xcb_keysym_t keysym = getkeysym(e->detail);

  
  /* Iterate throguh the keybinds and check if one of them was pressed. */
  for (uint32_t i = 0; i < numkeybinds; ++i) {
    // If it was pressed, call the callback of the keybind
    if ((keysym == keybinds[i].key) && (e->state == keybinds[i].modmask)) {
      if(keybinds[i].cb)
        keybinds[i].cb();
      else 
        system(keybinds[i].cmd);
    }
  }
}

void
evbuttonpress(xcb_generic_event_t* ev) {
  xcb_button_press_event_t* button_ev = (xcb_button_press_event_t*)ev;

  client* cl = clientfromwin(button_ev->event);
  if(!cl) return;
  // Focusing client 
  if(cl != s.focus) {
    focusclient(cl);
  }

  // Setting grab position
  s.grabwin = cl->area;
  s.grabcursor = (v2){.x = (float)button_ev->root_x, (float)button_ev->root_y};

  // Raising the client to the top of the stack
  raiseclient(cl);
}

void
evmotionnotify(xcb_generic_event_t* ev) {
  xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)ev;

  client* cl = clientfromwin(motion_ev->event);
  if(!cl) return;


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
}

void 
evconfigrequest(xcb_generic_event_t* ev) {
  xcb_configure_request_event_t* req = (xcb_configure_request_event_t*)ev;
  xcb_window_t win = req->window;

  client* cl = clientfromwin(win);
  if(!cl) return;

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


void
cyclefocus() {
  /* If there is a next window ater the window that's currently
   * focused, focus that window */
  if(s.focus->next != NULL) {
    s.focus = s.focus->next;
  }
  else {
    // Set the focus back to the first client on the list
    s.focus = s.clients;
  }
  /* Update & raise focus */
  focusclient(s.focus);
  raiseclient(s.focus);
}

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

  // Sending a configure event
  configclient(cl);

  // Updating the linked list of clients
  cl->next = s.clients;
  s.clients = cl;
  return cl;
}

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

client*
clientfromwin(xcb_window_t win) {
  client* cl;
  for(cl = s.clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->win == win) return cl;
  }
  return NULL;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
	xcb_keysym_t keysym = (!(keysyms) ? 0 : xcb_key_symbols_get_keysym(keysyms, keycode, 0));
	xcb_key_symbols_free(keysyms);
	return keysym;
}

xcb_keycode_t*
getkeycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
	xcb_keycode_t *keycode = (!(keysyms) ? NULL : xcb_key_symbols_get_keycode(keysyms, keysym));
	xcb_key_symbols_free(keysyms);
	return keycode;
}

int 
main() {
  setup();
  loop();
  terminate();
  return 0;
}

