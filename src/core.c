#include "core.h"
#include "def.h"
#include "listeners.h"

#include <unistd.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon-keysyms.h>

static rg_server g_server;

void core_init() {
  wlr_log_init(WLR_DEBUG, NULL);

  g_server.display = wl_display_create();
  g_server.backend = wlr_backend_autocreate(wl_display_get_event_loop(g_server.display), NULL);
  if(!g_server.backend)
    terminate("Failed to create wlroots backend.");

  g_server.renderer = wlr_renderer_autocreate(g_server.backend);

  wlr_renderer_init_wl_display(g_server.renderer, g_server.display);

  g_server.allocator = wlr_allocator_autocreate(g_server.backend, g_server.renderer);
  if(!g_server.allocator)
    terminate("Failed to create wlroots allocator");

  wlr_compositor_create(g_server.display, 5, g_server.renderer);
  wlr_subcompositor_create(g_server.display);
  wlr_data_device_manager_create(g_server.display);

  g_server.output_layout = wlr_output_layout_create(g_server.display);

  wl_list_init(&g_server.monitors);
  bind_listen(new_monitor_listener, g_server.new_monitor_cb, &g_server.backend->events.new_output); 

  g_server.scene = wlr_scene_create();
  g_server.scene_layout = wlr_scene_attach_output_layout(g_server.scene, g_server.output_layout);

  wl_list_init(&g_server.clients);
  g_server.xdg_shell = wlr_xdg_shell_create(g_server.display, 3);
  bind_listen(new_xdg_client_listener, g_server.new_xdg_toplevel_cb, &g_server.xdg_shell->events.new_toplevel); 
  bind_listen(new_xdg_popup_listener, g_server.new_xdg_popup_cb, &g_server.xdg_shell->events.new_popup); 

  g_server.cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(g_server.cursor, g_server.output_layout);

  g_server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
  
  g_server.cursor_mode = CursorPassthrough;
  bind_listen(cursor_move_listener, g_server.cursor_move_cb, &g_server.cursor->events.motion); 
  bind_listen(cursor_move_absolute_listener, g_server.cursor_move_absolute_cb, &g_server.cursor->events.motion_absolute); 
  bind_listen(cursor_button_listener, g_server.cursor_button_cb, &g_server.cursor->events.button); 
  bind_listen(cursor_axis_listener, g_server.cursor_axis_cb, &g_server.cursor->events.axis); 
  bind_listen(cursor_frame_listener, g_server.cursor_frame_cb, &g_server.cursor->events.frame); 

  wl_list_init(&g_server.keyboards);
  bind_listen(new_input_listener, g_server.new_input_cb, &g_server.backend->events.new_input); 

	g_server.seat = wlr_seat_create(g_server.display, "seat0");

  bind_listen(seat_request_cursor_listener, g_server.request_cursor_cb, &g_server.seat->events.request_set_cursor); 
  bind_listen(seat_request_set_selection_listener, g_server.request_set_selection_cb, &g_server.seat->events.request_set_selection); 

  const char *socket = wl_display_add_socket_auto(g_server.display);
  if (!socket) {
    wlr_backend_destroy(g_server.backend);
    return;
  }

  rg_assert_exec_msg(wlr_backend_start(g_server.backend), {
    wlr_backend_destroy(g_server.backend);
    wl_display_destroy(g_server.display);
  }, "Failed to start wlroots backend.");

  setenv("WAYLAND_DISPLAY", socket, true);
  if (fork() == 0) {
    execl("/bin/sh", "/bin/sh", "-c", "alacritty", (void *)NULL);
  }
  wlr_log(WLR_INFO, "Running tica on WAYLAND_DISPLAY=%s", socket);
}

void core_run() {
  wl_display_run(g_server.display);
}

void core_terminate() {
	wl_display_destroy_clients(g_server.display);
	wlr_scene_node_destroy(&g_server.scene->tree.node);
	wlr_xcursor_manager_destroy(g_server.cursor_mgr);
	wlr_cursor_destroy(g_server.cursor);
	wlr_allocator_destroy(g_server.allocator);
	wlr_renderer_destroy(g_server.renderer);
	wlr_backend_destroy(g_server.backend);
	wl_display_destroy(g_server.display);
}

void core_reset_cursor_mode(rg_server* server) {
  server->cursor_mode = CursorPassthrough;
  server->grabbed_client = NULL;
}

void core_process_cursor_move_grabbed(rg_server* server, uint32_t time) {
  (void)time;
  rg_client* client = server->grabbed_client;
  wlr_scene_node_set_position(&client->scene_tree->node, 
                              server->cursor->x - server->grab_x,
                              server->cursor->y - server->grab_y);
}

