#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <err.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define CLIENT_WINDOW_CAP 256

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
    Window win;
    Window frame;
} Client;

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    Display* display;
    Window root;

    bool running;

    Client client_windows[CLIENT_WINDOW_CAP];
    uint32_t clients_count;

    Vec2 cursor_start_pos, 
        cursor_start_frame_pos, 
        cursor_start_frame_size;

    Atom ATOM_WM_DELETE_WINDOW;
    Atom ATOM_WM_PROTOCOLS;
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
static void handle_button_relaese(XButtonEvent e);
static void handle_key_press(XKeyEvent e);
static void handle_key_release(XKeyEvent e);
static bool client_window_exists(Window win);
static Atom* find_atom_ptr_range(Atom* ptr1, Atom* ptr2, Atom val);

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
    const uint32_t BORDER_WIDTH = 3;
    const unsigned long BORDER_COLOR = 0x2b34d9;
    const unsigned long BG_COLOR = 0xffffff;

    XWindowAttributes attribs;

    assert(XGetWindowAttributes(wm.display, win, &attribs) && "Failed to retrieve window attributes");

    if(created_before_window_manager) {
        if(attribs.override_redirect || attribs.map_state != IsViewable) {
            return;
        }
    }

    Window win_frame = XCreateSimpleWindow(wm.display, wm.root, attribs.x, attribs.y, attribs.width, attribs.height, BORDER_WIDTH, BORDER_COLOR, BG_COLOR);

    XSelectInput(wm.display, win_frame, SubstructureRedirectMask | SubstructureNotifyMask); 
    XAddToSaveSet(wm.display, win);
    XReparentWindow(wm.display, win, win_frame, 0, 0);

    XMapWindow(wm.display, win_frame);

    wm.client_windows[wm.clients_count].frame = win_frame;
    wm.client_windows[wm.clients_count++].win = win;

    XGrabButton(wm.display,Button1,Mod1Mask,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabButton(wm.display,Button3,Mod1Mask,win,false,ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync,GrabModeAsync,None,None);
    XGrabKey(wm.display, XKeysymToKeycode(wm.display, XK_Q),Mod1Mask, win,false, GrabModeAsync, GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, XK_Tab),Mod1Mask,win,false, GrabModeAsync,GrabModeAsync);
    printf("HELLO\n");
}

void xwm_window_unframe(Window win) {
    Window frame_win = get_frame_window(win);

    XUnmapWindow(wm.display, frame_win);
    XReparentWindow(wm.display, win, wm.root, 0, 0);
    XRemoveFromSaveSet(wm.display, win);
    XDestroyWindow(wm.display, frame_win);
    for(uint64_t i = win; i < wm.clients_count - 1; i++)
        wm.client_windows[i] = wm.client_windows[i + 1];

    wm.clients_count--;

}
void xwm_run() {
    XSetErrorHandler(handle_wm_detected);
    XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask); 
    XSync(wm.display, false);
    wm.clients_count = 0;
    wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.ATOM_WM_DELETE_WINDOW = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
    wm.ATOM_WM_PROTOCOLS = XInternAtom(wm.display, "WM_PROTOCOLS", false);

    if(wm_detected) {
        printf("Another window manager is already running on this X display.\n");
        return;
    }
    XSetErrorHandler(handle_x_error);

    XGrabServer(wm.display);

    Window ret_root, ret_parent;
    Window* top_level_windows;
    uint32_t top_level_windows_count;
    assert(XQueryTree(wm.display, wm.root, &ret_root, &ret_parent, &top_level_windows, &top_level_windows_count) 
           && "Failed to query X tree");

    for(uint32_t i = 0; i < top_level_windows_count; i++) {
        xwm_window_frame(top_level_windows[i], true);
    }
    XFree(top_level_windows);
    XUngrabServer(wm.display);
    
    wm.running = true;

    XGrabKey(wm.display,XKeysymToKeycode(wm.display, XK_C),Mod1Mask, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, XK_Return),Mod1Mask, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, XK_W),Mod1Mask, wm.root,false, GrabModeAsync,GrabModeAsync);
    while(wm.running) {
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
                handle_button_relaese(e.xbutton);
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
        if(wm.client_windows[i].win == win) {
            return wm.client_windows[i].frame;
        }
    }
    return 0;
}
void handle_create_notify(XCreateWindowEvent e) {
    
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

}
void handle_motion_notify(XMotionEvent e) {
    assert(client_window_exists(e.window) && "Motion on unmapped window");
    Window frame = get_frame_window(e.window);
    Vec2 drag_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};
    Vec2 delta_drag = (Vec2){.x = drag_pos.x - wm.cursor_start_pos.x, .y = drag_pos.y - wm.cursor_start_pos.y};

    if(e.state & Button1Mask) {
        /* Pressed alt + left mouse */
        Vec2 drag_dest = (Vec2){.x = (float)(wm.cursor_start_frame_pos.x + delta_drag.x), 
            .y = (float)(wm.cursor_start_frame_pos.y + delta_drag.y)};
        XMoveWindow(wm.display, frame, drag_dest.x, drag_dest.y);

    } else if(e.state & Button3Mask) {
        /* Pressed alt + right mouse*/
        Vec2 resize_delta = (Vec2){.x = MAX(delta_drag.x, -wm.cursor_start_frame_size.x),
                                    .y = MAX(delta_drag.y, -wm.cursor_start_frame_size.y)};
        Vec2 resize_dest = (Vec2){.x = wm.cursor_start_frame_size.x + resize_delta.x, 
                                    .y = wm.cursor_start_frame_size.y + resize_delta.y};
        XResizeWindow(wm.display, frame, resize_dest.x, resize_dest.y);
        XResizeWindow(wm.display, e.window, resize_dest.x, resize_dest.y);
    }
}

