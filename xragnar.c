#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcursor/Xcursor.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"

#define CLIENT_WINDOW_CAP 256

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef enum {
    WINDOW_LAYOUT_TILED_MASTER = 0
} WindowLayout;

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    Window win;
    Window frame;
    bool fullscreen;
    Vec2 fullscreen_revert_size;
    uint8_t monitor_index;
} Client;

typedef struct {
    Display* display;
    Window root;
    uint32_t focused_monitor;

    bool running;

    Client client_windows[CLIENT_WINDOW_CAP];
    uint32_t clients_count;

    Vec2 cursor_start_pos, 
        cursor_start_frame_pos, 
        cursor_start_frame_size;

    WindowLayout current_layout;
} XWM;


static bool wm_detected = false;
static XWM wm;

static Window get_frame_window(Window win);
static void handle_create_notify(XCreateWindowEvent e);
static void handle_reparent_notify(XReparentEvent e);
static void handle_map_notify(XMapEvent e);
static void handle_unmap_notify(XUnmapEvent e);
static void handle_configure_request(XConfigureRequestEvent e);
static void handle_configure_notify(XConfigureEvent e);
static void handle_motion_notify(XMotionEvent e);
static void handle_map_request(XMapRequestEvent e);
static void handle_destroy_notify(XDestroyWindowEvent e);
static int handle_x_error(Display* display, XErrorEvent* e);
static int handle_wm_detected(Display* display, XErrorEvent* e);
static void handle_button_press(XButtonEvent e);
static void handle_button_release(XButtonEvent e);
static void handle_key_press(XKeyEvent e);
static void handle_key_release(XKeyEvent e);
static bool client_window_exists(Window win);
static int32_t get_client_index(Window win);
static void grab_global_input();
static void grab_window_input(Window win);
static void selected_focused_monitor(uint32_t x_cursor);
static Vec2 get_cursor_position();
static int32_t get_focused_monitor_window_center_x(int32_t window_x);
static int32_t get_focused_monitor_start_x();
static void set_fullscreen(Window win);
static void unset_fullscreen(Window win);
static void establish_window_layout();
static void move_client(Client* client, Vec2 pos);
static void resize_client(Client* client, Vec2 size);

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

void xwm_window_frame(Window win, bool created_before_window_manager) {
    XWindowAttributes attribs;

    assert(XGetWindowAttributes(wm.display, win, &attribs) && "Failed to retrieve window attributes");

    if(created_before_window_manager) {
        if(attribs.override_redirect || attribs.map_state != IsViewable) {
            return;
        }
    }
    // Calculate X Position of window based on the currently focused monitor
    int32_t win_x = get_focused_monitor_window_center_x(attribs.width / 2);
       Window win_frame = XCreateSimpleWindow(wm.display, wm.root, 
                                           win_x, 
                                           (Monitors[wm.focused_monitor].height / 2) - (attribs.height / 2), attribs.width, attribs.height, 
                                           WINDOW_BORDER_WIDTH,  WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);

    XSelectInput(wm.display, win_frame, SubstructureRedirectMask | SubstructureNotifyMask); 
    XReparentWindow(wm.display, win, win_frame, 0, 0);

    XMapWindow(wm.display, win_frame);

    wm.client_windows[wm.clients_count++] = (Client){.frame = win_frame, .win =  win, 
        .fullscreen = attribs.width >= (int32_t)Monitors[wm.focused_monitor].width && attribs.height >= (int32_t)Monitors[wm.focused_monitor].height, 
        .monitor_index = wm.focused_monitor};
    grab_window_input(win);
    establish_window_layout();
}

void xwm_window_unframe(Window win) {
    Window frame_win = get_frame_window(win);

    XUnmapWindow(wm.display, frame_win);
    XDestroyWindow(wm.display, frame_win);
    for(uint32_t i = get_client_index(win); i < wm.clients_count - 1; i++)
        wm.client_windows[i] = wm.client_windows[i + 1];
    XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
    wm.clients_count--;
    establish_window_layout();
}
void xwm_run() {
    wm.clients_count = 0;
    wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f}; 
    wm.running = true;
    wm.focused_monitor = MONITOR_COUNT - 1;
    wm.current_layout = WINDOW_LAYOUT_TILED_MASTER;

    XSetErrorHandler(handle_wm_detected);
    XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask); 
    XSync(wm.display, false);
    if(wm_detected) {
        printf("Another window manager is already running on this X display.\n");
        return;
    }
    
    Cursor cursor = XcursorLibraryLoadCursor(wm.display, "arrow");
    XDefineCursor(wm.display, wm.root, cursor);
    XSetErrorHandler(handle_x_error);

    grab_global_input();
     
    while(wm.running) {
        // Query mouse position to get focused monitor
        selected_focused_monitor(get_cursor_position().x);
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
    XWindowChanges changes;
    changes.x = e.x;
    changes.y = e.y;
    changes.width = e.width;
    changes.height = e.height;
    changes.border_width = e.border_width;
    changes.sibling = e.above;
    changes.stack_mode = e.detail;
    if(client_window_exists(e.window)) {
        Window frame_win = get_frame_window(e.window);
        XConfigureWindow(wm.display,frame_win, e.value_mask, &changes);
    }
    XConfigureWindow(wm.display, e.window, e.value_mask, &changes);
}

