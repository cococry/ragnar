#include "listeners.h"
#include "def.h"
#include "core.h"

#include <assert.h>
#include <stdlib.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <xkbcommon/xkbcommon.h>


/* Monitors | Event Listeners */
static void monitor_frame_listener(struct wl_listener* listener, void* data);
static void monitor_request_state_listener(struct wl_listener* listener, void* data);
static void monitor_destroy_listener(struct wl_listener* listener, void* data);

/* Clients - XDG Toplevels | Event Listeners */
static void client_map_listener(struct wl_listener* listener, void* data);
static void client_unmap_listener(struct wl_listener* listener, void* data);
static void client_commit_listener(struct wl_listener* listener, void* data);
static void client_destroy_listener(struct wl_listener* listener, void* data);
static void client_request_move_listener(struct wl_listener* listener, void* data);
static void client_request_resize_listener(struct wl_listener* listener, void* data);
static void client_request_fullscreen_listener(struct wl_listener* listener, void* data);
static void client_request_maximize_listener(struct wl_listener* listener, void* data);
static void client_focus(rg_client *client, struct wlr_surface *surface);

/* XDG Popups | Event Listeners */
static void xdg_popup_commit_listener(struct wl_listener* listener, void* data);
static void xdg_popup_destroy_listener(struct wl_listener* listener, void* data);


/* ==== PUBLIC FUNCTION DEFINITIONS ==== */
void new_monitor_listener(struct wl_listener* listener, void* data) {
rg_server *server = wl_container_of(listener, server, new_monitor_cb);
  struct wlr_output* output = data;

  wlr_output_init_render(output, server->allocator, server->renderer);

  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  struct wlr_output_mode* mode = wlr_output_preferred_mode(output);
  if(mode) 
    wlr_output_state_set_mode(&state, mode);

  wlr_output_commit_state(output, &state);
  wlr_output_state_finish(&state);

  rg_monitor* monitor = malloc(sizeof(rg_monitor));
  monitor->output = output;
  monitor->server = server;

  bind_listen(monitor_frame_listener, monitor->frame_cb, &output->events.frame); 
  bind_listen(monitor_request_state_listener, monitor->request_state_cb, &output->events.request_state); 
  bind_listen(monitor_destroy_listener, monitor->destroy_cb, &output->events.destroy); 

  wl_list_insert(&server->monitors, &monitor->link);
	struct wlr_output_layout_output *output_layout_output = wlr_output_layout_add_auto(server->output_layout,
		output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, output);
	wlr_scene_output_layout_add_output(server->scene_layout, output_layout_output, scene_output);
}

void cursor_move_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, cursor_move_cb);
  struct wlr_pointer_motion_event* ev = data;
  wlr_cursor_move(server->cursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
  core_process_cursor_motion(server, ev->time_msec);
}

void cursor_move_absolute_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, cursor_move_absolute_cb);
  struct wlr_pointer_motion_absolute_event* ev = data;
  wlr_cursor_warp_absolute(server->cursor, &ev->pointer->base, ev->y, ev->y);
  core_process_cursor_motion(server, ev->time_msec);
}
void cursor_button_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, cursor_button_cb);
  struct wlr_pointer_button_event* ev = data;

  wlr_seat_pointer_notify_button(server->seat, ev->time_msec, ev->button, ev->state);
  double clientx, clienty;
  struct  wlr_surface* client_surface;
  rg_client* client = core_get_client_at(server, server->cursor->x, server->cursor->y, &client_surface, &clientx, &clienty);
  if(ev->state == WL_POINTER_BUTTON_STATE_RELEASED) {
    core_reset_cursor_mode(server);
  } else {
    client_focus(client, client_surface);
  }

}

void cursor_axis_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, cursor_axis_cb);
	struct wlr_pointer_axis_event* ev = data;
  wlr_seat_pointer_notify_axis(server->seat, ev->time_msec, ev->orientation, ev->delta, 
                               ev->delta_discrete, ev->source, ev->relative_direction);
}

void cursor_frame_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_server* server = wl_container_of(listener, server, cursor_frame_cb);
  wlr_seat_pointer_notify_frame(server->seat);
}

