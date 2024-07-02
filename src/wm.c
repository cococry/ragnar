#include "wm.h"

#include <X11/X.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_cursor.h>
#include <xcb/randr.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/keysym.h>

#include <leif/leif.h>

// This needs to be included after the function definitions
#include "config.h"
#include "structs.h"

/* Evaluates to the length (count of elements) in a given array */
#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))
/* Evaluates to the minium of two given numbers */
#define MIN(a, b) (((a)<(b))?(a):(b))
/* Evaluates to the maximum of two given numbers */
#define MAX(a, b) (((a)>(b))?(a):(b))

/* Event handlers */
static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]         = evmaprequest,
  [XCB_UNMAP_NOTIFY]        = evunmapnotify,
  [XCB_ENTER_NOTIFY]        = eventernotify,
  [XCB_KEY_PRESS]           = evkeypress,
  [XCB_BUTTON_PRESS]        = evbuttonpress,
  [XCB_MOTION_NOTIFY]       = evmotionnotify,
  [XCB_CONFIGURE_REQUEST]   = evconfigrequest,
  [XCB_CONFIGURE_NOTIFY]    = evconfignotify,
  [XCB_PROPERTY_NOTIFY]     = evpropertynotify,
  [XCB_CLIENT_MESSAGE]      = evclientmessage,
  [XCB_DESTROY_NOTIFY]      = evdestroynotify,
  [XCB_EXPOSE]              = evexpose,
};

static State s;

/* ===================== CORE ===================== */

void
setup() {
  // Setup SIGCHLD handler
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    terminate(EXIT_FAILURE);
  }

  s.clients = NULL;

  s.dsp = XOpenDisplay(NULL);
  if(!s.dsp) {
    fprintf(stderr, "rangar: cannot open X Display.\n");
    terminate(EXIT_FAILURE);
  }
  // s.conecting to the X server
  s.con = XGetXCBConnection(s.dsp);
  // Checking for errors
  if (xcb_connection_has_error(s.con) || !s.con) {
    fprintf(stderr, "ragnar: cannot open display.\n");
    terminate(EXIT_FAILURE);
  }
  
  // Lock the display to prevent concurrency issues
  XSetEventQueueOwner(s.dsp, XCBOwnsEventQueue);

  // Only initialize OpenGL when decoration is rendered
  if(usedecoration) {
    // Initialize OpenGL
    initglcontext();
  }

  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s.con)).data;
  s.root = screen->root;
  s.screen = screen;

  /* Setting event mask for root window */
  uint32_t evmask[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_FOCUS_CHANGE | 
    XCB_EVENT_MASK_POINTER_MOTION
  };
  xcb_change_window_attributes_checked(s.con, s.root, XCB_CW_EVENT_MASK, evmask);

  loaddefaultcursor();

  // Grab the window manager's keybinds
  grabkeybinds();
  // Setup atoms for EWMH and so on
  setupatoms();

  // Handle monitor setup 
  uint32_t registered_monitors = updatemons();
  s.monfocus = cursormon();

  s.curdesktop = malloc(sizeof(*s.curdesktop) * registered_monitors);
  if(s.curdesktop) {
    for(uint32_t i = 0; i < registered_monitors; i++) {
      s.curdesktop[i] = desktopinit;
    }
  }
}

void
loop() {
  xcb_generic_event_t *ev;
  while ((ev = xcb_wait_for_event(s.con))) {
    uint8_t evcode = ev->response_type & ~0x80;
    /* If the event we receive is listened for by our 
     * event listeners, call the callback for the event. */
    if(evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
      evhandlers[evcode](ev);
    }
    free(ev);
  }
}

void 
terminate(int32_t exitcode) {
  {
    client* cl = s.clients;
    client* next;
    while(cl != NULL) {
      next = cl->next;
      releaseclient(cl->win);
      cl = next; 
    }
  }
  {
    monitor* mon = s.monitors;
    monitor* next;
    while (mon != NULL) {
      next = mon->next;
      free(mon);
      mon = next;
    }
  }
  xcb_disconnect(s.con);
  exit(exitcode);
}

void
setupatoms() {
  s.wm_atoms[WMprotocols]             = getatom("WM_PROTOCOLS");
	s.wm_atoms[WMdelete]                = getatom("WM_DELETE_WINDOW");
	s.wm_atoms[WMstate]                 = getatom("WM_STATE");
	s.wm_atoms[WMtakeFocus]             = getatom("WM_TAKE_FOCUS");
	s.ewmh_atoms[EWMHactiveWindow]      = getatom("_NET_ACTIVE_WINDOW");
	s.ewmh_atoms[EWMHsupported]         = getatom("_NET_SUPPORTED");
	s.ewmh_atoms[EWMHname]              = getatom("_NET_WM_NAME");
	s.ewmh_atoms[EWMHstate]             = getatom("_NET_WM_STATE");
  s.ewmh_atoms[EWMHstateHidden]       = getatom("_NET_WM_STATE_HIDDEN");
  s.ewmh_atoms[EWMHcheck]             = getatom("_NET_SUPPORTING_WM_CHECK");
  s.ewmh_atoms[EWMHfullscreen]        = getatom("_NET_WM_STATE_FULLSCREEN");
  s.ewmh_atoms[EWMHwindowType]        = getatom("_NET_WM_WINDOW_TYPE");
  s.ewmh_atoms[EWMHwindowTypeDialog]  = getatom("_NET_WM_WINDOW_TYPE_DIALOG");
  s.ewmh_atoms[EWMHclientList]        = getatom("_NET_CLIENT_LIST");
  s.ewmh_atoms[EWMHcurrentDesktop]    = getatom("_NET_CURRENT_DESKTOP");

  xcb_atom_t utf8str = getatom("UTF8_STRING");

  xcb_window_t wmcheckwin = xcb_generate_id(s.con);
    xcb_create_window(s.con, XCB_COPY_FROM_PARENT, wmcheckwin, s.root, 
                      0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 
                      XCB_COPY_FROM_PARENT, 0, NULL);

  // Set _NET_WM_CHECK property on the wmcheckwin
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, wmcheckwin, s.ewmh_atoms[EWMHcheck],
                      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_WM_NAME property on the wmcheckwin
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, wmcheckwin, s.ewmh_atoms[EWMHname],
                      utf8str, 8, strlen("ragnar"), "ragnar");

  // Set _NET_WM_CHECK property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHcheck],
                      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_SUPPORTED property on the root window
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHsupported],
                      XCB_ATOM_ATOM, 32, EWMHcount, s.ewmh_atoms);

  // Delete _NET_CLIENT_LIST property from the root window
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

  xcb_flush(s.con);
}

