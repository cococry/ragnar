#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <stdbool.h>

#define _XCB_EV_LAST 35
#define CLIENT_CAP_INIT 32

#define arraylen(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct {
  int16_t x, y;
} v2;

typedef struct {
  v2 pos, size;
} area;

static void setup();
static void loop();
static void terminate();

static bool pointinarea(v2 p, area a);
static v2 cursorpos(bool* success);
static area winarea(xcb_window_t win, bool* success);

static void evmaprequest(xcb_generic_event_t* ev);
static void evunmapnotify(xcb_generic_event_t* ev);
static void eventernotify(xcb_generic_event_t* ev);

typedef void (*event_handler_t)(xcb_generic_event_t* ev);

typedef struct {
  v2 pos, size;
} client;

typedef struct {
  client** data;
  uint32_t count, cap;
} client_list;

typedef struct {
  xcb_connection_t* con;
  xcb_window_t root;

  client_list clients;
} State;

static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST] = evmaprequest,
  [XCB_UNMAP_NOTIFY] = evunmapnotify,
  [XCB_ENTER_NOTIFY] = eventernotify
};

static State s;

void
setup() {
  s.con = xcb_connect(NULL, NULL);
  if (xcb_connection_has_error(s.con)) {
    fprintf(stderr, "Cannot open display\n");
    exit(1);
  }

  const xcb_setup_t *setup = xcb_get_setup(s.con);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  xcb_screen_t *screen = iter.data;
  s.root = screen->root;

  uint32_t values[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_ENTER_WINDOW
  };
  xcb_change_window_attributes(s.con, s.root, XCB_CW_EVENT_MASK, values);
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
  // Map the window
  xcb_map_window(s.con, map_ev->window);

  // Listen for enter events on that window
  uint32_t values[] = { XCB_EVENT_MASK_ENTER_WINDOW };
  xcb_change_window_attributes(s.con, map_ev->window, XCB_CW_EVENT_MASK, values);

  // Retrieving cursor position
  bool cursor_success;
  v2 cursor = cursorpos(&cursor_success);
  if(!cursor_success) goto flush;

  // Retrieving geometry of the mapped window 
  bool area_success;
  area win = winarea(map_ev->window, &area_success);
  if(!area_success) goto flush;

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, win)) {
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, map_ev->window, XCB_CURRENT_TIME);
  }
flush:
  xcb_flush(s.con);
}

void 
evunmapnotify(xcb_generic_event_t* ev) {
  xcb_unmap_notify_event_t* unmap_ev = (xcb_unmap_notify_event_t*)ev;
  xcb_unmap_window(s.con, unmap_ev->window);
}

void 
eventernotify(xcb_generic_event_t* ev) {
  xcb_enter_notify_event_t *enter_ev = (xcb_enter_notify_event_t*)ev;

  // Set input focus on window enter
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, enter_ev->event, XCB_CURRENT_TIME);
  xcb_flush(s.con);
}

int 
main() {
  setup();
  system("st &");
  loop();
  terminate();
  return 0;
}
