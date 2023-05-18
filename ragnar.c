#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrender.h>
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "config.h"

#define VERSION "1.0"

#define CLIENT_WINDOW_CAP 256

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef enum {
    WINDOW_LAYOUT_TILED_MASTER = 0,
    WINDOW_LAYOUT_FLOATING
} WindowLayout;

typedef struct {
    float x, y;
} Vec2;


typedef struct {
    Window win;
    bool hidden;
    FontStruct font;
    bool spawned;
} Bar;

typedef struct {
    bool hidden;
    Window titlebar;
    Window closebutton;
    Window maximize_button;
    FontStruct closebutton_font, 
        titlebar_font, maximize_button_font;
} Decoration;

typedef struct {
    Window win;
    Window frame;

    bool fullscreen;
    Vec2 fullscreen_revert_size;
    Vec2 fullscreen_revert_pos;

    uint8_t monitor_index;
    int8_t desktop_index;

    bool in_layout;
    int32_t layout_y_size_offset;
    bool ignore_unmap;

    Decoration decoration;
} Client;

typedef struct {
    Display* display;
    Window root;
    GC gc;
    int screen;

    bool running;

    Client client_windows[CLIENT_WINDOW_CAP];
    uint32_t clients_count;

    int8_t focused_monitor;
    int32_t bar_monitor;
    int32_t focused_client;
    int8_t focused_desktop[MONITOR_COUNT];

    Vec2 cursor_start_pos, 
        cursor_start_frame_pos, 
        cursor_start_frame_size;

    WindowLayout current_layout;
    uint32_t layout_master_size_x[MONITOR_COUNT][DESKTOP_COUNT];
    int32_t window_gap;

    bool decoration_hidden;
    Bar bar;
} XWM;


static bool wm_detected = false;
static XWM wm;

static void         handle_create_notify(XCreateWindowEvent e);
static void         handle_reparent_notify(XReparentEvent e);
static void         handle_map_notify(XMapEvent e);
static void         handle_unmap_notify(XUnmapEvent e);
static void         handle_configure_request(XConfigureRequestEvent e);
static void         handle_configure_notify(XConfigureEvent e);
static void         handle_motion_notify(XMotionEvent e);
static void         handle_map_request(XMapRequestEvent e);
static void         handle_destroy_notify(XDestroyWindowEvent e);
static int          handle_x_error(Display* display, XErrorEvent* e);
static int          handle_wm_detected(Display* display, XErrorEvent* e);
static void         handle_button_press(XButtonEvent e);
static void         handle_button_release(XButtonEvent e);
static void         handle_key_press(XKeyEvent e);
static void         handle_key_release(XKeyEvent e);
static void         grab_global_input();
static void         grab_window_input(Window win);

static Vec2         get_cursor_position();

static void         select_focused_monitor(uint32_t x_cursor);
static int32_t      get_monitor_index_by_window(int32_t xpos);
static int32_t      get_focused_monitor_window_center_x(int32_t window_x);
static int32_t      get_monitor_start_x(int8_t monitor);
static int32_t      get_client_index_window(Window win);

static void         set_fullscreen(Window win, bool hide_decoration);
static void         unset_fullscreen(Window win);

static void         establish_window_layout();
static void         cycle_client_layout_up(Client* client);
static void         cycle_client_layout_down(Client* client);

static void         move_client(Client* client, Vec2 pos);
static void         resize_client(Client* client, Vec2 size);

static void         hide_client_decoration(Client* client); 
static void         unhide_client_decoration(Client* client); 
static void         redraw_client_decoration(Client* client);

static void         change_desktop(int8_t desktop_index);

static void         create_bar();
static void         hide_bar();
static void         unhide_bar();
static void         draw_bar();
static void         change_bar_monitor(int32_t monitor);
static void         raise_bar();

static void         get_cmd_output(const char* cmd, char* dst);

static FontStruct   font_create(const char* fontname, const char* fontcolor, Window win);

static void         draw_str(const char* str, FontStruct font, int x, int y);
static void         draw_triangle(Window win, Vec2 p1, Vec2 p2, Vec2 p3, uint32_t color);
static uint32_t     draw_text_icon_color(const char* icon, const char* text, Vec2 pos, FontStruct font, uint32_t color, uint32_t rect_x_add, Window win, uint32_t height);

XWM xwm_init() {
    XWM wm;

    wm.display = XOpenDisplay(NULL);
    if(!wm.display) {
        printf("Failed to open X Display.\n");
    }
    wm.root = DefaultRootWindow(wm.display);

    return wm;
}

void xwm_terminate() {
    XCloseDisplay(wm.display);
}