void
grabkeybinds() {
  // Ungrab any grabbed keys
  xcb_ungrab_key(s.con, XCB_GRAB_ANY, s.root, XCB_MOD_MASK_ANY);

  // Grab every keybind
  size_t numkeybinds = sizeof(keybinds) / sizeof(keybinds[0]);
	for (size_t i = 0; i < numkeybinds; ++i) {
    // Get the keycode for the keysym of the keybind
		xcb_keycode_t *keycode = getkeycodes(keybinds[i].key);
    // Grab the key if it is valid 
		if (keycode != NULL) {
			xcb_grab_key(s.con, 1, s.root, keybinds[i].modmask, *keycode,
				XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
		}
	}
  xcb_flush(s.con);
}

void
loaddefaultcursor() {
  xcb_cursor_context_t* context;
  // Create the cursor context
  if (xcb_cursor_context_new(s.con, s.screen, &context) < 0) {
    fprintf(stderr, "ragnar: cannot create cursor context.\n");
    terminate(EXIT_FAILURE);
  }
  // Load the context
  xcb_cursor_t cursor = xcb_cursor_load_cursor(context, "arrow");

  // Set the cursor to the root window
  xcb_change_window_attributes(s.con, s.root, XCB_CW_CURSOR, &cursor);

  // Flush the requests to the X server
  xcb_flush(s.con);

  // Free allocated resources
  xcb_cursor_context_free(context);
}

void 
initglcontext() {
  static int32_t visattribs[] = {
    GLX_X_RENDERABLE, True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    GLX_DOUBLEBUFFER, True,
    None
  };

  int fbcount;
  GLXFBConfig* fbconfs = glXChooseFBConfig(s.dsp, DefaultScreen(s.dsp), visattribs, &fbcount);
  if(!fbconfs) {
    fprintf(stderr, "rangar: cannot retrieve an OpenGL framebuffer config.\n");
    terminate(EXIT_FAILURE);
  }

  s.glfbconf = fbconfs[0];
  XFree(fbconfs);

  s.glvisual = glXGetVisualFromFBConfig(s.dsp, s.glfbconf);
  if(!s.glvisual) {
    fprintf(stderr, "ragnar: no appropriate OpenGL visual found.\n");
    terminate(EXIT_FAILURE);
  }

  s.glcontext = glXCreateContext(s.dsp, s.glvisual, NULL, GL_TRUE);
  if(!s.glcontext) {
    fprintf(stderr, "ragnar: failed to create an OpenGL context.\n");
    terminate(EXIT_FAILURE);
  }
}


/* ===================== UTIL ===================== */

bool
pointinarea(v2 p, area area) {
  return (p.x >= area.pos.x &&
  p.x < (area.pos.x + area.size.x) &&
  p.x >= area.pos.y &&
  p.y < (area.pos.y + area.size.y));
}

v2 
cursorpos(bool* success) {
  // Query the pointer position
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s.con, xcb_query_pointer(s.con, s.root), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve cursor position."); 
    free(reply);
    return (v2){0};
  }
  // Create a v2 for to store the position 
  v2 cursor = (v2){.x = reply->root_x, .y = reply->root_y};

  // Check for errors
  free(reply);
  return cursor;
}


area 
winarea(xcb_window_t win, bool* success) {
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, win), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    fprintf(stderr, "ragnar: failed to retrieve window geometry."); 
    free(reply);
    return (area){0};
  }
  // Creating the area structure to store the geometry in
  area a = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height + ((usedecoration) ? titlebarheight : 0) }};

  // Error checking
  free(reply);
  return a;
}


float
getoverlaparea(area a, area b) {
    float x = MAX(0, MIN(a.pos.x + a.size.x, b.pos.x + b.size.x) - MAX(a.pos.x, b.pos.x));
    float y = MAX(0, MIN(a.pos.y + a.size.y, b.pos.y + b.size.y) - MAX(a.pos.y, b.pos.y));
    return x * y;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
  // Allocate key symbols 
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
  // Get the keysym
	xcb_keysym_t keysym = (!(keysyms) ? 0 : xcb_key_symbols_get_keysym(keysyms, keycode, 0));
  // Free allocated resources
	xcb_key_symbols_free(keysyms);
	return keysym;
}

xcb_keycode_t*
getkeycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s.con);
	xcb_keycode_t *keycode = (!(keysyms) ? NULL : xcb_key_symbols_get_keycode(keysyms, keysym));
	xcb_key_symbols_free(keysyms);
	return keycode;
}

xcb_atom_t
getatom(const char* atomstr) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(s.con, 0, strlen(atomstr), atomstr);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(s.con, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

void
ewmh_updateclients() {
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHclientList]);

  client* cl;
  for (cl = s.clients; cl != NULL; cl = cl->next) {
    xcb_change_property(s.con, XCB_PROP_MODE_APPEND, s.root, s.ewmh_atoms[EWMHclientList],
                        XCB_ATOM_WINDOW, 32, 1, &cl->win);
  }
}