void new_xdg_client_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, new_xdg_toplevel_cb);
  struct wlr_xdg_toplevel* xdg_toplevel = data;
  rg_client *client = malloc(sizeof(*client));
  client->server = server;
  client->xdg_toplevel = xdg_toplevel;
  client->scene_tree = wlr_scene_xdg_surface_create(&client->server->scene->tree, xdg_toplevel->base);
  client->scene_tree->node.data = client->scene_tree;

  bind_listen(client_map_listener, client->map_cb, &xdg_toplevel->base->surface->events.map); 
  bind_listen(client_unmap_listener, client->unmap_cb, &xdg_toplevel->base->surface->events.unmap); 
  bind_listen(client_commit_listener, client->commit_cb, &xdg_toplevel->base->surface->events.commit); 
  bind_listen(client_destroy_listener, client->destroy_cb, &xdg_toplevel->events.destroy); 
  bind_listen(client_request_move_listener, client->request_move_cb, &xdg_toplevel->events.request_move); 
  bind_listen(client_request_resize_listener, client->request_resize_cb, &xdg_toplevel->events.request_resize); 
  bind_listen(client_request_fullscreen_listener, client->request_fullscreen_cb, &xdg_toplevel->events.request_fullscreen); 
  bind_listen(client_request_maximize_listener, client->request_maximize_cb, &xdg_toplevel->events.request_maximize); 
}

void new_xdg_popup_listener(struct wl_listener* listener, void* data) {
  (void)listener;
  struct wlr_xdg_popup* xdg_popup_data = data;
  rg_popup_window* popup = malloc(sizeof(*popup));
  popup->xdg_popup = xdg_popup_data;

  struct wlr_xdg_surface* parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup_data->parent);

  assert(parent != NULL && "Trying to add popup without parent surface.");

  struct wlr_scene_tree* parent_tree = parent->data;
  xdg_popup_data->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup_data->base);

  popup->commit_cb.notify = xdg_popup_commit_listener;

  bind_listen(xdg_popup_commit_listener, popup->commit_cb, &xdg_popup_data->base->surface->events.commit); 
  bind_listen(xdg_popup_destroy_listener, popup->destroy_cb, &xdg_popup_data->events.destroy); 
}

void new_input_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, new_input_cb);
  struct wlr_input_device* device = data;
  switch(device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
      core_handle_new_keyboard(server, device);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      core_handle_new_pointer(server, device);
      break;
    default:
      break;
  }
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
  if(!wl_list_empty(&server->keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }
  wlr_seat_set_capabilities(server->seat, caps);
}

void keyboard_modifier_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_keyboard* keyboard = wl_container_of(listener, keyboard, modifiers_cb);
  wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
  wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->wlr_keyboard->modifiers);
}

void keyboard_generic_key_listener(struct wl_listener* listener, void* data) {
  rg_keyboard* keyboard = wl_container_of(listener, keyboard, key_cb);
  rg_server* server = keyboard->server;
  struct wlr_seat* seat = server->seat;

  struct wlr_keyboard_key_event* ev = data;

  uint32_t xkbkey = ev->keycode + 8;
  const xkb_keysym_t* keys;
  int32_t numkeys = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, xkbkey, &keys);
  
  bool handledkey = false;
  uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);

  if((modifiers & WLR_MODIFIER_ALT) && ev->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
    for(int32_t i = 0 ; i < numkeys; i++) {
      handledkey = core_handle_compositor_keybinding(server, keys[i]);
    }
  }
  if(!handledkey) {
    wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_key(seat, ev->time_msec, ev->keycode, ev->state);
  }
}

void keyboard_destroy_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_keyboard* keyboard = wl_container_of(listener, keyboard, destroy_cb);
	wl_list_remove(&keyboard->modifiers_cb.link);
	wl_list_remove(&keyboard->key_cb.link);
	wl_list_remove(&keyboard->destroy_cb.link);
	wl_list_remove(&keyboard->link);
  free(keyboard);
}

/* ==== STATIC FUNCTION DEFINITIONS ==== */

void monitor_frame_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_monitor* monitor = wl_container_of(listener, monitor, frame_cb);
  struct wlr_scene* scene = monitor->server->scene;
  struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(scene, monitor->output);
  wlr_scene_output_commit(scene_output, NULL);
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_scene_output_send_frame_done(scene_output, &now);
}

