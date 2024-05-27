
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
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

#define bind_listen(_cb, _to, _handle) {  \
  _to.notify = _cb;                       \
  wl_signal_add(_handle, &_to);           \
}

typedef enum {
	CursorModeNone,
  CursorModeMove,
  CursorModeResize
} cursor_mode;

typedef struct clientwin clientwin;

typedef struct {	
	struct wl_list clients, keyboards, mons;

	struct wlr_cursor* cursor;
	struct wlr_xcursor_manager* cursormgr;

	struct wlr_seat *seat;

	struct wl_display* display;
	struct wlr_backend* backend;
	struct wlr_renderer* renderer;
	struct wlr_allocator* allocator;
	struct wlr_scene* scene;
	struct wlr_xdg_shell* xdgsh;

	struct wlr_scene_output_layout* scenelayout;
	struct wlr_output_layout* monlayout;

	struct wl_listener xdg_client_cb;
	struct wl_listener xdg_popup_xb;

	struct wl_listener curmove_cb;
	struct wl_listener curmove_abs_cb;
	struct wl_listener cur_btn_cb;
	struct wl_listener cur_axis_cb;
	struct wl_listener cur_frame_cb;

	struct wl_listener input_cb;
	struct wl_listener reqcur_cb;
	struct wl_listener reqsetsel_cb;

	struct wl_listener mon_cb;

  cursor_mode curmode;
	struct wlr_box grabgeo;
	double grabx, graby;
	clientwin* grabclient;
	uint32_t resize_edges;
} wm_state;

typedef struct {
	struct wl_list link;
	wm_state* state;
	struct wlr_output* wlroutput;
	struct wl_listener frame_cb;
	struct wl_listener reqstate_cb;
	struct wl_listener destroy_cb;
} monitor;

struct clientwin {
	wm_state* state;

	struct wl_list link;
	struct wlr_xdg_toplevel* xdgtoplevel;
	struct wlr_scene_tree* scenetree;

	struct wl_listener map_cb;
	struct wl_listener unmap_cb;
	struct wl_listener commit_cb;
	struct wl_listener destroy_cb;
	struct wl_listener reqmove_cb;
	struct wl_listener reqresize_cb;
	struct wl_listener reqmaximize_cb;
	struct wl_listener reqfullscreen_cb;
};

typedef struct {
	struct wlr_xdg_popup* xdg_popup;
	struct wl_listener commit_cb;
	struct wl_listener destroy_cb;
} popupwin;

typedef struct {
	wm_state* state;

	struct wl_list link;
	struct wlr_keyboard* wlrkb;

	struct wl_listener mod_cb;
	struct wl_listener key_cb;
	struct wl_listener destroy_cb;
} keyboard_device;

static void         clientfocus(clientwin* client, struct wlr_surface *surface);

static bool         handlekeybind(wm_state* state, xkb_keysym_t key);
static void         kbmods(struct wl_listener *listener, void *data);
static void         kbkey(struct wl_listener *listener, void *data);
static void         kbdestroy(struct wl_listener *listener, void *data);
static void         kbnew(wm_state *state, struct wlr_input_device *device);

static void         pointernew(wm_state* state, struct wlr_input_device *device);

static void         inputnew(struct wl_listener *listener, void *data);

static void         seatreqcur(struct wl_listener *listener, void *data);
static void         seatreqsetsel(struct wl_listener *listener, void *data);

static clientwin*   clientwinat(wm_state* state, double x, double y, struct wlr_surface **surface, double *retx, double *rety);

static void         curmodereset(wm_state* state);

static void         movegrab(wm_state* state, uint32_t time);
static void         resizegrab(wm_state* state, uint32_t time);
static void         curmotion(wm_state* state, uint32_t time);

static void         curmove(struct wl_listener *listener, void *data);
static void         curmoveabs(struct wl_listener *listener, void *data);
static void         curbtn(struct wl_listener *listener, void *data);
static void         curaxis(struct wl_listener *listener, void *data);
static void         curframe(struct wl_listener *listener, void *data);

static void         monframe(struct wl_listener *listener, void *data);
static void         monreqstate(struct wl_listener *listener, void *data);
static void         mondestroy(struct wl_listener *listener, void *data);
static void         monnew(struct wl_listener *listener, void *data);

