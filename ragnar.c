#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>
#include <X11/keysym.h>

#include "config.h"

#define _XCB_EV_LAST 36 

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

typedef struct {
  float x, y;
} v2;

typedef struct {
  v2 pos, size;
} area;

typedef void (*event_handler_t)(xcb_generic_event_t* ev);

typedef struct client client;

struct client {
  area area;
  xcb_window_t win;

  client* next;
};

typedef struct {
  xcb_connection_t* con;
  xcb_window_t root;

  client* clients;
  client* focus;

  xcb_key_symbols_t* key_symbols;

  v2 grabcursor;
  area grabwin;
} State;


static void     setup();
static void     loop();
static void     terminate();

static bool     pointinarea(v2 p, area a);
static v2       cursorpos(bool* success);
static area     winarea(xcb_window_t win, bool* success);

static void     setbordercolor(xcb_window_t win, uint32_t color);
static void     moveclient(client* cl, v2 pos);
static void     resizeclient(client* cl, v2 size);
static void     raisewin(xcb_window_t win);

static void     evmaprequest(xcb_generic_event_t* ev);
static void     evunmapnotify(xcb_generic_event_t* ev);
static void     eventernotify(xcb_generic_event_t* ev);
static void     evfocusin(xcb_generic_event_t* ev);
static void     evfocusout(xcb_generic_event_t* ev);
static void     evkeypress(xcb_generic_event_t* ev);
static void     evbuttonpress(xcb_generic_event_t* ev);
static void     evmotionnotify(xcb_generic_event_t* ev);

static client*  addclient(xcb_window_t win);
static void     releaseclient(xcb_window_t win);
static client*  clientfromwin(xcb_window_t win);

static void     focusclient(client* cl);

static void     grabkeybind(keybind bind, xcb_window_t win);
static void     setupkeybinds(xcb_window_t win);
static bool     checkkeybind(xcb_keysym_t keysym, uint16_t state, keybind bind);


static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]   = evmaprequest,
  [XCB_UNMAP_NOTIFY]  = evunmapnotify,
  [XCB_ENTER_NOTIFY]  = eventernotify,
  [XCB_KEY_PRESS]     = evkeypress,
  [XCB_FOCUS_IN]      = evfocusin,
  [XCB_FOCUS_OUT]     = evfocusout,
  [XCB_BUTTON_PRESS]  = evbuttonpress,
  [XCB_MOTION_NOTIFY]  = evmotionnotify,
};

static State s;

void
setup() {
  s.clients = NULL;
  s.con = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(s.con)) {
    fprintf(stderr, "ragnar: cannot open display\n");
    exit(1);
  }
  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  s.root = screen->root;

  s.key_symbols = xcb_key_symbols_alloc(s.con);
  if (!s.key_symbols) {
    fprintf(stderr, "ragnar: unable to allocate key symbols\n");
    exit(1);
  }

  /* Setting event mask for root window */
  uint32_t evmask[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_PROPERTY_CHANGE |
    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_FOCUS_CHANGE |
    XCB_EVENT_MASK_BUTTON_PRESS
  };
  xcb_change_window_attributes(s.con, s.root, XCB_CW_EVENT_MASK, evmask);

  // Ungrabbing any grabbed keys
	xcb_ungrab_key(s.con, XCB_GRAB_ANY, s.root, XCB_MOD_MASK_ANY);

  // Grabbing window manager's keybindings
  setupkeybinds(s.root);

  xcb_flush(s.con);
}

void
loop() {
  xcb_generic_event_t *ev;
  while ((ev= xcb_wait_for_event(s.con))) {
    uint8_t evcode = ev->response_type & ~0x80;
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
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s.con, xcb_query_pointer(s.con, s.root), NULL);
  v2 cursor = (v2){.x = reply->root_x, .y = reply->root_y};
  if((*success = (reply != NULL))) { 
    free(reply);
  } else {
    fprintf(stderr, "ragnar: failed to retrieve cursor position"); 
  }
  return cursor;
}

area 
winarea(xcb_window_t win, bool* success) {
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  area a = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height}};
  printf("%f, %f, | %f, %f\n", a.pos.x, a.pos.y, a.size.x, a.size.y);
  if((*success = (reply != NULL))) {
    free(reply);
  } else {
    fprintf(stderr, "ragnar: failed to retrieve cursor position"); 
  }
  return a;
}

void
setbordercolor(xcb_window_t win, uint32_t color) {
  xcb_change_window_attributes(s.con, win, XCB_CW_BORDER_PIXEL, &color);
  xcb_configure_window(s.con, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){winborderwidth});
  xcb_flush(s.con);
}