void xwm_window_frame(Window win) {
    // If the window is already framed, return
    if(get_client_index_window(win) != -1) return;

    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, win, &attribs);
    
    // Creating the X Window frame
    int32_t win_x = get_focused_monitor_window_center_x(attribs.width / 2);
    Window win_frame = XCreateSimpleWindow(wm.display, wm.root, 
        win_x, 
        (Monitors[wm.focused_monitor].height / 2) - (attribs.height / 2), attribs.width, attribs.height, 
        WINDOW_BORDER_WIDTH,  WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);
    
    XSelectInput(wm.display, win_frame, SubstructureRedirectMask | SubstructureNotifyMask); 
    XReparentWindow(wm.display, win, win_frame, 0, ((SHOW_DECORATION && !wm.decoration_hidden) ? DECORATION_TITLEBAR_SIZE : 0));
    XMapWindow(wm.display, win_frame);
    Client client = (Client){
        .frame = win_frame, 
        .win =  win, 
        .fullscreen = attribs.width >= (int32_t)Monitors[wm.focused_monitor].width && attribs.height >= (int32_t)Monitors[wm.focused_monitor].height, 
        .monitor_index = wm.focused_monitor,
        .desktop_index = wm.focused_desktop[wm.focused_monitor],
        .in_layout = true,
        .layout_y_size_offset = 0,
        .ignore_unmap = false};
    
    // Adding the window to the clients 
    wm.client_windows[wm.clients_count++] = client; 
    wm.focused_client = wm.clients_count - 2;

    grab_window_input(win);
    establish_window_layout();
    if (SHOW_DECORATION) { 
        int32_t client_index = get_client_index_window(win);

        XWindowAttributes attribs_frame;
        XGetWindowAttributes(wm.display, win_frame, &attribs_frame);
        
        /* Titlebar */
        wm.client_windows[client_index].decoration.titlebar = XCreateSimpleWindow(wm.display, win_frame, 0, 0, attribs_frame.width, DECORATION_TITLEBAR_SIZE,0,0, DECORATION_COLOR);
        XSelectInput(wm.display, wm.client_windows[client_index].decoration.titlebar, SubstructureRedirectMask | SubstructureNotifyMask); 
        XMapWindow(wm.display, wm.client_windows[client_index].decoration.titlebar);
        XReparentWindow(wm.display, wm.client_windows[client_index].decoration.titlebar, win_frame, 0, 0);
       
        /* Close button */
        uint32_t x_offset = 0;
        if(DECORATION_SHOW_CLOSE_ICON) {
            x_offset += DECORATION_CLOSE_ICON_SIZE;
            wm.client_windows[client_index].decoration.closebutton = XCreateSimpleWindow(wm.display, win_frame, attribs_frame.width - x_offset, 0, 
                                                    DECORATION_CLOSE_ICON_SIZE, DECORATION_TITLEBAR_SIZE, 0,  WINDOW_BORDER_COLOR, DECORATION_CLOSE_ICON_COLOR);
            XSelectInput(wm.display,wm.client_windows[client_index].decoration.closebutton, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask); 
            XMapWindow(wm.display, wm.client_windows[client_index].decoration.closebutton);
            XRaiseWindow(wm.display, wm.client_windows[client_index].decoration.closebutton);
        }
        /* Fullscreen button */
        if(DECORATION_SHOW_MAXIMIZE_ICON) {
            x_offset += DECORATION_MAXIMIZE_ICON_SIZE;
            wm.client_windows[client_index].decoration.maximize_button = XCreateSimpleWindow(wm.display, win_frame, attribs_frame.width - x_offset, 0, 
                                                    DECORATION_MAXIMIZE_ICON_SIZE, DECORATION_TITLEBAR_SIZE, 0,  WINDOW_BORDER_COLOR, DECORATION_MAXIMIZE_ICON_COLOR);
            XSelectInput(wm.display,wm.client_windows[client_index].decoration.maximize_button, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask); 
            XMapWindow(wm.display, wm.client_windows[client_index].decoration.maximize_button);
            XRaiseWindow(wm.display, wm.client_windows[client_index].decoration.maximize_button);
        }

        /* Creating fonts & drawing decoration */
        wm.client_windows[client_index].decoration.closebutton_font = font_create(FONT, FONT_COLOR, wm.client_windows[client_index].decoration.closebutton);
        wm.client_windows[client_index].decoration.maximize_button_font = font_create(FONT, FONT_COLOR, wm.client_windows[client_index].decoration.maximize_button);
        wm.client_windows[client_index].decoration.titlebar_font = font_create(FONT, FONT_COLOR, wm.client_windows[client_index].decoration.titlebar);

        if(wm.decoration_hidden) {
            hide_client_decoration(&wm.client_windows[client_index]);
            XMoveWindow(wm.display, wm.client_windows[client_index].win, 0, 0);
            XResizeWindow(wm.display, wm.client_windows[client_index].win, attribs_frame.width, attribs_frame.height);
        } else { 
            redraw_client_decoration(&wm.client_windows[client_index]);
        }
    }
    if(!wm.bar.spawned) {
        hide_bar();
        unhide_bar();
        wm.bar.spawned = true;
    }
    raise_bar();
}

void xwm_window_unframe(Window win) {
    // If the window was not framed, return
    int32_t client_index = get_client_index_window(win);
    if(client_index == -1) return;

    // If the unframe happend through a changed desktop, keep the client in ram & return
    if(wm.client_windows[client_index].ignore_unmap) {
        wm.client_windows[client_index].ignore_unmap = false;
        return;
    }
    Window frame_win = wm.client_windows[client_index].frame;

    // If master window of layout was unframed, reset master size
    if(wm.client_windows[client_index].in_layout && get_client_index_window(win) == 0) {
        wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;
    }
    if(SHOW_DECORATION) {
        /* Title bar */
        XftColorFree(wm.display, DefaultVisual(wm.display, 0), DefaultColormap(wm.display, wm.screen), &wm.client_windows[client_index].decoration.titlebar_font.color);
        XftFontClose(wm.display, wm.client_windows[client_index].decoration.titlebar_font.font);
        /* Close button */
        XftColorFree(wm.display, DefaultVisual(wm.display, 0), DefaultColormap(wm.display, wm.screen), &wm.client_windows[client_index].decoration.titlebar_font.color);
        XftFontClose(wm.display, wm.client_windows[client_index].decoration.titlebar_font.font);
    }


    // Destroying the window in X
    XReparentWindow(wm.display, frame_win, wm.root,0,0);
    XUnmapWindow(wm.display, frame_win);
    XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);

    // Removing the window from the clients
    for(uint32_t i = client_index; i < wm.clients_count - 1; i++) {
        wm.client_windows[i] = wm.client_windows[i + 1];
    }
    memset(&wm.client_windows[wm.clients_count - 1], 0, sizeof(Client));
    wm.clients_count--;
    
    if(wm.clients_count != 0) {
        wm.client_windows[wm.clients_count - 1].layout_y_size_offset = 0;
    }

    establish_window_layout();
    unhide_bar();
}
void xwm_run() {
    // Setting variables to default state 
    wm.clients_count = 0;
    wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f}; 
    wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f}; 
    wm.running = true;
    wm.focused_monitor = MONITOR_COUNT - 1;
    wm.current_layout = WINDOW_LAYOUT_TILED_MASTER;
    wm.bar_monitor = BAR_START_MONITOR;
    wm.window_gap = 30;
    wm.decoration_hidden = !SHOW_DECORATION;
    memset(wm.focused_desktop, 0, sizeof(wm.focused_desktop));
    memset(wm.layout_master_size_x, 0, sizeof(wm.layout_master_size_x));
    wm.screen = DefaultScreen(wm.display);

    XSetErrorHandler(handle_wm_detected);
    XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask); 
    if(wm_detected) {
        printf("Another window manager is already running on this X display.\n");
        return;
    }
    XSync(wm.display, false);

    Cursor cursor = XcursorLibraryLoadCursor(wm.display, "arrow");
    XDefineCursor(wm.display, wm.root, cursor);
    XSetErrorHandler(handle_x_error);

    XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
    grab_global_input();
    
    if(SHOW_BAR)
        create_bar();

    double refresh_timer = WM_REFRESH_SPEED;

    while(wm.running) {
        // Query mouse position to get focused monitor
        select_focused_monitor(get_cursor_position().x);
        if(XPending(wm.display)) {
            XEvent e;
            XNextEvent(wm.display, &e);
            switch (e.type) {
                case MapNotify:
                    handle_map_notify(e.xmap);
                    break;
                case UnmapNotify:
                    handle_unmap_notify(e.xunmap);
                    break;
                case MapRequest:
                    handle_map_request(e.xmaprequest);
                    break;
                case ConfigureRequest:
                    handle_configure_request(e.xconfigurerequest);
                    break;
                case ConfigureNotify:
                    handle_configure_notify(e.xconfigure);
                    break;
                case CreateNotify:
                    handle_create_notify(e.xcreatewindow);
                    break;
                case ReparentNotify:
                    handle_reparent_notify(e.xreparent);
                    break;
                case DestroyNotify:
                    handle_destroy_notify(e.xdestroywindow);
                    break;
                case ButtonPress:
                    handle_button_press(e.xbutton);
                    break;
                case ButtonRelease:
                    handle_button_release(e.xbutton);
                    break;
                case KeyPress:
                    handle_key_press(e.xkey);
                    break;
                case KeyRelease:
                    handle_key_release(e.xkey);
                    break;
                case MotionNotify:
                    while(XCheckTypedWindowEvent(wm.display, e.xmotion.window, MotionNotify, &e)) {}
                    handle_motion_notify(e.xmotion);
                    break;
            }
        }
        float delta_time = 0.00001f;
        refresh_timer += delta_time;
        if(SHOW_BAR) {
            for(uint32_t i = 0; i < BAR_SLICES_COUNT; i++) {
                BarCommands[i].timer += delta_time;
            }
        }
        if(refresh_timer >= WM_REFRESH_SPEED) {
            if(SHOW_BAR) 
                draw_bar();
            if(SHOW_DECORATION && !wm.decoration_hidden) {
                for(uint32_t i = 0; i < wm.clients_count; i++) {
                    redraw_client_decoration(&wm.client_windows[i]);
                }
            }
            refresh_timer = 0;
        }    
    }

    xwm_terminate();
}

