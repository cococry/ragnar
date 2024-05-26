#include "core.h"
#include "tc_def.h"
#include "listeners.h"
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/util/log.h>

static tc_server server;

void core_init() {
  wlr_log_init(WLR_DEBUG, NULL);

  server.display = wl_display_create();
  server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.display), NULL);
  if(!server.backend)
    terminate("Failed to create wlroots backend.");

  server.renderer = wlr_renderer_autocreate(server.backend);

  wlr_renderer_init_wl_display(server.renderer, server.display);

  server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
  if(!server.allocator)
    terminate("Failed to create wlroots allocator");

  wlr_compositor_create(server.display, 5, server.renderer);
  wlr_subcompositor_create(server.display);
  wlr_data_device_manager_create(server.display);

  server.output_layout = wlr_output_layout_create(server.display);

  wl_list_init(&server.monitors);
  server.new_monitor_cb.notify = new_monitor_listener;
  wl_signal_add(&server.backend->events.new_output, &server.new_monitor_cb);

  server.scene = wlr_scene_create();
  server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

  wl_list_init(&server.clients);
  server.xdg_shell = wlr_xdg_shell_create(server.display, 3);
  server.new_xdg_toplevel_cb.notify = new_xdg_client_listener;
  wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel_cb);
  server.new_xdg_popup_cb.notify = new_xdg_popup_listener;
  wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup_cb);

  server.cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

  server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

  server.cursor_mode = CursorPassthrough;
  server.cursor_move_cb.notify = cursor_move_listener;

  wl_signal_add(&server.cursor->events.motion, &server.cursor_move_cb);
  server.cursor_move_absolute_cb.notify = cursor_move_absolute_listener;
  wl_signal_add(&server.cursor->events.motion_absolute, &server.cursor_move_absolute_cb);
  server.cursor_button_cb.notify = cursor_button_listener;
  wl_signal_add(&server.cursor->events.button, &server.cursor_button_cb);
  server.cursor_axis_cb.notify = cursor_axis_listener;
  wl_signal_add(&server.cursor->events.axis, &server.cursor_axis_cb);
  server.cursor_frame_cb.notify = cursor_frame_listener;
  wl_signal_add(&server.cursor->events.frame, &server.cursor_frame_cb);

  const char *socket = wl_display_add_socket_auto(server.display);
  if (!socket) {
    wlr_backend_destroy(server.backend);
    return;
  }

  tc_assert_exec_msg(wlr_backend_start(server.backend), {
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.display);
  }, "Failed to start wlroots backend.");

  setenv("WAYLAND_DISPLAY", socket, true);
  wlr_log(WLR_INFO, "Running tica on WAYLAND_DISPLAY=%s", socket);
}

void core_run() {
  wl_display_run(server.display);
}

void core_terminate() {
	wl_display_destroy_clients(server.display);
	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wlr_cursor_destroy(server.cursor);
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.display);
}

void core_reset_cursor_mode(tc_server* server) {
  server->cursor_mode = CursorPassthrough;
  server->grabbed_client = NULL;
}

void core_process_cursor_move(tc_server* server, uint32_t time) {
  tc_client* client = server->grabbed_client;
  wlr_scene_node_set_position(&client->scene_tree->node, 
                              server->cursor->x - server->grab_x,
                              server->cursor->y - server->grab_y);
}

void core_interactive_operation(tc_client* client, tc_cursor_mode cursor_mode, uint32_t edges) {
  tc_server* server = client->server;
  struct wlr_surface* focused_surface = server->seat->pointer_state.focused_surface;
  if(client->xdg_toplevel->base->surface != wlr_surface_get_root_surface(focused_surface)) {
    return;
  }
  server->grabbed_client = client;
  server->cursor_mode = cursor_mode;

  if(mode == CursorMove) {
    server->grab_x = server->cursor->x - client->scene_tree->node.x;
    server->grab_y = server->cursor->y - client->scene_tree->node.y;
  } else if(mode == CursorResize) {
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