void
moveclient(client* cl, v2 pos) {
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
raisewin(xcb_window_t win) {
  uint32_t evmask[] = { XCB_STACK_MODE_ABOVE };
  xcb_configure_window(s.con, win, XCB_CONFIG_WINDOW_STACK_MODE, evmask);
  xcb_flush(s.con);
}

void 
evmaprequest(xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

  // Setup listened events for the mapped window
  {
    uint32_t evmask[] = {  XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE }; 
    xcb_change_window_attributes(s.con, map_ev->window, XCB_CW_EVENT_MASK, evmask);
  }

  // Grabbing mouse events for interactive moves/resizes 
  {
    uint16_t evmask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION;
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s.root, XCB_NONE, 1, winmod);
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s.root, XCB_NONE, 3, winmod);
  }

  // Set initial border color
  setbordercolor(map_ev->window, winbordercolor);

  // Map the window
  xcb_map_window(s.con, map_ev->window);


  // Retrieving cursor position
  bool cursor_success;
  v2 cursor = cursorpos(&cursor_success);
  if(!cursor_success) goto flush;

  client* cl = addclient(map_ev->window);
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
    // Reset border color of every client 
    client* cl;
    for(cl = s.clients; cl != NULL; cl = cl->next) {
      setbordercolor(cl->win, winbordercolor);
    }
  }

  xcb_flush(s.con);
}

void
evfocusin(xcb_generic_event_t* ev) {
  xcb_focus_in_event_t* focus_ev = (xcb_focus_in_event_t*)ev;
  setbordercolor(focus_ev->event, winbordercolor_selected);
}

void
evfocusout(xcb_generic_event_t* ev) {
  xcb_focus_out_event_t* focus_ev = (xcb_focus_out_event_t*)ev;
  setbordercolor(focus_ev->event, winbordercolor);
}

void
evkeypress(xcb_generic_event_t* ev) {
  xcb_key_press_event_t* key_ev = (xcb_key_press_event_t*)ev;
  xcb_keysym_t keysym = xcb_key_symbols_get_keysym(s.key_symbols, key_ev->detail, 0);

  // Handle terminal keybind
  if(checkkeybind(keysym, key_ev->state, terminalkeybind)) {
    system(terminalcmd);
  }

  // Handle exit keybind
  else if(checkkeybind(keysym, key_ev->state, exitkeybind)) {
    terminate();
  }

  // Handle window cycle keybind
  else if(checkkeybind(keysym, key_ev->state, wincyclekeybind)) {
    if(s.focus->next != NULL) {
      s.focus = s.focus->next;
    }
    else {
      s.focus = s.clients;
    }
    focusclient(s.focus);
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
  raisewin(cl->win);
}

void
evmotionnotify(xcb_generic_event_t* ev) {
  xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)ev;

  client* cl = clientfromwin(motion_ev->event);


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

client*
addclient(xcb_window_t win) {
  // Allocate client structure
  client* cl = (client*)malloc(sizeof(*cl));
  cl->win = win;

  // Get the window area
  bool success;
  area area = winarea(win, &success);
  assert(success && "Failed to get window area.");
  cl->area = area;

  cl->next = s.clients;
  s.clients = cl;
  return cl;
}

void 
releaseclient(xcb_window_t win) {
  client** prev = &s.clients;
  client* cl = s.clients;
  while(cl) {
    if(cl->win == win) {
      *prev = cl->next;
      free(cl);
      return;
    }
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

void
focusclient(client* cl) {
  if(!cl->win || cl->win == s.root) return;
  // Set input focus to client
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);

  // Change border color to indicate selection
  setbordercolor(cl->win, winbordercolor_selected);

  // Set the focused client
  s.focus = cl;
}

void 
grabkeybind(keybind bind, xcb_window_t win) {
  xcb_keycode_t keycode = xcb_key_symbols_get_keycode(s.key_symbols, bind.key)[0];
  if (!keycode) {
    fprintf(stderr, "ragnar: unable to get keycode for given key.\n");
    exit(1);
  }
  xcb_grab_key(s.con, 1, win, bind.modmask, keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
}

void
setupkeybinds(xcb_window_t win) {
  grabkeybind(terminalkeybind, win);
  grabkeybind(exitkeybind, win);
  grabkeybind(wincyclekeybind, win);
}

bool
checkkeybind(xcb_keysym_t keysym, uint16_t state, keybind bind) {
  return keysym == bind.key && (state & (bind.modmask)) == (bind.modmask);
}

int 
main() {
  setup();
  loop();
  terminate();
  return 0;
}