Window get_frame_window(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win || wm.client_windows[i].frame == win) {
            return wm.client_windows[i].frame;
        }
    }
    return 0;
}
void handle_create_notify(XCreateWindowEvent e) {
  (void)e;
}
void handle_configure_request(XConfigureRequestEvent e) {
    // Window 
    {
        XWindowChanges changes;
        changes.x = 0;
        changes.y = 0;
        changes.width = e.width;
        changes.height = e.height;
        changes.border_width = e.border_width;
        changes.sibling = e.above;
        changes.stack_mode = e.detail;
        XConfigureWindow(wm.display, e.window, e.value_mask, &changes);
    }
    // Frame
    {
        XWindowChanges changes;
        changes.x = e.x;
        changes.y = e.y;
        changes.width = e.width;
        changes.height = e.height;
        changes.border_width = e.border_width;
        changes.sibling = e.above;
        changes.stack_mode = e.detail;
        XConfigureWindow(wm.display, get_frame_window(e.window), e.value_mask, &changes);
    }
}
void handle_configure_notify(XConfigureEvent e) {
    (void)e;
}
void handle_motion_notify(XMotionEvent e) {
    if(get_client_index_window(e.window) == -1) return;
    select_focused_monitor(e.x_root);

    Vec2 drag_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};
    Vec2 delta_drag = (Vec2){.x = drag_pos.x - wm.cursor_start_pos.x, .y = drag_pos.y - wm.cursor_start_pos.y};
    
    Client* client = &wm.client_windows[get_client_index_window(e.window)];

    // If left click
    if(e.state & Button1Mask) {
        Vec2 drag_dest = (Vec2){.x = (float)(wm.cursor_start_frame_pos.x + delta_drag.x), 
            .y = (float)(wm.cursor_start_frame_pos.y + delta_drag.y)};
        // Unset fullscreen of the client
        if(client->fullscreen) {
            client->fullscreen = false;
            XSetWindowBorderWidth(wm.display, client->frame, WINDOW_BORDER_WIDTH);
            unhide_bar();
            unhide_client_decoration(client);
        } 
        move_client(client, drag_dest);
        // Remove the client from the layout 
        if(client->in_layout) {
            client->in_layout = false;
            client->layout_y_size_offset = 0;
            establish_window_layout();
        }
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, client->frame, &attribs);
        client->monitor_index = get_monitor_index_by_window(drag_dest.x); 
    // If right click
    } else if(e.state & Button3Mask) {
        Vec2 resize_delta = (Vec2){.x = MAX(delta_drag.x, -wm.cursor_start_frame_size.x),
                                    .y = MAX(delta_drag.y, -wm.cursor_start_frame_size.y)};
        Vec2 resize_dest = (Vec2){.x = wm.cursor_start_frame_size.x + resize_delta.x, 
                                    .y = wm.cursor_start_frame_size.y + resize_delta.y};
        resize_client(client, resize_dest);
        // Remove the client from the layout
        if(client->in_layout) {
            client->in_layout = false;
            establish_window_layout();
        }
    }
}

void handle_map_request(XMapRequestEvent e) {
    xwm_window_frame(e.window);
    XMapWindow(wm.display, e.window);
}
int handle_x_error(Display* display, XErrorEvent* e) {
    (void)display;
    char err_msg[1024];
    XGetErrorText(display, e->error_code, err_msg, sizeof(err_msg));
    printf("X Error:\n\tRequest: %i\n\tError Code: %i - %s\n\tResource ID: %i\n", 
           e->request_code, e->error_code, err_msg, (int)e->resourceid); 
    return 0;  
}

int handle_wm_detected(Display* display, XErrorEvent* e) {
    (void)display;
    wm_detected = ((int32_t)e->error_code == BadAccess);
    return 0;
}

void handle_reparent_notify(XReparentEvent e) {
    (void)e;
}

void handle_destroy_notify(XDestroyWindowEvent e) {
    (void)e;
}
void handle_map_notify(XMapEvent e) {
	(void)e;
}
void handle_unmap_notify(XUnmapEvent e) {
    if(get_client_index_window(e.window) == -1) {
        return;
    }
    xwm_window_unframe(e.window);
}

void handle_button_press(XButtonEvent e) {
    Window frame = get_frame_window(e.window);
    wm.cursor_start_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};

    Window root;
    int32_t x, y;
    unsigned width, height, border_width, depth;
    XGetGeometry(wm.display, frame, &root, &x, &y, &width, &height, &border_width, &depth);
    wm.cursor_start_frame_pos = (Vec2){.x = (float)x, .y = (float)y};
    wm.cursor_start_frame_size = (Vec2){.x = (float)width, .y = (float)height};

    XRaiseWindow(wm.display, frame);

    // Check if bar button was pressed
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        if(BarButtons[i].win == e.window) {
            system(BarButtons[i].cmd);
            break;
        }
    }
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(DECORATION_SHOW_MAXIMIZE_ICON) {
            if(wm.client_windows[i].decoration.maximize_button == e.window) {
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                    establish_window_layout();
                } else {
                    set_fullscreen(wm.client_windows[i].frame, false);
                }
                break;
            }
        }
        if(DECORATION_SHOW_CLOSE_ICON) {
            if(wm.client_windows[i].decoration.closebutton == e.window) {
                xwm_window_unframe(wm.client_windows[i].win);
                break;
            }
        }
    }
    XSetInputFocus(wm.display, e.window, RevertToPointerRoot, CurrentTime);
    wm.focused_client = get_client_index_window(e.window);
    raise_bar();
}
void handle_button_release(XButtonEvent e) {
    (void)e;
}

