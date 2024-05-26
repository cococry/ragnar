#pragma once
#include "tc_def.h"

/*
 * Initializes the tica compositor with wayland server & wlroots setup
*/
void core_init();

/*
 * Runs the tica compositor by executing the run loop of the wayland display
*/
void core_run();

/*
 * Terminates the tica compositor by shutting down the wayland display 
 * and cleaning up allocated memory.
*/
void core_terminate();

/*
 * Resets the current cursormode to CursorPassthrough and resets the grabbed client
 * */
void core_reset_cursor_mode(tc_server* server);

/*
 * Processes cursor movement by moving the grabbed toplevel to the new position
 * */
void core_process_cursor_move(tc_server* server, uint32_t time);

/*
 * Sets up an interactive more/resize operation. The compositor stops propegating events
 * to client and instead consumes them to move/resize windows.
 * */
void core_interactive_operation(tc_client* client, tc_cursor_mode cursor_mode, uint32_t edges);

