#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/wait.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_util.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_cursor.h>
#include <xcb/randr.h>
#include <xcb/composite.h>

#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include "config.h"
#include "ipc/sockets.h"
#include "structs.h"

#include "funcs.h"
#include "keycallbacks.h"

/* */
static event_handler_t evhandlers[_XCB_EV_LAST] = {
  [XCB_MAP_REQUEST]         = evmaprequest,
  [XCB_UNMAP_NOTIFY]        = evunmapnotify,
  [XCB_DESTROY_NOTIFY]      = evdestroynotify,
  [XCB_ENTER_NOTIFY]        = eventernotify,
  [XCB_LEAVE_NOTIFY]        = evleavenotify,
  [XCB_KEY_PRESS]           = evkeypress,
  [XCB_BUTTON_PRESS]        = evbuttonpress,
  [XCB_BUTTON_RELEASE]      = evbuttonrelease,
  [XCB_MOTION_NOTIFY]       = evmotionnotify,
  [XCB_CONFIGURE_REQUEST]   = evconfigrequest,
  [XCB_CONFIGURE_NOTIFY]    = evconfignotify,
  [XCB_PROPERTY_NOTIFY]     = evpropertynotify,
  [XCB_CLIENT_MESSAGE]      = evclientmessage,
  [XCB_EXPOSE]              = evexpose,
};

/* --- Static functions to handle another running X window manager */
static int (*xerrorhandler)(Display*, XErrorEvent*);

static int
otherwmhandler(Display* dsp, XErrorEvent* ev) {
  (void)dsp;
  (void)ev;
  fprintf(stderr, "ragnar: another X window manager is already running.\n");
  exit(EXIT_FAILURE);
  return -1;
}

/* --- This function is taken from DWM --- */ 
static int
xerror(Display *dpy, XErrorEvent *ee)
{
  if (ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "ragnar: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return xerrorhandler(dpy, ee); 
}

/**
 * @brief Sets up the WM state and the X server 
 *
 * @param s The window manager's state
 * This function establishes a connection to the X server,
 * sets up the root window and window manager keybindings.
 * The event mask of the root window is being cofigured to  
 * listen to necessary events. 
 * After the configuration of the root window, all the specified
 * keybinds in the config are grabbed by the window manager.
 */
void
setup(state_t* s) {
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGCHLD, &sa, NULL);

  signal(SIGINT, sigchld_handler);
  signal(SIGTERM, sigchld_handler);
  signal(SIGQUIT, sigchld_handler);


  initconfig(s);
  readconfig(s, &s->config);

  fclose(fopen(s->config.logfile, "w"));

  s->clients = NULL;

  if(!s->config.usedecoration) {
    s->config.titlebarheight = 0;
  }

  s->lastexposetime = 0;
  s->lastmotiontime = 0;

  s->showtitlebars = s->config.showtitlebars_init;

  // Create IPC thread
  pthread_t ipc_thread;
  if(pthread_create(&ipc_thread, NULL, ipcserverthread, s) != 0) {
    logmsg(s, LogLevelError, "Failed to create IPC thread.");
  }

  // Opening Xorg display
  s->dsp = XOpenDisplay(NULL);
  if(!s->dsp) {
    logmsg(s, LogLevelError, "Failed to open X Display.");
    terminate(s, EXIT_FAILURE);
  }
  logmsg(s, LogLevelTrace, "successfully opened X display.");


  /* Checking for other window manager and terminating if one is found. */
  {
    xerrorhandler = XSetErrorHandler(otherwmhandler);
    XSelectInput(s->dsp, DefaultRootWindow(s->dsp), SubstructureRedirectMask);
    XSync(s->dsp, False);
    XSetErrorHandler(xerror);
    XSync(s->dsp, False);
  }
  // Run the startup script
  runcmd(NULL, (passthrough_data_t){.cmd = "ragnarstart"});

  // Setting up xcb connection 
  s->con = XGetXCBConnection(s->dsp);
  // Checking for errors
  if (xcb_connection_has_error(s->con) || !s->con) {
    logmsg(s,  LogLevelError, "cannot connect to XCB.");
    terminate(s, EXIT_FAILURE);
  }
  logmsg(s,  LogLevelTrace, "successfully opened XCB connection.");

  // Lock the display to prevent concurrency issues
  XSetEventQueueOwner(s->dsp, XCBOwnsEventQueue);

  s->initgl = false;

  xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(s->con)).data;
  s->root = screen->root;
  s->screen = screen;

  /* Setting event mask for root window */
  uint32_t evmask[] = {
    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
      XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
      XCB_EVENT_MASK_ENTER_WINDOW |
      XCB_EVENT_MASK_FOCUS_CHANGE | 
      XCB_EVENT_MASK_POINTER_MOTION
  };
  xcb_change_window_attributes_checked(s->con, s->root, XCB_CW_EVENT_MASK, evmask);

  // Load the default root cursor image
  loaddefaultcursor(s);

  // Grab the window manager's keybinds
  grabkeybinds(s);

  // Handle monitor setup 
  uint32_t registered_monitors = updatemons(s);

  // Setup atoms for EWMH and NetWM standards
  setupatoms(s);

  // Gather strut information for layouts 
  s->nwinstruts = 0;
  s->winstruts = malloc(sizeof(strut_t) * s->config.maxstruts);

  getwinstruts(s, s->root);

  // Initialize virtual desktops
  s->curdesktop = malloc(sizeof(*s->curdesktop) * registered_monitors);
  if(s->curdesktop) {
    for(uint32_t i = 0; i < registered_monitors; i++) {
      s->curdesktop[i].idx = s->config.desktopinit;
    }
  }

  s->scratchpads = malloc(sizeof(*s->scratchpads) * s->config.maxscratchpads);
  for(uint32_t i = 0; i < s->config.maxscratchpads; i++) {
    s->scratchpads[i].needs_restart = true;
    s->scratchpads[i].hidden = true;
    s->scratchpads[i].win = 0;
  }

  s->mapping_scratchpad_index = -1;
}

/**
 * @brief Event loop of the window manager 
 *
 * This function polls for X server events and 
 * handles them accoringly by calling the associated event handler.
 */
void
loop(state_t* s) {
  xcb_generic_event_t *ev;

  while (1) {
    // Poll for events without blocking
    while ((ev = xcb_wait_for_event(s->con))) {
      uint8_t evcode = ev->response_type & ~0x80;
      /* If the event we receive is listened for by our 
       * event listeners, call the callback for the event. */
      if (evcode < ARRLEN(evhandlers) && evhandlers[evcode]) {
        evhandlers[evcode](s, ev);
      }
      free(ev);
    }
  }
}

/**
 * @brief Terminates the window manager 
 *
 * This function terminates the window manager by
 * diss->conecting the s->conection to the X server and
 * exiting the program.
 */
void 
terminate(state_t* s, int32_t exitcode) {
  // Release every client
  {
    client_t* cl = s->clients;
    client_t* next;
    while(cl != NULL) {
      next = cl->next;
      releaseclient(s, cl->win);
      cl = next; 
    }
  }
  // Release every monitor
  {
    monitor_t* mon = s->monitors;
    monitor_t* next;
    while (mon != NULL) {
      next = mon->next;
      free(mon);
      mon = next;
    }
  }

  XCloseDisplay(s->dsp);
  // Give up the X connection
  xcb_disconnect(s->con);

  logmsg(s,  LogLevelTrace, "terminated with exit code %i.", exitcode);

  destroyconfig();

  // Free the window manager's state
  free(s);

  exit(exitcode);
}

/**
 * @brief Manages all windows that are avaiable on the 
 * X display.
 *
 * @param s The window manager' state
 */
void
managewins(state_t* s) {
  xcb_get_window_attributes_reply_t* wareply;
  xcb_get_window_attributes_cookie_t wacookie;
  xcb_atom_t atomtransientfor;
  xcb_get_property_cookie_t transientcookie;
  xcb_get_property_reply_t* transientreply;
  xcb_atom_t* transientlist;
  xcb_window_t transientwindow;
  xcb_get_window_attributes_cookie_t transientwacookie;
  xcb_get_window_attributes_reply_t* transientwareply;
  xcb_window_t* allwindows;
  xcb_window_t* windowlist;
  int i, num;

  atomtransientfor = getatom(s, "WM_TRANSIENT_FOR"); 

  // Query the list of windows on the root
  xcb_query_tree_cookie_t treecookie = xcb_query_tree(s->con, s->root);
  xcb_query_tree_reply_t *treereply = xcb_query_tree_reply(s->con, treecookie, NULL);
  if (!treereply) {
    return; 
  }

  allwindows = xcb_query_tree_children(treereply);
  num = xcb_query_tree_children_length(treereply);

  windowlist = malloc(num * sizeof(xcb_window_t));
  if (!windowlist) {
    return; 
  }

  for (i = 0; i < num; i++) {
    windowlist[i] = allwindows[i];
  }
  free(treereply);

  // Scan for windows
  for (i = 0; i < num; i++) {
    xcb_window_t window = windowlist[i];

    wacookie = xcb_get_window_attributes(s->con, window);
    wareply = xcb_get_window_attributes_reply(s->con, wacookie, NULL);
    if (!wareply) {
      continue; 
    }

    if (wareply->override_redirect || wareply->map_state != XCB_MAP_STATE_VIEWABLE) {
      free(wareply);
      continue;
    }

    transientcookie = xcb_get_property(s->con, 0, window, atomtransientfor, XCB_ATOM_WINDOW, 0, sizeof(xcb_window_t));
    transientreply = xcb_get_property_reply(s->con, transientcookie, NULL);
    if (transientreply) {
      if (xcb_get_property_value_length(transientreply) > 0) {
        transientlist = xcb_get_property_value(transientreply);
        transientwindow = *transientlist;
        transientwacookie = xcb_get_window_attributes(s->con, transientwindow);
        transientwareply = xcb_get_window_attributes_reply(s->con, transientwacookie, NULL);
        if (transientwareply &&
          transientwareply->map_state == XCB_MAP_STATE_VIEWABLE) {
          makeclient(s, transientwindow);
        }
        free(transientwareply);
      }
      free(transientreply);
    }

    free(wareply);
  }

  // Clean up
  if (windowlist) {
    free(windowlist);
  }
}

client_t*
makeclient(state_t* s, xcb_window_t win) {
  // Setup listened events for the mapped window
  {
    uint32_t evmask[] = { XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_POINTER_MOTION };
    xcb_change_window_attributes_checked(s->con, win, XCB_CW_EVENT_MASK, evmask);
  }

  // Grabbing mouse events for interactive moves/resizes 
  {
    uint16_t evmask = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION;
    xcb_grab_button(s->con, 0, win, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s->root, XCB_NONE, 1, s->config.winmod);
    xcb_grab_button(s->con, 0, win, evmask, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, 
                    s->root, XCB_NONE, 3, s->config.winmod);
  }

  // Adding the mapped client to our linked list
  client_t* cl = addclient(s, &s->clients, win);


  // Set initial border 
  setbordercolor(s, cl, s->config.winbordercolor);
  setborderwidth(s, cl, s->config.winborderwidth);


  // Set window type of client (e.g dialog)
  setwintype(s, cl);

  // Set client's monitor
  cl->mon = clientmon(s, cl);
  cl->desktop = mondesktop(s, s->monfocus)->idx;

  // Update the EWMH client list
  ewmh_updateclients(s);

  bool success;
  cl->area = winarea(s, cl->frame, &success);
  if(!success) return NULL;

  cl->area.size = applysizehints(s, cl, cl->area.size);
  if(!cl->floating) {
    cl->floating = cl->fixed;
  }

  if(s->monfocus) {
    resizeclient(s, cl, cl->area.size);
    // Spawn the window in the center of the focused monitor
    moveclient(s, cl, (v2_t){
      s->monfocus->area.pos.x + (s->monfocus->area.size.x - cl->area.size.x) / 2.0f, 
      s->monfocus->area.pos.y + (s->monfocus->area.size.y - cl->area.size.y) / 2.0f});
  }

  // Map the window
  xcb_map_window(s->con, win);

  // Map the window on the screen
  xcb_map_window(s->con, cl->frame);

  // Retrieving cursor position
  bool cursor_success;
  v2_t cursor = cursorpos(s, &cursor_success);
  if(!cursor_success) return NULL;
  // If the cursor is on the mapped window when it spawned, focus it.
  if(pointinarea(cursor, cl->area)) {
    focusclient(s, cl);
  }
  // Raise the newly created client over all other clients
  raiseclient(s, cl);

  return cl;
}

/**
 * @brief Evaluates if a given point is inside a given area 
 *
 * @param p The point to check if it is inside the given area
 * @param area The area to check
 *
 * @return True if the point p is in the area, false if it is not in the area 
 */
bool
pointinarea(v2_t p, area_t area) {
  return (p.x >= area.pos.x &&
      p.x < (area.pos.x + area.size.x) &&
      p.x >= area.pos.y &&
      p.y < (area.pos.y + area.size.y));
}

/**
 * @brief Returns the current cursor position on the X display 
 *
 * @param s The window manager's state
 * @param success Gets assigned whether or not the pointer query
 * was successfull.
 *
 * @return The cursor position as a two dimensional vector 
 */
v2_t 
cursorpos(state_t* s, bool* success) {
  // Query the pointer position
  xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(s->con, xcb_query_pointer(s->con, s->root), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    logmsg(s,  LogLevelError, "failed to retrieve cursor position."); 
    free(reply);
    return (v2_t){0};
  }
  // Create a v2_t for to store the position 
  v2_t cursor = (v2_t){.x = reply->root_x, .y = reply->root_y};

  // Check for errors
  free(reply);
  return cursor;
}