void handle_key_press(XKeyEvent e) {
    int32_t client_index = get_client_index_window(e.window);
    if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_CLOSE_KEY)) {
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.message_type =  XInternAtom(wm.display, "WM_PROTOCOLS", false);
        msg.xclient.window = e.window;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
        XSendEvent(wm.display, e.window, false, 0, &msg);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WM_TERMINATE_KEY)) {
        wm.running = false;
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, TERMINAL_OPEN_KEY)) {
        system(TERMINAL_CMD);  
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WEB_BROWSER_OPEN_KEY)) {
        system(WEB_BROWSER_CMD);   
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_FULLSCREEN_KEY)) {
        if(client_index == -1 || e.window == wm.root) return;
        if(!wm.client_windows[client_index].fullscreen) {
            set_fullscreen(e.window, true);
        } else {
            unset_fullscreen(e.window);
            establish_window_layout();
        }
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_ADD_TO_LAYOUT_KEY)) {
        wm.client_windows[client_index].in_layout = true;
        establish_window_layout();
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CLIENT_CYCLE_DOWN_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] - 1;
        if(desktop < 0)
            desktop = DESKTOP_COUNT - 1;
        wm.client_windows[client_index].desktop_index = desktop;
        wm.client_windows[client_index].ignore_unmap = true;
        XUnmapWindow(wm.display, wm.client_windows[client_index].frame);
        establish_window_layout();
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CLIENT_CYCLE_UP_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] + 1;
        if(desktop >= DESKTOP_COUNT)
            desktop = 0;
        wm.client_windows[client_index].desktop_index = desktop;
        wm.client_windows[client_index].ignore_unmap = true;
        XUnmapWindow(wm.display, wm.client_windows[client_index].frame);
        establish_window_layout();
    }  else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_UP_KEY)) {
        cycle_client_layout_up(&wm.client_windows[client_index]);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_DOWN_KEY)) {
        cycle_client_layout_down(&wm.client_windows[client_index]);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_MASTER_X_KEY)) {
        if(wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] <= (uint32_t)(Monitors[wm.focused_monitor].width / 1.5)) 
            wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] += 40;
        establish_window_layout();
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_MASTER_X_KEY)) {
        if(wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] >= (uint32_t)(Monitors[wm.focused_monitor].width / 6)) 
            wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] -= 40;
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_TILED_MASTER_KEY)) {
        wm.current_layout = WINDOW_LAYOUT_TILED_MASTER;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
                wm.client_windows[i].in_layout = true;
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                }
            }
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_FLOATING_KEY)) {
        wm.current_layout = WINDOW_LAYOUT_FLOATING;
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_GAP_INCREASE_KEY)) {
        if(wm.window_gap < WINDOW_MAX_GAP) {
            wm.window_gap += 4;
            establish_window_layout();
        }
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_GAP_DECREASE_KEY)) {
        if(wm.window_gap > 0) {
            wm.window_gap -= 4;
            if(wm.window_gap <= 0) wm.window_gap = 0;
            establish_window_layout();
        }
    }  else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_SLAVE_Y_KEY)&& wm.clients_count > 2) {
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, wm.client_windows[wm.focused_client].frame, &attribs);
        if(attribs.height < (int32_t)(Monitors[wm.focused_monitor].height - WINDOW_MIN_SIZE_Y_LAYOUT)) {
            wm.client_windows[wm.focused_client].layout_y_size_offset += 40;
            establish_window_layout();
        }
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_SLAVE_Y_KEY) && wm.clients_count > 2) {
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, wm.client_windows[wm.focused_client].frame, &attribs);
        if(attribs.height > WINDOW_MIN_SIZE_Y_LAYOUT) {
            wm.client_windows[wm.focused_client].layout_y_size_offset -= 40;
            establish_window_layout();
        }
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CYCLE_UP_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] + 1;
        if(desktop >= DESKTOP_COUNT) {
            desktop = 0;
        }
        change_desktop(desktop);
        wm.focused_desktop[wm.focused_monitor] = desktop;
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CYCLE_DOWN_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] - 1;
        if(desktop < 0) {
            desktop = DESKTOP_COUNT - 1;
        }
        change_desktop(desktop);
        wm.focused_desktop[wm.focused_monitor] = desktop;
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, APPLICATION_LAUNCHER_OPEN_KEY)) {
        system(APPLICATION_LAUNCHER_CMD);   
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, BAR_TOGGLE_KEY)) {
        if(!SHOW_BAR)
            return;
        if(wm.bar.hidden) {
            unhide_bar();
        } else {
            hide_bar();
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_UP_KEY)) {
        if(!SHOW_BAR)
            return;
        if(wm.bar_monitor >= MONITOR_COUNT - 1) {
            wm.bar_monitor = 0;
        } else {
            wm.bar_monitor++;
        }
        change_bar_monitor(wm.bar_monitor);
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_DOWN_KEY)) {
        if(!SHOW_BAR)
            return;
        if(wm.bar_monitor <= 0) {
            wm.bar_monitor = MONITOR_COUNT - 1;
        } else {
            wm.bar_monitor--;
        }
        change_bar_monitor(wm.bar_monitor);
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_CYCLE_KEY)) {
        int32_t index = wm.focused_client + 1;
        uint32_t first_index = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] && 
                wm.client_windows[i].monitor_index == wm.focused_monitor) {
                first_index = i;
                break;
            }
        }
        if(index >= (int32_t)wm.clients_count) {
            index = first_index;
        }
        if(wm.client_windows[index].desktop_index != wm.focused_desktop[wm.focused_monitor] || 
            wm.client_windows[index].monitor_index != wm.focused_monitor) {
            index = first_index;
        }
        XSetInputFocus(wm.display, wm.client_windows[index].win, RevertToPointerRoot, CurrentTime);
        wm.focused_client = index;
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, DECORATION_TOGGLE_KEY)) {
        if(!SHOW_DECORATION)
            return;
        if(wm.decoration_hidden) {
            for(uint32_t i = 0; i < wm.clients_count; i++) {
                unhide_client_decoration(&wm.client_windows[i]);
            }
            wm.decoration_hidden = false;
        } else {
            for(uint32_t i = 0; i < wm.clients_count; i++) {
                hide_client_decoration(&wm.client_windows[i]);
            }
            wm.decoration_hidden = true;
        }
        establish_window_layout();
    }

}
void handle_key_release(XKeyEvent e) {
    (void)e;
}

int32_t get_client_index_window(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win || wm.client_windows[i].frame == win) return i;
    }
    return -1;
}

