#pragma once
#include <wayland-server-core.h>

/**
 * Listener function that fires when the wayland server gets a new 
 * avaiable output/monitor 
*/
void new_monitor_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when the wayland server dispatches a 
 * cursor motion event.
*/
void cursor_move_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when the wayland server dispatches a 
 * cursor motion event. The cursor position of the move is treated 
 * absolute.
*/
void cursor_move_absolute_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when a button event on a cursor input device
 * is dispatched
*/
void cursor_button_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when a axis event on a cursor input device
 * is dispatched. An example of an axis event is scrolling the mousewheel. 
*/
void cursor_axis_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when a frame event on a cursor input device
 * is dispatched. Frame events are sent after regular pointer events to group
 * multiple events together.
*/
void cursor_frame_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when a new xdg toplevel window (client window)
 * is created on the wayland server.
*/
void new_xdg_client_listener(struct wl_listener* listener, void* data);

/**
 * Listener function that fires when a new xdg popup is created on the wayland 
 * server.
*/
void new_xdg_popup_listener(struct wl_listener* listener, void* data);