/**
 * @brief Returns the area (position and size) of a given window 
 *
 * @param s The window manager's state
 * @param win The window to get the area from 
 * @param success Gets assigned whether or not the geometry query
 * was successfull.
 *
 * @return The area of the given window 
 */
area_t
winarea(state_t* s, xcb_window_t win, bool* success) {
  // Retrieve the geometry of the window 
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(s->con, xcb_get_geometry(s->con, win), NULL);
  *success = (reply != NULL);
  if(!(*success)) {
    logmsg(s,  LogLevelError, "failed to retrieve window geometry of window %i", win); 
    free(reply);
    return (area_t){0};
  }
  // Creating the area structure to store the geometry in
  area_t a = (area_t){.pos = (v2_t){reply->x, reply->y}, .size = (v2_t){reply->width, reply->height}};

  free(reply);
  return a;
}

/**
 * @brief Creates a X window with a trucolor visual 
 * and colormap.
 *
 * @param s The window manager's state
 * @param a The geometry of the window 
 * @param bw The border width of the window 
 *
 * @return The ID of the created X window 
 */
xcb_window_t
truecolorwindow(state_t* s, area_t a, uint32_t bw) {
  xcb_window_t win;
  XVisualInfo vinfo;
  if (!XMatchVisualInfo(s->dsp, DefaultScreen(s->dsp), 32, TrueColor, &vinfo)) {
    logmsg(s,  LogLevelError, "no true-color visual found.");
    terminate(s, EXIT_FAILURE);
  }

  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(s->dsp, DefaultRootWindow(s->dsp), vinfo.visual, AllocNone);
  attr.border_pixel = bw;
  attr.background_pixel = 0;
  attr.override_redirect = True; 

  unsigned long mask = CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect;

  win = XCreateWindow(s->dsp, DefaultRootWindow(s->dsp),
      a.pos.x, a.pos.y, a.size.x, a.size.y,
      s->config.winborderwidth, vinfo.depth, InputOutput, vinfo.visual,
      mask, &attr);

  logmsg(s,  LogLevelTrace, "created window with true-color colormap.",
         a.pos.x, a.pos.y, a.size.x, a.size.y);

  return win;
}

/**
 * @brief Gets the area (in float) of the overlap between two areas 
 *
 * @param a The inner area 
 * @param a The outer area 
 *
 * @return The overlap between the two areas 
 */
float
getoverlaparea(area_t a, area_t b) {
  float x = MAX(0, MIN(a.pos.x + a.size.x, b.pos.x + b.size.x) - MAX(a.pos.x, b.pos.x));
  float y = MAX(0, MIN(a.pos.y + a.size.y, b.pos.y + b.size.y) - MAX(a.pos.y, b.pos.y));
  return x * y;
}

/**
 * @brief Sets the border color of a given clients window 
 *
 * @param s The window manager's state
 * @param cl The client to set the border color of 
 * @param color The border color 
 */
void
setbordercolor(state_t* s, client_t* cl, uint32_t color) {
  // Return if the client is NULL
  if(!cl) {
    return;
  }
  // Change the configuration for the border color of the clients window
  xcb_change_window_attributes_checked(s->con, cl->frame, XCB_CW_BORDER_PIXEL, &color);
}

/**
 * @brief Sets the border width of a given clients window 
 * and updates its 'borderwidth' variable 
 *
 * @param s The window manager's state
 * @param cl The client to set the border width of 
 * @param width The border width 
 */
void
setborderwidth(state_t* s, client_t* cl, uint32_t width) {
  if(!cl) {
    return;
  }
  // Change the configuration for the border width of the clients window
  xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_BORDER_WIDTH, &(uint32_t){width});
  // Update the border width of the client
  cl->borderwidth = width;
}

/**
 * @brief Moves the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to move 
 * @param pos The position to move the client to 
 */
void
moveclient(state_t* s, client_t* cl, v2_t pos) {
  if(!cl) {
    return;
  }
  int32_t posval[2] = {
    (uint32_t)pos.x, (uint32_t)pos.y
  };

  // Move the window by configuring it's x and y position property
  xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);

  cl->area.pos = pos;

  configclient(s, cl);
  updatetitlebar(s, cl);

  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(s, cl);
  cl->desktop = mondesktop(s, cl->mon)->idx;
  uploaddesktopnames(s, s->monfocus);
  s->monfocus = cl->mon;
}

/**
 * @brief Resizes the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to resize 
 * @param pos The new size of the clients window 
 */
void
resizeclient(state_t* s, client_t* cl, v2_t size) {
  if (!cl) {
    return;
  }

  uint32_t sizeval[2] = { (uint32_t)size.x, (uint32_t)size.y };
  uint32_t sizeval_content[2] = { (uint32_t)size.x, (uint32_t)size.y - ((cl->showtitlebar) ? s->config.titlebarheight : 0.0f)};

  // Resize the window by configuring its width and height property
  xcb_configure_window(s->con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval_content);
  xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);

  updatetitlebar(s, cl);

  cl->area.size = size;
}

/**
 * @brief Moves and resizes the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to move and resize
 * @param a The new area (position and size) for the client's window
 */
void
moveresizeclient(state_t* s, client_t* cl, area_t a) {
  if (!cl) {
    return;
  }
  uint32_t values[4] = {
    (uint32_t)a.pos.x, 
    (uint32_t)a.pos.y,
    (uint32_t)a.size.x,
    (uint32_t)a.size.y
  };
  uint32_t values_content[4] = {
    (uint32_t)a.size.x,
    (uint32_t)a.size.y - ((cl->showtitlebar) ? s->config.titlebarheight : 0.0f)
  };

  // Move and resize the window by configuring its x, y, width, and height properties
  xcb_configure_window(s->con, cl->frame, 
      XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
  xcb_configure_window(s->con, cl->win, 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values_content);

  // Update clients area
  cl->area = a;
  // Update focused monitor in case the window was moved onto another monitor
  cl->mon = clientmon(s, cl);
  uploaddesktopnames(s, s->monfocus);

  s->monfocus = cl->mon; 
  updatetitlebar(s, cl);
}

/**
 * @brief Raises the window of a given client to the top of the stack
 *
 * @param s The window manager's state
 * @param cl The client to raise 
 */
void
raiseclient(state_t* s, client_t* cl) {
  if(!cl) {
    return;
  }
  uint32_t config[] = { XCB_STACK_MODE_ABOVE };
  // Change the configuration of the window to be above 
  xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_STACK_MODE, config);
}


/**
 * @brief Checks if a client has a WM_DELETE atom set 
 *
 * @param s The window manager's state
 * @param cl The client to check delete atom for
 *
 * @return Whether or not the given client has a WM_DELETE atom 
 */
bool 
clienthasdeleteatom(state_t* s, client_t* cl) {
  bool ret = false;
  xcb_icccm_get_wm_protocols_reply_t reply;

  if(xcb_icccm_get_wm_protocols_reply(s->con, xcb_icccm_get_wm_protocols_unchecked(s->con, cl->win, s->wm_atoms[WMprotocols]), &reply, NULL)) {
    for(uint32_t i = 0; !ret && i < reply.atoms_len; i++) {
      if(reply.atoms[i] == s->wm_atoms[WMdelete]) {
	ret = true;
      }
    }
    xcb_icccm_get_wm_protocols_reply_wipe(&reply);
  }

  return ret;
}

/**
 * @brief Returns wether or not a client should be added 
 * to the tiling layout based on window properties:
 * (_NET_WM_WINDOW_TYPE, NET_WM_WINDOW_TYPE_NORMAL)
 *
 * @param s The window manager's state
 * @param cl The client to check if it should tile for 
 *
 * @return Whether or not a given client should tile 
 */
bool 
clientshouldtile(state_t* s, client_t* cl) {
  // Get atoms
  xcb_atom_t wintype_atom = getatom(s, "_NET_WM_WINDOW_TYPE");
  xcb_atom_t wintypenormal_atom = getatom(s, "_NET_WM_WINDOW_TYPE_NORMAL");

  // Get window property for type
  xcb_get_property_cookie_t cookie = xcb_get_property(s->con, 0, cl->win, wintype_atom, XCB_ATOM_ATOM, 0, 1);
  xcb_get_property_reply_t* propreply = xcb_get_property_reply(s->con, cookie, NULL);

  if (!propreply) {
    return false;
  }
  xcb_atom_t* propval = (xcb_atom_t*)xcb_get_property_value(propreply);
  xcb_atom_t proptype = xcb_get_property_value_length(propreply) > 0 ? propval[0] : XCB_ATOM_NONE;

  free(propreply);

  // Check if the type of the window is not _NET_WM_WINDOW_TYPE_NORMAL
  return proptype != wintypenormal_atom;
}
/**
 * @brief Checks if a client is on a given monitor and if it is 
 * currently visible (on the current desktop) 
 *
 * @param cl The client to check visibility for 
 *
 * @return Whether or not the given client is visible and on the given monitor 
 */
bool 
clientonscreen(state_t* s, client_t* cl, monitor_t* mon) {
  return cl->mon == mon && cl->desktop == mondesktop(s, cl->mon)->idx;
}

/**
 * @brief Retrieves the name of a given client (allocates memory) 
 *
 * @param s The window manager's state
 * @param cl The client to retrieve a name from  
 *
 * @return The name of the given client  
 */
char* 
getclientname(state_t* s, client_t* cl) {
  xcb_icccm_get_text_property_reply_t prop;
  // Try to get _NET_WM_NAME
  xcb_get_property_cookie_t cookie = xcb_icccm_get_text_property(s->con, cl->win, XCB_ATOM_WM_NAME);
  if (xcb_icccm_get_text_property_reply(s->con, cookie, &prop, NULL)) {
    char* name = strndup(prop.name, prop.name_len);
    xcb_icccm_get_text_property_reply_wipe(&prop);
    return name;
  }

  // If _NET_WM_NAME is not available, try WM_NAME
  cookie = xcb_icccm_get_text_property(s->con, cl->win, XCB_ATOM_WM_NAME);
  if (xcb_icccm_get_text_property_reply(s->con, cookie, &prop, NULL)) {
    char *name = strndup(prop.name, prop.name_len);
    xcb_icccm_get_text_property_reply_wipe(&prop);
    return name;
  }

  return NULL;
}

/**
 * @brief Kills a given client by destroying the associated window and 
 * removing it from the linked list.
 *
 * @param s The window manager's state
 * @param cl The client to kill 
 */
void
killclient(state_t* s, client_t* cl) {
  // If the client specifically has a delete atom set, send a delete event  
  if(clienthasdeleteatom(s, cl)) {
    xcb_client_message_event_t ev;
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = cl->win;
    ev.format = 32;
    ev.data.data32[0] = s->wm_atoms[WMdelete];
    ev.data.data32[1] = XCB_TIME_CURRENT_TIME;
    ev.type = s->wm_atoms[WMprotocols];
    xcb_send_event(s->con, false, cl->win, XCB_EVENT_MASK_NO_EVENT, (const char*)&ev);
  } else {
    // Force kill the client without sending an event
    xcb_grab_server(s->con);
    xcb_set_close_down_mode(s->con, XCB_CLOSE_DOWN_DESTROY_ALL);
    xcb_kill_client(s->con, cl->win);
    xcb_ungrab_server(s->con);
  }
  makelayout(s, s->monfocus);
  xcb_flush(s->con);
}


/**
 * @brief Sets the current focus to a given client
 *
 * @param s The window manager's state
 * @param cl The client to focus
 */

// Helper to reduce EWMH property changes 
static monitor_t* lastmon;

void
focusclient(state_t* s, client_t* cl) {
  if(!cl || cl->win == s->root) {
    return;
  }

  s->ignore_enter_layout = false;

  // Unfocus the previously focused window to ensure that there is only
  // one focused (highlighted) window at a time.
  if(s->focus) {
    unfocusclient(s, s->focus);
  }

  // Set input focus 
  setxfocus(s, cl);

  if(!cl->fullscreen) {
    // Change border color to indicate selection
    setbordercolor(s, cl, s->config.winbordercolor_selected);
    setborderwidth(s, cl, s->config.winborderwidth);
  }

  // Set the focused client
  s->focus = cl;
  uploaddesktopnames(s, s->monfocus);
  s->monfocus = cursormon(s);

  if(s->monfocus != lastmon && lastmon != NULL) {
    desktop_t* desk = mondesktop(s, s->monfocus); 

    if(desk) {
      xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHcurrentDesktop],
                          XCB_ATOM_CARDINAL, 32, 1, &desk->idx);
      xcb_flush(s->con);
    }
  }
  lastmon = s->monfocus;
}

/**
 * @brief Sets the X input focus to a given client and handles EWMH atoms & 
 * focus event.
 *
 * @param s The window manager's state
 * @param cl The client to set the input focus to
 */
void
setxfocus(state_t* s, client_t* cl) {
  // Set input focus to client
  xcb_set_input_focus(s->con, XCB_INPUT_FOCUS_POINTER_ROOT, cl->win, XCB_CURRENT_TIME);

  // Set active window hint
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, cl->win, s->ewmh_atoms[EWMHactiveWindow],
      XCB_ATOM_WINDOW, 32, 1, &cl->win);

  // Raise take-focus event on the client
  raiseevent(s, cl, s->wm_atoms[WMtakeFocus]);
}