static void grab_global_input() {     
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WM_TERMINATE_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, TERMINAL_OPEN_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WEB_BROWSER_OPEN_KEY),MASTER_KEY, wm.root, false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CYCLE_UP_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CYCLE_DOWN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_TILED_MASTER_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_FLOATING_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, APPLICATION_LAUNCHER_OPEN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_CYCLE_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    if(SHOW_BAR) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_TOGGLE_KEY), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_UP_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_DOWN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    }
    if(SHOW_DECORATION) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, DECORATION_TOGGLE_KEY), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    }

}
static void grab_window_input(Window win) {
    XGrabButton(wm.display,Button1,MASTER_KEY,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabButton(wm.display,Button3,MASTER_KEY,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabButton(wm.display,Button2,MASTER_KEY,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabKey(wm.display, XKeysymToKeycode(wm.display, WINDOW_CLOSE_KEY),MASTER_KEY, win,false, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_FULLSCREEN_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_ADD_TO_LAYOUT_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_UP_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_DOWN_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_MASTER_X_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_MASTER_X_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_SLAVE_Y_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_SLAVE_Y_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_GAP_INCREASE_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_GAP_DECREASE_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CLIENT_CYCLE_UP_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CLIENT_CYCLE_DOWN_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
}

void select_focused_monitor(uint32_t x_cursor) {
    uint32_t x_offset = 0;
    for(uint32_t i = 0; i < MONITOR_COUNT; i++) {
        if(x_cursor >= x_offset && x_cursor <= Monitors[i].width + x_offset) {
            wm.focused_monitor = i;
            break;
        }
        x_offset += Monitors[i].width;
    }
}

int32_t get_monitor_index_by_window(int32_t xpos) {
    for(int32_t i = 0; i < MONITOR_COUNT; i++) {
        if(xpos >= get_monitor_start_x(i) && xpos <= get_monitor_start_x(i) + (int32_t)Monitors[i].width) {
            return i;
        }
    }
    return 0;
}

Vec2 get_cursor_position() {
    Window root_return, child_return;
    int win_x_return, win_y_return, root_x_return, root_y_return; 
    uint32_t mask_return;
    XQueryPointer(wm.display, wm.root, &root_return, &child_return, &root_x_return, &root_y_return, 
        &win_x_return, &win_y_return, &mask_return);
    return (Vec2){.x = root_x_return, .y = root_y_return};  
}

int32_t get_focused_monitor_window_center_x(int32_t window_x) {
    int32_t x = 0;
    for(uint8_t i = 0; i < wm.focused_monitor + 1; i++) {
        if(i > 0) 
            x += Monitors[i - 1].width;
        if(i == wm.focused_monitor)
            x += Monitors[wm.focused_monitor].width / 2;
    }
    x -= window_x;
    return x;
}

int32_t get_monitor_start_x(int8_t monitor) {
    int32_t x = 0;
    for(int8_t i = 0; i < monitor; i++) {
        x += Monitors[i].width;    
    }
    return x;
}

static void set_fullscreen(Window win, bool hide_decoration) {
    uint32_t client_index = get_client_index_window(win);
    if(wm.client_windows[client_index].fullscreen) return;

    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, wm.client_windows[client_index].frame, &attribs);

    wm.client_windows[client_index].fullscreen_revert_size = (Vec2){.x = attribs.width, .y = attribs.height};
    wm.client_windows[client_index].fullscreen_revert_pos = (Vec2){.x = attribs.x, .y = attribs.y};
    wm.client_windows[client_index].fullscreen = true;

    XSetWindowBorderWidth(wm.display, wm.client_windows[client_index].frame, 0);
    XRaiseWindow(wm.display, win);
    raise_bar();
    if(wm.client_windows[client_index].monitor_index == wm.bar_monitor)
        hide_bar();

    resize_client(&wm.client_windows[client_index], (Vec2){. x = Monitors[wm.focused_monitor].width, .y = Monitors[wm.focused_monitor].height});
    move_client(&wm.client_windows[client_index], (Vec2){.x = get_monitor_start_x(wm.focused_monitor), 0});

    if(hide_decoration)
        hide_client_decoration(&wm.client_windows[client_index]);
}
static void unset_fullscreen(Window win)  {
    uint32_t client_index = get_client_index_window(win);
    if(!wm.client_windows[client_index].fullscreen) return;

    resize_client(&wm.client_windows[client_index], wm.client_windows[client_index].fullscreen_revert_size);
    move_client(&wm.client_windows[client_index], wm.client_windows[client_index].fullscreen_revert_pos);

    wm.client_windows[client_index].fullscreen = false;
    XSetWindowBorderWidth(wm.display, wm.client_windows[client_index].frame, WINDOW_BORDER_WIDTH);

    if(SHOW_BAR) {
        if(wm.client_windows[client_index].monitor_index == wm.bar_monitor) {
            unhide_bar();
        }
    }
    if(wm.client_windows[client_index].decoration.closebutton_font.font != NULL && !wm.decoration_hidden && SHOW_DECORATION) {
        redraw_client_decoration(&wm.client_windows[client_index]);
    }
    if(SHOW_DECORATION && !wm.decoration_hidden)
        unhide_client_decoration(&wm.client_windows[client_index]);
}

void move_client(Client* client, Vec2 pos) {
    XMoveWindow(wm.display, client->frame, pos.x, pos.y);
}
void resize_client(Client* client, Vec2 size) {
    if(size.x >= Monitors[wm.focused_monitor].width || size.y >= Monitors[wm.focused_monitor].height) {
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, client->win, &attribs);
        client->fullscreen_revert_size = (Vec2){.x = attribs.width, .y = attribs.height};
                client->fullscreen = true;
    }
    if(!wm.decoration_hidden && SHOW_DECORATION) {
        XMoveWindow(wm.display, client->win, 0, DECORATION_TITLEBAR_SIZE);
        XResizeWindow(wm.display, client->decoration.titlebar, size.x, DECORATION_TITLEBAR_SIZE);
    }

    XResizeWindow(wm.display, client->win, size.x, size.y - ((!wm.decoration_hidden && SHOW_DECORATION) ? DECORATION_TITLEBAR_SIZE : 0));
    XResizeWindow(wm.display, client->frame, size.x, size.y);

    if(client->decoration.closebutton_font.font != NULL && !wm.decoration_hidden && SHOW_DECORATION) {
        redraw_client_decoration(client);
        uint32_t x_offset = 0;
        if(DECORATION_SHOW_CLOSE_ICON) {
            x_offset += DECORATION_CLOSE_ICON_SIZE;
            XMoveWindow(wm.display, client->decoration.closebutton, size.x - x_offset, 0);
        }
        if(DECORATION_SHOW_MAXIMIZE_ICON) {
            x_offset += DECORATION_MAXIMIZE_ICON_SIZE;
            XMoveWindow(wm.display, client->decoration.maximize_button, size.x - x_offset, 0);
        }
    }
}

void hide_client_decoration(Client* client) {
    if(!SHOW_DECORATION) return;
    if(client->decoration.hidden) return;
    if(DECORATION_SHOW_CLOSE_ICON)
        XUnmapWindow(wm.display, client->decoration.closebutton);
    if(DECORATION_SHOW_MAXIMIZE_ICON)
        XUnmapWindow(wm.display, client->decoration.maximize_button);

    XUnmapWindow(wm.display, client->decoration.titlebar);
    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, client->win, &attribs);
    XMoveWindow(wm.display, client->win, 0, 0);
    XResizeWindow(wm.display, client->win, attribs.width, attribs.height + DECORATION_TITLEBAR_SIZE);
    client->decoration.hidden = true;
    
}

void unhide_client_decoration(Client* client) {
    if(!SHOW_DECORATION) return;
    if(!client->decoration.hidden) return;
    if(DECORATION_SHOW_CLOSE_ICON)
        XMapWindow(wm.display, client->decoration.closebutton);
    if(DECORATION_SHOW_MAXIMIZE_ICON)
        XMapWindow(wm.display, client->decoration.maximize_button);

    XMapWindow(wm.display, client->decoration.titlebar);
    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, client->win, &attribs);
    XMoveWindow(wm.display, client->win, 0, DECORATION_TITLEBAR_SIZE);
    XResizeWindow(wm.display, client->win, attribs.x, attribs.y - DECORATION_TITLEBAR_SIZE);
    redraw_client_decoration(client);
    client->decoration.hidden = false;
}