static void         clientmap(struct wl_listener *listener, void *data);
static void         clientunmap(struct wl_listener *listener, void *data);
static void         clientcommit(struct wl_listener *listener, void *data);
static void         clientdestroy(struct wl_listener *listener, void *data);

static void         interactive(clientwin* client, cursor_mode mode, uint32_t edges);

static void         clientreqmove(struct wl_listener *listener, void *data);
static void         clientreqresize(struct wl_listener *listener, void *data);
static void         clientreqmaximize(struct wl_listener *listener, void *data);
static void         clientreqfullscreen(struct wl_listener *listener, void *data);
static void         clientnew(struct wl_listener *listener, void *data);

static void         popupcommit(struct wl_listener *listener, void *data);
static void         popupdestroy(struct wl_listener *listener, void *data);
static void         popupnew(struct wl_listener *listener, void *data);

static void         ragnar_init(wm_state* state);
static void         ragnar_run(wm_state* state);
static void         ragnar_terminate(wm_state* state);

void clientfocus(clientwin* client, struct wlr_surface *surface) {
	if (!client) return;

	wm_state* state = client->state;
	struct wlr_seat *seat = state->seat;

	struct wlr_surface *prev = seat->keyboard_state.focused_surface;
	if (prev == surface) return;

  // Deactive previous toplevel
	if (prev) {
		struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev);
		if (prev_toplevel) 
      wlr_xdg_toplevel_set_activated(prev_toplevel, false);
	}

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);

  // Raise & activate client
	wlr_scene_node_raise_to_top(&client->scenetree->node);
	wl_list_remove(&client->link);
	wl_list_insert(&state->clients, &client->link);
	wlr_xdg_toplevel_set_activated(client->xdgtoplevel, true);

  // Raise keyboard enter event to get keyboard focus on the client
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, client->xdgtoplevel->base->surface, keyboard->keycodes, 
                                 keyboard->num_keycodes, &keyboard->modifiers);
	}
}

void kbmods(struct wl_listener* listener, void *data) {
	keyboard_device* kb = wl_container_of(listener, kb, mod_cb);
  // Set the seats current keyboard
	wlr_seat_set_keyboard(kb->state->seat, kb->wlrkb);

	wlr_seat_keyboard_notify_modifiers(kb->state->seat,
		&kb->wlrkb->modifiers);
}

bool handlekeybind(wm_state* state, xkb_keysym_t key) {
	switch (key) {
	case XKB_KEY_Escape:
		wl_display_terminate(state->display);
		break;
	case XKB_KEY_Tab:
		if (wl_list_length(&state->clients) < 2) {
			break;
		}
		clientwin* next_cylce = wl_container_of(state->clients.prev, next_cylce, link);
		clientfocus(next_cylce, next_cylce->xdgtoplevel->base->surface);
		break;
	default:
		return false;
	}
	return true;
}

void kbkey(struct wl_listener *listener, void *data) {
	keyboard_device* kb = wl_container_of(listener, kb, key_cb);
	wm_state* state = kb->state;

	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = state->seat;

  // Translate key from libinput to xkb
	uint32_t key = event->keycode + 8;
	const xkb_keysym_t *keys;

	int32_t numkeys = xkb_state_key_get_syms(kb->wlrkb->xkb_state, key, &keys);

	bool consumed = false;
	uint32_t modifiers = wlr_keyboard_get_modifiers(kb->wlrkb);
	if ((modifiers & WLR_MODIFIER_ALT) &&
			event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (int i = 0; i < numkeys; i++) {
			consumed = handlekeybind(state, keys[i]);
		}
	}

  // If the key was not consumed by the compositors keybindings, pass it to the client
	if (!consumed) {
		wlr_seat_set_keyboard(seat, kb->wlrkb);
		wlr_seat_keyboard_notify_key(seat, event->time_msec,
			event->keycode, event->state);
	}
}

void kbdestroy(struct wl_listener *listener, void *data) {
	keyboard_device* kb = wl_container_of(listener, kb, destroy_cb);
	wl_list_remove(&kb->mod_cb.link);
	wl_list_remove(&kb->key_cb.link);
	wl_list_remove(&kb->destroy_cb.link);
	wl_list_remove(&kb->link);
	free(kb);
}