void core_process_cursor_resize_grabbed(rg_server* server, uint32_t time) {
  (void)time;
	rg_client* client = server->grabbed_client;
	double border_x = server->cursor->x - server->grab_x;
	double border_y = server->cursor->y - server->grab_y;
	int new_left = server->grab_geobox.x;
	int new_right = server->grab_geobox.x + server->grab_geobox.width;
	int new_top = server->grab_geobox.y;
	int new_bottom = server->grab_geobox.y + server->grab_geobox.height;

	if (server->resize_edges & WLR_EDGE_TOP) {
		new_top = border_y;
		if (new_top >= new_bottom) {
			new_top = new_bottom - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_BOTTOM) {
		new_bottom = border_y;
		if (new_bottom <= new_top) {
			new_bottom = new_top + 1;
		}
	}
	if (server->resize_edges & WLR_EDGE_LEFT) {
		new_left = border_x;
		if (new_left >= new_right) {
			new_left = new_right - 1;
		}
	} else if (server->resize_edges & WLR_EDGE_RIGHT) {
		new_right = border_x;
		if (new_right <= new_left) {
			new_right = new_left + 1;
		}
	}

	struct wlr_box geo_box;
	wlr_xdg_surface_get_geometry(client->xdg_toplevel->base, &geo_box);
	wlr_scene_node_set_position(&client->scene_tree->node,
		new_left - geo_box.x, new_top - geo_box.y);

	int new_width = new_right - new_left;
	int new_height = new_bottom - new_top;
	wlr_xdg_toplevel_set_size(client->xdg_toplevel, new_width, new_height);
}

void core_process_cursor_motion(rg_server* server, uint32_t time) {
  if(server->cursor_mode == CursorMove) {
    core_process_cursor_move_grabbed(server, time);
    return;
  } else if(server->cursor_mode == CursorResize) {
    core_process_cursor_resize_grabbed(server, time);
    return;
  }

  double clientx, clienty;
  struct wlr_surface* client_surface = NULL;
  struct wlr_seat* seat = server->seat;
  rg_client* client = core_get_client_at(server, server->cursor->x, server->cursor->y, &client_surface, &clientx, &clienty);
  if(!client) {
    wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
  }
  if(client_surface) {
    wlr_seat_pointer_notify_enter(seat, client_surface, clientx, clienty);
    wlr_seat_pointer_notify_motion(seat, time, clientx, clienty);
  } else {
    wlr_seat_pointer_clear_focus(seat);
  }
}

void core_interactive_operation(rg_client* client, rg_cursor_mode cursor_mode, uint32_t edges) {
  rg_server* server = client->server;
  struct wlr_surface* focused_surface = server->seat->pointer_state.focused_surface;
  if(client->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
    return;
  }
  server->grabbed_client = client;
  server->cursor_mode = cursor_mode;

  if(cursor_mode == CursorMove) {
    server->grab_x = server->cursor->x - client->scene_tree->node.x;
    server->grab_y = server->cursor->y - client->scene_tree->node.y;
  } else if(cursor_mode == CursorResize) {
    struct wlr_box geometry;
    wlr_xdg_surface_get_geometry(client->xdg_toplevel->base, &geometry);

    double borderx = (client->scene_tree->node.x + geometry.x) + ((edges & WLR_EDGE_RIGHT) ? geometry.width : 0);
    double bordery = (client->scene_tree->node.y + geometry.y) + ((edges & WLR_EDGE_BOTTOM) ? geometry.height : 0);

    server->grab_geobox = geometry;
    server->grab_x = server->cursor->x - borderx;
    server->grab_y = server->cursor->x - bordery;
    server->resize_edges = edges;
  }
}

rg_client* core_get_client_at(rg_server* server, double x, double y, struct wlr_surface** retsurface, double* retx, double* rety) {
  struct wlr_scene_node* node = wlr_scene_node_at(&server->scene->tree.node, x, y, retx, rety);
  if(!node || node->type != WLR_SCENE_NODE_BUFFER) return NULL;

  struct wlr_scene_buffer* scene_buffer = wlr_scene_buffer_from_node(node);
  struct wlr_scene_surface* scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
  if(!scene_surface) return NULL;

  *retsurface = scene_surface->surface;
  struct wlr_scene_tree* tree = node->parent;
  while(tree && !tree->node.data) {
    tree = tree->node.parent;
  }
  return tree->node.data;
}

void core_handle_new_keyboard(rg_server* server, struct wlr_input_device* device) {
  struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	rg_keyboard* keyboard = malloc(sizeof(*keyboard));
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(wlr_keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

  bind_listen(keyboard_modifier_listener, keyboard->modifiers_cb, &wlr_keyboard->events.modifiers);
  bind_listen(keyboard_generic_key_listener, keyboard->key_cb, &wlr_keyboard->events.key);
  bind_listen(keyboard_destroy_listener, keyboard->destroy_cb, &device->events.destroy);

  wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
  wl_list_insert(&server->keyboards, &keyboard->link);
}

void core_handle_new_pointer(rg_server* server, struct wlr_input_device* device) {
  wlr_cursor_attach_input_device(server->cursor, device);
}

bool core_handle_compositor_keybinding(rg_server* server, xkb_keysym_t key) {
  switch(key) {
    case XKB_KEY_Escape:
      wl_display_terminate(server->display);
      break;
    default:
      return false;
  }
  return true;
}