void redraw_client_decoration(Client* client) {
    if(!SHOW_DECORATION || (!DECORATION_SHOW_CLOSE_ICON && !DECORATION_SHOW_MAXIMIZE_ICON)) return;
    uint32_t x_offset = 0;
    if(DECORATION_SHOW_CLOSE_ICON) {
        x_offset += DECORATION_CLOSE_ICON_SIZE;
        XClearWindow(wm.display, client->decoration.closebutton);
        XGlyphInfo extents;
        XftTextExtents8(wm.display, client->decoration.closebutton_font.font, (FcChar8*)DECORATION_CLOSE_ICON, strlen(DECORATION_CLOSE_ICON), &extents);
        draw_str(DECORATION_CLOSE_ICON, client->decoration.closebutton_font,
                 (DECORATION_CLOSE_ICON_SIZE / 2.0f) - (extents.xOff / 2.0f), (DECORATION_TITLEBAR_SIZE / 2.0f) + (extents.height / 2.0f)); 
    }
    if(DECORATION_SHOW_MAXIMIZE_ICON) {
        x_offset += DECORATION_MAXIMIZE_ICON_SIZE;
        XClearWindow(wm.display, client->decoration.maximize_button);
        XGlyphInfo extents;
        XftTextExtents8(wm.display, client->decoration.maximize_button_font.font, (FcChar8*)DECORATION_MAXIMIZE_ICON, strlen(DECORATION_MAXIMIZE_ICON), &extents);
        draw_str(DECORATION_MAXIMIZE_ICON, client->decoration.maximize_button_font,
                 (DECORATION_MAXIMIZE_ICON_SIZE / 2.0f) - (extents.xOff / 2.0f), (DECORATION_TITLEBAR_SIZE / 2.0f) + (extents.height / 2.0f)); 
    }
    XClearWindow(wm.display, client->decoration.titlebar);
    char* window_name = NULL;
    XFetchName(wm.display, client->win, &window_name);
    if(window_name != NULL) {
        XGlyphInfo extents;
        XftTextExtents8(wm.display, client->decoration.titlebar_font.font, (FcChar8*)window_name, strlen(window_name), &extents);
        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), DECORATION_TITLE_COLOR);
        XFillRectangle(wm.display, client->decoration.titlebar, DefaultGC(wm.display, wm.screen), 0, 0, extents.xOff, DECORATION_TITLEBAR_SIZE);
        draw_str(window_name, client->decoration.titlebar_font, 0, 20);
        XFree(window_name);
        draw_triangle(client->decoration.titlebar, 
                  (Vec2){extents.xOff + 20, DECORATION_TITLEBAR_SIZE}, 
                  (Vec2){extents.xOff, DECORATION_TITLEBAR_SIZE}, 
                  (Vec2){extents.xOff, 0}, DECORATION_TITLE_COLOR);

    }
    
    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, client->frame, &attribs);
    draw_triangle(client->decoration.titlebar, 
                  (Vec2){attribs.width - x_offset - 20, DECORATION_TITLEBAR_SIZE}, 
                  (Vec2){attribs.width - x_offset, DECORATION_TITLEBAR_SIZE}, 
                  (Vec2){attribs.width - x_offset, 0}, DECORATION_MAXIMIZE_ICON_COLOR);
}

void establish_window_layout() {
    if(wm.current_layout == WINDOW_LAYOUT_FLOATING) return;

    // Retrieving the clients
    Client* clients[CLIENT_WINDOW_CAP]; 
    uint32_t client_count = 0;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].monitor_index == wm.focused_monitor && 
            wm.client_windows[i].in_layout &&
            wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    if(client_count == 0) return;

    if(wm.current_layout == WINDOW_LAYOUT_TILED_MASTER) {
        // Default master size is half of the screen
        if(wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] == 0) {
            wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = Monitors[wm.focused_monitor].width / 2;
        }
        Client* master = clients[0];

        // set master "fullscreen" if no slaves

        int32_t offset_bar_master =  (master->monitor_index == wm.bar_monitor && SHOW_BAR && !wm.bar.hidden) ? BAR_SIZE : 0;
        if(client_count == 1) {
            resize_client(master, (Vec2){.x = Monitors[wm.focused_monitor].width - wm.window_gap * 2.3f, .y = (Monitors[wm.focused_monitor].height - wm.window_gap * 2.3f) - offset_bar_master});
            move_client(master, (Vec2){get_monitor_start_x(wm.focused_monitor) + wm.window_gap, wm.window_gap + offset_bar_master});
            master->fullscreen = false;
            XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);
            return;
        }
            
        // set master
        move_client(master, (Vec2){get_monitor_start_x(wm.focused_monitor) + wm.window_gap, wm.window_gap + offset_bar_master});
        resize_client(master, (Vec2){
            .x = wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] - wm.window_gap * 2.3f,
            .y = Monitors[wm.focused_monitor].height - (wm.window_gap * 2.3f) - offset_bar_master});
        master->fullscreen = false;
        XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);

        // set slaves
        int32_t last_y_offset = 0;
        for(uint32_t i = 1; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            int32_t offset_bar = ((clients[i]->monitor_index == wm.bar_monitor && SHOW_BAR && !wm.bar.hidden) ? ((i == client_count - 1) ? BAR_SIZE : 0 ) : 0);
            resize_client(clients[i], (Vec2){
                (Monitors[wm.focused_monitor].width 
                - wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]]) 
                - wm.window_gap * 2.3f, 

                (int32_t)(Monitors[wm.focused_monitor].height / (client_count - 1)) 
                + clients[i]->layout_y_size_offset
                - last_y_offset
                - wm.window_gap * 2.3f
                - offset_bar
            });

            move_client(clients[i], (Vec2){
                get_monitor_start_x(wm.focused_monitor) 
                + wm.layout_master_size_x[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] 
                + wm.window_gap,

                (Monitors[wm.focused_monitor].height - (int32_t)((Monitors[wm.focused_monitor].height / (client_count - 1) * i)))
                - ((i != client_count - 1) ? clients[i]->layout_y_size_offset : 0)
                + offset_bar
                + wm.window_gap
            });

            // The top window in the layout has to be treated diffrently 
            // for the y size offset. This is because 
            // there is no window on top of it which it can modify
            // the y size offset of. 
            if(i == client_count - 1) {
                XWindowAttributes attribs;
                XGetWindowAttributes(wm.display, clients[i - 1]->frame, &attribs);
                resize_client(clients[i - 1], (Vec2){
                    .x = attribs.width,
                    .y = attribs.height - clients[i]->layout_y_size_offset
                });
                move_client(clients[i - 1], (Vec2){
                    .x = attribs.x,
                    .y = attribs.y + clients[i]->layout_y_size_offset
                });
            }
            if(clients[i]->decoration.closebutton_font.font != NULL) {
                redraw_client_decoration(clients[i]);
            }
            last_y_offset = clients[i]->layout_y_size_offset;
        }
    }    
}