void kbnew(wm_state *state, struct wlr_input_device *device) {
	struct wlr_keyboard* wlrkb = wlr_keyboard_from_input_device(device);

	keyboard_device* kb = malloc(sizeof(keyboard_device));
	kb->state = state;
	kb->wlrkb = wlrkb;

  // Creating + setting keymap and context for the keyboard
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlrkb, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

  // Setup repeat info
	wlr_keyboard_set_repeat_info(wlrkb, 25, 600);

  // Setup event listeners
  bind_listen(kbmods, kb->mod_cb, &wlrkb->events.modifiers);
  bind_listen(kbkey, kb->key_cb, &wlrkb->events.key);
  bind_listen(kbdestroy, kb->destroy_cb, &device->events.destroy);

  // Set the seat's keyboard
	wlr_seat_set_keyboard(state->seat, kb->wlrkb);

  // Add the keyboard to the list of keyboards
	wl_list_insert(&state->keyboards, &kb->link);
}

void pointernew(wm_state* state, struct wlr_input_device *device) {
	wlr_cursor_attach_input_device(state->cursor, device);
}

void inputnew(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, input_cb);
	struct wlr_input_device *device = data;

  // Handle creation of devices
  switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
      kbnew(state, device);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      pointernew(state, device);
      break;
    default:
      break;
  }

  // Set seat capabilities
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&state->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(state->seat, caps);
}

void seatreqcur(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, reqcur_cb);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client = state->seat->pointer_state.focused_client;
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(state->cursor, event->surface, event->hotspot_x, event->hotspot_y);
	}
}

void seatreqsetsel(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, reqsetsel_cb);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(state->seat, event->source, event->serial);
}

clientwin* clientwinat(wm_state* state, double x, double y, struct wlr_surface **surface, 
                                                   double *retx, double *rety) {
  // Get the scene node at the given position
	struct wlr_scene_node* node = wlr_scene_node_at(&state->scene->tree.node, x, y, retx, rety);

  // Return if the node is invalid
	if (!node || node->type != WLR_SCENE_NODE_BUFFER) return NULL;

	struct wlr_scene_buffer* scenebuf = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface* scenesurface =
		wlr_scene_surface_try_from_buffer(scenebuf);

	if (!scenesurface) {
		return NULL;
	}

	*surface = scenesurface->surface;

  // Retrive the corresponding client to the node
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && !tree->node.data) {
		tree = tree->node.parent;
	}
	return (clientwin*)tree->node.data;
}

void curmodereset(wm_state* state) {
	state->curmode = CursorModeNone;
	state->grabclient = NULL;
}

void movegrab(wm_state* state, uint32_t time) {
  // Move grabbed client
	wlr_scene_node_set_position(&state->grabclient->scenetree->node,
		state->cursor->x - state->grabx,
		state->cursor->y - state->graby);
}

void resizegrab(wm_state* state, uint32_t time) {
  (void)state;
  (void)time;
}

void curmotion(wm_state* state, uint32_t time) {
	if (state->curmode == CursorModeMove) {
		movegrab(state, time);
		return;
	} else if (state->curmode == CursorModeResize) {
		resizegrab(state, time);
		return;
	}

	double clientx, clienty;
	struct wlr_surface* clientsurface = NULL;

	struct wlr_seat* seat = state->seat;

	clientwin* client = clientwinat(state, state->cursor->x, state->cursor->y, &clientsurface, &clientx, &clienty);

  // If there is no client under the cursor, set the cursor to the default cursor
	if (!client) {
		wlr_cursor_set_xcursor(state->cursor, state->cursormgr, "default");
	}

	if (clientsurface) {
    // If there is a surface associated with the client, send motion and enter events
    // wlroots does not send duplicate enter/motiton events.
		wlr_seat_pointer_notify_enter(seat, clientsurface, clientx, clienty);
		wlr_seat_pointer_notify_motion(seat, time, clientx, clienty);
	} else {
    // Clear the focus so future events are not sent to the last client
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void curmove(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, curmove_cb);
	struct wlr_pointer_motion_event* ev = data;
	wlr_cursor_move(state->cursor, &ev->pointer->base, ev->delta_x, ev->delta_y);
	curmotion(state, ev->time_msec);
}

void curmoveabs(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, curmove_abs_cb);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(state->cursor, &event->pointer->base, event->x, event->y);
	curmotion(state, event->time_msec);
}