void
sigchld_handler(int32_t signum) {
  (void)signum;
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ==== CLIENT INTERACTION ==== */

client*
addclient(xcb_window_t win) {
  // Allocate client structure
  client* cl = (client*)malloc(sizeof(*cl));
  cl->win = win;

  /* Get the window area */
  bool success;
  area area = winarea(win, &success);
  // Assert that it worked
  assert(success && "Failed to get window area.");

  cl->area = area;
  cl->borderwidth = winborderwidth;
  cl->fullscreen = false;
  cl->floating = false;

  cl->frame = xcb_generate_id(s.con);
  // Set up the frame window
  uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
  uint32_t values[2] = {1, 
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
    XCB_EVENT_MASK_PROPERTY_CHANGE |
    XCB_EVENT_MASK_ENTER_WINDOW |
    XCB_EVENT_MASK_FOCUS_CHANGE |
    XCB_EVENT_MASK_BUTTON_PRESS};

  xcb_create_window(s.con, XCB_COPY_FROM_PARENT, cl->frame, s.root,
                    area.pos.x, area.pos.y, area.size.x, area.size.y,
                    1, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
                    mask, values);

  // Setup client decoration
  setupdecoration(cl);

  // Reparent the client window to the frame window
  xcb_reparent_window(s.con, cl->win, cl->frame, 0, usedecoration ? titlebarheight : 0);

  if(usedecoration) {
    uint32_t client_size[2] = {
      cl->area.size.x,
      cl->area.size.y -  titlebarheight
    };
    xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, client_size);
  }

  // Map the frame window
  xcb_map_window(s.con, cl->frame);


  // Updating the linked list of clients
  cl->next = s.clients;
  s.clients = cl;
  return cl;
}


void 
releaseclient(xcb_window_t win) {
  client** prev = &s.clients;
  client* cl = s.clients;
  /* Iterate throguh the clients to find the client thats 
   * associated with the window */
  while(cl) {
    if(cl->win == win) {
      /* Setting the pointer to previous client to the next client 
       * after the client we want to release, effectivly removing it
       * from our list of clients*/ 
      *prev = cl->next;
      // Freeing memory allocated for client
      xcb_unmap_window(s.con, cl->frame);
      xcb_destroy_window(s.con, cl->frame);

      xcb_destroy_window(s.con, cl->decoration);
      free(cl);
      return;
    }
    // Advancing the client
    prev = &cl->next;
    cl = cl->next;
  }
}

client*
clientfromwin(xcb_window_t win) {
  client* cl;
  for (cl = s.clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->win == win) {
      return cl;
    }
  }
  return NULL;
}

client*
clientfromdecoration(xcb_window_t decoration) {
  client* cl;
  for(cl = s.clients; cl != NULL; cl = cl->next) {
    if(cl->decoration == decoration) {
      return cl;
    }
  }
  return NULL;
}

void
setbordercolor(client* cl, uint32_t color) {
  // Return if the client is NULL
  if(!cl) {
    return;
  }
  // Change the configuration for the border color of the clients window
  xcb_change_window_attributes_checked(s.con, cl->frame, XCB_CW_BORDER_PIXEL, &color);
  xcb_flush(s.con);
}


void
setborderwidth(client* cl, uint32_t width) {
  if(!cl) {
    return;
  }
  // Change the configuration for the border width of the clients window
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){width});
  // Update the border width of the client
  cl->borderwidth = width;
  xcb_flush(s.con);
}

void
moveclient(client* cl, v2 pos) {
  if(!cl) {
    return;
  }
  uint32_t posval[2] = {
    (uint32_t)pos.x, (uint32_t)pos.y
  };

  // Move the window by configuring it's x and y position property
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);
  xcb_flush(s.con);

  cl->area.pos = pos;

  // Update the position and size of the client's decoration to make it stick to the client
  updatedecoration(cl);
  

  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
  s.monfocus = cl->mon;
}

void
resizeclient(client* cl, v2 size) {
  if(!cl) {
    return;
  }
  uint32_t sizeval[2] = { (uint32_t)size.x, (uint32_t)size.y };
  uint32_t sizeval_client[2] = { (uint32_t)size.x, (uint32_t)size.y - ((usedecoration) ? titlebarheight : 0) };

  // Resize the window by configuring it's width and height property
  xcb_configure_window(s.con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval_client);
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);
  xcb_flush(s.con);

  cl->area.size = size;

  // Update the position and size of the client's decoration to make it stick to the client
  updatedecoration(cl);

}

void
moveresizeclient(client* cl, area a) {
  if (!cl) {
    return;
  }
  uint32_t values[4] = {
    (uint32_t)a.pos.x, 
    (uint32_t)a.pos.y,
    (uint32_t)a.size.x,
    (uint32_t)a.size.y
  };
  uint32_t values_client[4] = {
    (uint32_t)a.size.x,
    (uint32_t)a.size.y - ((usedecoration) ? titlebarheight : 0)
  };

  // Move and resize the window by configuring its x, y, width, and height properties
  xcb_configure_window(s.con, cl->frame, 
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | 
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
  xcb_configure_window(s.con, cl->win,  
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values_client);
  xcb_flush(s.con);

  cl->area = a;

  // Update the position and size of the client's decoration to make it stick to the client
  updatedecoration(cl);

  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(cl);
  s.monfocus = cl->mon; 
}
  
void
raiseclient(client* cl) {
  if(!cl) {
    return;
  }
  uint32_t config[] = { XCB_STACK_MODE_ABOVE };
  // Change the configuration of the window to be above 
  xcb_configure_window(s.con, cl->frame, XCB_CONFIG_WINDOW_STACK_MODE, config);
  xcb_flush(s.con);
}

void
killclient(client* cl) {
  if(!cl) {
    return;
  }
  // Destroy the window on the X server
  if(!raiseevent(s.focus, s.wm_atoms[WMdelete])) {
    xcb_destroy_window(s.con, cl->win);
  }
  // Remove the client from the linked list
  releaseclient(cl->win);
  // Unset focus if the client was focused
  if(s.focus == cl) {
    s.focus = NULL;
  }
  xcb_flush(s.con);
}

void
focusclient(client* cl) {
  if(!cl || cl->win == s.root) {
    return;
  }

  // Unfocus the previously focused window to ensure that there is only
  // one focused (highlighted) window at a time.
  if(s.focus) {
    unfocusclient(s.focus);
  }

  // Set input focus 
  setxfocus(cl);

  if(!cl->fullscreen) {
    // Change border color to indicate selection
    setbordercolor(cl, winbordercolor_selected);
    setborderwidth(cl, winborderwidth);
  }

  // Set the focused client
  s.focus = cl;
  s.monfocus = cursormon();

  xcb_flush(s.con);
}

void
setxfocus(client* cl) {
  // Set input focus to client
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);

  // Set active window hint
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHactiveWindow],
                      XCB_ATOM_WINDOW, 32, 1, &cl->win);

  // Raise take-focus event
  raiseevent(cl, s.wm_atoms[WMtakeFocus]);
}