void cycle_client_layout_up(Client* client) {
    // Retrieving the clients in the layout
    Client* clients[CLIENT_WINDOW_CAP];
    uint32_t client_count = 0;
    uint32_t client_index = 0;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].monitor_index == wm.focused_monitor && 
            wm.client_windows[i].in_layout &&
            wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
            if(wm.client_windows[i].win == client->win && wm.client_windows[i].frame == client->frame) {
                client_index = client_count;
            }
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    uint32_t new_index = client_index + 1;

    if(new_index >= client_count) {
        new_index = 0;
    }
   
    // swap the clients
    Client tmp = *clients[client_index];
    *clients[client_index] = *clients[new_index];
    *clients[new_index] = tmp;
    establish_window_layout();
}

void cycle_client_layout_down(Client* client) {
    // Retrieving the clients in the layout
    Client* clients[CLIENT_WINDOW_CAP];
    uint32_t client_count = 0;
    uint32_t client_index = 0;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].monitor_index == wm.focused_monitor && 
            wm.client_windows[i].in_layout &&
            wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
            if(wm.client_windows[i].win == client->win && wm.client_windows[i].frame == client->frame) {
                client_index = client_count;
            }
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    int32_t new_index = client_index - 1;

    if(new_index < 0) {
        new_index = client_count - 1;
    }
    // swap the clients
    Client tmp = *clients[client_index];
    *clients[client_index] = *clients[new_index];
    *clients[new_index] = tmp;
    establish_window_layout();
}

void change_desktop(int8_t desktop_index) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] && 
            wm.client_windows[i].monitor_index == wm.focused_monitor) {
            XUnmapWindow(wm.display, wm.client_windows[i].frame);
            wm.client_windows[i].ignore_unmap = true;
        }
        if(wm.client_windows[i].desktop_index == desktop_index &&
            wm.client_windows[i].monitor_index == wm.focused_monitor) {
            XMapWindow(wm.display, wm.client_windows[i].frame);
        }
    }
    wm.focused_client = get_client_index_window(wm.root);
}

void create_bar() {
    if(!SHOW_BAR) return;
    wm.bar.win = XCreateSimpleWindow(wm.display, 
                                     wm.root, get_monitor_start_x(wm.bar_monitor), 0, 
                                     Monitors[wm.bar_monitor].width - 6, BAR_SIZE, 
                                     WINDOW_BORDER_WIDTH,  WINDOW_BORDER_COLOR, BAR_COLOR);
    XSelectInput(wm.display, wm.bar.win, SubstructureRedirectMask | SubstructureNotifyMask); 
    XSetStandardProperties(wm.display, wm.bar.win, "RagnarBar", "RagnarBar", None, NULL, 0, NULL);
    XMapWindow(wm.display, wm.bar.win);
    wm.bar.font = font_create(FONT, FONT_COLOR, wm.bar.win);
    uint32_t xoffset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XGlyphInfo extents;
        XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BarButtons[i].icon, strlen(BarButtons[i].icon), &extents);

        BarButtons[i].win = XCreateSimpleWindow(wm.display, wm.root, get_monitor_start_x(wm.bar_monitor) + BarButtonLabelPos[wm.bar_monitor] + xoffset, 0, extents.xOff, 
                                                BAR_SIZE, WINDOW_BORDER_WIDTH,  WINDOW_BORDER_COLOR, BAR_BUTTON_LABEL_COLOR);
        XSelectInput(wm.display, BarButtons[i].win, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask); 
        XMapWindow(wm.display, BarButtons[i].win);
        XRaiseWindow(wm.display, BarButtons[i].win);
        BarButtons[i].font = font_create(FONT, FONT_COLOR, BarButtons[i].win);
        draw_text_icon_color(BarButtons[i].icon, "", (Vec2){0, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, BarButtons[i].font, BAR_BUTTON_LABEL_COLOR, 0, BarButtons[i].win, BAR_SIZE);
        draw_str(BarButtons[i].icon, BarButtons[i].font, 0, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));
        xoffset += extents.xOff + 20;
    }
    wm.bar.hidden = false;
    wm.bar.spawned = false;
}

void hide_bar() {
    if(!SHOW_BAR) return;
    if(wm.bar.hidden) return;
    wm.client_windows[get_client_index_window(wm.bar.win)].ignore_unmap = true;
    XUnmapWindow(wm.display, wm.bar.win);
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XUnmapWindow(wm.display, BarButtons[i].win);
    }
    wm.bar.hidden = true;
}

void unhide_bar() {
    if(!SHOW_BAR) return;
    if(!wm.bar.hidden) return;
    XMapWindow(wm.display, wm.bar.win);
    uint32_t xoffset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XMapWindow(wm.display, BarButtons[i].win);
        XClearWindow(wm.display, BarButtons[i].win);
        XGlyphInfo extents;
        XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BarButtons[i].icon, strlen(BarButtons[i].icon), &extents);

        draw_text_icon_color(BarButtons[i].icon, "", (Vec2){0, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, BarButtons[i].font, BAR_BUTTON_LABEL_COLOR, 0, BarButtons[i].win, BAR_SIZE);
        draw_str(BarButtons[i].icon, BarButtons[i].font, 0, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));
        xoffset += extents.xOff + 20;
    } 
    wm.bar.hidden = false;
    draw_bar();
}


static void change_bar_monitor(int32_t monitor) {
    XMoveWindow(wm.display, wm.bar.win, get_monitor_start_x(monitor), 0);
    XResizeWindow(wm.display, wm.bar.win, Monitors[wm.bar_monitor].width - 6, BAR_SIZE);

    int32_t offset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XGlyphInfo extents;
        XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BarButtons[i].icon, strlen(BarButtons[i].icon), &extents);
        XMoveWindow(wm.display, BarButtons[i].win, get_monitor_start_x(monitor) + BarButtonLabelPos[wm.bar_monitor] + offset, 0);
        offset += extents.xOff + 20;
    }
    draw_bar();
}
void get_cmd_output(const char* cmd, char* dst) {
    FILE* fp;
    char line[256];
    fp = popen(cmd, "r");
    while(fgets(line, sizeof(line), fp) != NULL) {
        strcpy(dst, line);
    }
    dst[strlen(dst) - 1] = '\0';
    pclose(fp);
}

FontStruct font_create(const char* fontname, const char* fontcolor, Window win) {
    FontStruct fs;
    XftFont* xft_font = XftFontOpenName(wm.display, wm.screen, fontname);
    XftDraw* xft_draw = XftDrawCreate(wm.display, win, DefaultVisual(wm.display, wm.screen), DefaultColormap(wm.display, wm.screen)); 
    XftColor xft_font_color;
    XftColorAllocName(wm.display,DefaultVisual(wm.display,0),DefaultColormap(wm.display,0),fontcolor, &xft_font_color);
    
    fs.font = xft_font;
    fs.draw = xft_draw;
    fs.color = xft_font_color;

    return fs;
}
void draw_str(const char* str, FontStruct font, int x, int y) {
    XftDrawStringUtf8(font.draw, &font.color, font.font, x, y, (XftChar8 *)str, strlen(str));
}