void monitor_request_state_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_monitor* monitor = wl_container_of(listener, monitor, request_state_cb);
  const struct wlr_output_event_request_state *ev = data;
  wlr_output_commit_state(monitor->output, ev->state);
}

void monitor_destroy_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_monitor *monitor = wl_container_of(listener, monitor, destroy_cb);

  wl_list_remove(&monitor->frame_cb.link);
  wl_list_remove(&monitor->request_state_cb.link);
  wl_list_remove(&monitor->destroy_cb.link);
  wl_list_remove(&monitor->link);
}
void client_map_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, map_cb);
  wl_list_insert(&client->server->clients, &client->link);
  client_focus(client, client->xdg_toplevel->base->surface);
}

void client_unmap_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, unmap_cb);
  rg_server* server = client->server;
  if(client == server->grabbed_client) {
    core_reset_cursor_mode(server);
  }
  wl_list_remove(&client->link);

}

void client_commit_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, commit_cb);
  if(client->xdg_toplevel->base->initial_commit) {
    wlr_xdg_toplevel_set_size(client->xdg_toplevel, 0, 0);
  }
}

void client_destroy_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, destroy_cb);

  wl_list_remove(&client->map_cb.link);
  wl_list_remove(&client->unmap_cb.link);
  wl_list_remove(&client->commit_cb.link);
  wl_list_remove(&client->destroy_cb.link);
  wl_list_remove(&client->request_move_cb.link);
  wl_list_remove(&client->request_resize_cb.link);
  wl_list_remove(&client->request_fullscreen_cb.link);

  free(client);
}

void client_request_move_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, request_move_cb);
  core_interactive_operation(client, CursorMove, 0);
}

void client_request_resize_listener(struct wl_listener* listener, void* data) {
  struct wlr_xdg_toplevel_resize_event* ev = data;
  rg_client* client = wl_container_of(listener, client, request_resize_cb);
  core_interactive_operation(client, CursorResize, ev->edges);
}

void client_request_fullscreen_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, request_fullscreen_cb);
  if(client->xdg_toplevel->base->initialized) {
    wlr_xdg_surface_schedule_configure(client->xdg_toplevel->base);
  }
}
void client_request_maximize_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_client* client = wl_container_of(listener, client, request_maximize_cb);
  if(client->xdg_toplevel->base->initialized) {
    wlr_xdg_surface_schedule_configure(client->xdg_toplevel->base);
  }
}

void client_focus(rg_client *client, struct wlr_surface *surface) {
  if(!client) return;

  rg_server *server = client->server;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
  if(prev_surface == surface) return;

  if(prev_surface) {
    struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
    if(prev_toplevel) {
      wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }
  }

  wlr_scene_node_raise_to_top(&client->scene_tree->node);
  wl_list_remove(&client->link);
  wl_list_insert(&server->clients, &client->link);

  wlr_xdg_toplevel_set_activated(client->xdg_toplevel, true);

  struct wlr_keyboard *kb = wlr_seat_get_keyboard(seat);
  if(kb) {
    wlr_seat_keyboard_notify_enter(seat, client->xdg_toplevel->base->surface,
                                   kb->keycodes, kb->num_keycodes, &kb->modifiers);
  }
}

void xdg_popup_commit_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_popup_window* popup = wl_container_of(listener, popup, commit_cb);

  if(popup->xdg_popup->base->initial_commit) {
    wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
  }
}

void xdg_popup_destroy_listener(struct wl_listener* listener, void* data) {
  (void)data;
  rg_popup_window* popup = wl_container_of(listener, popup, destroy_cb);

  wl_list_remove(&popup->commit_cb.link);
  wl_list_remove(&popup->destroy_cb.link);

  free(popup);
}

void seat_request_cursor_listener(struct wl_listener* listener, void* data) {
  rg_server* server = wl_container_of(listener, server, request_cursor_cb);
  struct wlr_seat_pointer_request_set_cursor_event *event = data;
  struct wlr_seat_client *focused_client =
    server->seat->pointer_state.focused_client;
  if (focused_client == event->seat_client) {
    wlr_cursor_set_surface(server->cursor, event->surface,
                           event->hotspot_x, event->hotspot_y);
  }
}

void seat_request_set_selection_listener(struct wl_listener* listener, void* data) {
	rg_server* server = wl_container_of(listener, server, request_set_selection_cb);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}
