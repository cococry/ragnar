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

#define _XCB_EV_LAST 35
#define arraylen(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
  int16_t x, y;
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

  xcb_key_symbols_t* key_symbols;
} State;


static void     setup();
static void     loop();
static void     terminate();

static bool     pointinarea(v2 p, area a);
static v2       cursorpos(bool* success);
static area     winarea(xcb_window_t win, bool* success);

static void     evmaprequest(xcb_generic_event_t* ev);
static void     evunmapnotify(xcb_generic_event_t* ev);
static void     eventernotify(xcb_generic_event_t* ev);
static void     evkeypress(xcb_generic_event_t* ev);

static client*  addclient(xcb_window_t win);
static void     releaseclient(xcb_window_t win);
static client*  clientfromwin(xcb_window_t win);

static void     focusclient(client* cl);
static void     unfocusclient(client* cl);

static void     grabkeybind(keybind bind, xcb_window_t win);
static void     setupkeybinds(xcb_window_t win);
static bool     checkkeybind(xcb_keysym_t keysym, uint16_t state, keybind bind);

static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]   = evmaprequest,
  [XCB_UNMAP_NOTIFY]  = evunmapnotify,
  [XCB_ENTER_NOTIFY]  = eventernotify,
  [XCB_KEY_PRESS]     = evkeypress 
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


  uint32_t values[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_ENTER_WINDOW | 
    XCB_EVENT_MASK_KEY_PRESS
  };
  xcb_change_window_attributes(s.con, s.root, XCB_CW_EVENT_MASK, values);
  setupkeybinds(s.root);
  xcb_flush(s.con);
}

void
loop() {
  xcb_generic_event_t *ev;
  while ((ev= xcb_wait_for_event(s.con))) {
    uint8_t evcode = ev->response_type & ~0x80;
    if(evcode < arraylen(evhandlers) && evhandlers[evcode]) {
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
  }
  return cursor;
}

area 
winarea(xcb_window_t win, bool* success) {
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  area a = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height}};
  if((*success = (reply != NULL))) {
    free(reply);
  }
  return a;
}
void 
evmaprequest(xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

  // Set window properties
  xcb_change_window_attributes(s.con, map_ev->window, XCB_CW_BORDER_PIXEL, &winbordercolor);
  xcb_configure_window(s.con, map_ev->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){winborderwidth});


  // Map the window
  xcb_map_window(s.con, map_ev->window);

  // Listen for enter events on that window
  uint32_t values[] = { XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_KEY_PRESS };
  xcb_change_window_attributes(s.con, map_ev->window, XCB_CW_EVENT_MASK, values);

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

  // Set input focus on window enter
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, enter_ev->event, XCB_CURRENT_TIME);
  xcb_flush(s.con);
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
      unfocusclient(cl);
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
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);
}

void
unfocusclient(client* cl) {
  (void)cl;
}

void 
grabkeybind(keybind bind, xcb_window_t win) {
  xcb_keycode_t keycode = xcb_key_symbols_get_keycode(s.key_symbols, bind.key)[0];
  if (!keycode) {
    fprintf(stderr, "ragnar: unable to get keycode for key\n");
    exit(1);
  }
  xcb_grab_key(s.con, 1, win, bind.modmask, keycode,
               XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
}

void
setupkeybinds(xcb_window_t win) {
  grabkeybind(terminalkeybind, win);
  grabkeybind(exitkeybind, win);
}

bool
checkkeybind(xcb_keysym_t keysym, uint16_t state, keybind bind) {
  return keysym == bind.key && (state & (bind.modmask)) == (bind.modmask);
}

int 
main() {
  setup();
  system("st &");
  loop();
  terminate();
  return 0;
}
