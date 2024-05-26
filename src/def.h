#pragma once
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

/* --- MACROS --- */
#define exec_and_terminate(exec, msg) exec terminate(msg)

/**
 * Checks if a given condition is not met and executes a given codeblock before terminating the program if 
 * the condition is not met. 
*/
#define rg_assert_exec_msg(cond, exec, msg) if(!cond) {                                               \
  exec_and_terminate({                                                                                \
      printf("[Assertion failed]: '%s' in file %s, line %i. | %s", #cond, __FILE__, __LINE__, msg);   \
      exec                                                                                            \
      }, "Assertion failed.");                                                                        \
}                                                                                                     \

/**
 * Checks if a given condition is not met. If the condition is not met, a given message is printed 
 * before exit(1) is called.
*/
#define rg_assert_msg(cond, msg) rg_assert_exec_msg(cond {}, msg)

/**
 * Terminates the program if a given condition is not met.
*/
#define rg_assert(cond) if(!cond) {                                                                   \
  terminate("Assertion failed: '%s' in file, line %i", #cond, __FILE__, __LINE__)                     \
}                                                                                                     \

/* --- ENUMS --- */
typedef enum {
  CursorPassthrough,
  CursorMove,
  CursorResize,
} rg_cursor_mode;

/* --- STRUCTS --- */

typedef struct rg_client rg_client;

/** 
 * Defines the main state of the compositor. The structure contains core handles that are necassary for 
 * communicating with wayland and using wlroots. Furthermore, it also contains state for the behaviour of window 
 * management, input and compositor related stuff.
 */
typedef struct {
  // Core
  struct wl_display *display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  struct wlr_scene *scene;
  struct wlr_scene_output_layout *scene_layout;
  struct wlr_output_layout *output_layout;
  struct wlr_xdg_shell *xdg_shell;

  // Lists
  struct wl_list keyboards, monitors, clients;

  // Input 
  rg_cursor_mode cursor_mode;
  struct wlr_cursor *cursor;
  struct wlr_xcursor_manager *cursor_mgr;

  struct wlr_seat *seat;
  rg_client *grabbed_client;
  double grab_x, grab_y;
  struct wlr_box grab_geobox;
  uint32_t resize_edges;


  // Callbacks
  struct wl_listener new_xdg_toplevel_cb;
  struct wl_listener new_xdg_popup_cb;
  struct wl_listener cursor_move_cb;
  struct wl_listener cursor_move_absolute_cb;
  struct wl_listener cursor_button_cb;
  struct wl_listener cursor_axis_cb;
  struct wl_listener cursor_frame_cb;
  struct wl_listener new_input_cb;
  struct wl_listener request_cursor_cb;
  struct wl_listener request_set_selection_cb;
  struct wl_listener new_monitor_cb;
} rg_server;

/**
 * Defines a monitor/output device with wayland/wlroots handles 
*/
typedef struct {
  struct wl_list link;
  struct wlr_output *output;
  struct wl_listener frame_cb;
  struct wl_listener request_state_cb;
  struct wl_listener destroy_cb;

  rg_server *server;
} rg_monitor;

/**
 * Defines a xdg toplevel wayland window in the compositor (client window) 
*/
struct rg_client {
  struct wl_list link;
  struct wlr_xdg_toplevel *xdg_toplevel;
  struct wlr_scene_tree *scene_tree;
  struct wl_listener map_cb;
  struct wl_listener unmap_cb;
  struct wl_listener commit_cb;
  struct wl_listener destroy_cb;
  struct wl_listener request_move_cb;
  struct wl_listener request_resize_cb;
  struct wl_listener request_maximize_cb;
  struct wl_listener request_fullscreen_cb;

  rg_server *server;
};

/**
 * Defines a wayland popup window 
*/
typedef struct {
  struct wlr_xdg_popup *xdg_popup;
  struct wl_listener commit_cb;
  struct wl_listener destroy_cb;
} rg_popup_window;

/**
 * Wrapper structure for wayland keyboard devices
*/
typedef struct {
  struct wl_list link;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers_cb;
  struct wl_listener key_cb;
  struct wl_listener destroy_cb;

  rg_server* server;
} rg_keyboard;

/** 
 * Terminates the program by exiting with exit code 1 and logs a given message
 * with wlroots log api on trace level WLR_ERROR
*/
void terminate(const char* msg);