void curbtn(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, cur_btn_cb);
	struct wlr_pointer_button_event *event = data;

	wlr_seat_pointer_notify_button(state->seat, event->time_msec, event->button, event->state);
	double clientx, clienty;
	struct wlr_surface* clientsurface = NULL;

  // Retrieve client underneath the cursor
	clientwin* client = clientwinat(state, state->cursor->x, state->cursor->y, &clientsurface, &clientx, &clienty);

	if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
    // Reset cursor mode if a button was released
		curmodereset(state);
	} else if(event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
    // Focus client if button was pressed 
		clientfocus(client, clientsurface);
	}
}

void curaxis(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, cur_axis_cb);
	struct wlr_pointer_axis_event* ev = data;
	wlr_seat_pointer_notify_axis(state->seat, ev->time_msec, ev->orientation, ev->delta, ev->delta_discrete, 
                              ev->source, ev->relative_direction);
}

void curframe(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, cur_frame_cb);
	wlr_seat_pointer_notify_frame(state->seat);
}

void monframe(struct wl_listener *listener, void *data) {
	monitor* mon = wl_container_of(listener, mon, frame_cb);
	struct wlr_scene* scene = mon->state->scene;

	struct wlr_scene_output* scene_output = wlr_scene_get_scene_output(scene, mon->wlroutput);
	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

void monreqstate(struct wl_listener *listener, void *data) {
	monitor* mon = wl_container_of(listener, mon, reqstate_cb);
	const struct wlr_output_event_request_state *ev = data;
	wlr_output_commit_state(mon->wlroutput, ev->state);
}

void mondestroy(struct wl_listener *listener, void *data) {
	monitor* mon = wl_container_of(listener, mon, destroy_cb);
	wl_list_remove(&mon->frame_cb.link);
	wl_list_remove(&mon->reqstate_cb.link);
	wl_list_remove(&mon->destroy_cb.link);
	wl_list_remove(&mon->link);
	free(mon);
}

void monnew(struct wl_listener *listener, void *data) {
	wm_state* state = wl_container_of(listener, state, mon_cb);
	struct wlr_output* wlroutput = data;

  // Initializing the renderer for the output
	wlr_output_init_render(wlroutput, state->allocator, state->renderer);

  // Switch monitor on
	struct wlr_output_state monstate;
	wlr_output_state_init(&monstate);
	wlr_output_state_set_enabled(&monstate, true);

  // Set the monitors mode to the preferred mode
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlroutput);
	if (mode) {
		wlr_output_state_set_mode(&monstate, mode);
	}
	wlr_output_commit_state(wlroutput, &monstate);
	wlr_output_state_finish(&monstate);

	monitor* mon = malloc(sizeof(monitor));
	mon->wlroutput = wlroutput;
	mon->state = state;

  // Set up event listners
  bind_listen(monframe, mon->frame_cb, &wlroutput->events.frame); 
  bind_listen(monreqstate, mon->reqstate_cb, &wlroutput->events.request_state); 
  bind_listen(mondestroy, mon->destroy_cb, &wlroutput->events.destroy); 

  // Add the monitor to the list of monitors
	wl_list_insert(&state->mons, &mon->link);

  // Add the monitor to the monitor layout. add_auto automatically 
  // adds the monitor so that the monitor layout is in the order they appear from left 
  // to right.
	struct wlr_output_layout_output* moninlayout = wlr_output_layout_add_auto(state->monlayout, wlroutput);
	struct wlr_scene_output *sceneoutput = wlr_scene_output_create(state->scene, wlroutput);
	wlr_scene_output_layout_add_output(state->scenelayout, moninlayout, sceneoutput);
}

void clientmap(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, map_cb);
	wl_list_insert(&client->state->clients, &client->link);
	clientfocus(client, client->xdgtoplevel->base->surface);
}

void clientunmap(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, unmap_cb);

	if (client == client->state->grabclient) {
		curmodereset(client->state);
	}

	wl_list_remove(&client->link);
}

void clientcommit(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, commit_cb);

	if (client->xdgtoplevel->base->initial_commit) {
		wlr_xdg_toplevel_set_size(client->xdgtoplevel, 0, 0);
	}
}