void handle_map_request(XMapRequestEvent e) {
    xwm_window_frame(e.window, false);
    XMapWindow(wm.display, e.window);
}
int handle_x_error(Display* display, XErrorEvent* e) {
    return 0;  
}

int handle_wm_detected(Display* display, XErrorEvent* e) {
    wm_detected = ((int32_t)e->error_code != BadAccess);
    return 0;
}

void handle_reparent_notify(XReparentEvent e) {
}

void handle_destroy_notify(XDestroyWindowEvent e) {
}
void handle_map_notify(XMapEvent e) {
}
void handle_unmap_notify(XUnmapEvent e) {
    if(!client_window_exists(e.window) || e.event == wm.root) {
        printf("Ignoring unmap of window %li. Not a client window\n", e.window);
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
    XSetInputFocus(wm.display, frame, RevertToParent, CurrentTime);
}
void handle_button_relaese(XButtonEvent e) {

}

Atom* find_atom_ptr_range(Atom* ptr1, Atom* ptr2, Atom val) {
    while(ptr1 < ptr2) {
        ptr1++;
        if(*ptr1 == val) {
            return ptr1;
        }
    }
    return ptr1;
}
void handle_key_press(XKeyEvent e) {
    if(e.state & Mod1Mask && e.keycode == XKeysymToKeycode(wm.display, XK_Q)) {
        Atom* protocols;
        int32_t num_protocols;
        if(XGetWMProtocols(wm.display, e.window, &protocols, &num_protocols)
            && find_atom_ptr_range(protocols, protocols + num_protocols, wm.ATOM_WM_DELETE_WINDOW)
            != protocols + num_protocols) {
            XEvent msg;
            memset(&msg, 0, sizeof(msg));
            msg.xclient.type = ClientMessage;
            msg.xclient.message_type = wm.ATOM_WM_PROTOCOLS;
            msg.xclient.window = e.window;
            msg.xclient.format = 32;
            msg.xclient.data.l[0] = wm.ATOM_WM_DELETE_WINDOW;
            XSendEvent(wm.display, e.window, false, 0, &msg);
        } else {
            XKillClient(wm.display, e.window);
        }
    } else if(e.state & Mod1Mask && e.keycode == XKeysymToKeycode(wm.display, XK_Tab)) {
        Client client;
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
    } else if(e.state & Mod1Mask && e.keycode == XKeysymToKeycode(wm.display, XK_C)) {
        wm.running = false;
    } else if(e.state & Mod1Mask && e.keycode == XKeysymToKeycode(wm.display, XK_Return)) {
        system("alacritty &");   
    } else if(e.state & Mod1Mask && e.keycode == XKeysymToKeycode(wm.display, XK_W)) {

        system("chromium &");   
    }
}
void handle_key_release(XKeyEvent e) {

}
bool client_window_exists(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win) return true;
    }

    return false;
}

int main(void) {
    wm = xwm_init();
    xwm_run();
    xwm_terminate();
}