/**
 * @brief Creates a frame window for the content of a client window and decoration to live in.
 * The frame window encapsulates the client's window and decoration like titlebar.
 * focus event.
 *
 * @param s The window manager's state
 * @param cl The client to create a frame window for 
 */
void
frameclient(state_t* s, client_t* cl) {
  // Create the frame window 
  {
    cl->frame = truecolorwindow(s, cl->area, s->config.winborderwidth);

    // Select input events 
    unsigned long event_mask = StructureNotifyMask | SubstructureNotifyMask | SubstructureRedirectMask |
      PropertyChangeMask | EnterWindowMask | FocusChangeMask | ButtonPressMask;
    XSelectInput(s->dsp, cl->frame, event_mask); 
  }
  setuptitlebar(s, cl);

  // Reparent the client's content to the newly created frame
  xcb_reparent_window(s->con, cl->win, cl->frame, 0, s->showtitlebars ? s->config.titlebarheight : 0);

  // Update the window's geometry
  {
    // Update the window's position by configuring it's X and Y property
    int32_t posval[2] = {(int32_t)cl->area.pos.x, (int32_t)cl->area.pos.y};
    xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, posval);

    uint32_t sizeval[2] = { (uint32_t)cl->area.size.x, (uint32_t)cl->area.size.y };
    uint32_t sizeval_content[2] = { (uint32_t)cl->area.size.x, (uint32_t)cl->area.size.y - 
      (s->showtitlebars ? s->config.titlebarheight : 0)};
    // Resize the window by configuring it's width and height property
    xcb_configure_window(s->con, cl->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval_content);
    xcb_configure_window(s->con, cl->frame, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, sizeval);
  }

  // Send configure notify eventevent  to the client 
  configclient(s, cl);
}

/**
 * @brief Destroys and unmaps the frame window of a given client 
 * which consequently removes the client's window from the display
 *
 * @param s The window manager's state
 * @param cl The client to unframe 
 */
void
unframeclient(state_t* s, client_t* cl) {
  xcb_unmap_window(s->con, cl->titlebar);
  xcb_unmap_window(s->con, cl->frame);
  xcb_reparent_window(s->con, cl->win, s->root, 0, 0);
  xcb_destroy_window(s->con, cl->titlebar);
  xcb_destroy_window(s->con, cl->frame);
  xcb_flush(s->con);
}

/**
 * @brief Removes the focus from client's window by setting the 
 * X input focus to the root window and unsetting the highlight color 
 * of the window's border.
 *
 * @param s The window manager's state
 * @param cl The client to unfocus
 */
void
unfocusclient(state_t* s, client_t* cl) {
  if (!cl) {
    return;
  }
  setbordercolor(s, cl, s->config.winbordercolor);
  xcb_set_input_focus(s->con, XCB_INPUT_FOCUS_POINTER_ROOT, s->root, XCB_CURRENT_TIME);
  xcb_delete_property(s->con, s->root, s->ewmh_atoms[EWMHactiveWindow]);

  cl->ignoreexpose = false;
}

void 
removescratchpad(state_t* s, uint32_t idx) {
  s->scratchpads[idx].needs_restart = true;
}

/**
 * @brief Notifies a given client window about it's configuration 
 * (geometry) by sending a configure notify event to it.
 *
 * @param s The window manager's state
 * @param cl The client to send a configure notify event to 
 */
void
configclient(state_t* s, client_t* cl) {
  if(!cl) {
    return;
  }
  xcb_configure_notify_event_t event;
  memset(&event, 0, sizeof(event));

  event.response_type = XCB_CONFIGURE_NOTIFY;
  event.window = cl->win;
  event.event = cl->win;
  // New X position of the window
  event.x = cl->area.pos.x;                
  // New Y position of the window
  event.y = cl->area.pos.y + ((cl->showtitlebar) ? s->config.titlebarheight : 0.0f);
  // New width of the window
  event.width = cl->area.size.x;            
  // New height of the window
  event.height = cl->area.size.y - ((cl->showtitlebar) ? s->config.titlebarheight : 0.0f);
  // Border width
  event.border_width = cl->borderwidth;
  // Above sibling window (None in this case)
  event.above_sibling = XCB_NONE;
  // Override-redirect flag
  event.override_redirect = 0;
  // Send the event
  xcb_send_event(s->con, 0, cl->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *)&event);
}

/*
 * @brief Hides a given client by unframing it's window
 *
 * @param s The window manager's state
 * @param cl The client to hide 
 */
void
hideclient(state_t* s, client_t* cl) {
  cl->ignoreunmap = true;
  cl->hidden = true;
  xcb_unmap_window(s->con, cl->frame);
}

/*
 * @brief Shows a given client by unframing it's window
 *
 * @param s The window manager's state
 * @param cl The client to show 
 */
void
showclient(state_t* s, client_t* cl) {
  cl->hidden = false;
  xcb_map_window(s->con, cl->frame);
}

/**
 * @brief Sends a given X event by atom to the window of a given client
 *
 * @param s The window manager's state
 * @param cl The client to send the event to
 * @param protocol The atom to store the event
 */
bool
raiseevent(state_t* s, client_t* cl, xcb_atom_t protocol) {
  bool exists = false;
  xcb_icccm_get_wm_protocols_reply_t reply;

  // Checking if the event protocol exists
  if (xcb_icccm_get_wm_protocols_reply(s->con, xcb_icccm_get_wm_protocols(s->con, cl->win, s->wm_atoms[WMprotocols]), &reply, NULL)) {
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
    event.type = s->wm_atoms[WMprotocols];
    event.format = 32;
    event.data.data32[0] = protocol;
    event.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(s->con, 0, cl->win, XCB_EVENT_MASK_NO_EVENT, (const char *)&event);
  }
  return exists;
}


/**
 * @brief Checks for window type of clients by EWMH and acts 
 * accoringly.
 *
 * @param cl The client to set/update the window type of
 * @param s The window manager's state
 */
void
setwintype(state_t* s, client_t* cl) {
  xcb_atom_t state = getclientprop(s, cl, s->ewmh_atoms[EWMHstate]);
  xcb_atom_t wintype = getclientprop(s, cl, s->ewmh_atoms[EWMHwindowType]);

  if(state == s->ewmh_atoms[EWMHfullscreen]) {
    setfullscreen(s, cl, true);
    hidetitlebar(s, cl);
  } 
  if(wintype == s->ewmh_atoms[EWMHwindowTypeDialog]) {
    cl->floating = true;
  }
}

/**
 * @brief Sets/unsets urgency flag on a given client 
 * accoringly.
 *
 * @param s The window manager's state
 * @param cl The client to set/unset urgency on 
 * @param urgent The urgency of the client's window
 */
void
seturgent(state_t* s, client_t* cl, bool urgent) {
  xcb_icccm_wm_hints_t wmh;
  cl->urgent = urgent;

  xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_hints(s->con, cl->win);
  if (!xcb_icccm_get_wm_hints_reply(s->con, cookie, &wmh, NULL)) {
    return;
  }

  if (urgent) {
    wmh.flags |= XCB_ICCCM_WM_HINT_X_URGENCY;
  } else {
    wmh.flags &= ~XCB_ICCCM_WM_HINT_X_URGENCY;
  }
  xcb_icccm_set_wm_hints(s->con, cl->win, &wmh);
}

/**
 * @brief Returns the next on-screen client after the 
 * focused client.
 *
 * @param s The window manager's state
 * @param skip_floating Whether or not to skip floating clients
 */
client_t* 
nextvisible(state_t* s, bool skip_floating) {
  client_t* next = NULL;
  // Find the next client on the current monitor & desktop 
  for(client_t* cl = s->focus->next; cl != NULL; cl = cl->next) {
    bool checktiled = (skip_floating) ? !cl->floating : true;
    if(checktiled && clientonscreen(s, cl, s->monfocus)) {
      next = cl;
      break;
    }
  }

  // If there is no next client, cycle back to the first client on the 
  // current monitor & desktop
  if(!next) {
    for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    bool checktiled = (skip_floating) ? !cl->floating : true;
      if(checktiled && clientonscreen(s, cl, s->monfocus)) {
	next = cl;
	break;
      }
    }
  }
  return next;
 }

/**
 * @brief Gets the value of a given property on a 
 * window of a given client.
 *
 * @param s The window manager's state
 * @param cl The client to retrieve the property value from
 * @param prop The property to retrieve
 *
 * @return The value of the given property on the given window
 */
xcb_atom_t
getclientprop(state_t* s, client_t* cl, xcb_atom_t prop) {
  xcb_generic_error_t *error;
  xcb_atom_t atom = XCB_NONE;
  xcb_get_property_cookie_t cookie;
  xcb_get_property_reply_t *reply;

  // Get the property from the window
  cookie = xcb_get_property(s->con, 0, cl->win, prop, XCB_ATOM_ATOM, 0, sizeof(xcb_atom_t));
  // Get the reply
  reply = xcb_get_property_reply(s->con, cookie, &error);

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

/**
 * @brief Puts a given client in or out of fullscreen
 * mode based on the input.
 *
 * @param s The window manager's state
 * @param cl The client to toggle fullscreen of 
 * @param fullscreen Whether the client should be set 
 * fullscreen or not
 */
void
setfullscreen(state_t* s, client_t* cl, bool fullscreen) {
  if(!s->monfocus || !cl) return;

  cl->fullscreen = fullscreen;
  if(cl->fullscreen) {
    xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, cl->win, s->ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 1, &s->ewmh_atoms[EWMHfullscreen]);
    // Store previous position of client
    cl->area_prev = cl->area;
    // Store previous floating state of client
    cl->floating_prev = cl->floating;
    cl->floating = true;
    // Set the client's area to the focused monitors area, effictivly
    // making the client as large as the monitor screen
    cl->area = s->monfocus->area;

    // Unset border of client if it's fullscreen
    cl->borderwidth = 0;
  } else {
    xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, cl->win, s->ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    // Set the client's area to the area before the last fullscreen occured 
    cl->area = cl->area_prev;
    cl->floating = cl->floating_prev;
    cl->borderwidth = s->config.winborderwidth;
  }
  // Update client's border width
  setborderwidth(s, cl, cl->borderwidth);

  // Update the clients geometry 
  moveresizeclient(s, cl, cl->area);

  // Send configure event to the client
  configclient(s, cl);

  // Raise fullscreened clients
  raiseclient(s, cl);

  // If the client is 'sudo fullscreen', fullscreen it on the WM
  if(cl->area.size.x >= cl->mon->area.size.x && cl->area.size.y >= cl->mon->area.size.y && !cl->fullscreen) {
    setfullscreen(s, cl, true);
  }

  makelayout(s, s->monfocus);
}

/**
 * @brief Switches the desktop of a given client and hides that client.
 *
 * @param s The window manager's state
 * @param cl The client to switch the desktop of
 * @param desktop The desktop to set the client of 
 */
void
switchclientdesktop(state_t* s, client_t* cl, int32_t desktop) {
  char** names = (char**)malloc(s->monfocus->desktopcount * sizeof(char*));
  for (size_t i = 0; i < s->monfocus->desktopcount ; i++) {
    names[i] = strdup(s->monfocus->activedesktops[i].name);
  }
  // Create the desktop if it was not created yet
  if(!strinarr(names, s->monfocus->desktopcount, s->config.desktopnames[desktop])) {
    createdesktop(s, desktop, s->monfocus);
  }
  free(names);

  cl->desktop = desktop;
  if(cl == s->focus) {
    unfocusclient(s, cl);
  }
  hideclient(s, cl);
  makelayout(s, s->monfocus);
}

/**
 * Returns how many client are currently in the layout on a 
 * given monitor.
 *
 * @param s The window manager's state
 * @param mon The monitor to count clients on 
 * */
uint32_t
numinlayout(state_t* s, monitor_t* mon) {
  uint32_t nlayout = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, mon) && !cl->floating) {
      nlayout++;
    }
  }
  return nlayout;
}

layout_type_t
getcurlayout(state_t* s, monitor_t* mon) {
  desktop_t* desk = mondesktop(s, mon);
  if(!desk) {
    return LayoutFloating;
  }
  uint32_t deskidx = desk->idx;
  return mon->layouts[deskidx].curlayout;
}

/**
 * @brief Establishes the current tiling layout for the windows
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void
makelayout(state_t* s, monitor_t* mon) {
  layout_type_t curlayout = getcurlayout(s, mon); 
  if(curlayout == LayoutFloating) return;

  /* Make sure that there is always at least one slave window */
  uint32_t nlayout = numinlayout(s, s->monfocus);
  uint32_t deskidx = mondesktop(s, mon)->idx;
  while(nlayout - mon->layouts[deskidx].nmaster == 0 && nlayout != 1) {
    mon->layouts[deskidx].nmaster--;
  }
  

  switch(curlayout) {
    case LayoutTiledMaster:  {
      tiledmaster(s, mon);
      break;
    }
    case LayoutVerticalStripes: {
      verticalstripes(s, mon);
      break;
    }
    case LayoutHorizontalStripes: {
      horizontalstripes(s, mon);
      break;
    }
    default: {
        break;
      }
  }
}

/**
 * @brief Resets the size modifications of 
 * all client windows within the layout.
 *
 * @param s The window manager's state
 * @param mon The monitor on which the layout is 
 */
void 
resetlayoutsizes(state_t* s, monitor_t* mon) {
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating ||
      !clientonscreen(s, cl, mon)) continue;

    cl->layoutsizeadd = 0.0f;
  }
}