void handle_configure_notify(XConfigureEvent e) {
    (void)e;
}
void handle_motion_notify(XMotionEvent e) {
    assert(client_window_exists(e.window) && "Motion on unmapped window");
    selected_focused_monitor(e.x_root);

    Vec2 drag_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};
    Vec2 delta_drag = (Vec2){.x = drag_pos.x - wm.cursor_start_pos.x, .y = drag_pos.y - wm.cursor_start_pos.y};

    if(e.state & Button1Mask) {
        Vec2 drag_dest = (Vec2){.x = (float)(wm.cursor_start_frame_pos.x + delta_drag.x), 
            .y = (float)(wm.cursor_start_frame_pos.y + delta_drag.y)};
        if(wm.client_windows[get_client_index(e.window)].fullscreen) {
            wm.client_windows[get_client_index(e.window)].fullscreen = false;
            XSetWindowBorderWidth(wm.display, wm.client_windows[get_client_index(e.window)].frame, WINDOW_BORDER_WIDTH);
        } 
        move_client(&wm.client_windows[get_client_index(e.window)], drag_dest);
    } else if(e.state & Button3Mask) {
        Vec2 resize_delta = (Vec2){.x = MAX(delta_drag.x, -wm.cursor_start_frame_size.x),
                                    .y = MAX(delta_drag.y, -wm.cursor_start_frame_size.y)};
        Vec2 resize_dest = (Vec2){.x = wm.cursor_start_frame_size.x + resize_delta.x, 
                                    .y = wm.cursor_start_frame_size.y + resize_delta.y};
        resize_client(&wm.client_windows[get_client_index(e.window)], resize_dest);
    }
}

void handle_map_request(XMapRequestEvent e) {
    xwm_window_frame(e.window, false);
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
    wm_detected = ((int32_t)e->error_code != BadAccess);
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
    if(!client_window_exists(e.window)) {
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
    XSetInputFocus(wm.display, e.window, RevertToParent, CurrentTime);
}
void handle_button_release(XButtonEvent e) {
    (void)e;
}

void handle_key_press(XKeyEvent e) {
    if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_CLOSE_KEY)) {
        XEvent msg;
        memset(&msg, 0, sizeof(msg));
        msg.xclient.type = ClientMessage;
        msg.xclient.message_type =  XInternAtom(wm.display, "WM_PROTOCOLS", false);
        msg.xclient.window = e.window;
        msg.xclient.format = 32;
        msg.xclient.data.l[0] = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
        XSendEvent(wm.display, e.window, false, 0, &msg);
        //establish_window_layout();
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_CYCLE_KEY)) {
        Client client = { 0 };
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].win == e.window ||
                wm.client_windows[i].frame == e.window) {
                if(i + 1 >= wm.clients_count) {
                    client = wm.client_windows[0];
                } else {
                    client = wm.client_windows[i + 1];
                }
                break;
            }
        }
        XRaiseWindow(wm.display, client.frame);
        XSetInputFocus(wm.display, client.win, RevertToParent, CurrentTime);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WM_TERMINATE_KEY)) {
        wm.running = false;
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, TERMINAL_OPEN_KEY)) {
        system(TERMINAL_CMD);  
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WEB_BROWSER_KEY)) {
        system(WEB_BROWSER_CMD);   
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, FULLSCREEN_KEY)) {
        if(!wm.client_windows[get_client_index(e.window)].fullscreen) {
            set_fullscreen(e.window);
        } else {
            unset_fullscreen(e.window);
        }
    }
}
void handle_key_release(XKeyEvent e) {
    (void)e;
}
bool client_window_exists(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win || wm.client_windows[i].frame == win) return true;
    }
    return false;
}


int32_t get_client_index(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win || wm.client_windows[i].frame == win) return i;
    }
    return -1;
}