void
unfocusclient(client* cl) {
  if (!cl) {
    return;
  }
  setbordercolor(cl, winbordercolor);
  xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
  xcb_delete_property(s.con, s.root, s.ewmh_atoms[EWMHactiveWindow]);
  xcb_flush(s.con);
}

void
hideclient(client* cl) {
  cl->ignoreunmap = true;
  xcb_unmap_window(s.con, cl->win);
  xcb_unmap_window(s.con, cl->frame);
  xcb_unmap_window(s.con, cl->decoration);
  xcb_flush(s.con);
}

void
showclient(client* cl) {
  xcb_map_window(s.con, cl->frame);
  xcb_map_window(s.con, cl->win);
  xcb_map_window(s.con, cl->decoration);
  xcb_flush(s.con);
}

void
updateclient(client* cl) {
  if(!cl) return;
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s.con, xcb_get_geometry(s.con, cl->frame), NULL);
  // Update area
  cl->area = (area){.pos = (v2){reply->x, reply->y}, .size = (v2){reply->width, reply->height}};
  // Update border width 
  cl->borderwidth = reply->border_width; 
  // Update decoration
  updatedecoration(cl);

  free(reply);
}

bool
raiseevent(client* cl, xcb_atom_t protocol) {
  bool exists = false;
  xcb_icccm_get_wm_protocols_reply_t reply;

  // Checking if the event protocol exists
  if (xcb_icccm_get_wm_protocols_reply(s.con, xcb_icccm_get_wm_protocols(s.con, cl->win, s.wm_atoms[WMprotocols]), &reply, NULL)) {
    for (uint32_t i = 0; i < reply.atoms_len && !exists; i++) {
      exists = (reply.atoms[i] == protocol);
    }
    xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  }

  if(exists) {
    /* Creating and sending the event structure if the event protocol is 
     * valid. */
    xcb_client_message_event_t event;
    event.response_type = XCB_CLIENT_MESSAGE;
    event.window = cl->win;
    event.type = s.wm_atoms[WMprotocols];
    event.format = 32;
    event.data.data32[0] = protocol;
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(s.con, 0, cl->win, XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
  }
  return exists;
}

void
setwintype(client* cl) {
  xcb_atom_t state = getclientprop(cl, s.ewmh_atoms[EWMHstate]);
  xcb_atom_t wintype = getclientprop(cl, s.ewmh_atoms[EWMHwindowType]);

  if(state == s.ewmh_atoms[EWMHfullscreen]) {
    setfullscreen(cl, true);
  } 
  if(wintype == s.ewmh_atoms[EWMHwindowTypeDialog]) {
    cl->floating = true;
  }
}

void
seturgent(client* cl, bool urgent) {
  xcb_icccm_wm_hints_t wmh;
  cl->urgent = urgent;

  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints(s.con, cl->win);
  if (!xcb_icccm_get_wm_hints_reply(s.con, cookie, &wmh, NULL)) {
    return;
  }

  if (urgent) {
    wmh.flags |= XCB_ICCCM_WM_HINT_X_URGENCY;
  } else {
    wmh.flags &= ~XCB_ICCCM_WM_HINT_X_URGENCY;
  }
  xcb_icccm_set_wm_hints(s.con, cl->win, &wmh);
}

xcb_atom_t
getclientprop(client* cl, xcb_atom_t prop) {
  xcb_generic_error_t *error;
  xcb_atom_t atom = XCB_NONE;
  xcb_get_property_cookie_t cookie;
  xcb_get_property_reply_t *reply;

  // Get the property from the window
  cookie = xcb_get_property(s.con, 0, cl->win, prop, XCB_ATOM_ATOM, 0, sizeof(xcb_atom_t));
  // Get the reply
  reply = xcb_get_property_reply(s.con, cookie, &error);

  if(reply) { 
    // Check if the property type matches the expected type and has the right format
    if(reply->type == XCB_ATOM_ATOM && reply->format == 32 && reply->value_len > 0) {
      atom = *(xcb_atom_t*)xcb_get_property_value(reply);
    }
    free(reply);
  }
  if(error) {
    free(error);
  }
  return atom;
}

void
setfullscreen(client* cl, bool fullscreen) {
  if(!s.monfocus || !cl) return;

  cl->fullscreen = fullscreen;
  if(cl->fullscreen) {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 1, &s.ewmh_atoms[EWMHfullscreen]);
    // Store previous position of client
    cl->area_prev = cl->area;
    // Set the client's area to the focused monitors area, effictivly
    // making the client as large as the monitor screen
    cl->area = s.monfocus->area;

    // Unset border of client if it's fullscreen
    cl->borderwidth = 0;

    // Hide client decoration in fullscreen mode
    xcb_unmap_window(s.con, cl->decoration);
  } else {
    xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s.ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    // Set the client's area to the area before the last fullscreen occured 
    cl->area = cl->area_prev;
    cl->borderwidth = winborderwidth;

    // Show client decoration again if the client is unfullscreened
    xcb_map_window(s.con, cl->decoration);
  }
  // Update client's border width
  setborderwidth(cl, cl->borderwidth);
  // Update client's geometry
  moveresizeclient(cl, cl->area);

  raiseclient(cl);

  if(cl->area.size.x >= cl->mon->area.size.x && cl->area.size.y >= cl->mon->area.size.y && !cl->fullscreen) {
    setfullscreen(cl, true);
  }
}
  
void
switchclientdesktop(client* cl, int32_t desktop) {
  cl->desktop = desktop;
  if(cl == s.focus) {
    unfocusclient(cl);
  }
  raiseclient(cl);
  hideclient(cl);
  xcb_flush(s.con);
}