/**
 * @brief Establishes a tiled master layout for the windows that are 
 * currently visible.
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void 
tiledmaster(state_t* s, monitor_t* mon) {
  if(!mon) return;

  uint32_t nslaves    = 0, nmaster = 0;
  uint32_t deskidx    = mondesktop(s, mon)->idx;
  int32_t gapsize     = mon->layouts[deskidx].gapsize;
  float   masterarea  = mon->layouts[deskidx].masterarea;

  enumartelayout(s, mon, &nmaster, &nslaves);

  uint32_t w = mon->area.size.x;
  uint32_t h = mon->area.size.y;
  int32_t x = mon->area.pos.x;
  int32_t y = mon->area.pos.y;

  // Apply strut information to the layout
  for(uint32_t i = 0; i < s->nwinstruts; i++) {
    bool onmonitor = 
      s->winstruts[i].startx >= mon->area.pos.x  
      && s->winstruts[i].endx <= mon->area.pos.x + mon->area.size.x;

    if(!onmonitor) continue;

    if(s->winstruts[i].left != 0) {
      x += s->winstruts[i].left;
      w -= s->winstruts[i].left;
    }
    if(s->winstruts[i].right != 0) {
      w -= s->winstruts[i].right;
    }
    if(s->winstruts[i].top != 0) {
      y += s->winstruts[i].top;
      h -= s->winstruts[i].top;
    }
    if(s->winstruts[i].bottom != 0) {
      h -= s->winstruts[i].bottom;
    }
  }

  int32_t ymaster = y;
  float wmaster = w * masterarea;

  mon->layouts[deskidx].mastermaxed = false;

  uint32_t i = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || 
      cl->desktop != mondesktop(s, cl->mon)->idx || 
      cl->mon != mon) continue;
    if(i >= nmaster) break;

    if(wmaster <= cl->minsize.x && cl->minsize.x != 0) {
      wmaster = cl->minsize.x;
      mon->layouts[deskidx].mastermaxed = true;
      break;
    }
  }

  i = 0;

  float lastadd = 0.0f;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating ||
      cl->desktop != mondesktop(s, cl->mon)->idx || 
      cl->mon != mon) continue;

    bool ismaster = (i < nmaster);


    float height = ((float)h / (ismaster ? nmaster : nslaves)) + cl->layoutsizeadd
     - lastadd;

    lastadd = cl->layoutsizeadd;
    float width = (ismaster ? (uint32_t)wmaster : (uint32_t)w - wmaster);

    if(width > cl->maxsize.x && cl->maxsize.x != 0) {
      width = cl->maxsize.x;
    } if(width < cl->minsize.x && cl->minsize.x != 0) {
      width = cl->minsize.x;
    } if(height >= cl->maxsize.y && cl->maxsize.y != 0) {
      height = cl->maxsize.y;
    } if(height < cl->minsize.y && cl->minsize.y != 0) {
      height = cl->minsize.y;
    }

    bool singleclient = !nslaves;

    moveclient(s, cl, (v2_t){
      (ismaster ? x : (int32_t)(x + wmaster)) + gapsize,
      (ismaster ? ymaster : y) + gapsize});
    resizeclient(s, cl, (v2_t){
      ((singleclient ? w : width) 
      - cl->borderwidth * 2) - gapsize * 2,
      (height - cl->borderwidth * 2) - gapsize * 2});

    if(!ismaster) {
      y += height;
    } else {
      ymaster += height;
    }
    i++;
  }
}

/**
 * @brief Establishes a layout in which windows are 
 * layed out left to right as vertical stripes.
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void  
verticalstripes(state_t* s, monitor_t* mon) {
 if(!mon) return;

  uint32_t nwins = 0;
  uint32_t deskidx    = mondesktop(s, mon)->idx;
  int32_t gapsize     = mon->layouts[deskidx].gapsize;

  {
    for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
      if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx
        || cl->mon != mon) continue;
      nwins++;
    }
  }

  uint32_t w = mon->area.size.x;
  uint32_t h = mon->area.size.y;
  int32_t x = mon->area.pos.x;
  int32_t y = mon->area.pos.y;

  // Apply strut information to the layout
  for(uint32_t i = 0; i < s->nwinstruts; i++) {
    bool onmonitor = 
      s->winstruts[i].startx >= mon->area.pos.x  
      && s->winstruts[i].endx <= mon->area.pos.x + mon->area.size.x;

    if(!onmonitor) continue;

    if(s->winstruts[i].left != 0) {
      x += s->winstruts[i].left;
      w -= s->winstruts[i].left;
    }
    if(s->winstruts[i].right != 0) {
      w -= s->winstruts[i].right;
    }
    if(s->winstruts[i].top != 0) {
      y += s->winstruts[i].top;
      h -= s->winstruts[i].top;
    }
    if(s->winstruts[i].bottom != 0) {
      h -= s->winstruts[i].bottom;
    }
  }
  

  float lastadd = 0.0f;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx || cl->mon != mon) continue;

    float winw = (float)w / nwins + cl->layoutsizeadd - lastadd;
    lastadd = cl->layoutsizeadd;

    moveclient(s, cl, (v2_t){
      x + gapsize,
      y + gapsize});
    resizeclient(s, cl, (v2_t){
      winw - cl->borderwidth * 2 - gapsize * 2,
      h - cl->borderwidth * 2 - gapsize * 2});

    x += winw;
  }
}

/**
 * @brief Establishes a layout in which windows are 
 * layed out top to bottom as horizontal stripes 
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void
horizontalstripes(state_t* s, monitor_t* mon) {
  if(!mon) return;

  uint32_t nwins = 0;
  uint32_t deskidx    = mondesktop(s, mon)->idx;
  int32_t gapsize     = mon->layouts[deskidx].gapsize;

  {
    for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
      if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx
        || cl->mon != mon) continue;
      nwins++;
    }
  }

  uint32_t w = mon->area.size.x;
  uint32_t h = mon->area.size.y;
  int32_t x = mon->area.pos.x;
  int32_t y = mon->area.pos.y;

  // Apply strut information to the layout
  for(uint32_t i = 0; i < s->nwinstruts; i++) {
    bool onmonitor = 
      s->winstruts[i].startx >= mon->area.pos.x  
      && s->winstruts[i].endx <= mon->area.pos.x + mon->area.size.x;

    if(!onmonitor) continue;

    if(s->winstruts[i].left != 0) {
      x += s->winstruts[i].left;
      w -= s->winstruts[i].left;
    }
    if(s->winstruts[i].right != 0) {
      w -= s->winstruts[i].right;
    }
    if(s->winstruts[i].top != 0) {
      y += s->winstruts[i].top;
      h -= s->winstruts[i].top;
    }
    if(s->winstruts[i].bottom != 0) {
      h -= s->winstruts[i].bottom;
    }
  }

  float lastadd = 0.0f;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx || cl->mon != mon) continue;

    float winh = (float)h / nwins + cl->layoutsizeadd - lastadd;
    lastadd = cl->layoutsizeadd;

    moveclient(s, cl, (v2_t){
      x + gapsize,
      y + gapsize});
    resizeclient(s, cl, (v2_t){
      w - cl->borderwidth * 2 - gapsize * 2,
      winh - cl->borderwidth * 2 - gapsize * 2});

    y += winh;
  }
}


/**
 * @brief Swaps two clients within the linked list of clients 
 *
 * @param s The window manager's state 
 * @param c1 The client to swap with c2 
 * @param c2 The client to swap with c1
 */
void 
swapclients(state_t* s, client_t* c1, client_t* c2) {
  if (c1 == c2) {
    return;
  }

  client_t *prev1 = NULL, 
	   *prev2 = NULL, 
	   *tmp = s->clients;

  while (tmp && tmp != c1) {
    prev1 = tmp;
    tmp = tmp->next;
  }

  if (tmp == NULL) return;

  tmp = s->clients;

  while (tmp && tmp != c2) {
    prev2 = tmp;
    tmp = tmp->next;
  }

  if (tmp== NULL) return;

  if (prev1) {
    prev1->next = c2;
  } else { 
    s->clients = c2;
  }

  if (prev2) {
    prev2->next = c1;
  } else { 
    s->clients = c1;
  }

  tmp = c1->next;
  c1->next = c2->next;
  c2->next = tmp;
}

/**
 * @brief Uploads the active desktop names and sends them to EWMH 
 * @param s The window manager's state
 */
void
uploaddesktopnames(state_t* s, monitor_t* mon) {
  // Calculate the total length of the property value
  size_t total_length = 0;
  for (uint32_t i = 0; i < mon->desktopcount; i++) {
    if(!mon->activedesktops[i].init) continue;
    total_length += strlen(mon->activedesktops[i].name) + 1; // +1 for the null byte
  }

  // Allocate memory for the data
  char* data = malloc(total_length);

  // Concatenate the desktop names into the data buffer
  char* ptr = data;
  for (uint32_t i = 0; i < mon->desktopcount; i++) {
    if(!mon->activedesktops[i].init) continue;
    strcpy(ptr, mon->activedesktops[i].name);
    ptr += strlen(mon->activedesktops[i].name) + 1; 
  }

  // Get the root window
  xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(s->con)).data;
  xcb_window_t root_window = screen->root;

  // Set the _NET_DESKTOP_NAMES property
  xcb_change_property(s->con,
                      XCB_PROP_MODE_REPLACE,
                      root_window,
                      s->ewmh_atoms[EWMHdesktopNames],
                      XCB_ATOM_STRING,
                      8,
                      total_length,
                      data);
  free(data);
}

/**
 * @brief Creates a new virtual desktop and notifies EWMH about it.
 * @param s The window manager's state 
 * @param idx The index of the virtual desktop
 * @param mon The monitor that the virtual desktop is on
 */
void
createdesktop(state_t* s, uint32_t idx, monitor_t* mon) {
  mon->activedesktops[mon->desktopcount].name = malloc(strlen(s->config.desktopnames[idx]) + 1);
  mon->activedesktops[mon->desktopcount].init = false; 
  if(mon->activedesktops[mon->desktopcount].name != NULL) {
    strcpy(mon->activedesktops[mon->desktopcount].name, s->config.desktopnames[idx]);
  }

  mon->desktopcount++;

  logmsg(s,  LogLevelTrace, "created virtual desktop %i.\n", idx);
}


/**
 * @brief Initializes all important atoms for EWMH &
 * NetWM compatibility.
 *
 * @param s The window manager's state 
 * */
void
setupatoms(state_t* s) {
  s->wm_atoms[WMprotocols]             = getatom(s, "WM_PROTOCOLS");
  s->wm_atoms[WMdelete]                = getatom(s, "WM_DELETE_WINDOW");
  s->wm_atoms[WMstate]                 = getatom(s, "WM_STATE");
  s->wm_atoms[WMtakeFocus]             = getatom(s, "WM_TAKE_FOCUS");
  s->ewmh_atoms[EWMHactiveWindow]      = getatom(s, "_NET_ACTIVE_WINDOW");
  s->ewmh_atoms[EWMHsupported]         = getatom(s, "_NET_SUPPORTED");
  s->ewmh_atoms[EWMHname]              = getatom(s, "_NET_WM_NAME");
  s->ewmh_atoms[EWMHstate]             = getatom(s, "_NET_WM_STATE");
  s->ewmh_atoms[EWMHstateHidden]       = getatom(s, "_NET_WM_STATE_HIDDEN");
  s->ewmh_atoms[EWMHcheck]             = getatom(s, "_NET_SUPPORTING_WM_CHECK");
  s->ewmh_atoms[EWMHfullscreen]        = getatom(s, "_NET_WM_STATE_FULLSCREEN");
  s->ewmh_atoms[EWMHwindowType]        = getatom(s, "_NET_WM_WINDOW_TYPE");
  s->ewmh_atoms[EWMHwindowTypeDialog]  = getatom(s, "_NET_WM_WINDOW_TYPE_DIALOG");
  s->ewmh_atoms[EWMHclientList]        = getatom(s, "_NET_CLIENT_LIST");
  s->ewmh_atoms[EWMHcurrentDesktop]    = getatom(s, "_NET_CURRENT_DESKTOP");
  s->ewmh_atoms[EWMHnumberOfDesktops]  = getatom(s, "_NET_NUMBER_OF_DESKTOPS");
  s->ewmh_atoms[EWMHdesktopNames]      = getatom(s, "_NET_DESKTOP_NAMES");

  xcb_atom_t utf8str = getatom(s, "UTF8_STRING");

  xcb_window_t wmcheckwin = xcb_generate_id(s->con);
  xcb_create_window(s->con, XCB_COPY_FROM_PARENT, wmcheckwin, s->root, 
      0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, 
      XCB_COPY_FROM_PARENT, 0, NULL);

  // Set _NET_WM_CHECK property on the wmcheckwin
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, wmcheckwin, s->ewmh_atoms[EWMHcheck],
      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_WM_NAME property on the wmcheckwin
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, wmcheckwin, s->ewmh_atoms[EWMHname],
      utf8str, 8, strlen("ragnar"), "ragnar");

  // Set _NET_WM_CHECK property on the root window
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHcheck],
      XCB_ATOM_WINDOW, 32, 1, &wmcheckwin);

  // Set _NET_CURRENT_DESKTOP property on the root window
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHcurrentDesktop],
      XCB_ATOM_CARDINAL, 32, 1, &s->config.desktopinit);

  // Set _NET_SUPPORTED property on the root window
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHsupported],
      XCB_ATOM_ATOM, 32, EWMHcount, s->ewmh_atoms);

  // Delete _NET_CLIENT_LIST property from the root window
  xcb_delete_property(s->con, s->root, s->ewmh_atoms[EWMHclientList]);

  s->monfocus = cursormon(s);
  // Create initial desktop for all monitors 
  for(monitor_t* mon = s->monitors; mon != NULL; mon = mon->next) {
    for(uint32_t i = 0; i < s->config.maxdesktops; i++) {
      createdesktop(s, i, mon);
    }
    mon->activedesktops[s->config.desktopinit].init = true;
  }

  int32_t desktopcount = 1;
  // Set number of desktops (_NET_NUMBER_OF_DESKTOPS)
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHnumberOfDesktops],
                      XCB_ATOM_CARDINAL, 32, 1, &desktopcount);
  uploaddesktopnames(s, s->monfocus);

  xcb_flush(s->con);
}