static void grab_global_input() {     
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WM_TERMINATE_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, TERMINAL_OPEN_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WEB_BROWSER_KEY),MASTER_KEY, wm.root, false, GrabModeAsync,GrabModeAsync);

}
static void grab_window_input(Window win) {
    XGrabButton(wm.display,Button1,MASTER_KEY,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabButton(wm.display,Button3,MASTER_KEY,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabKey(wm.display, XKeysymToKeycode(wm.display, WINDOW_CLOSE_KEY),MASTER_KEY, win,false, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_CYCLE_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, FULLSCREEN_KEY),MASTER_KEY,win,false, GrabModeAsync,GrabModeAsync);
}

void selected_focused_monitor(uint32_t x_cursor) {
    uint32_t x_offset = 0;
    for(uint32_t i = 0; i < MONITOR_COUNT; i++) {
        if(x_cursor >= x_offset && x_cursor <= Monitors[i].width + x_offset) {
            wm.focused_monitor = i;
            break;
        }
        x_offset += Monitors[i].width;
    }
}

static Vec2 get_cursor_position() {
    Window root_return, child_return;
    int win_x_return, win_y_return, root_x_return, root_y_return; 
    uint32_t mask_return;
    XQueryPointer(wm.display, wm.root, &root_return, &child_return, &root_x_return, &root_y_return, 
        &win_x_return, &win_y_return, &mask_return);
    return (Vec2){.x = root_x_return, .y = root_y_return};  
}

int32_t get_focused_monitor_window_center_x(int32_t window_x) {
    int32_t x = 0;
    for(uint32_t i = 0; i < wm.focused_monitor + 1; i++) {
        if(i > 0) 
            x += Monitors[i - 1].width;
        if(i == wm.focused_monitor)
            x += Monitors[wm.focused_monitor].width / 2;
    }
    x -= window_x;
    return x;
}


int32_t get_focused_monitor_start_x() {
    int32_t x = 0;
    for(uint32_t i = 0; i < wm.focused_monitor; i++) {
        x += Monitors[i].width;    
    }
    return x;
}


static void set_fullscreen(Window win) {
    uint32_t client_index = get_client_index(win);
    resize_client(&wm.client_windows[client_index], (Vec2){. x = Monitors[wm.focused_monitor].width, .y = Monitors[wm.focused_monitor].height});
    move_client(&wm.client_windows[client_index], (Vec2){.x = get_focused_monitor_start_x(), 0});
}
static void unset_fullscreen(Window win)  {
    uint32_t client_index = get_client_index(win);
    resize_client(&wm.client_windows[client_index], wm.client_windows[client_index].fullscreen_revert_size);
    move_client(&wm.client_windows[client_index], (Vec2){get_focused_monitor_start_x(), 0});
}

void move_client(Client* client, Vec2 pos) {
    XMoveWindow(wm.display, client->frame, pos.x, pos.y);
}
void resize_client(Client* client, Vec2 size) {
    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, client->win, &attribs);
    if(size.x >= Monitors[wm.focused_monitor].width || size.y >= Monitors[wm.focused_monitor].height) {
        client->fullscreen = true;
        XSetWindowBorderWidth(wm.display, client->frame, 0);
        client->fullscreen_revert_size = (Vec2){.x = attribs.width, .y = attribs.height};
    } else {
        client->fullscreen = false;
        XSetWindowBorderWidth(wm.display, client->frame, WINDOW_BORDER_WIDTH);
    }
    XResizeWindow(wm.display, client->win, size.x, size.y);
    XResizeWindow(wm.display, client->frame, size.x, size.y);
}


void establish_window_layout() {
    Client* clients[CLIENT_WINDOW_CAP]; 
    uint32_t client_count = 0;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    if(client_count == 0) return;
    Client* master = clients[0];
    if(wm.current_layout == WINDOW_LAYOUT_TILED_MASTER) {
        // set master fullscreen if no slaves
        if(client_count == 1) {
            set_fullscreen(master->frame);
            return;
        }
            
        // set master
        move_client(master, (Vec2){get_focused_monitor_start_x(), 0});
        resize_client(master, (Vec2){
            .x = (int32_t)(Monitors[wm.focused_monitor].width / 2),
            .y = Monitors[wm.focused_monitor].height});
        master->fullscreen = false;
        XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);

        // set slaves
        for(uint32_t i = 1; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            resize_client(clients[i], (Vec2){
                (int32_t)(Monitors[wm.focused_monitor].width / 2),
                (int32_t)(Monitors[wm.focused_monitor].height / (client_count - 1))
            });

            move_client(clients[i], (Vec2){
                get_focused_monitor_start_x() + (int32_t)(Monitors[wm.focused_monitor].width / 2),
                Monitors[wm.focused_monitor].height - (int32_t)((Monitors[wm.focused_monitor].height / (client_count - 1) * i))
            });
        }
    }    
}
int main(void) {
    wm = xwm_init();
    xwm_run();
    xwm_terminate();
}