char*
getclientname(client* cl)
{
	xcb_icccm_get_text_property_reply_t t_reply;
  // Retrieve name cookie
  xcb_get_property_cookie_t	cookie = xcb_icccm_get_wm_name(s.con, cl->win);
  // Get text property
	const uint8_t wr = xcb_icccm_get_wm_name_reply(s.con, cookie, &t_reply, NULL);
	if (wr == 1) {
    // Allocate string to store name in
		char *str = (char *)malloc(t_reply.name_len + 1 * sizeof(char));
		if (str == NULL)
			return NULL;
    // Copy data
		strncpy(str, t_reply.name, t_reply.name_len);
    str[t_reply.name_len] = '\0';
    // Wipe the reply
		xcb_icccm_get_text_property_reply_wipe(&t_reply);
		return str;
	}
	return NULL;
}


/* ============ DECORATION ============= */

void 
setupdecoration(client* cl) {
  if(!usedecoration) return;

  area geom = cl->area; 
  Window root = DefaultRootWindow(s.dsp);
  XSetWindowAttributes attribs;
  attribs.colormap = XCreateColormap(s.dsp, s.root, s.glvisual->visual, AllocNone);
  attribs.event_mask = EnterWindowMask | ExposureMask | StructureNotifyMask;
  cl->decoration = (xcb_window_t)XCreateWindow(s.dsp, root, 0, 0, cl->area.size.x,
                                               titlebarheight, 0, s.glvisual->depth, InputOutput, s.glvisual->visual,
                                               CWColormap | CWEventMask, &attribs);
  XReparentWindow(s.dsp, cl->decoration, cl->frame, 0, 0);
  XMapWindow(s.dsp, cl->decoration);
  // Grab Buttons
  {
    unsigned int event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    // Grab Button 1
    XGrabButton(s.dsp, Button1, AnyModifier, cl->decoration, 
                False, event_mask, GrabModeAsync, GrabModeAsync, None, None);

    // Grab Button 3
    XGrabButton(s.dsp, Button3, AnyModifier, cl->decoration, 
                False, event_mask, GrabModeAsync, GrabModeAsync, None, None);
  }

  {
    GLXDrawable drawable = cl->decoration;
    if(!glXMakeCurrent(s.dsp, drawable, s.glcontext)) {
      fprintf(stderr, "ragnar: cannot make OpenGL context.\n");
      terminate(EXIT_FAILURE);
    }

    if(!s.ui.init) {
      s.ui = lf_init_x11(geom.size.x, titlebarheight);  
      lf_free_font(&s.ui.theme.font);
      s.ui.theme.font = lf_load_font(decorationfont, 20);
    }

    renderdecoration(cl);
  }
}

void
updatedecoration(client* cl) {
  if(!usedecoration) return;

  bool success;
  // Update client geometry 
  cl->area = winarea(cl->frame, &success);
  if(!success) return;

  // Update decoration geometry
  uint32_t vals[2];
  vals[0] = cl->area.size.x;
  vals[1] = titlebarheight;
  xcb_configure_window(s.con, cl->decoration, 
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
  xcb_map_window(s.con, cl->decoration);
}

void
renderdecoration(client* cl) {
  if(!usedecoration) return;

  GLXDrawable drawable = cl->decoration;
  if(!glXMakeCurrent(s.dsp, drawable, s.glcontext)) {
    fprintf(stderr, "ragnar: cannot make OpenGL context.\n");
    terminate(EXIT_FAILURE);
  }

  {
    LfColor clearcolor = lf_color_from_hex(decorationcolor);
    vec4s zto = lf_color_to_zto(clearcolor);
    glClearColor(zto.r, zto.g, zto.b, zto.a);
  }

  glClear(GL_COLOR_BUFFER_BIT);
  lf_begin(&s.ui);
  char* name = getclientname(cl);
  lf_text(&s.ui, name);
  free(name);
  lf_end(&s.ui);
  glXSwapBuffers(s.dsp, cl->decoration);
}

/* ========= EVENT HANDLERS ========== */

void 
evmaprequest(xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

  // Retrieving attributes of the mapped window
  xcb_get_window_attributes_cookie_t wa_cookie = xcb_get_window_attributes(s.con, map_ev->window);
  xcb_get_window_attributes_reply_t *wa_reply = xcb_get_window_attributes_reply(s.con, wa_cookie, NULL);
  // Return if attributes could not be retrieved or if the window uses override_redirect
  if (!wa_reply || wa_reply->override_redirect) {
    free(wa_reply);
    return;
  }
  free(wa_reply);

  // Don't handle already managed clients 
  if(clientfromwin(map_ev->window) != NULL) {
    return;
  }

  // Setup listened events for the mapped window
  {
    uint32_t evmask[] = {  XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_PROPERTY_CHANGE };
    xcb_change_window_attributes_checked(s.con, map_ev->window, XCB_CW_EVENT_MASK, evmask);
  }


  // Grabbing mouse events for interactive moves/resizes 
  {
    uint16_t evmask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION;
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s.root, XCB_NONE, 1, winmod);
    xcb_grab_button(s.con, 0, map_ev->window, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s.root, XCB_NONE, 3, winmod);
  }


  // Retrieving cursor position
  bool cursor_success;
  v2 cursor = cursorpos(&cursor_success);
  if(!cursor_success) return;

  // Adding the mapped client to our linked list
  client* cl = addclient(map_ev->window);

  // Set initial border 
  setbordercolor(cl, winbordercolor);
  setborderwidth(cl, winborderwidth);

  // Send configure event to the client

  // Set window type of client (e.g dialog)
  setwintype(cl);


  // Set client's monitor
  cl->mon = clientmon(cl);
  cl->desktop = s.curdesktop[s.monfocus->idx];

  // Set all clients floating for now (TODO)
  cl->floating = true;

  updateclient(cl);

  // Update the EWMH client list
  ewmh_updateclients();

  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(cl);
  }

  if(s.monfocus) {
    // Spawn the window in the center of the focused monitor
    moveclient(cl, (v2){
      s.monfocus->area.pos.x + (s.monfocus->area.size.x - cl->area.size.x) / 2.0f, 
      s.monfocus->area.pos.y + (s.monfocus->area.size.y - cl->area.size.y) / 2.0f});
  }


  // Map the window
  xcb_map_window(s.con, map_ev->window);
  xcb_flush(s.con);
}