/**
 * @brief Grabs all the keybinds specified in the config for the window 
 * manager. The function also ungrabs all previously grabbed keys
 *
 * @param s The window manager's state 
 * */
void
grabkeybinds(state_t* s) {
  // Ungrab any grabbed keys
  xcb_ungrab_key(s->con, XCB_GRAB_ANY, s->root, XCB_MOD_MASK_ANY);

  // Grab every keybind
  for (size_t i = 0; i < s->config.numkeybinds; ++i) {
    // Get the keycode for the keysym of the keybind
    xcb_keycode_t *keycode = getkeycodes(s, s->config.keybinds[i].key);
    // Grab the key if it is valid 
    if (keycode != NULL) {
      xcb_grab_key(s->con, 1, s->root, s->config.keybinds[i].modmask, *keycode,
	  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
      logmsg(s,  LogLevelTrace, "grabbed key '%s' on X server.",
             XKeysymToString(s->config.keybinds[i].key));

    }
  }
  xcb_flush(s->con);
}

/**
 * @brief Loads and sets the default cursor image of the window manager.
 * The default image is the left facing pointer.
 * */
void
loaddefaultcursor(state_t* s) {
  xcb_cursor_context_t* context;
  // Create the cursor context
  if (xcb_cursor_context_new(s->con, s->screen, &context) < 0) {
    logmsg(s,  LogLevelError, "cannot create cursor context.");
    terminate(s, EXIT_FAILURE);
  }
  // Load the context
  xcb_cursor_t cursor = xcb_cursor_load_cursor(context, s->config.cursorimage); 

  // Set the cursor to the root window
  xcb_change_window_attributes(s->con, s->root, XCB_CW_CURSOR, &cursor);

  // Flush the requests to the X server
  xcb_flush(s->con);

  // Free allocated resources
  xcb_cursor_context_free(context);

  logmsg(s,  LogLevelTrace, "loaded cursor image '%s'.", s->config.cursorimage);
}

/**
 * @brief Sets up the titlebar window for a given client.
 * This involves:
 *  - initializing an OpenGL & leif context if there is none
 *  - creating an X window for the titlebar and reparenting it 
 *  - Setting OpenGL context to the created window 
 *  - Setting swap interval for the window 
 *  - Rendering the initial state of the titlebar
 *
 *  @param s The window manager's state 
 *  @param cl The client to setup a titlebar window for
 * */
void
setuptitlebar(state_t* s, client_t* cl) {
  if(!s->config.usedecoration) return;

  // Initialize OpenGL
  if(!s->initgl) {
    initglcontext(s);
    s->initgl = true;
  }


  Colormap colormap = DefaultColormap(s->dsp, DefaultScreen(s->dsp));

  // Initialize background color
  XColor color;
  color.red   = ((s->config.titlebarcolor >> 16) & 0xFF) * 256; 
  color.green = ((s->config.titlebarcolor >> 8) & 0xFF) * 256;  
  color.blue  = (s->config.titlebarcolor & 0xFF) * 256;
  color.flags = DoRed | DoGreen | DoBlue;

  // Allocate the color in the colormap
  if (!XAllocColor(s->dsp, colormap, &color)) {
    logmsg(s,  LogLevelError, "unable to allocate X color.");
    return;
  }


  Window root = DefaultRootWindow(s->dsp);
  XSetWindowAttributes attribs;
  attribs.colormap = XCreateColormap(s->dsp, s->root, s->glvis->visual, AllocNone); 
  attribs.event_mask = EnterWindowMask | LeaveWindowMask | ExposureMask | StructureNotifyMask;
  attribs.background_pixel = color.pixel;

  cl->titlebar = (xcb_window_t)XCreateWindow(s->dsp, root, 0, 0, cl->area.size.x,
      s->config.titlebarheight, 0, s->glvis->depth, InputOutput, s->glvis->visual,
      CWColormap | CWEventMask | CWBackPixel, &attribs);
  XReparentWindow(s->dsp, cl->titlebar, cl->frame, 0, 0);
  XMapWindow(s->dsp, cl->titlebar);
  XSync(s->dsp, false);
 
  // Grab Buttons
  {
    unsigned int event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    // Grab Button 1
    XGrabButton(s->dsp, Button1, AnyModifier, cl->titlebar, 
	False, event_mask, GrabModeAsync, GrabModeAsync, None, None);

    // Grab Button 3
    XGrabButton(s->dsp, Button3, AnyModifier, cl->titlebar, 
	False, event_mask, GrabModeAsync, GrabModeAsync, None, None);
  }

  // Set OpenGL context
  setglcontext(s, cl->titlebar);

  // Set GL swap interval which disables or enables vsync 
  PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = 
    (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
  if (glXSwapIntervalEXT) {
    glXSwapIntervalEXT(s->dsp, cl->titlebar, s->config.glvsync); 
    logmsg(s,  LogLevelTrace, "Set swap interval of titlebar window %i to %i (%s).",
           cl->titlebar, (int32_t)s->config.glvsync, s->config.glvsync ? "vsync" : "no vsync");
  } else {
    logmsg(s,  LogLevelError, "GLX_EXT_swap_control not supported.");
  }
  rendertitlebar(s, cl);

  if(!s->showtitlebars) {
    hidetitlebar(s, cl);
  }
}


/**
 * @brief Renders the content of the titlebar of a given client 
 * to the associated titlebar window. By default, this renders the 
 * name of the client, a close button and a additional 'put-in-layout'
 * button if the titlebar is hovered.
 *
 * @param s The window manager's state
 * @param cl The client to render the titlebar of
 * */
void
rendertitlebar(state_t* s, client_t* cl) {
  if(!s->config.usedecoration) return;
  if(!cl) return;
  if(cl->desktop != mondesktop(s, s->monfocus)->idx) return;
  setglcontext(s, cl->titlebar);

  // Clear background
  {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  glXSwapBuffers(s->dsp, cl->titlebar);
}

/**
 * @brief Updates the geometry of the titlebar of a given client 
 * to match the client's geometry.
 *
 * @param s The window manager's state
 * @param cl The client of which to update the titlebar geometry of 
 * */
void
updatetitlebar(state_t* s, client_t* cl) {
  if(!s->config.usedecoration) return;

  bool success;
  // Update client geometry 
  cl->area = winarea(s, cl->frame, &success);
  if(!success) return;

  // Update decoration geometry
  uint32_t vals[2];
  vals[0] = cl->area.size.x;
  vals[1] = s->config.titlebarheight;
  xcb_configure_window(s->con, cl->titlebar, 
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
}

/**
 * @brief Hides the titlebar window of a given client 
 *
 * @param s The window manager's state
 * @param cl The client of which to hide the titlebar of 
 * */
void
hidetitlebar(state_t* s, client_t* cl) {
  xcb_unmap_window(s->con, cl->titlebar);
  {
    uint32_t vals[2] = {0, 0};
    xcb_configure_window(s->con, cl->win, 
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
  }
  {
    uint32_t vals[2] = {cl->area.size.x, cl->area.size.y};
    xcb_configure_window(s->con, cl->win, 
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
  }
  cl->showtitlebar = false;
}

/**
 * @brief Takes in a size for a client window and adjusts it 
 * if it does not meet the requirements of the client's hints.
 * The adjusted value is returned.
 *
 * @param s The window manager's state
 * @param cl The client of which to get the adjusted size of
 * @param size The size before applying hints 
 *
 * @return The size after applying hints
 * */
v2_t 
applysizehints(state_t* s, client_t* cl, v2_t size) {
  // Retrieve size hints
  xcb_size_hints_t hints;
  if (xcb_icccm_get_wm_normal_hints_reply(s->con, xcb_icccm_get_wm_normal_hints(s->con, cl->win), &hints, NULL)) {
    // Enforce minimum size
    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
      if (size.x < hints.min_width) size.x = hints.min_width;
      if (size.y < hints.min_height) size.y = hints.min_height;
    }

    // Enforce maximum size
    if (hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) {
      if (size.x > hints.max_width) size.x = hints.max_width;
      if (size.y > hints.max_height) size.y = hints.max_height;
    }
  }

  cl->minsize.x = hints.min_width;
  cl->minsize.y = hints.min_height;

  cl->maxsize.x = hints.max_width;
  cl->maxsize.y = hints.max_height;

  // Check if client is fixed size
  cl->fixed = (hints.max_width != 0 && hints.max_height != 0 &&
               hints.max_width == hints.min_width && 
               hints.max_height == hints.min_height);
  return size;
}


/**
 * @brief Adds a given client to the current tiling layout
 *
 * @param s The window manager's state
 * @param cl The client to add to the layout 
 * */
void
addtolayout(state_t* s, client_t* cl)  {
  cl->floating = false;
  // Add all fullscreened clients to the layout
  for(client_t* it = s->clients; it != NULL; it = it->next) {
    if(clientonscreen(s, it, s->monfocus) && it->fullscreen) {
      setfullscreen(s, it, false);
      it->floating = false; 
    }
  }
  makelayout(s, cl->mon);
}

/**
 * @brief Returns the number of master- and slave windows 
 * within a given layout
 *
 * @param s The window manager's state
 * @param mon The monitor on which the layout is 
 * @param nmaster The number of master windows [out] 
 * @param nslaves The number of slave windows [out] 
 * */
void 
enumartelayout(state_t* s, monitor_t* mon, uint32_t* nmaster, uint32_t* nslaves) {
  uint32_t deskidx = mondesktop(s, mon)->idx;
  *nmaster = mon->layouts[deskidx].nmaster;

  layout_type_t curlayout = getcurlayout(s, mon);
  if( curlayout == LayoutVerticalStripes || 
      curlayout == LayoutHorizontalStripes) {
    *nmaster = 0; 
  } 

  uint32_t i = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx
      || cl->mon != mon) continue;
    if(i >= *nmaster) {
      *nslaves = *nslaves + 1;
    }
    i++;
  }
}

/**
 * @brief Checks if a given client is a master window 
 * within layouts that can have master windows.
 *
 * @param s The window manager's state
 * @param cl The client to check if it is a master window 
 * @param mon The monitor on which the layout is 
 * */
bool 
isclientmaster(state_t* s, client_t* cl, monitor_t* mon) {
  layout_type_t curlayout = getcurlayout(s, mon);
  if( curlayout == LayoutVerticalStripes || 
      curlayout == LayoutHorizontalStripes) {
    return false;
  } 

  uint32_t i = 0; 
  uint32_t deskidx = mondesktop(s, mon)->idx;
  uint32_t nmaster = mon->layouts[deskidx].nmaster;

  for(client_t* iter = s->clients; iter != NULL; iter = iter->next) {
    if(iter->floating ||
      iter->desktop != mondesktop(s, iter->mon)->idx || 
      iter->mon != mon) continue;

    if(iter == cl && i < nmaster) {
      return true;
    }
    i++;
  }
  return false;
}

/**
 * @brief Removes a given client from the current tiling layout
 *
 * @param s The window manager's state
 * @param cl The client to remove from the layout 
 * */
void 
removefromlayout(state_t* s, client_t* cl) {
  cl->floating = true;
  makelayout(s, cl->mon);

  // Apply the size hints of the clients if it left 
  // the layout. 
  cl->area.size = applysizehints(s, cl, cl->area.size);
  resizeclient(s, cl, cl->area.size);
}

/**
 * @brief Shows the titlebar window of a given client 
 *
 * @param s The window manager's state
 * @param cl The client of which to show the titlebar of 
 * */
void
showtitlebar(state_t* s, client_t* cl) {
  xcb_map_window(s->con, cl->titlebar);
  {
    uint32_t vals[2] = {0, s->config.titlebarheight};
    xcb_configure_window(s->con, cl->win, 
	XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
  }
  {
    uint32_t vals[2] = {cl->area.size.x, cl->area.size.y - s->config.titlebarheight};
    xcb_configure_window(s->con, cl->win, 
	XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
  }
  cl->showtitlebar = true;
}

/**
 * @brief Handles a map request event on the X server by 
 * adding the mapped window to the linked list of clients and 
 * setting up necessary stuff.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
evmaprequest(state_t* s, xcb_generic_event_t* ev) {
  xcb_map_request_event_t* map_ev = (xcb_map_request_event_t*)ev;

  // Retrieve attributes of the mapped window
  xcb_get_window_attributes_cookie_t wa_cookie = xcb_get_window_attributes(s->con, map_ev->window);
  xcb_get_window_attributes_reply_t *wa_reply = xcb_get_window_attributes_reply(s->con, wa_cookie, NULL);

  // Return if attributes could not be retrieved or if the window uses override_redirect
  if (!wa_reply) {
    // If wa_reply is null, there was an issue with the request
    return;
  }

  if (wa_reply->override_redirect) {
    // The window is not managed by the window manager
    free(wa_reply);
    return;
  }

  // Free the reply after checking
  free(wa_reply);

  // Don't handle already managed clients
  if (clientfromwin(s, map_ev->window) != NULL) {
    return;
  }

  // Handle new client
  client_t* cl = makeclient(s, map_ev->window);

  resetlayoutsizes(s, s->monfocus); 

  for(client_t* it = s->clients; it != NULL; it = it->next) {
    if(clientonscreen(s, it, s->monfocus) && it->fullscreen) {
      setfullscreen(s, it, false);
      it->floating = false; 
    }
  }

  cl->scratchpad_index = s->mapping_scratchpad_index;
  cl->is_scratchpad = s->mapping_scratchpad_index != -1;

  if(s->mapping_scratchpad_index == -1) {
    makelayout(s, s->monfocus);
  } else {
    cl->floating = true;

    s->scratchpads[s->mapping_scratchpad_index] = (scratchpad_t) {
      .win = cl->win,
      .hidden = false,
      .needs_restart = false,
    };
    s->mapping_scratchpad_index = -1;
  }

  xcb_flush(s->con);
}

/**
 * @brief Handles a X unmap event by unmapping the window 
 * and removing the associated client from the linked list.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
evunmapnotify(state_t* s, xcb_generic_event_t* ev) {
  // Retrieve the event
  xcb_unmap_notify_event_t* unmap_ev = (xcb_unmap_notify_event_t*)ev;

  client_t* cl = clientfromwin(s, unmap_ev->window);
  if(cl && cl->ignoreunmap) {
    cl->ignoreunmap = false;
    return;
  }


  if(cl) {
    if(cl->is_scratchpad) {
      removescratchpad(s, cl->scratchpad_index);
    }
    unframeclient(s, cl);
  } else {
    xcb_unmap_window(s->con, unmap_ev->window);
  }

  // Remove the client from the list
  releaseclient(s, unmap_ev->window);

  // Update the EWMH client list
  ewmh_updateclients(s);

  // Reset the sizes of all clients
  resetlayoutsizes(s, s->monfocus); 

  // Re-establish the window layout
  makelayout(s, s->monfocus);

  xcb_flush(s->con);
}

/**
 * @brief Handles an X destroy event by unframing the client 
 * associated with the event and removing it from the list of 
 * clients.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
evdestroynotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_destroy_notify_event_t* destroy_ev = (xcb_destroy_notify_event_t*)ev;
  client_t* cl = clientfromwin(s, destroy_ev->window);
  if(!cl) {
    return;
  }
  if(cl->is_scratchpad) {
    removescratchpad(s, cl->scratchpad_index);
  }
  unframeclient(s, cl);
  releaseclient(s, destroy_ev->window);
}

/**
 * @brief Handles a X enter-window event by focusing the window
 * that's associated with the enter event.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
eventernotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_enter_notify_event_t *enter_ev = (xcb_enter_notify_event_t*)ev;
  if(s->ignore_enter_layout) return;

  if((enter_ev->mode != XCB_NOTIFY_MODE_NORMAL || enter_ev->detail == XCB_NOTIFY_DETAIL_INFERIOR)
      && enter_ev->event != s->root) {
    return;
  }

  client_t* cl = clientfromwin(s, enter_ev->event);
  bool istitlebar = false;
  if(!cl) {
    cl = clientfromtitlebar(s, enter_ev->event);
    istitlebar = cl != NULL;

    if(!cl && enter_ev->event != s->root) return;
  }

  // Focus entered client
  if(cl) {
    if(istitlebar) {
      cl->titlebar_render_additional = true;
      rendertitlebar(s, cl);
    }
    focusclient(s, cl);
    uploaddesktopnames(s, s->monfocus);
  }
  else if(enter_ev->event == s->root) {
    // Set Input focus to root
    xcb_set_input_focus(s->con, XCB_INPUT_FOCUS_POINTER_ROOT, s->root, XCB_CURRENT_TIME);
    /* Reset border color to unactive for every client */
    client_t* cl;
    for (cl = s->clients; cl != NULL; cl = cl->next) {
      if(cl->fullscreen) {
        continue;
      }
      setbordercolor(s, cl, s->config.winbordercolor);
      setborderwidth(s, cl, s->config.winborderwidth);
    }
  }

  xcb_flush(s->con);
 }


/**
 * @brief Handles a X leave-window event by unsetting 
 * the titlebar_render_additional on the associated titlebar which 
 * un-renders the additional UI of it. 
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
evleavenotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_leave_notify_event_t* leave_ev = (xcb_leave_notify_event_t*)ev;
  client_t* cl = clientfromtitlebar(s, leave_ev->event);
  if(!cl) return;
  cl->titlebar_render_additional = false;
  rendertitlebar(s, cl);

  xcb_flush(s->con);
}

/**
 * @brief Handles a X key press event by checking if the pressed 
 * key (and modifiers) match any window manager keybind and then executing
 * that keybinds function.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evkeypress(state_t* s, xcb_generic_event_t* ev) {
  xcb_key_press_event_t *e = ( xcb_key_press_event_t *) ev;
  // Get associated keysym for the keycode of the event
  xcb_keysym_t keysym = getkeysym(s, e->detail);

  /* Iterate throguh the keybinds and check if one of them was pressed. */
  for (uint32_t i = 0; i < s->config.numkeybinds; ++i) {
    // If it was pressed, call the callback of the keybind
    if ((keysym == s->config.keybinds[i].key) && (e->state == s->config.keybinds[i].modmask)) {
      if(s->config.keybinds[i].cb) {
        s->config.keybinds[i].cb(s, s->config.keybinds[i].data);
      }
    }
  }
  xcb_flush(s->con);
}

/**
 * @brief Handles a X button press event by focusing the client 
 * associated with the pressed window and setting cursor and window grab positions->
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evbuttonpress(state_t* s, xcb_generic_event_t* ev) {
  xcb_button_press_event_t* button_ev = (xcb_button_press_event_t*)ev;

  // Get the window attributes
  xcb_get_window_attributes_cookie_t attr_cookie = xcb_get_window_attributes(s->con, button_ev->event);
  xcb_get_window_attributes_reply_t* attr_reply = xcb_get_window_attributes_reply(s->con, attr_cookie, NULL);

  if (attr_reply) {
    if (attr_reply->override_redirect) {
      free(attr_reply);
      return; // Do nothing if override_redirect is set
    }
    free(attr_reply);
  } else {
    // Unable to get window attributes
    return;
  }

  client_t* cl = clientfromwin(s, button_ev->event);
  if (!cl) {
    cl = clientfromtitlebar(s, button_ev->event);
    if (!cl) return;
    v2_t cursorpos = (v2_t){.x = (float)button_ev->root_x - cl->area.pos.x, .y = (float)button_ev->root_y - cl->area.pos.y};
    area_t closebtnarea = (area_t){
      .pos = cl->closebutton,
      .size = (v2_t){30, s->config.titlebarheight}
    };
    area_t layoutbtnarea = (area_t){
      .pos = cl->layoutbutton,
      .size = (v2_t){30, s->config.titlebarheight}
    };
    if (pointinarea(cursorpos, closebtnarea)) {
      killclient(s, cl);
      return;
    }
    if (pointinarea(cursorpos, layoutbtnarea) &&
      getcurlayout(s, cl->mon) != LayoutFloating && cl->floating &&
      cl->titlebar_render_additional) {
      addtolayout(s, cl);
      return;
    }
    focusclient(s, cl);
  } else {
    // Focusing client 
    if (cl != s->focus) {
      // Unfocus the previously focused window to ensure that there is only
      // one focused (highlighted) window at a time.
      if (s->focus) {
        unfocusclient(s, s->focus);
      }
      focusclient(s, cl);
    }
  }
  bool success;
  area_t area = winarea(s, cl->frame, &success);
  if(!success) return;

  cl->area = area;
  // Setting grab position
  s->grabwin = cl->area;
  s->grabcursor = (v2_t){.x = (float)button_ev->root_x, .y = (float)button_ev->root_y};

  cl->ignoreexpose = false;

  // Raising the client to the top of the stack
  raiseclient(s, cl);
  xcb_flush(s->con);
}

/**
 * @brief Handles a X button release event by fullscreening the associated client 
 * if the event happend on the client's titlebar and the mouse position is <= 0.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evbuttonrelease(state_t* s, xcb_generic_event_t* ev) {
  xcb_button_release_event_t* button_ev = (xcb_button_release_event_t*)ev;
  client_t* cl = clientfromtitlebar(s, button_ev->event);
  if(!cl) return;

  if(button_ev->root_y <= 0) {
    setfullscreen(s, cl, true);
  }
  cl->ignoreexpose = false;
  rendertitlebar(s, cl);
  xcb_flush(s->con);
}


// Function definition
void print_to_file(const char *filename, const char *format, ...) {
  FILE *file = fopen(filename, "a"); // Open file for appending
  if (file == NULL) {
    perror("Error opening file");
    return;
  }

  va_list args;
  va_start(args, format);
  vfprintf(file, format, args); // Format and print to the file
  va_end(args);

  fclose(file);
}

/**
 * @brief Handles a X motion notify event by moving the clients window if left mouse 
 * button is held and resizing the clients window if right mouse is held. 
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evmotionnotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)ev;

  // Throttle motiton notify events for performance and to avoid jiterring on certain 
  // high polling-rate mouses   
  uint32_t curtime = motion_ev->time;
  if((curtime - s->lastmotiontime) <= (1000.0 / s->config.motion_notify_debounce_fps)) {
    return;
  }
  s->lastmotiontime = curtime;

  // Position of the cursor in the drag event
  v2_t dragpos    = (v2_t){.x = (float)motion_ev->root_x, .y = (float)motion_ev->root_y};
  // Drag difference from the current drag event to the initial grab 
  v2_t dragdelta  = (v2_t){.x = dragpos.x - s->grabcursor.x, .y = dragpos.y - s->grabcursor.y};
  // New position of the window
  v2_t movedest   = (v2_t){.x = (float)(s->grabwin.pos.x + dragdelta.x), .y = (float)(s->grabwin.pos.y + dragdelta.y)};

  client_t* cl = clientfromwin(s, motion_ev->event);

  if((!(motion_ev->state & s->config.movebtn) && 
    !(motion_ev->state & s->config.resizebtn) && cl)) {
    if(s->ignore_enter_layout) {
      s->ignore_enter_layout = false;
    }
    if(pointinarea(dragpos, cl->area)) {
      if(cl != s->focus) {
        focusclient(s, cl);
      }
    }
    xcb_flush(s->con);
    return;
  }

  if(motion_ev->event == s->root) {
    // Update the focused monitor to the monitor under the cursor
    monitor_t* mon = cursormon(s);
    uploaddesktopnames(s, s->monfocus);
    s->monfocus = mon;
    desktop_t* desk = mondesktop(s, s->monfocus);
    if(desk) {
      xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHcurrentDesktop],
                          XCB_ATOM_CARDINAL, 32, 1, &desk->idx);
    }
    xcb_flush(s->con);
    return;
  }
  if(!(motion_ev->state & s->config.resizebtn || motion_ev->state & s->config.movebtn)) return;

  if(!cl) {
    if(!(cl = clientfromtitlebar(s, motion_ev->event))) return;
    if(cl->fullscreen) {
      setfullscreen(s, cl, false);
      s->grabwin = cl->area;
      movedest = (v2_t){.x = (float)(s->grabwin.pos.x + dragdelta.x), .y = (float)(s->grabwin.pos.y + dragdelta.y)};
    }
    moveclient(s, cl, movedest);

    if(!cl->floating) {
      // Remove the client from the layout when the user moved it 
      resetlayoutsizes(s, s->monfocus);
      removefromlayout(s, cl);
    }
    xcb_flush(s->con);
    return;
  }
  if(cl->fullscreen) {
    // Unset fullscreen
    cl->fullscreen = false;
    xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, cl->win, s->ewmh_atoms[EWMHstate], XCB_ATOM_ATOM, 32, 0, 0); 
    cl->borderwidth = s->config.winborderwidth;
    setborderwidth(s, cl, cl->borderwidth);
    showtitlebar(s, cl);
  }

  // Move the window
  if(motion_ev->state & s->config.movebtn) {
    moveclient(s, cl, movedest);
  } 
  // Resize the window
  else if(motion_ev->state & s->config.resizebtn) {
    // Resize delta (clamped)
    v2_t resizedelta  = (v2_t){.x = MAX(dragdelta.x, -s->grabwin.size.x), .y = MAX(dragdelta.y, -s->grabwin.size.y)};
    // New window size
    v2_t sizedest = (v2_t){.x = s->grabwin.size.x + resizedelta.x, .y = s->grabwin.size.y + resizedelta.y};

    sizedest = applysizehints(s, cl, sizedest);
    resizeclient(s, cl, sizedest);
    cl->ignoreexpose = true;
  }

  if(!cl->floating) {
    // Remove the client from the layout when the user moved it 
    resetlayoutsizes(s, s->monfocus);
    removefromlayout(s, cl);
  }

  xcb_flush(s->con);
}

/**
 * @brief Handles a Xorg configure request by configuring the client that 
 * is associated with the window how the event requested it.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void 
evconfigrequest(state_t* s, xcb_generic_event_t* ev) {
  xcb_configure_request_event_t *config_ev = (xcb_configure_request_event_t *)ev;

  client_t* cl = clientfromwin(s, config_ev->window);
  if(!cl) {
    uint16_t mask = 0;
    uint32_t values[7];
    uint32_t i = 0;

    if (config_ev->value_mask & XCB_CONFIG_WINDOW_X) {
      mask |= XCB_CONFIG_WINDOW_X;
      values[i++] = config_ev->x;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_Y) {
      mask |= XCB_CONFIG_WINDOW_Y;
      values[i++] = config_ev->y;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
      mask |= XCB_CONFIG_WINDOW_WIDTH;
      values[i++] = config_ev->width;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
      mask |= XCB_CONFIG_WINDOW_HEIGHT;
      values[i++] = config_ev->height;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
      mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
      values[i++] = config_ev->border_width;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
      mask |= XCB_CONFIG_WINDOW_SIBLING;
      values[i++] = config_ev->sibling;
    }
    if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
      mask |= XCB_CONFIG_WINDOW_STACK_MODE;
      values[i++] = config_ev->stack_mode;
    }

    // Configure the window with the specified values
    xcb_configure_window(s->con, config_ev->window, mask, values);
  } else {
    if(getcurlayout(s, cl->mon) != LayoutFloating && !cl->floating) return;
    {
      uint16_t mask = 0;
      uint32_t values[5];
      uint32_t i = 0;
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
        mask |= XCB_CONFIG_WINDOW_WIDTH;
        values[i++] = config_ev->width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
        mask |= XCB_CONFIG_WINDOW_HEIGHT;
        values[i++] = config_ev->height;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        values[i++] = config_ev->border_width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
        mask |= XCB_CONFIG_WINDOW_SIBLING;
        values[i++] = config_ev->sibling;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = config_ev->stack_mode;
      }
      xcb_configure_window(s->con, cl->frame, mask, values);
    }
    {
      uint16_t mask = 0;
      uint32_t values[7];
      uint32_t i = 0;
      mask |= XCB_CONFIG_WINDOW_X;
      mask |= XCB_CONFIG_WINDOW_Y;
      values[i++] = 0;
      values[i++] = ((cl->showtitlebar) ?  s->config.titlebarheight : 0.0f);
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
        mask |= XCB_CONFIG_WINDOW_WIDTH;
        values[i++] = config_ev->width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
        mask |= XCB_CONFIG_WINDOW_HEIGHT;
        values[i++] = config_ev->height - ((cl->showtitlebar ? s->config.titlebarheight : 0.0f));
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
        mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
        values[i++] = config_ev->border_width;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
        mask |= XCB_CONFIG_WINDOW_SIBLING;
        values[i++] = config_ev->sibling;
      }
      if (config_ev->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
        mask |= XCB_CONFIG_WINDOW_STACK_MODE;
        values[i++] = config_ev->stack_mode;
      }
      xcb_configure_window(s->con, cl->win, mask, values);
    }
    bool success;
    cl->area = winarea(s, cl->frame, &success);
    if(!success) return;

    updatetitlebar(s, cl);
    configclient(s, cl);
  }

  xcb_flush(s->con);
}


/*
 * @brief Handles a X configure notify event. If the root window 
 * recieved a configure notify event, the list of monitors is refreshed.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 * */
void
evconfignotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_configure_notify_event_t* config_ev = (xcb_configure_notify_event_t*)ev;

  // If the root window configures itself, update the monitor arrangment
  if(config_ev->window == s->root) {
    updatemons(s);
  }

  // Update the client's titlebar geometry
  client_t* cl = clientfromwin(s, config_ev->window);
  if(!cl) return;
  updatetitlebar(s, cl);

  xcb_flush(s->con);
}

/**
 * @brief Handles a X property notify event for window properties by handling various Extended Window Manager Hints (EWMH). 
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evpropertynotify(state_t* s, xcb_generic_event_t* ev) {
  xcb_property_notify_event_t* prop_ev = (xcb_property_notify_event_t*)ev;
  client_t* cl = clientfromwin(s, prop_ev->window);

  if(cl) {
    // Updating the window type if we receive a window type change event.
    if(prop_ev->atom == s->ewmh_atoms[EWMHwindowType]) {
      setwintype(s, cl);
    }
    if(s->config.usedecoration) {
      if(prop_ev->atom == s->ewmh_atoms[EWMHname]) {
	if(cl->name)
	  free(cl->name);
	cl->name = getclientname(s, cl);
	rendertitlebar(s, cl);
      }
    }
  }
  xcb_flush(s->con);
}

/**
 * @brief Handles a X client message event by setting fullscreen or 
 * urgency (based on the event request).
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evclientmessage(state_t* s, xcb_generic_event_t* ev) {
  xcb_client_message_event_t* msg_ev = (xcb_client_message_event_t*)ev;
  client_t* cl = clientfromwin(s, msg_ev->window);

  if(!cl) {
    return;
  }

  if(msg_ev->type == s->ewmh_atoms[EWMHstate]) {
    // If client requested fullscreen toggle
    if(msg_ev->data.data32[1] == s->ewmh_atoms[EWMHfullscreen] ||
	msg_ev->data.data32[2] == s->ewmh_atoms[EWMHfullscreen]) {
      // Set/unset client fullscreen 
      bool fs =  (msg_ev->data.data32[0] == 1 || (msg_ev->data.data32[0] == 2 && !cl->fullscreen));
      setfullscreen(s, cl, fs);
      if(s->showtitlebars) {
        if(fs) {
          hidetitlebar(s, cl);
        } else {
          showtitlebar(s, cl);
        }
      }
    }
  } else if(msg_ev->type == s->ewmh_atoms[EWMHactiveWindow]) {
    if(s->focus != cl && !cl->urgent) {
      seturgent(s, cl, true);
    }
  }
  xcb_flush(s->con);
}

/**
 * @brief Handles a X expose event by redrawing the content of 
 * the associated titlebar with the event.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void
evexpose(state_t* s, xcb_generic_event_t* ev) {
  xcb_expose_event_t* expose_ev = (xcb_expose_event_t*)ev;
  client_t* cl = clientfromtitlebar(s, expose_ev->window);
  rendertitlebar(s, cl);
  xcb_flush(s->con);
}

/**
 * @brief Adds a client window to the linked list of clients->
 *
 * @param s The window manager's state
 * @param win The window to create a client from and add it 
 * to the clients
 *
 * @return The newly created client
 */
client_t*
addclient(state_t* s, client_t** clients, xcb_window_t win) {
  // Allocate client structure
  client_t* cl = (client_t*)malloc(sizeof(*cl));
  cl->win = win;

  /* Get the window area */
  bool success;
  area_t area = winarea(s, win, &success);
  if(!success) return cl;

  cl->area = area;
  cl->borderwidth = s->config.winborderwidth;
  cl->fullscreen = false;
  cl->hidden = false;
  cl->floating = getcurlayout(s, s->monfocus) == LayoutFloating;
  cl->titlebar_render_additional = false;
  cl->name = getclientname(s, cl);
  cl->showtitlebar = s->config.usedecoration;
  cl->layoutsizeadd = 0;

  // Create frame window for the client
  frameclient(s, cl);

  // Insert the new client at the beginning of the list
  cl->next = *clients;
  // Update the head of the list to the new client
  *clients = cl;


  logmsg(s,  LogLevelTrace, "Added client ('%s') to the linked list of clients.", 
         cl->name ? cl->name : "No name");

  return cl;
}

/**
 * @brief Removes a given client from the list of clients
 * by window.
 *
 * @param s The window manager's state
 * @param win The window of the client to remove
 */
void 
releaseclient(state_t* s, xcb_window_t win) {
  client_t** prev = &s->clients;
  client_t* cl = s->clients;
  /* Iterate throguh the clients to find the client thats 
   * associated with the window */
  while(cl) {
    if(cl->win == win) {
      /* Setting the pointer to previous client to the next client 
       * after the client we want to release, effectivly removing it
       * from our list of clients*/ 
      *prev = cl->next;
      // Freeing memory allocated for client
      free(cl);
      return;
    }
    // Advancing the client
    prev = &cl->next;
    cl = cl->next;
  }
}

/**
 * @brief Returns the associated client from a given window.
 * Returns NULL if there is no client associated with the window.
 *
 * @param s The window manager's state
 * @param win The window to get the client from
 *
 * @return The client associated with the given window (NULL if no associated client)
 */
client_t*
clientfromwin(state_t* s, xcb_window_t win) {
  client_t* cl;
  for (cl = s->clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->win == win) {
      return cl;
    }
  }
  return NULL;
}