void draw_bar() {
    if(!SHOW_BAR) return;
    // Main Label
    uint32_t xoffset = 0;
    {
        XClearWindow(wm.display, wm.bar.win);
        for(uint32_t i = 0; i < BAR_SLICES_COUNT; i++) {
            if(!BarCommands[i].init) {
                get_cmd_output(BarCommands[i].cmd, BarCommands[i].text);
                BarCommands[i].init = true;
            }
            if(BarCommands[i].timer >= BarCommands[i].refresh_time) {
                get_cmd_output(BarCommands[i].cmd, BarCommands[i].text);
                BarCommands[i].timer = 0.0f;
            }
            XGlyphInfo extents;
            XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BarCommands[i].text, strlen(BarCommands[i].text), &extents);
            XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), BAR_MAIN_LABEL_COLOR);
            XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), xoffset, 0, extents.xOff, BAR_SIZE);
            draw_str(BarCommands[i].text, wm.bar.font, xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));
            xoffset += extents.xOff;
        }

        draw_triangle(wm.bar.win, 
                      (Vec2){.x = xoffset, .y = 0}, 
                      (Vec2){.x = xoffset + 20, .y = BAR_SIZE}, 
                      (Vec2){.x = xoffset, .y = BAR_SIZE}, 
                      BAR_MAIN_LABEL_COLOR);
        xoffset += 20;
    }

    
    // Info Label
    if(BAR_SHOW_INFO_LABEL)
    {
        Window focused;
        int revert_to;
        XGetInputFocus(wm.display, &focused, &revert_to);
        char* window_name = NULL;
        XFetchName(wm.display, focused, &window_name);
        bool focused_window = (window_name != NULL);
        draw_triangle(wm.bar.win, 
                      (Vec2){.x = xoffset + BAR_LABEL_PADDING, .y = 0}, 
                      (Vec2){.x = xoffset + BAR_LABEL_PADDING + 20, .y = BAR_SIZE}, 
                      (Vec2){.x = xoffset + BAR_LABEL_PADDING + 20, .y = 0}, 
                      BAR_INFO_LABEL_COLOR);
        xoffset += 20;
        xoffset += BAR_LABEL_PADDING;

        if(focused_window) {
            uint32_t program_label_offset = draw_text_icon_color(BAR_INFO_PROGRAM_ICON, window_name, 
                                                                (Vec2){xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)},
                                                                 wm.bar.font, BAR_INFO_LABEL_COLOR, 20, wm.bar.win, BAR_SIZE);
            xoffset += program_label_offset;
        }
        char monitor_index_str[8];
        sprintf(monitor_index_str, "%i", wm.focused_monitor);
        uint32_t monitor_label_offset = draw_text_icon_color(BAR_INFO_MONITOR_ICON, monitor_index_str, 
                                                             (Vec2){xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, 
                                                              wm.bar.font, BAR_INFO_LABEL_COLOR, 20, wm.bar.win, BAR_SIZE);
        xoffset += monitor_label_offset;
        
        char desktop_index_str[8];
        sprintf(desktop_index_str, "%hhd", wm.focused_desktop[wm.focused_monitor]);
        uint32_t desktop_label_offset = draw_text_icon_color(BAR_INFO_DESKTOP_ICON, desktop_index_str, 
                                                             (Vec2){xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, 
                                                             wm.bar.font, BAR_INFO_LABEL_COLOR, 20, wm.bar.win, BAR_SIZE);
        xoffset += desktop_label_offset;

        char current_layout_str[64];

        switch (wm.current_layout) {
            case WINDOW_LAYOUT_FLOATING:
                strcpy(current_layout_str, "Floating");
                break;
            case WINDOW_LAYOUT_TILED_MASTER:
                strcpy(current_layout_str, "Tiled Master");
                break;
        }
        uint32_t window_layout_label_offset = draw_text_icon_color(BAR_INFO_WINDOW_LAYOUT_ICON, current_layout_str, 
                                                             (Vec2){xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, 
                                                             wm.bar.font, BAR_INFO_LABEL_COLOR, 20, wm.bar.win, BAR_SIZE);
        xoffset += window_layout_label_offset;

        XFree(window_name);

        draw_triangle(wm.bar.win, 
                      (Vec2){.x = xoffset, .y = 0}, 
                      (Vec2){.x = xoffset + 20, .y = BAR_SIZE}, 
                      (Vec2){.x = xoffset, .y = BAR_SIZE}, 
                      BAR_INFO_LABEL_COLOR);
        xoffset += 20;
    }
    // Version label
    if(BAR_SHOW_VERSION_LABEL)
    {
        const char* text = "RagnarWM v"VERSION;
        uint32_t monitor_width = Monitors[wm.bar_monitor].width - 6;
        XGlyphInfo extents;
        XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)text, strlen(text), &extents);
        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), BAR_VERSION_LABEL_COLOR);
        XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), 
                        monitor_width - extents.xOff, 0, extents.xOff, BAR_SIZE);

        draw_str(text, wm.bar.font, monitor_width - extents.xOff, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));

        draw_triangle(wm.bar.win, 
                (Vec2){.x = monitor_width - extents.xOff - 20, .y = BAR_SIZE}, 
                  (Vec2){.x = monitor_width - extents.xOff, .y = BAR_SIZE}, 
                  (Vec2){.x = monitor_width - extents.xOff, .y = 0}, 
                  BAR_INFO_LABEL_COLOR);
    }
} 


void raise_bar() {
    if(SHOW_BAR) {
        XRaiseWindow(wm.display, wm.bar.win);
        for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
            XRaiseWindow(wm.display, BarButtons[i].win);
        }
    }
}

void draw_triangle(Window win, Vec2 p1, Vec2 p2, Vec2 p3, uint32_t color) {
    XPoint points[3] = {
        {.x = p1.x, .y = p1.y},
        {.x = p2.x, .y = p2.y},
        {.x = p3.x, .y = p3.y},
    };
    XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), color);
    XFillPolygon(wm.display, win, DefaultGC(wm.display, wm.screen), points, 3, Convex, CoordModeOrigin);

}
uint32_t draw_text_icon_color(const char* icon, const char* text, Vec2 pos, FontStruct font, uint32_t color, uint32_t rect_x_add, Window win, uint32_t height) {
        XGlyphInfo extents;
        XftTextExtents8(wm.display, font.font, (FcChar8*)text, strlen(text), &extents);
        XGlyphInfo extents_icon;
        XftTextExtents8(wm.display, font.font, (FcChar8*)icon, strlen(icon), &extents_icon);

        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), color);
        XFillRectangle(wm.display, win, DefaultGC(wm.display, wm.screen), pos.x, 0, extents.xOff + extents_icon.xOff + rect_x_add, height);

        draw_str(icon, font, pos.x, pos.y);
        draw_str(text, font, pos.x + extents_icon.xOff, pos.y);
        
        return extents.xOff + extents_icon.xOff + rect_x_add;
}

int main(void) {
    wm = xwm_init();
    xwm_run();
    xwm_terminate();
}