void 
evunmapnotify(xcb_generic_event_t* ev) {
  // Retrieve the event
  xcb_unmap_notify_event_t* unmap_ev = (xcb_unmap_notify_event_t*)ev;

  client* cl = clientfromwin(unmap_ev->window);
  if(cl && cl->ignoreunmap) {
    cl->ignoreunmap = false;
    return;
  }

  // Remove the client from the list
  releaseclient(unmap_ev->window);
  // Unmap the window
  xcb_unmap_window(s.con, unmap_ev->window);


  // Update the EWMH client list
  ewmh_updateclients();

  xcb_flush(s.con);
}

void 
eventernotify(xcb_generic_event_t* ev) {
  xcb_enter_notify_event_t *enter_ev = (xcb_enter_notify_event_t*)ev;

  if((enter_ev->mode != XCB_NOTIFY_MODE_NORMAL || enter_ev->detail == XCB_NOTIFY_DETAIL_INFERIOR)
    && enter_ev->event != s.root) {
    return;
  }

  client* cl = clientfromwin(enter_ev->event);

  // Not a client window, check if its decoration
  if(!cl) {
    if(!(cl = clientfromdecoration(enter_ev->event)) && enter_ev->event != s.root) return;
  }

  // Focus entered client
  if(cl) { 
    focusclient(cl);
  }
  else if(enter_ev->event == s.root) {
    // Set Input focus to root
    xcb_set_input_focus(s.con, XCB_INPUT_FOCUS_POINTER_ROOT, s.root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client* cl;
    for (cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->fullscreen) {
        continue;
      }
      setbordercolor(cl, winbordercolor);
      setborderwidth(cl, winborderwidth);
    }
  }
  xcb_flush(s.con);
}

void
evkeypress(xcb_generic_event_t* ev) {
  xcb_key_press_event_t *e = ( xcb_key_press_event_t *) ev;
  // Get associated keysym for the keycode of the event
  xcb_keysym_t keysym = getkeysym(e->detail);

  /* Iterate throguh the keybinds and check if one of them was pressed. */
  size_t numkeybinds = sizeof(keybinds) / sizeof(keybinds[0]);
  for (uint32_t i = 0; i < numkeybinds; ++i) {
    // If it was pressed, call the callback of the keybind
    if ((keysym == keybinds[i].key) && (e->state == keybinds[i].modmask)) {
      if(keybinds[i].cb) {
        keybinds[i].cb(keybinds[i].data);
      }
    }
  }
  xcb_flush(s.con);
}

void
evbuttonpress(xcb_generic_event_t* ev) {
  xcb_button_press_event_t* button_ev = (xcb_button_press_event_t*)ev;

  client* cl = clientfromwin(button_ev->event);
  if(!cl) {
    // Not a client window, check if it's decoration
    cl = clientfromdecoration(button_ev->event);
    if(!cl) return;
    focusclient(cl);
  } else {
    // Focusing client 
    if(cl != s.focus) {
      // Unfocus the previously focused window to ensure that there is only
      // one focused (highlighted) window at a time.
      if(s.focus) {
        unfocusclient(s.focus);
      }
      focusclient(cl);
    }
  }
  bool success;
  area area = winarea(cl->frame, &success);
  // Assert that it worked
  assert(success && "Failed to get window area.");

  cl->area = area;
  // Setting grab position
  s.grabwin = cl->area;
  s.grabcursor = (v2){.x = (float)button_ev->root_x, (float)button_ev->root_y};

  // Raising the client to the top of the stack
  raiseclient(cl);
  xcb_flush(s.con);
}

void
evmotionnotify(xcb_generic_event_t* ev) {
  xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)ev;

  if(motion_ev->event == s.root) {
    // Update the focused monitor to the monitor under the cursor
    s.monfocus = cursormon();
    return;
  }
  // Position of the cursor in the drag event
  v2 dragpos    = (v2){.x = (float)motion_ev->root_x, 
    .y = (float)motion_ev->root_y};
  // Drag difference from the current drag event to the initial grab 
  v2 dragdelta  = (v2){.x = dragpos.x - s.grabcursor.x, 
    .y = dragpos.y - s.grabcursor.y};
  // The destination of the window after the interactive resize 
  v2 movedest = (v2){.x = (float)(s.grabwin.pos.x + dragdelta.x), 
    .y = (float)(s.grabwin.pos.y + dragdelta.y)};

  client* cl = clientfromwin(motion_ev->event);
  if(!cl) {
    // Not a client window, check if it's decoration
    if(!(cl = clientfromdecoration(motion_ev->event))) return;
    moveclient(cl, movedest);
    xcb_flush(s.con);
    return;
  }

  if(!(motion_ev->state & XCB_BUTTON_MASK_1 ||
    motion_ev->state & XCB_BUTTON_MASK_3)) return;

  // Unset fullscreen
  cl->fullscreen = false;
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, cl->win, s
                      .ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
  cl->borderwidth = winborderwidth;
  setborderwidth(cl, cl->borderwidth);

  // Move the window
  if(motion_ev->state & movebtn) {
    // New position of the window
    v2 movedest = (v2){.x = (float)(s.grabwin.pos.x + dragdelta.x), 
      .y = (float)(s.grabwin.pos.y + dragdelta.y)};

    moveclient(cl, movedest);
  } 
  // Resize the window
  else if(motion_ev->state & resizebtn) {
    // Resize delta (clamped)
    v2 resizedelta  = (v2){.x = MAX(dragdelta.x, -s.grabwin.size.x), 
      .y = MAX(dragdelta.y, -s.grabwin.size.y)};
    // New window size
    v2 sizedest = (v2){.x = s.grabwin.size.x + resizedelta.x, 
      .y = s.grabwin.size.y + resizedelta.y};

    resizeclient(cl, sizedest);
  }
  xcb_flush(s.con);
}