/**
 * @brief Returns a filtered linked list of all clients 
 * that are currently visible on the given monitor
 *
 * @param s The window manager's state
 * @param mon The monitor to get visible clients off
 * @param tiled Whether or not to ignore floating clients
 *
 * @return A filtered linked list with all visible clients on 
 * the given monitor
 */
client_t* 
visibleclients(state_t* s, monitor_t* mon, bool tiled) {
  client_t dummy;
  client_t* tail = &dummy;
  dummy.next = NULL;

  client_t* current = s->clients;

  while (current != NULL) {
    bool tiled_check = tiled ? current->floating : !tiled;
    if (tiled_check && clientonscreen(s, current, mon)) {
      tail->next = current;
      tail = current;
    }
    current = current->next;
  }

  tail->next = NULL;

  return dummy.next;
}

/**
 * @brief Returns the associated client from a given titlebar window.
 * Returns NULL if there is no client associated with the titlebar window.
 *
 * @param s The window manager's state
 * @param win The titlebar window to get the client from
 *
 * @return The client associated with the given titlbar window (NULL if no associated client)
 */
client_t*
clientfromtitlebar(state_t* s, xcb_window_t titlebar) {
  if(!s->config.usedecoration) return NULL;
  client_t* cl;
  for (cl = s->clients; cl != NULL; cl = cl->next) {
    // If the window is found in the clients, return the client
    if(cl->titlebar == titlebar) {
      return cl;
    }
  }
  return NULL;
}