void clientdestroy(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, destroy_cb);
	wl_list_remove(&client->map_cb.link);
	wl_list_remove(&client->unmap_cb.link);
	wl_list_remove(&client->commit_cb.link);
	wl_list_remove(&client->destroy_cb.link);
	wl_list_remove(&client->reqmove_cb.link);
	wl_list_remove(&client->reqresize_cb.link);
	wl_list_remove(&client->reqmaximize_cb.link);
	wl_list_remove(&client->reqfullscreen_cb.link);
	free(client);
}

void interactive(clientwin* client, cursor_mode mode, uint32_t edges) {
	wm_state* state = client->state;
	struct wlr_surface* focused = state->seat->pointer_state.focused_surface;
  // Don't handle move/resize events from unfocused clients 
	if (client->xdgtoplevel->base->surface != wlr_surface_get_root_surface(focused)) return;

	state->grabclient = client;
	state->curmode = mode;

	if (mode == CursorModeMove) {
		state->grabx = state->cursor->x - client->scenetree->node.x;
		state->graby = state->cursor->y - client->scenetree->node.y;
	} else {
		struct wlr_box geo;
		wlr_xdg_surface_get_geometry(client->xdgtoplevel->base, &geo);

		double borderx = (client->scenetree->node.x + geo.x) + ((edges & WLR_EDGE_RIGHT) ? geo.width : 0);
		double bordery = (client->scenetree->node.y + geo.y) + ((edges & WLR_EDGE_BOTTOM) ? geo.height : 0);

		state->grabx = state->cursor->x - borderx;
		state->graby = state->cursor->y - bordery;

		state->grabgeo = geo;
		state->grabgeo.x += client->scenetree->node.x;
		state->grabgeo.y += client->scenetree->node.y;

		state->resize_edges = edges;
	}
}

void clientreqmove(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, reqmove_cb);
	interactive(client, CursorModeMove, 0);
}

void clientreqresize(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_resize_event *event = data;
	clientwin* client = wl_container_of(listener, client, reqresize_cb);
	interactive(client, CursorModeResize, event->edges);
}

void clientreqmaximize(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, reqmaximize_cb);
	if (client->xdgtoplevel->base->initialized) {
    // Don't do anything
		wlr_xdg_surface_schedule_configure(client->xdgtoplevel->base);
	}
}

void clientreqfullscreen(struct wl_listener *listener, void *data) {
	clientwin* client = wl_container_of(listener, client, reqfullscreen_cb);
	if (client->xdgtoplevel->base->initialized) {
    // Don't do anything
		wlr_xdg_surface_schedule_configure(client->xdgtoplevel->base);
	}
}

void clientnew(struct wl_listener *listener, void *data) {
  wm_state* state = wl_container_of(listener, state, xdg_client_cb);
	struct wlr_xdg_toplevel* xdgtoplevel = data;

  // Create the client
	clientwin* client = malloc(sizeof(clientwin));
	client->state = state;
	client->xdgtoplevel = xdgtoplevel;
	client->scenetree = wlr_scene_xdg_surface_create(&client->state->scene->tree, xdgtoplevel->base);
	client->scenetree->node.data = client;
	xdgtoplevel->base->data = client->scenetree;

  // Set up event listeners
  bind_listen(clientmap, client->map_cb, &xdgtoplevel->base->surface->events.map);
  bind_listen(clientunmap, client->unmap_cb, &xdgtoplevel->base->surface->events.unmap);
  bind_listen(clientcommit, client->commit_cb, &xdgtoplevel->base->surface->events.commit);
  bind_listen(clientdestroy, client->commit_cb, &xdgtoplevel->events.destroy);
  bind_listen(clientreqmove, client->reqmove_cb, &xdgtoplevel->events.request_move);
  bind_listen(clientreqresize, client->reqresize_cb, &xdgtoplevel->events.request_resize);
  bind_listen(clientreqmaximize, client->reqmaximize_cb, &xdgtoplevel->events.request_maximize);
  bind_listen(clientreqfullscreen, client->reqfullscreen_cb, &xdgtoplevel->events.request_fullscreen);
}