void 
evconfigrequest(xcb_generic_event_t* ev) {
  xcb_configure_request_event_t *config_ev = (xcb_configure_request_event_t *)ev;

  uint16_t mask = 0;
  uint32_t values[7];
  uint32_t i = 0;

  if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
    mask |= XCB_CONFIG_WINDOW_WIDTH;
    values[i++] = config_ev->width;
  }
  if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
    mask |= XCB_CONFIG_WINDOW_HEIGHT;
    values[i++] = config_ev->height;
  }
  if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
    mask |= XCB_CONFIG_WINDOW_SIBLING;
    values[i++] = config_ev->sibling;
  }
  if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
    mask |= XCB_CONFIG_WINDOW_STACK_MODE;
    values[i++] = config_ev->stack_mode;
  }

  client* cl = clientfromwin(config_ev->window);
  if(cl) {
    cl->area.size = (v2){config_ev->width, config_ev->height};
  }

  // Configure the window with the specified values
  xcb_configure_window(s.con, config_ev->window, mask, values);

  xcb_flush(s.con);
}

void
evconfignotify(xcb_generic_event_t* ev) {
  xcb_configure_notify_event_t* config_ev = (xcb_configure_notify_event_t*)ev;

  if(config_ev->window == s.root) {
    updatemons();
    return;
  }
  client* cl = clientfromwin(config_ev->window);
  if(!cl) return;
  updatedecoration(cl);
}

void
evpropertynotify(xcb_generic_event_t* ev) {
  xcb_property_notify_event_t* prop_ev = (xcb_property_notify_event_t*)ev;
  if(prop_ev->window == s.root) return;

  client* cl = clientfromwin(prop_ev->window);

  if(prop_ev->atom == s.ewmh_atoms[EWMHname]) {
    renderdecoration(cl);
  }
  xcb_flush(s.con);
}

void
evclientmessage(xcb_generic_event_t* ev) {
  xcb_client_message_event_t* msg_ev = (xcb_client_message_event_t*)ev;
  client* cl = clientfromwin(msg_ev->window);

  if(!cl) {
    return;
  }

  if(msg_ev->type == s.ewmh_atoms[EWMHstate]) {
    // If client requested fullscreen toggle
    if(msg_ev->data.data32[1] == s.ewmh_atoms[EWMHfullscreen] ||
       msg_ev->data.data32[2] == s.ewmh_atoms[EWMHfullscreen]) {
      // Set/unset client fullscreen 
      setfullscreen(cl, (msg_ev->data.data32[0] == 1 || (msg_ev->data.data32[0] == 2 && !cl->fullscreen)));
    }
  } else if(msg_ev->type == s.ewmh_atoms[EWMHactiveWindow]) {
    if(s.focus != cl && !cl->urgent) {
      seturgent(cl, true);
    }
  }
}

void
evdestroynotify(xcb_generic_event_t* ev) {
  xcb_destroy_notify_event_t* destroy_ev = (xcb_destroy_notify_event_t*)ev;
  client* cl = clientfromwin(destroy_ev->window);
  if(!cl) return;
  releaseclient(destroy_ev->window);
}

void
evexpose(xcb_generic_event_t* ev) {
  xcb_expose_event_t* expose_ev = (xcb_expose_event_t*)ev;
  client* cl = clientfromdecoration(expose_ev->window);
  if(!cl) return;
  renderdecoration(cl);
}

monitor* addmon(area a, uint32_t idx) {
  monitor* mon  = (monitor*)malloc(sizeof(*mon));
  mon->area     = a;
  mon->next     = s.monitors;
  mon->idx      = idx;
  s.monitors    = mon;
  return mon;
}

monitor*
monbyarea(area a) {
  monitor* mon;
  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    if((mon->area.pos.x == a.pos.x && mon->area.pos.y == a.pos.y) &&
        (mon->area.size.x == a.size.x && mon->area.size.y == a.size.y)) {
      return mon;
    }
  }
  return NULL;
}


monitor*
clientmon(client* cl) {
  monitor* ret = s.monitors;
  if (!cl) {
    return ret;
  }

  monitor* mon;
  float biggest_overlap_area = -1.0f;

  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    float overlap = getoverlaparea(cl->area, mon->area);

    // Assign the monitor on which the client has the biggest overlap area
    if (overlap > biggest_overlap_area) {
      biggest_overlap_area = overlap;
      ret = mon;
    }
  }
  return ret;
}

monitor*
cursormon() {
  bool success;
  v2 cursor = cursorpos(&success);
  if(!success) {
    return s.monitors;
  }
  monitor* mon;
  for (mon = s.monitors; mon != NULL; mon = mon->next) {
    if(pointinarea(cursor, mon->area)) {
      return mon;
    }
  }
  return s.monitors;
}

uint32_t
updatemons() {
  // Get xrandr screen resources
  xcb_randr_get_screen_resources_current_cookie_t res_cookie = xcb_randr_get_screen_resources_current(s.con, s.screen->root);
  xcb_randr_get_screen_resources_current_reply_t *res_reply = xcb_randr_get_screen_resources_current_reply(s.con, res_cookie, NULL);

  if (!res_reply) {
    fprintf(stderr, "ragnar: not get screen resources.\n");
    terminate(EXIT_FAILURE);
  }

  // Get number of connected monitors
  int32_t num_outputs = xcb_randr_get_screen_resources_current_outputs_length(res_reply);
  // Get list of monitors
  xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(res_reply);

  uint32_t registered_count = 0; 
  for (int32_t i = 0; i < num_outputs; i++) {
    // Get the monitor info
    xcb_randr_get_output_info_cookie_t info_cookie = xcb_randr_get_output_info(s.con, outputs[i], XCB_TIME_CURRENT_TIME);
    xcb_randr_get_output_info_reply_t *info_reply = xcb_randr_get_output_info_reply(s.con, info_cookie, NULL);

    // Check if montior has CRTC
    if (info_reply && info_reply->crtc != XCB_NONE) {
      // Get CRTC info
      xcb_randr_get_crtc_info_cookie_t crtc_cookie = xcb_randr_get_crtc_info(s.con, info_reply->crtc, XCB_TIME_CURRENT_TIME);
      xcb_randr_get_crtc_info_reply_t *crtc_reply = xcb_randr_get_crtc_info_reply(s.con, crtc_cookie, NULL);

      // If that worked, register the monitor if it isn't registered yet 
      if (crtc_reply) {
        area monarea = (area){
          .pos = (v2){
            crtc_reply->x, crtc_reply->y
          },
          .size = (v2){
            crtc_reply->width, crtc_reply->height
          }
        };
        if(!monbyarea(monarea)) {
          addmon(monarea, registered_count++);
        }
        free(crtc_reply);
      }
    }
    if (info_reply) {
      free(info_reply);
    }
  }
  free(res_reply);

  return registered_count;
}