/**
 * @brief Adds a monitor area to the linked list of monitors->
 *
 * @param s The window manager's state 
 * @param a The area of the monitor to create 
 *
 * @return The newly created monitor 
 */
monitor_t* addmon(state_t* s, area_t a, uint32_t idx) {
  // Allocate a new item for the monitor in the linked list 
  // of monitors.
  monitor_t* mon  = (monitor_t*)malloc(sizeof(*mon));
  mon->area     = a;
  mon->next     = s->monitors;
  mon->idx      = idx;
  mon->desktopcount = 0;
  mon->activedesktops = malloc(sizeof(*mon->activedesktops) * s->config.maxdesktops);


  // Initialize the layout properties of all virtual desktops on the monitor
  mon->layouts = malloc(sizeof(*mon->layouts) * s->config.maxdesktops);
  for(uint32_t i = 0; i < s->config.maxdesktops; i++) {
    mon->layouts[i].nmaster = 1;
    mon->layouts[i].gapsize = s->config.winlayoutgap;
    mon->layouts[i].masterarea = MIN(MAX(s->config.layoutmasterarea, 0.0), 1.0);
    mon->layouts[i].curlayout = s->config.initlayout;
  }
  // Update linked list pointer
  s->monitors    = mon;


  logmsg(s,  LogLevelTrace, "registered monitor %i (position: %ix%i size. %ix%i).", 
         mon->idx, (int32_t)mon->area.pos.x, (int32_t)mon->area.pos.y,
         (int32_t)mon->area.size.x, (int32_t)mon->area.size.y);

  return mon;
}


/**
 * @brief Returns the monitor within the list of monitors 
 * that matches the given area. 
 * Returns NULL if there is no monitor associated with the area.
 *
 * @param s The window manager's state
 * @param a The area to find within the monitors 
 *
 * @return The monitor associated with the given area (NULL if no associated monitor)
 */
monitor_t*
monbyarea(state_t* s, area_t a) {
  monitor_t* mon;
  for (mon = s->monitors; mon != NULL; mon = mon->next) {
    if((mon->area.pos.x == a.pos.x && mon->area.pos.y == a.pos.y) &&
	(mon->area.size.x == a.size.x && mon->area.size.y == a.size.y)) {
      return mon;
    }
  }
  return NULL;
}


/**
 * @brief Returns the monitor in which a given client is contained in. 
 * any registered monitor.
 *
 * @param s The window manager's state
 * @param cl The client to get the monitor of 
 *
 * @return The monitor that the given client is contained in.
 * Returns the first monitor if the given client is not contained within
 */
monitor_t*
clientmon(state_t* s, client_t* cl) {
  monitor_t* ret = s->monitors;
  if (!cl) {
    return ret;
  }

  monitor_t* mon;
  float biggest_overlap_area = -1.0f;

  for (mon = s->monitors; mon != NULL; mon = mon->next) {
    float overlap = getoverlaparea(cl->area, mon->area);

    // Assign the monitor on which the client has the biggest overlap area
    if (overlap > biggest_overlap_area) {
      biggest_overlap_area = overlap;
      ret = mon;
    }
  }
  return ret;
}


/**
 * @brief Returns the currently selected virtual desktop on 
 * a given monitor
 *
 * @param s The window manager's state
 * @param mon The monitor to get the selected desktop of
 *
 * @return The selected virtual desktop on the given monitor 
 */
desktop_t* 
mondesktop(state_t* s, monitor_t* mon) {
  return &s->curdesktop[mon->idx];
}

/**
 * @brief Returns the monitor under the cursor 
 * Returns the first monitor if there is no monitor under the cursor. 
 *
 * @param s The window manager's state
 *
 * @return The monitor that is under the cursor.
 */
monitor_t*
cursormon(state_t* s) {
  bool success;
  v2_t cursor = cursorpos(s, &success);
  if(!success) {
    return s->monitors;
  }
  monitor_t* mon;
  for (mon = s->monitors; mon != NULL; mon = mon->next) {
    if(pointinarea(cursor, mon->area)) {
      return mon;
    }
  }
  return s->monitors;
}