void popupcommit(struct wl_listener *listener, void *data) {
	popupwin* popup = wl_container_of(listener, popup, commit_cb);
	if (popup->xdg_popup->base->initial_commit) {
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

void popupdestroy(struct wl_listener *listener, void *data) {
	popupwin* popup = wl_container_of(listener, popup, destroy_cb);
	wl_list_remove(&popup->commit_cb.link);
	wl_list_remove(&popup->destroy_cb.link);
	free(popup);
}

void popupnew(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup* xdgpopup = data;

	popupwin* popup = malloc(sizeof(popupwin));
	popup->xdg_popup = xdgpopup;

	struct wlr_xdg_surface *parentsurface = wlr_xdg_surface_try_from_wlr_surface(xdgpopup->parent);
	struct wlr_scene_tree* parenttree = parentsurface->data;

	xdgpopup->base->data = wlr_scene_xdg_surface_create(parenttree, xdgpopup->base);

  bind_listen(popupcommit, popup->commit_cb, &xdgpopup->base->surface->events.commit);
  bind_listen(popupdestroy, popup->destroy_cb, &xdgpopup->events.destroy);
}

void ragnar_init(wm_state* state) {

}

void ragnar_run(wm_state* state) {
}

void ragnar_terminate(wm_state* state) {
	wl_display_destroy_clients(state->display);
	wlr_scene_node_destroy(&state->scene->tree.node);
	wlr_xcursor_manager_destroy(state->cursormgr);
	wlr_cursor_destroy(state->cursor);
	wlr_allocator_destroy(state->allocator);
	wlr_renderer_destroy(state->renderer);
	wlr_backend_destroy(state->backend);
	wl_display_destroy(state->display);
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);

	wm_state* state = malloc(sizeof(wm_state));
  state->display = wl_display_create();
	state->backend = wlr_backend_autocreate(wl_display_get_event_loop(state->display), NULL);
	if (state->backend == NULL) {
		wlr_log(WLR_ERROR, "Failed to create Failed wlr_backend");
    exit(1);
	}

	state->renderer = wlr_renderer_autocreate(state->backend);
	if (state->renderer == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_renderer");
    exit(1);
	}

	wlr_renderer_init_wl_display(state->renderer, state->display);

	state->allocator = wlr_allocator_autocreate(state->backend, state->renderer);
	if (state->allocator == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_allocator");
		exit(1);
	}
	wlr_compositor_create(state->display, 5, state->renderer);
	wlr_subcompositor_create(state->display);
	wlr_data_device_manager_create(state->display);

	state->monlayout = wlr_output_layout_create(state->display);

	wl_list_init(&state->mons);
  bind_listen(monnew, state->mon_cb, &state->backend->events.new_output);

	state->scene = wlr_scene_create();
	state->scenelayout = wlr_scene_attach_output_layout(state->scene, state->monlayout);

	wl_list_init(&state->clients);
	state->xdgsh = wlr_xdg_shell_create(state->display, 3);

  bind_listen(clientnew, state->xdg_client_cb, &state->xdgsh->events.new_toplevel);
  bind_listen(popupnew, state->xdg_popup_xb, &state->xdgsh->events.new_popup);

	state->cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(state->cursor, state->monlayout);

	state->cursormgr = wlr_xcursor_manager_create(NULL, 24);

	state->curmode = CursorModeNone;
  
  bind_listen(curmove, state->curmove_cb, &state->cursor->events.motion);
  bind_listen(curmoveabs, state->curmove_abs_cb, &state->cursor->events.motion_absolute);
  bind_listen(curbtn, state->cur_btn_cb, &state->cursor->events.button);
  bind_listen(curaxis, state->cur_axis_cb, &state->cursor->events.axis);
  bind_listen(curframe, state->cur_frame_cb, &state->cursor->events.frame);

	wl_list_init(&state->keyboards);
  bind_listen(inputnew, state->input_cb, &state->backend->events.new_input);
	state->seat = wlr_seat_create(state->display, "seat0");
  bind_listen(seatreqcur, state->reqcur_cb, &state->seat->events.request_set_cursor);
  bind_listen(seatreqsetsel, state->reqsetsel_cb, &state->seat->events.request_set_selection);

	const char *socket = wl_display_add_socket_auto(state->display);
	if (!socket) {
		wlr_backend_destroy(state->backend);
		exit(1);
	}

	if (!wlr_backend_start(state->backend)) {
		wlr_backend_destroy(state->backend);
		wl_display_destroy(state->display);
		exit(1);
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);

  if (fork() == 0) {
    execl("/bin/sh", "/bin/sh", "-c", "alacritty", (void *)NULL);
  }

	wl_display_run(state->display);
	return 0;
}