/* ================== CLIENT FUNCTIONS ================== */

// Terminates the window manager with a successfull exit code
void 
terminate_successfully(passthrough_data data) {
  (void)data;
  terminate(EXIT_SUCCESS);
}

// Cycles the currently focused client 
void 
cyclefocus(passthrough_data data) {
  (void)data;
  if (!s.clients || !s.focus)
    return;
  client* next = NULL;
  // Find the next client on the current monitor & desktop 
  for(client* cl = s.focus->next; cl != NULL; cl = cl->next) {
    if(cl->mon == s.monfocus && cl->desktop == s.curdesktop[s.monfocus->idx]) {
      next = cl;
      break;
    }
  }

  // If there is a next client, just focus it
  if (next != NULL) {
    focusclient(next);
    raiseclient(next);
  }
  // If there is no next client, cycle back to the first client on the 
  // current monitor & desktop
  else {
    for(client* cl = s.clients; cl != NULL; cl = cl->next) {
      if(cl->mon == s.monfocus && cl->desktop == s.curdesktop[s.monfocus->idx]) {
        next = cl;
        break;
      }
    }
    // If there is one, focus it
    if (next != NULL) {
      focusclient(next);
      raiseclient(next);
    }
  }
}
// Kills the currently focused client
void 
killfocus(passthrough_data data) {
  (void)data;
  if(!s.focus) {
    return;
  }
  killclient(s.focus);
}

// Raises the currently focused client to the top of the window stack
void 
raisefocus(passthrough_data data) {
  (void)data;
  if(!s.focus) return;
  raiseclient(s.focus);
}

// Toggles fullscreen mode on the currently focused client
void 
togglefullscreen(passthrough_data data) {
  (void)data;
  if(!s.focus) return;
  bool fs = !(s.focus->fullscreen);
  setfullscreen(s.focus, fs); 
}

// Cycles the desktop on the currently focused desktop one up 
void 
cycledesktopup(passthrough_data data) {
  (void)data;
  int32_t newdesktop = s.curdesktop[s.monfocus->idx];
  if(newdesktop + 1 < desktopcount) {
    newdesktop++;
  } else {
    newdesktop = 0;
  }
  switchdesktop((passthrough_data){.i = newdesktop});
}

// Cycles the desktop on the currently focused desktop one down 
void 
cycledesktopdown(passthrough_data data) {
  (void)data;
  int32_t newdesktop = s.curdesktop[s.monfocus->idx];
  if(newdesktop - 1 >= 0) {
    newdesktop--;
  } else {
    newdesktop = desktopcount - 1;
  }
  switchdesktop((passthrough_data){.i = newdesktop});
}

// Cycles the desktop of the currently focused client up
void 
cyclefocusdesktopup(passthrough_data data) {
  (void)data;
  if(!s.focus) return;
  int32_t new_desktop = s.focus->desktop;
  if(new_desktop + 1 < desktopcount) {
    new_desktop++;
  } else {
    new_desktop = 0;
  }
  switchclientdesktop(s.focus, new_desktop); 
}

// Cycles the desktop of the currently focused client down
void 
cyclefocusdesktopdown(passthrough_data data) {
  (void)data;
  if(!s.focus) return;
  int32_t new_desktop = s.focus->desktop;
  if(new_desktop - 1 >= 0) {
    new_desktop--;
  } else {
    new_desktop = desktopcount - 1;
  }
  switchclientdesktop(s.focus, new_desktop);
}

// Switches to a given desktop (passthrough_data){.i = *desktop*}
void 
switchdesktop(passthrough_data data) {
  if(!s.monfocus) return;
  if(data.i == s.curdesktop[s.monfocus->idx]) return;

  // Notify EWMH for desktop change
  xcb_change_property(s.con, XCB_PROP_MODE_REPLACE, s.root, s.ewmh_atoms[EWMHcurrentDesktop],
                      XCB_ATOM_CARDINAL, 32, 1, &data.i);

  for (client* cl = s.clients; cl != NULL; cl = cl->next) {
    if(cl->mon != s.monfocus) continue;
    // Hide the clients on the current desktop
    if(cl->desktop == s.curdesktop[s.monfocus->idx]) {
      hideclient(cl);
    // Show the clients on the desktop we want to switch to
    } else if(cl->desktop == data.i) {
      showclient(cl);
    }
  }

  // Unfocus all selected clients
  for(client* cl = s.clients; cl != NULL; cl = cl->next) {
    unfocusclient(cl);
  }

  s.curdesktop[s.monfocus->idx] = data.i;
  xcb_flush(s.con);


}

// Switches the desktop of the currently focused client to a given desktop 
// (passthrough_data){.i = *desktop*}
void 
switchfocusdesktop(passthrough_data data) {
  if(!s.focus) return;
  switchclientdesktop(s.focus, data.i);
}

// Runs a given command (passthrough_data){.cmd = *cmd_to_run*}
void 
runcmd(passthrough_data data) {
  if (data.cmd == NULL) {
    return;
  }
  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    execl("/bin/sh", "sh", "-c", data.cmd, (char *)NULL);
    // If execl fails
    fprintf(stderr, "ragnar: failed to execute command.\n");
  } else if (pid > 0) {
    // Parent process
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    // Fork failed
    perror("fork");
    fprintf(stderr, "ragnar: failed to execute command.\n");
    return;
  }
}

int 
main() {
  // Run the startup script
  runcmd((passthrough_data){.cmd = "ragnarstart"});
  // Setup the window manager
  setup();
  // Enter the event loop
  loop();
  // Terminate after the loop
  terminate(EXIT_SUCCESS);
}

