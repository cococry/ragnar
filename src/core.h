#pragma once
#include "def.h"

/*
 * Initializes the ragnar compositor with wayland server & wlroots setup
*/
void core_init();

/*
 * Runs the ragnar compositor by executing the run loop of the wayland display
*/
void core_run();

/*
 * Terminates the ragnar compositor by shutting down the wayland display 
 * and cleaning up allocated memory.
*/
void core_terminate();

/*
 * Resets the current cursormode to CursorPassthrough and resets the grabbed client
 * */
void core_reset_cursor_mode(rg_server* server);

/*
 * Processes cursor movement by moving the grabbed toplevel to the new position
 * */
void core_process_cursor_move_grabbed(rg_server* server, uint32_t time);

/*
 * Processes cursor movement by resizing the grabbed toplevel
 * */
void core_process_cursor_resize_grabbed(rg_server* server, uint32_t time);

/*
 * Processes a cursor motion 
 * */
void core_process_cursor_motion(rg_server* server, uint32_t time);

/*
 * Sets up an interactive more/resize operation. The compositor stops propegating events
 * to client and instead consumes them to move/resize windows.
 * */
void core_interactive_operation(rg_client* client, rg_cursor_mode cursor_mode, uint32_t edges);

/*
 * Returns a rg_client that is underneeth the given coordinates. The x and y coords of the 
 * client and the surface is also returned.
 * */
rg_client* core_get_client_at(rg_server* server, double x, double y, struct wlr_surface** retsurface, double* retx, double* rety);

/*
 * Handles creation of new keyboard device
 * */
void core_handle_new_keyboard(rg_server* server, struct wlr_input_device* device);

/*
 * Handles creation of new pointer device
 * */
void core_handle_new_pointer(rg_server* server, struct wlr_input_device* device);

/*
 * Handles a compositor keybind
 * */
bool core_handle_compositor_keybinding(rg_server* server, xkb_keysym_t key);