/**
 * @brief Gets all screens registed by xrandr and adds newly registered 
 * monitors to the linked list of monitors in the window manager.
 *
 * @param s The window manager's state 
 *
 * @return The number of newly registered monitors 
 */
uint32_t
updatemons(state_t* s) {

  // Get xrandr screen resources
  xcb_randr_get_screen_resources_current_cookie_t res_cookie = 
    xcb_randr_get_screen_resources_current(s->con, s->screen->root);
  xcb_randr_get_screen_resources_current_reply_t *res_reply =
    xcb_randr_get_screen_resources_current_reply(s->con, res_cookie, NULL);

  if (!res_reply) {
    logmsg(s,  LogLevelError, "cannot get Xrandr screen resources.");
    terminate(s, EXIT_FAILURE);
  }

  // Get number of connected monitors
  int32_t num_outputs = xcb_randr_get_screen_resources_current_outputs_length(res_reply);
  // Get list of monitors
  xcb_randr_output_t *outputs = xcb_randr_get_screen_resources_current_outputs(res_reply);

  uint32_t registered_count = 0; 
  // Iterate through the outputs
  for (int32_t i = 0; i < num_outputs; i++) {
    // Get the monitor info
    xcb_randr_get_output_info_cookie_t info_cookie = xcb_randr_get_output_info(s->con, outputs[i], XCB_TIME_CURRENT_TIME);
    xcb_randr_get_output_info_reply_t *info_reply = xcb_randr_get_output_info_reply(s->con, info_cookie, NULL);

    // Check if montior has CRTC
    if (info_reply && info_reply->crtc != XCB_NONE) {
      // Get CRTC info
      xcb_randr_get_crtc_info_cookie_t crtc_cookie = xcb_randr_get_crtc_info(s->con, info_reply->crtc, XCB_TIME_CURRENT_TIME);
      xcb_randr_get_crtc_info_reply_t *crtc_reply = xcb_randr_get_crtc_info_reply(s->con, crtc_cookie, NULL);

      // If that worked, register the monitor if it isn't registered yet 
      if (crtc_reply) {
        area_t monarea = (area_t){
          .pos = (v2_t){
            crtc_reply->x, crtc_reply->y
          },
          .size = (v2_t){
            crtc_reply->width, crtc_reply->height
          }
        };
        if(!monbyarea(s, monarea)) {
          addmon(s, monarea, registered_count++);
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

/**
 * @brief Returns the keysym of a given key code. 
 * Returns 0 if there is no keysym for the given keycode.
 *
 * @param s The window manager's state
 * @param keycode The keycode to get the keysym from 
 *
 * @return The keysym of the given keycode (0 if no keysym associated) 
 */
xcb_keysym_t
getkeysym(state_t* s, xcb_keycode_t keycode) {
  // Allocate key symbols 
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s->con);
  // Get the keysym
  xcb_keysym_t keysym = (!(keysyms) ? 0 : xcb_key_symbols_get_keysym(keysyms, keycode, 0));
  // Free allocated resources
  xcb_key_symbols_free(keysyms);
  return keysym;
}

/**
 * @brief Returns the keycode of a given keysym. 
 * Returns NULL if there is no keycode for the given keysym.
 *
 * @param s The window manager's state
 * @param keysym The keysym to get the keycode from 
 *
 * @return The keycode of the given keysym (NULL if no keysym associated) 
 */
xcb_keycode_t*
getkeycodes(state_t* s, xcb_keysym_t keysym) {
  xcb_key_symbols_t *keysyms = xcb_key_symbols_alloc(s->con);
  xcb_keycode_t *keycode = (!(keysyms) ? NULL : xcb_key_symbols_get_keycode(keysyms, keysym));
  xcb_key_symbols_free(keysyms);
  return keycode;

}

/**
 * @brief Reads the _NET_WM_STRUT_PARTIAL atom of a given window and 
 * uses the gathered strut information to populate a strut_t.
 *
 * @param s The window manager's state
 * @param win The window to retrieve the strut of 
 *
 * @return The strut of the window 
 */
strut_t
readstrut(state_t* s, xcb_window_t win) {
  strut_t strut = {0};
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(s->con, 0, strlen("_NET_WM_STRUT_PARTIAL"), "_NET_WM_STRUT_PARTIAL");
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(s->con, cookie, NULL);

  if (!reply) {
    logmsg(s,  LogLevelError, "failed to get _NET_WM_STRUT_PARTIAL atom.");
    return strut;
  }

  xcb_get_property_cookie_t propcookie = xcb_get_property(s->con, 0, win, reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 16);
  xcb_get_property_reply_t* propreply = xcb_get_property_reply(s->con, propcookie, NULL);

  if (reply && xcb_get_property_value_length(propreply) >= 16) {
    uint32_t* data = (uint32_t*)xcb_get_property_value(propreply);
    strut.left    = data[0];
    strut.right   = data[1];
    strut.top     = data[2];
    strut.bottom  = data[3];
    strut.starty  = data[4];
    strut.endy 	  = data[5];
    strut.startx  = data[8];
    strut.endx	  = data[9];
    logmsg(s,  LogLevelTrace, "registered strut data of window %i: L: %i, R: %i, T: %i B: %i SX: %i EX: %i SY: %i EY: %i", win,
           strut.left, strut.right, strut.top, strut.bottom, strut.startx, strut.endx, 
           strut.starty, strut.endy); 
  } 

  free(reply);
  free(propreply);

  return strut;
}

/**
 * @brief Gathers the struts of all subwindows of a given window
 *
 * @param s The window manager's state
 * @param win The window to use as a root 
 */
void 
getwinstruts(state_t* s, xcb_window_t win) {
  xcb_query_tree_cookie_t cookie = xcb_query_tree(s->con, win);
  xcb_query_tree_reply_t* reply = xcb_query_tree_reply(s->con, cookie, NULL);

  if (!reply) {
    logmsg(s,  LogLevelError, "failed to get the query tree for window %i.", win);
    return;
  }

  xcb_window_t* childs = xcb_query_tree_children(reply);
  uint32_t nchilds = xcb_query_tree_children_length(reply);

  for (uint32_t i = 0; i < nchilds; i++) {
    xcb_window_t child = childs[i];
    strut_t strut = readstrut(s, child);
    if(!(strut.left == 0 && strut.right == 0 && 
      strut.top == 0 && strut.bottom == 0) &&
      s->nwinstruts + 1 <= s->config.maxstruts) {
      s->winstruts[s->nwinstruts++] = strut;
    }
    getwinstruts(s, child);
  }

  free(reply);
}

/**
 * @brief Retrieves an intern X atom by name.
 *
 * @param s The window manager's state
 * @param atomstr The string (name) of the atom to retrieve
 *
 * @return The XCB atom associated with the given atom name.
 * Returns XCB_ATOM_NONE if the given name is not associated with 
 * any atom.
 * */
xcb_atom_t
getatom(state_t* s, const char* atomstr) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(s->con, 0, strlen(atomstr), atomstr);
  xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(s->con, cookie, NULL);
  xcb_atom_t atom = reply ? reply->atom : XCB_ATOM_NONE;
  free(reply);
  return atom;
}

/**
 * 
 * @brief Updates the client list EWMH atom tothe current list of clients->
 *
 * @param s The window manager's state 
 * */
void
ewmh_updateclients(state_t* s) {
  xcb_delete_property(s->con, s->root, s->ewmh_atoms[EWMHclientList]);

  client_t* cl;
  for (cl = s->clients; cl != NULL; cl = cl->next) {
    xcb_change_property(s->con, XCB_PROP_MODE_APPEND, s->root, s->ewmh_atoms[EWMHclientList],
	XCB_ATOM_WINDOW, 32, 1, &cl->win);
  }
}

/**
 * @brief Signal handler for SIGCHLD to avoid zombie processes
 * */
void
sigchld_handler(int32_t signum) {
  (void)signum;
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

/**
 * @brief Checks if a given string is within a given 
 * array of strings
 * @param array Haystack
 * @param count Number of items within the haystack
 * @param target Needle
 *
 * @return Wether or not the given string was found in the 
 * given array
 * */
bool 
strinarr(char* array[], int count, const char* target) {
  for (int i = 0; i < count; ++i) {
    if (array[i] != NULL && strcmp(array[i], target) == 0) {
      return true; // String found
    }
  }
  return false; // String not found
}

/*
 * @brief Compares two strings 
 * @param a The first string to compare
 * @param b The second string to compare
 * @return See 'man strcmp' */
int32_t 
compstrs(const void* a, const void* b) {
  return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * @brief Creates an GLX Context and sets up OpenGL visual. 
 * */
void 
initglcontext(state_t* s) {
  /* Create an OpenGL context */
  GLint attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, 
     None };

  int screen_num = DefaultScreen(s->dsp);
  s->glvis = glXChooseVisual(s->dsp, screen_num, attribs);
  if (!s->glvis) {
    logmsg(s,  LogLevelError, "no appropriate OpenGL visual found.");
    terminate(s, EXIT_FAILURE);
  }

  s->glcontext = glXCreateContext(s->dsp, s->glvis, NULL, GL_TRUE); 

  if(!s->glcontext) {
    logmsg(s,  LogLevelError, "failed to create OpenGL context");
    terminate(s, EXIT_FAILURE);
  }


  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/**
 * @brief Set the OpenGL context and drawable to a given window
 * @param win The window to draw on 
 * */
void
setglcontext(state_t* s, xcb_window_t win) {
  // Get the current drawable and context
  GLXDrawable curwin = glXGetCurrentDrawable();
  GLXContext curcontext = glXGetCurrentContext();

  // Check if the current drawable and context match the desired ones
  if (curwin != win || curcontext != s->glcontext) {
    // Make the given window and context the current drawable
    if (!glXMakeCurrent(s->dsp, win, s->glcontext)) {
      logmsg(s,  LogLevelError, "failed to set OpenGL context to window %i.", win); 
    }
    logmsg(s,  LogLevelTrace, "set OpenGL context to window %i.", win); 
  }
}


/**
 * @brief Logs a given message to stdout/stderr that differs based
 * on the log level given. This function also logs the given message to
 * the logfile specified in the config if 'shouldlogtofile' is enabled.
 * config.
 * @param lvl The log level
 * @param fmt The format string
 * @param ... The variadic arguments */
void logmsg(state_t* s, log_level_t lvl, const char* fmt, ...) {
  if (!s->config.logmessages) return;

  /* Print the log level */
  switch (lvl) {
    case LogLevelTrace:
      fprintf(stdout, "ragnar: INFO: ");
      break;
    case LogLevelWarn:
      fprintf(stdout, "ragnar: WARNING: ");
      break;
    case LogLevelError:
      fprintf(stderr, "ragnar: ERROR: ");
      break;
    default:
      break;
  }

  // Print the message to log with variadic arguments
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  printf("\n");
  va_end(args);

  if (s->config.shouldlogtofile) {
    // Write the logged message to the logfile
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);  // Copy the va_list
    logtofile(lvl, s, fmt, args_copy); // Pass the copied va_list to logtofile
    va_end(args_copy);         // Clean up the copied va_list
    va_end(args);              // End the original va_list
  }
}

/**
 * @brief Logs a given formatted message to the log file specified in the
 * config.
 * @param fmt The format string
 * @param args The variadic arguments list */
void logtofile(log_level_t lvl, state_t* s, const char* fmt, va_list args) {
  if (!s->config.shouldlogtofile) return;

  // Open the file in append mode
  FILE *file = fopen("/home/cococry/ragnarwm.log", "a");

  // Check if the file was opened successfully
  if (file == NULL) {
    perror("ragnar: error opening log file.");
    return;
  }

  // Write the date to the file
  char* date = cmdoutput("date +\"%d.%m.%y %H:%M:%S\"");
  fprintf(file, "%s | ", date);

  switch(lvl) {
    case LogLevelTrace: {
      fprintf(file, "TRACE: ");
      break;
    }
    case LogLevelWarn: {
      fprintf(file, "WARN: ");
      break;
    }
    case LogLevelError: {
      fprintf(file, "ERROR: ");
      break;
    }
  }
  // Write the formatted string to the file
  vfprintf(file, fmt, args);

  fprintf(file, "\n");

  // Close the file
  fclose(file);
}

/**
 * @brief Returns the output of a given command 
 *
 * @param cmd The command to get the output of 
 *
 * @return The output of the given command */ 
char* 
cmdoutput(const char* cmd) {
  FILE *fp;
  char buffer[512];
  char *result = NULL;
  size_t result_len = 0;

  // Open a pipe to the command
  fp = popen(cmd, "r");
  if (fp == NULL) {
    perror("popen");
    return NULL;
  }

  // Read the command's output
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    size_t buffer_len = strlen(buffer);
    char *new_result = realloc(result, result_len + buffer_len + 1);
    if (new_result == NULL) {
      perror("realloc");
      free(result);
      pclose(fp);
      return NULL;
    }
    result = new_result;
    memcpy(result + result_len, buffer, buffer_len);
    result_len += buffer_len;
    result[result_len] = '\0'; 
  }

  // Close the pipe
  if (pclose(fp) == -1) {
    perror("pclose");
    free(result);
    return NULL;
  }

  return result;
}


int
main(void) {
  state_t* wm_state = malloc(sizeof(state_t));
  // Setup the window manager
  setup(wm_state);
  // Manage all windows on the display
  managewins(wm_state);
  // Enter the event loop
  loop(wm_state);
  // Terminate after the loop
  terminate(wm_state, EXIT_SUCCESS);
}
