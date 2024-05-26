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
void core_process_cursor_move(rg_server* server, uint32_t time);

/*
 * Sets up an interactive more/resize operation. The compositor stops propegating events
 * to client and instead consumes them to move/resize windows.
 * */
void core_interactive_operation(rg_client* client, rg_cursor_mode cursor_mode, uint32_t edges);

