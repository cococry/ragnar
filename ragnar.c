#include <X11/X.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xatom.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"


#define VERSION "1.4"

#define CLIENT_WINDOW_CAP 256

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct {
    float x, y;
} Vec2;


typedef struct {
    Window win;
    bool hidden;
    FontStruct font;
    int32_t DESKTOP_LABEL_x;
    bool init;
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
    bool in, skip;
    int32_t change;
} LayoutProps;

typedef struct {
    Window win;
    Window frame;

    bool fullscreen;
    Vec2 fullscreen_revert_size;
    Vec2 fullscreen_revert_pos;

    uint8_t monitor_index;
    int8_t desktop_index;

    LayoutProps layout;
    bool ignore_unmap, ignore_enter;

    Decoration decoration;
} Client;

typedef struct {
    Display* display;
    Window root;
    int screen;

    bool running;

    Client client_windows[CLIENT_WINDOW_CAP];
    uint32_t clients_count;

    int8_t focused_monitor;
    int32_t bar_monitor;
    int32_t focused_client;
    int8_t focused_desktop[MONITOR_COUNT];
    int32_t hard_focused_window_index;

    Vec2 cursor_start_pos, 
    cursor_start_frame_pos, 
    cursor_start_frame_size;

    WindowLayout current_layout;
    uint32_t layout_master_size[MONITOR_COUNT][DESKTOP_COUNT];
    bool layout_full;
    int32_t window_gap;

    bool decoration_hidden;
    Bar bar;
    double bar_refresh_timer;

    bool spawning_scratchpad;
    int32_t spawned_scratchpad_index;

} RagnarWM; 

static RagnarWM wm;

static void         handle_unmap_notify(XUnmapEvent e);
static void         handle_configure_request(XConfigureRequestEvent e);
static void         handle_motion_notify(XMotionEvent e);
static void         handle_map_request(XMapRequestEvent e);
static int          handle_x_error(Display* display, XErrorEvent* e);
static void         handle_button_press(XButtonEvent e);
static void         handle_key_press(XKeyEvent e);
static void         grab_global_input();
static void         grab_window_input(Window win);

static Vec2         get_cursor_position();

static void         select_focused_monitor(uint32_t x_cursor);
static int32_t      get_monitor_index_by_window(int32_t xpos);
static int32_t      get_focused_monitor_window_center_x(int32_t window_x);
static int32_t      get_monitor_start_x(int8_t monitor);
static int32_t      get_client_index_window(Window win);
static int32_t      get_scratchpad_index_window(Window win);

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
static int32_t      draw_bar_desktop_label();
static void         draw_bar_buttons();
static void         get_cmd_output(const char* cmd, char* dst);
static bool         str_unicode(const char* str);
static FontStruct   font_create(const char* fontname, const char* fontcolor, Window win);

static void         draw_str(const char* str, FontStruct font, XftColor* color, int x, int y);
static void         draw_triangle(Window win, Vec2 p1, Vec2 p2, Vec2 p3, uint32_t color);
static uint32_t     draw_text_icon_color(const char* icon, const char* text, Vec2 pos, FontStruct font, uint32_t color, uint32_t rect_x_add, Window win, uint32_t height);
static void         draw_half_circle(Window win, Vec2 pos, int32_t radius, uint32_t color, bool to_left);
static void         draw_design(Window win, int32_t xpos, BarLabelDesign design, uint32_t color, uint32_t design_width, uint32_t design_height);
static void*        update_ui();

void ragnar_init() {
    system("ragnarstart");
    wm.display = XOpenDisplay(NULL);
    if(!wm.display) {
        printf("Failed to open X Display.\n");
    }
    // Setting the startup variables
    wm.clients_count = 0;
    wm.cursor_start_frame_size = (Vec2){ .x = 0.0f, .y = 0.0f};
    wm.cursor_start_frame_pos = (Vec2){ .x = 0.0f, .y = 0.0f}; 
    wm.cursor_start_pos = (Vec2){ .x = 0.0f, .y = 0.0f}; 
    wm.running = true;
    wm.focused_monitor = MONITOR_COUNT - 1;
    wm.current_layout = WINDOW_LAYOUT_DEFAULT;
    wm.bar_monitor = BAR_START_MONITOR;
    wm.window_gap = WINDOW_INITIAL_GAP;
    wm.decoration_hidden = !SHOW_DECORATION;
    wm.spawning_scratchpad = false;
    wm.layout_full = false;
    wm.spawned_scratchpad_index = 0;
    wm.hard_focused_window_index = -1;
    wm.focused_client = -1;
    memset(wm.focused_desktop, 0, sizeof(wm.focused_desktop));
    memset(wm.layout_master_size, 0, sizeof(wm.layout_master_size));
    wm.root = DefaultRootWindow(wm.display);
    wm.screen = DefaultScreen(wm.display);

    // Setting the window manager name
    const char *name = "Ragnar";
    XChangeProperty(wm.display, wm.root,
                    XInternAtom(wm.display, "_NET_WM_NAME", False),
                    XInternAtom(wm.display, "UTF8_STRING", False),
                    8, PropModeReplace, (const unsigned char *)name, strlen(name));
    XFlush(wm.display);
    XSelectInput(wm.display, wm.root, SubstructureRedirectMask | SubstructureNotifyMask); 
    XSync(wm.display, false);

    Cursor cursor = XcursorLibraryLoadCursor(wm.display, "arrow");
    XDefineCursor(wm.display, wm.root, cursor);
    XSetErrorHandler(handle_x_error);

    XSetWindowAttributes attributes;
    attributes.event_mask = ButtonPress | KeyPress | SubstructureRedirectMask | SubstructureNotifyMask | PropertyChangeMask;
    XChangeWindowAttributes(wm.display, wm.root, CWEventMask, &attributes);

    grab_global_input();
    if(SHOW_BAR) {
        create_bar();
        draw_bar();
        draw_bar_buttons();
    }
}

void ragnar_terminate() {
    XUnmapWindow(wm.display, wm.bar.win);

    // Free all the allocated colors 
    for(uint32_t i = 0; i < BAR_COMMANDS_COUNT; i++) {
        XftColorFree(wm.display, DefaultVisual(wm.display, wm.screen), DefaultColormap(wm.display, wm.screen), &BarCommands[i]._xcolor); 
    }
    for(uint32_t i = 0; i < DESKTOP_COUNT; i++) {
        XftColorFree(wm.display, DefaultVisual(wm.display, wm.screen), DefaultColormap(wm.display, wm.screen), &DesktopIcons[i]._xcolor); 
    }
    XCloseDisplay(wm.display);
}

void ragnar_window_frame(Window win) {
    // If the window is already framed, return
    if(get_client_index_window(win) != -1) return;

    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, win, &attribs);

    // Creating the X Window frame
    int32_t win_x = get_focused_monitor_window_center_x(attribs.width / 2);

    /* Frame window */
    Window win_frame;
    if(WINDOW_TRANSPARENT_FRAME) {
        XVisualInfo vinfo;
        XSetWindowAttributes attribs_set;
        XMatchVisualInfo(wm.display, DefaultScreen(wm.display), 32, TrueColor, &vinfo);
        attribs_set.background_pixel = 0;
        attribs_set.border_pixel = WINDOW_BORDER_COLOR;
        attribs_set.colormap = XCreateColormap(wm.display, wm.root, vinfo.visual, AllocNone);
        attribs_set.event_mask = StructureNotifyMask | ExposureMask;

        win_frame = XCreateWindow(wm.display, wm.root, 
                                  win_x, 
                                  (Monitors[wm.focused_monitor].height / 2) - (attribs.height / 2), attribs.width, attribs.height, 
                                  WINDOW_BORDER_WIDTH,vinfo.depth, InputOutput, vinfo.visual, CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &attribs_set);
        XCompositeRedirectWindow(wm.display, win_frame, CompositeRedirectAutomatic);
    } else {
        win_frame = XCreateSimpleWindow(wm.display, wm.root, win_x, (Monitors[wm.focused_monitor].height / 2) - (attribs.height / 2),
                                        attribs.width, attribs.height, WINDOW_BORDER_WIDTH, WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);
    }
    if(WINDOW_SELECT_HOVERED) {
        XSelectInput(wm.display, win_frame, SubstructureRedirectMask | SubstructureNotifyMask | EnterWindowMask); 
    } else {
        XSelectInput(wm.display, win_frame, SubstructureRedirectMask | SubstructureNotifyMask); 
    }

    XWMHints *sourceHints = XGetWMHints(wm.display, win);
    if(sourceHints) {
        XSetWMHints(wm.display, win_frame, sourceHints);
        XFree(sourceHints);
    }
    XReparentWindow(wm.display, win, win_frame, 0, ((SHOW_DECORATION && !wm.decoration_hidden && !wm.spawning_scratchpad) ? DECORATION_TITLEBAR_SIZE : 0));
    if(!wm.spawning_scratchpad && SHOW_DECORATION && !wm.decoration_hidden) {
        XResizeWindow(wm.display, win, attribs.width,attribs.height - DECORATION_TITLEBAR_SIZE);
        XMoveWindow(wm.display, win, 0, DECORATION_TITLEBAR_SIZE);
    }
    XMapWindow(wm.display, win_frame);
    grab_window_input(win);
    XRaiseWindow(wm.display, win_frame);
    raise_bar();

    if(wm.spawning_scratchpad) {
        ScratchpadDefs[wm.spawned_scratchpad_index].frame = win_frame;
        ScratchpadDefs[wm.spawned_scratchpad_index].win = win;
        wm.spawning_scratchpad = false;
        return;
    }
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        wm.client_windows[i].layout.change = 0;
    }
  
    Atom type;
    int format;
    unsigned long num_items, bytes_after;
    unsigned char *prop_value = NULL;
    bool tile = false;
    if (XGetWindowProperty(wm.display, win, XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE", False), 0, 1, False, XA_ATOM, &type, &format, &num_items, &bytes_after, &prop_value) == Success) {
        if (num_items > 0) {
            Atom hint = ((Atom *)prop_value)[0];

            if (hint == XInternAtom(wm.display, "_NET_WM_WINDOW_TYPE_NORMAL", False)) {
                tile = true;
            } else {
                tile = false;
            }
        }
    }
    Client client = (Client){
        .frame = win_frame, 
        .win =  win, 
        .fullscreen = attribs.width >= (int32_t)Monitors[wm.focused_monitor].width && attribs.height >= (int32_t)Monitors[wm.focused_monitor].height, 
        .monitor_index = wm.focused_monitor,
        .desktop_index = wm.focused_desktop[wm.focused_monitor],
        .layout.in = tile,
        .layout.change = 0,
        .layout.skip = false,
        .ignore_unmap = false};

    // Adding the window to the clients 
    wm.focused_client = wm.clients_count - 1;
    wm.client_windows[wm.clients_count++] = client; 
    establish_window_layout();

    // Drawing decoration
    if (SHOW_DECORATION) { 
        Window win_decoration = XCreateSimpleWindow(wm.display, wm.root, win_x, (Monitors[wm.focused_monitor].height / 2) - (attribs.height / 2), attribs.width, attribs.height, 
                                                    WINDOW_BORDER_WIDTH, WINDOW_BORDER_COLOR, WINDOW_BG_COLOR);
        XUnmapWindow(wm.display, win_decoration);

        int32_t client_index = get_client_index_window(win);

        XWindowAttributes attribs_frame;
        XGetWindowAttributes(wm.display, win_frame, &attribs_frame);

        /* Titlebar */
        wm.client_windows[client_index].decoration.titlebar = XCreateSimpleWindow(wm.display, win_decoration, 0, 0, attribs_frame.width, DECORATION_TITLEBAR_SIZE,0,0, DECORATION_COLOR);
        XSelectInput(wm.display, wm.client_windows[client_index].decoration.titlebar, SubstructureRedirectMask | SubstructureNotifyMask); 
        XMapWindow(wm.display, wm.client_windows[client_index].decoration.titlebar);
        XReparentWindow(wm.display, wm.client_windows[client_index].decoration.titlebar, win_frame, 0, 0);

        /* Close button */
        uint32_t x_offset = 0;
        if(DECORATION_SHOW_CLOSE_ICON) {
            x_offset += DECORATION_CLOSE_ICON_SIZE;
            wm.client_windows[client_index].decoration.closebutton = XCreateSimpleWindow(wm.display, win_decoration, attribs_frame.width - x_offset, 0, 
                                                                                         DECORATION_CLOSE_ICON_SIZE, DECORATION_TITLEBAR_SIZE, 0,  WINDOW_BORDER_COLOR, DECORATION_CLOSE_ICON_COLOR);
            XSelectInput(wm.display,wm.client_windows[client_index].decoration.closebutton, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask); 
            XMapWindow(wm.display, wm.client_windows[client_index].decoration.closebutton);
            XRaiseWindow(wm.display, wm.client_windows[client_index].decoration.closebutton);
            XReparentWindow(wm.display, wm.client_windows[client_index].decoration.closebutton, win_frame, attribs_frame.width - x_offset, 0);
        }
        /* Fullscreen button */
        if(DECORATION_SHOW_MAXIMIZE_ICON) {
            x_offset += DECORATION_MAXIMIZE_ICON_SIZE;
            wm.client_windows[client_index].decoration.maximize_button = XCreateSimpleWindow(wm.display, win_decoration, attribs_frame.width - x_offset, 0, 
                                                                                             DECORATION_MAXIMIZE_ICON_SIZE, DECORATION_TITLEBAR_SIZE, 0,  WINDOW_BORDER_COLOR, DECORATION_MAXIMIZE_ICON_COLOR);
            XSelectInput(wm.display,wm.client_windows[client_index].decoration.maximize_button, SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask); 
            XMapWindow(wm.display, wm.client_windows[client_index].decoration.maximize_button);
            XRaiseWindow(wm.display, wm.client_windows[client_index].decoration.maximize_button);
            XReparentWindow(wm.display, wm.client_windows[client_index].decoration.maximize_button, win_frame, attribs_frame.width - x_offset, 0);
        }

        /* Creating fonts & drawing decoration */
        wm.client_windows[client_index].decoration.closebutton_font = font_create(FONT, DECORATION_FONT_COLOR, wm.client_windows[client_index].decoration.closebutton);
        wm.client_windows[client_index].decoration.maximize_button_font = font_create(FONT, DECORATION_FONT_COLOR, wm.client_windows[client_index].decoration.maximize_button);
        wm.client_windows[client_index].decoration.titlebar_font = font_create(FONT, DECORATION_FONT_COLOR, wm.client_windows[client_index].decoration.titlebar);

        if(wm.decoration_hidden) {
            hide_client_decoration(&wm.client_windows[client_index]);
            XMoveWindow(wm.display, wm.client_windows[client_index].win, 0, 0);
            XResizeWindow(wm.display, wm.client_windows[client_index].win, attribs_frame.width, attribs_frame.height);
        } else { 
            redraw_client_decoration(&wm.client_windows[client_index]);
        }
    }
    draw_bar_buttons();
}
void ragnar_window_unframe(Window win) {
    // If the window was not framed, return
    int32_t client_index = get_client_index_window(win);
    if(client_index == -1) {
        return;
    }
    bool fullscreen = wm.client_windows[client_index].fullscreen;
    // If the unframe happend through a changed desktop, keep the client in ram & return
    if(wm.client_windows[client_index].ignore_unmap) {
        wm.client_windows[client_index].ignore_unmap = false;
        return;
    }
    if(wm.client_windows[client_index].fullscreen) {
        unhide_bar();
    }
    Window frame_win = wm.client_windows[client_index].frame;

    // If master window of layout was unframed, reset master size
    if(wm.client_windows[client_index].layout.in && get_client_index_window(win) == 0) {
        wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;
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
    XReparentWindow(wm.display, frame_win, wm.root, 0, 0);
    XUnmapWindow(wm.display, frame_win);
    XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
    // Removing the window from the clients
    for(uint32_t i = client_index; i < wm.clients_count - 1; i++) {
        wm.client_windows[i] = wm.client_windows[i + 1];
    }
    memset(&wm.client_windows[wm.clients_count - 1], 0, sizeof(Client));
    wm.clients_count--;

    if(wm.clients_count != 0) {
        wm.client_windows[wm.clients_count - 1].layout.change = 0;
    }
    if(wm.hard_focused_window_index == client_index) {
        wm.hard_focused_window_index = -1;
    }
    draw_bar_buttons();
    draw_bar();
    if(fullscreen) {
        unhide_bar();
    }
    establish_window_layout();
}
void ragnar_run() {
    XEvent e;
    while(wm.running) {
        XNextEvent(wm.display, &e);
        switch (e.type) {
            case UnmapNotify:
                handle_unmap_notify(e.xunmap);
                break;
            case MapRequest:
                handle_map_request(e.xmaprequest);
                break;
            case ConfigureRequest:
                handle_configure_request(e.xconfigurerequest);
                break;
            case ButtonPress:
                handle_button_press(e.xbutton);
                break;
            case KeyPress:
                handle_key_press(e.xkey);
                break;
            case MotionNotify: {
                select_focused_monitor(get_cursor_position().x);
                handle_motion_notify(e.xmotion);
                break;
            }
            case EnterNotify: {
                if(!WINDOW_SELECT_HOVERED || wm.hard_focused_window_index != -1) break;
                int32_t client_index = get_client_index_window(e.xcrossing.window);
                if(client_index == -1) break;
                if(wm.client_windows[client_index].ignore_enter) {
                    wm.client_windows[client_index].ignore_enter = false;
                    break;
                }
                wm.focused_client = client_index;
                for(uint32_t i = 0; i < wm.clients_count; i++) {
                    if(i == (uint32_t)client_index) {
                        XSetInputFocus(wm.display, wm.client_windows[i].win, RevertToPointerRoot, CurrentTime);
                        XSetWindowBorder(wm.display, wm.client_windows[i].frame, WINDOW_BORDER_COLOR_ACTIVE);
                    } else {
                        XSetWindowBorder(wm.display, wm.client_windows[i].frame, WINDOW_BORDER_COLOR);
                    }
                }
                break;
            }
        }
    }

    ragnar_terminate();
}

Window get_frame_window(Window win) {
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].win == win || wm.client_windows[i].frame == win) {
            return wm.client_windows[i].frame;
        }
    }
    return 0;
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
void handle_motion_notify(XMotionEvent e) {
    select_focused_monitor(e.x_root);

    Vec2 drag_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};
    Vec2 delta_drag = (Vec2){.x = drag_pos.x - wm.cursor_start_pos.x, .y = drag_pos.y - wm.cursor_start_pos.y};

    Client* client = &wm.client_windows[get_client_index_window(e.window)];

    // If left click
    if(e.state & Button1Mask) {
        Vec2 drag_dest = (Vec2){.x = (float)(wm.cursor_start_frame_pos.x + delta_drag.x), 
            .y = (float)(wm.cursor_start_frame_pos.y + delta_drag.y)};
        if(get_scratchpad_index_window(e.window) == -1) {
            // Unset fullscreen of the client
            if(client->fullscreen) {
                client->fullscreen = false;
                XSetWindowBorderWidth(wm.display, client->frame, WINDOW_BORDER_WIDTH);
                unhide_bar();
                unhide_client_decoration(client);
            } 
            move_client(client, drag_dest);
            // Remove the client from the layout 
            if(client->layout.in) {
                client->layout.in = false;
                client->layout.change = 0;
                establish_window_layout();
            }
            if(client->layout.skip) {
                client->layout.in = true;
                client->layout.change = 0;
                establish_window_layout();
            }
            XWindowAttributes attribs;
            XGetWindowAttributes(wm.display, client->frame, &attribs);
            client->monitor_index = get_monitor_index_by_window(drag_dest.x); 
        } else {
            XMoveWindow(wm.display, ScratchpadDefs[get_scratchpad_index_window(e.window)].frame, drag_dest.x, drag_dest.y);
        }
        // If right click
    } else if(e.state & Button3Mask) {
        Vec2 resize_delta = (Vec2){.x = MAX(delta_drag.x, -wm.cursor_start_frame_size.x),
            .y = MAX(delta_drag.y, -wm.cursor_start_frame_size.y)};
        Vec2 resize_dest = (Vec2){.x = wm.cursor_start_frame_size.x + resize_delta.x, 
            .y = wm.cursor_start_frame_size.y + resize_delta.y};
        if(get_scratchpad_index_window(e.window) == -1) {
            resize_client(client, resize_dest);
            // Remove the client from the layout
            if(client->layout.in) {
                client->layout.in = false;
                client->layout.change = 0;
                establish_window_layout();
            }
            if(client->layout.skip) {
                client->layout.change = 0;
                establish_window_layout();
            }
        } else {
            XResizeWindow(wm.display,  ScratchpadDefs[get_scratchpad_index_window(e.window)].frame, resize_dest.x, resize_dest.y);
            XResizeWindow(wm.display,  ScratchpadDefs[get_scratchpad_index_window(e.window)].win, resize_dest.x, resize_dest.y);
        }
    }
    redraw_client_decoration(client);
}

void handle_map_request(XMapRequestEvent e) {
    ragnar_window_frame(e.window);
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


void handle_unmap_notify(XUnmapEvent e) {
    if(get_client_index_window(e.window) != -1) {
        ragnar_window_unframe(e.window);
        return;
    }
    if(get_scratchpad_index_window(e.window) != -1) {
        XUnmapWindow(wm.display, ScratchpadDefs[get_scratchpad_index_window(e.window)].frame);
        ScratchpadDefs[get_scratchpad_index_window(e.window)].spawned = false;
        XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
    }
}

void handle_button_press(XButtonEvent e) {
    if(e.window == wm.root) return;
    Window frame;

    if(get_scratchpad_index_window(e.window) == -1)
        frame = get_frame_window(e.window);
    else 
        frame = ScratchpadDefs[get_scratchpad_index_window(e.window)].frame;

    wm.cursor_start_pos = (Vec2){.x = (float)e.x_root, .y = (float)e.y_root};

    Window root;
    int32_t x, y;
    unsigned width, height, border_width, depth;
    XGetGeometry(wm.display, frame, &root, &x, &y, &width, &height, &border_width, &depth);
    wm.cursor_start_frame_pos = (Vec2){.x = (float)x, .y = (float)y};
    wm.cursor_start_frame_size = (Vec2){.x = (float)width, .y = (float)height};

    XRaiseWindow(wm.display, frame);

    bool selected_client = false;
    int32_t client_index = get_client_index_window(e.window);
    if(e.button == Button2) {
        if(client_index != -1) { 
            if(wm.hard_focused_window_index == client_index) {
                XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
                wm.focused_client = -1;
                wm.hard_focused_window_index = -1;
                XSetWindowBorder(wm.display, frame, WINDOW_BORDER_COLOR);
            } else {
                XSetInputFocus(wm.display, e.window, RevertToPointerRoot, CurrentTime);
                wm.focused_client = client_index;
                selected_client = true;
                wm.hard_focused_window_index = client_index;
            }
        } else if(get_scratchpad_index_window(e.window) != -1) {
            XSetInputFocus(wm.display, ScratchpadDefs[get_scratchpad_index_window(e.window)].win, RevertToPointerRoot, CurrentTime);
            XSetWindowBorder(wm.display, frame, WINDOW_BORDER_COLOR_ACTIVE);
            return;
        }
    }

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
                XEvent msg;
                memset(&msg, 0, sizeof(msg));
                msg.xclient.type = ClientMessage;
                msg.xclient.message_type =  XInternAtom(wm.display, "WM_PROTOCOLS", false);
                msg.xclient.window = wm.client_windows[i].win;
                msg.xclient.format = 32;
                msg.xclient.data.l[0] = XInternAtom(wm.display, "WM_DELETE_WINDOW", false);
                XSendEvent(wm.display, wm.client_windows[i].win, false, 0, &msg);
                XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
                break;
            }
        }
        if(selected_client) {
            if(wm.client_windows[i].win != e.window) 
                XSetWindowBorder(wm.display, wm.client_windows[i].frame, WINDOW_BORDER_COLOR);
            else 
                XSetWindowBorder(wm.display, wm.client_windows[i].frame, ((int32_t)i == wm.hard_focused_window_index) ?  
                                 WINDOW_BORDER_COLOR_HARD_SELECTED : WINDOW_BORDER_COLOR_ACTIVE);
        }
    }   
    raise_bar();
    draw_bar_buttons();
}
void handle_button_release(XButtonEvent e) {
    (void)e;
}

void handle_key_press(XKeyEvent e) {
    int32_t client_index = get_client_index_window(e.window);
    if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_CLOSE_KEY)) {
        if(get_scratchpad_index_window(e.window) != -1) return;
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
        if(wm.layout_full) return;
        wm.client_windows[client_index].layout.in = true;
        if(wm.client_windows[client_index].fullscreen) {
            unhide_bar();
            unset_fullscreen(wm.client_windows[client_index].frame);
        }
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
        draw_bar_desktop_label();
    }  else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_UP_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        cycle_client_layout_up(&wm.client_windows[client_index]);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_CYCLE_DOWN_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        cycle_client_layout_down(&wm.client_windows[client_index]);
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_MASTER_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        uint32_t max_size = 0;
        switch (wm.current_layout) {
            case WINDOW_LAYOUT_TILED_MASTER:
                max_size = Monitors[wm.focused_monitor].width - Monitors[wm.focused_monitor].width / 6.0f;
                break;
            case WINDOW_LAYOUT_HORIZONTAL_MASTER:
                max_size = Monitors[wm.focused_monitor].height - Monitors[wm.focused_monitor].height / 6.0f - ((wm.window_gap * 2.0f) + BAR_SIZE + BAR_PADDING_Y);
                break;
            default:
                return;
        }
        if(wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] + 40 <= max_size) {
            wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] += 40;
            establish_window_layout();
        }
    } else if(e.state & MASTER_KEY && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_MASTER_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        uint32_t min_size = 0;
        switch (wm.current_layout) {
            case WINDOW_LAYOUT_TILED_MASTER:
                min_size = Monitors[wm.focused_monitor].width / 6.0f + wm.window_gap;
                break;
            case WINDOW_LAYOUT_HORIZONTAL_MASTER:
                min_size = Monitors[wm.focused_monitor].height / 6.0f + wm.window_gap + BAR_SIZE + BAR_PADDING_Y;
                break;
            default:
                return;
        }
        if((wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] - 40) >= min_size) {
            wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] -= 40;
            establish_window_layout();
        }
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_TILED_MASTER_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen || wm.layout_full) return;
        wm.current_layout = WINDOW_LAYOUT_TILED_MASTER;
        wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;

        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
                wm.client_windows[i].layout.in = true;
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                    unhide_client_decoration(&wm.client_windows[i]);
                }
                wm.client_windows[i].layout.change = 0;
            }
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_HORIZONTAL_MASTER_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen || wm.layout_full) return;

        wm.current_layout = WINDOW_LAYOUT_HORIZONTAL_MASTER;
        wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
                wm.client_windows[i].layout.in = true;
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                    unhide_client_decoration(&wm.client_windows[i]);
                }
                wm.client_windows[i].layout.change = 0;
            }
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_HORIZONTAL_STRIPES_KEY)) { 
        if(wm.client_windows[wm.focused_client].fullscreen || wm.layout_full) return;
        wm.current_layout = WINDOW_LAYOUT_HORIZONTAL_STRIPES;
        wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
                wm.client_windows[i].layout.in = true;
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                    unhide_client_decoration(&wm.client_windows[i]);
                }
                wm.client_windows[i].layout.change = 0;
            }
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_VERTICAL_STRIPES_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen || wm.layout_full) return;
        wm.current_layout = WINDOW_LAYOUT_VERTICAL_STRIPES;
        wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor) {
                wm.client_windows[i].layout.in = true;
                if(wm.client_windows[i].fullscreen) {
                    unset_fullscreen(wm.client_windows[i].frame);
                    unhide_client_decoration(&wm.client_windows[i]);
                }
                wm.client_windows[i].layout.change = 0;
            }
        }
        establish_window_layout();
    } else if(e.state & (MASTER_KEY | ShiftMask) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_FLOATING_KEY)) {
        wm.current_layout = WINDOW_LAYOUT_FLOATING;
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_GAP_INCREASE_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        if(wm.window_gap < WINDOW_MAX_GAP) {
            wm.window_gap += 4;
            establish_window_layout();
        }
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_GAP_DECREASE_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        if(wm.window_gap > 0) {
            wm.window_gap -= 4;
            if(wm.window_gap <= 0) wm.window_gap = 0;
            establish_window_layout();
        }
    }  else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_SLAVE_KEY)) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        int32_t client_count = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] &&
                wm.client_windows[i].monitor_index == wm.focused_monitor && wm.client_windows[i].layout.in) {
                client_count++;
            }
            wm.client_windows[i].ignore_enter = true;
        }
        if(client_count <= 2) return;
        int32_t index, size;
        if(wm.focused_client == (int32_t)(wm.clients_count - 1)) 
            index = wm.focused_client - 1; 
        else 
            index = wm.focused_client + 1;
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, wm.client_windows[index].frame, &attribs);
        switch(wm.current_layout) {
            case WINDOW_LAYOUT_TILED_MASTER:
            case WINDOW_LAYOUT_VERTICAL_STRIPES:
                size = attribs.height;
                if((size - 40) >= WINDOW_MIN_SIZE_LAYOUT_HORIZONTAL) {
                    wm.client_windows[wm.focused_client].layout.change += 40;
                    establish_window_layout();
                }
                break;
            case WINDOW_LAYOUT_HORIZONTAL_MASTER:
            case WINDOW_LAYOUT_HORIZONTAL_STRIPES:
                size = attribs.width;
                if((size - 40) >= WINDOW_MIN_SIZE_LAYOUT_VERTICAL) {
                    wm.client_windows[wm.focused_client].layout.change += 40;
                    establish_window_layout();
                }
                break;
            default:
                return;
        }
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_SLAVE_KEY) && wm.clients_count > 2) {
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        int32_t client_count = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] &&
                wm.client_windows[i].monitor_index == wm.focused_monitor && wm.client_windows[i].layout.in) {
                client_count++;
            }
            wm.client_windows[i].ignore_enter = true;
        }
        if(client_count <= 2) return;
        XWindowAttributes attribs;
        XGetWindowAttributes(wm.display, wm.client_windows[wm.focused_client].frame, &attribs);
        int32_t size;
        switch(wm.current_layout) {
            case WINDOW_LAYOUT_TILED_MASTER:
            case WINDOW_LAYOUT_VERTICAL_STRIPES:
                size = attribs.height;
                if((size - 40) >= WINDOW_MIN_SIZE_LAYOUT_HORIZONTAL) {
                    wm.client_windows[wm.focused_client].layout.change -= 40;
                    establish_window_layout();
                }
                break;
            case WINDOW_LAYOUT_HORIZONTAL_MASTER:
            case WINDOW_LAYOUT_HORIZONTAL_STRIPES:
                size = attribs.width;
                if((size - 40) >= WINDOW_MIN_SIZE_LAYOUT_VERTICAL) {
                    wm.client_windows[wm.focused_client].layout.change -= 40;
                    establish_window_layout();
                }
                break;
            default:
                return;
        }

    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CYCLE_UP_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] + 1;
        if(desktop >= DESKTOP_COUNT) {
            desktop = 0;
        }
        change_desktop(desktop);
        wm.focused_desktop[wm.focused_monitor] = desktop;
        establish_window_layout();
        draw_bar_desktop_label();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, DESKTOP_CYCLE_DOWN_KEY)) {
        int32_t desktop = wm.focused_desktop[wm.focused_monitor] - 1;
        if(desktop < 0) {
            desktop = DESKTOP_COUNT - 1;
        }
        change_desktop(desktop);
        wm.focused_desktop[wm.focused_monitor] = desktop;
        establish_window_layout();
        draw_bar_desktop_label();
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
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_UP_KEY) && MONITOR_COUNT > 1) {
        if(!SHOW_BAR)
            return;
        if(wm.bar_monitor >= MONITOR_COUNT - 1) {
            wm.bar_monitor = 0;
        } else {
            wm.bar_monitor++;
        }
        change_bar_monitor(wm.bar_monitor);
        establish_window_layout();
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_DOWN_KEY) && MONITOR_COUNT > 1) {
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
        if(wm.client_windows[wm.focused_client].fullscreen) return;
        uint32_t index = wm.focused_client + 1;
        Client* clients[CLIENT_WINDOW_CAP]; 
        uint32_t client_count = 0;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] &&
                wm.client_windows[i].monitor_index == wm.focused_monitor) {
                clients[client_count] = &wm.client_windows[i];
                XSetWindowBorder(wm.display, clients[client_count++]->frame, WINDOW_BORDER_COLOR);
            }
        }
        if(index >= client_count) {
            index = 0;
        }
        XSetInputFocus(wm.display, clients[index]->win, RevertToPointerRoot, CurrentTime);
        XRaiseWindow(wm.display, clients[index]->frame);
        XSetWindowBorder(wm.display, clients[index]->frame, WINDOW_BORDER_COLOR_ACTIVE);
        wm.focused_client = index;
        if(wm.hard_focused_window_index != -1) {
            wm.hard_focused_window_index = index;
        }
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
    } else if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, WINDOW_LAYOUT_SET_MASTER_KEY)) {
        if(!wm.client_windows[wm.focused_client].layout.in) return;
        int32_t first_index = -1;
        int32_t client_count = 0;
        bool found_first = false;
        for(uint32_t i = 0; i < wm.clients_count; i++) {
            if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor] &&
                wm.client_windows[i].monitor_index == wm.focused_monitor && wm.client_windows[i].layout.in) {
                if(!found_first) {
                    first_index = (int32_t)i;
                    found_first = true;
                }
                client_count++;
            }
        }
        if(first_index == -1 || client_count <= 1) return;

        Client tmp = wm.client_windows[wm.focused_client];
        wm.client_windows[wm.focused_client] = wm.client_windows[first_index];
        wm.client_windows[first_index] = tmp;
        establish_window_layout();
    }

    for(uint32_t i = 0; i < SCRATCH_PAD_COUNT; i++) {
        if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, ScratchpadDefs[i].key)) {
            if(!ScratchpadDefs[i].spawned) {
                wm.spawning_scratchpad = true;
                wm.spawned_scratchpad_index = i;
                system(ScratchpadDefs[i].cmd);
                ScratchpadDefs[i].spawned = true;
                ScratchpadDefs[i].hidden = false;
                break;
            } else {
                if(!ScratchpadDefs[i].hidden) {
                    XUnmapWindow(wm.display, ScratchpadDefs[i].frame);
                    XSetInputFocus(wm.display, wm.root, RevertToPointerRoot, CurrentTime);
                    ScratchpadDefs[i].hidden = true;
                } else {
                    XMapWindow(wm.display, ScratchpadDefs[i].frame);
                    XSetInputFocus(wm.display, ScratchpadDefs[i].win, RevertToPointerRoot, CurrentTime);
                    XRaiseWindow(wm.display, ScratchpadDefs[i].frame);
                    ScratchpadDefs[i].hidden = false;
                }
                break;
            }        
        }
    }
    for(uint32_t i = 0; i < CUSTOM_KEYBIND_COUNT; i++) {
        if(e.state & (MASTER_KEY) && e.keycode == XKeysymToKeycode(wm.display, CustomKeybinds[i].key)) {
            system(CustomKeybinds[i].cmd);
        }
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

int32_t get_scratchpad_index_window(Window win) {
    for(uint32_t i = 0; i < SCRATCH_PAD_COUNT; i++) {
        if(ScratchpadDefs[i].win == win) return i;
    }
    return -1;
}

static void grab_global_input() {     
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WM_TERMINATE_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);

    XGrabKey(wm.display,XKeysymToKeycode(wm.display, APPLICATION_LAUNCHER_OPEN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, TERMINAL_OPEN_KEY),MASTER_KEY, wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WEB_BROWSER_OPEN_KEY),MASTER_KEY, wm.root, false, GrabModeAsync,GrabModeAsync);

    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CYCLE_UP_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, DESKTOP_CYCLE_DOWN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);

    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_TILED_MASTER_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_FLOATING_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_HORIZONTAL_MASTER_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_HORIZONTAL_STRIPES_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_VERTICAL_STRIPES_KEY),MASTER_KEY | ShiftMask,wm.root,false, GrabModeAsync,GrabModeAsync);

    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_CYCLE_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_MASTER_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_MASTER_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_INCREASE_SLAVE_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_DECREASE_SLAVE_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    XGrabKey(wm.display,XKeysymToKeycode(wm.display, WINDOW_LAYOUT_SET_MASTER_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    if(SHOW_BAR) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_TOGGLE_KEY), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
        if(MONITOR_COUNT > 1) {
            XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_UP_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
            XGrabKey(wm.display,XKeysymToKeycode(wm.display, BAR_CYCLE_MONITOR_DOWN_KEY),MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
        }
    }
    if(SHOW_DECORATION) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, DECORATION_TOGGLE_KEY), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    }
    for(uint32_t i = 0; i < SCRATCH_PAD_COUNT; i++) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, ScratchpadDefs[i].key), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
    }
    for(uint32_t i = 0; i < CUSTOM_KEYBIND_COUNT; i++) {
        XGrabKey(wm.display,XKeysymToKeycode(wm.display, CustomKeybinds[i].key), MASTER_KEY,wm.root,false, GrabModeAsync,GrabModeAsync);
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
    XRaiseWindow(wm.display, wm.client_windows[client_index].frame);
    raise_bar();
    if(wm.client_windows[client_index].monitor_index == wm.bar_monitor)
        hide_bar();

    resize_client(&wm.client_windows[client_index], (Vec2){. x = Monitors[wm.focused_monitor].width, .y = Monitors[wm.focused_monitor].height});
    move_client(&wm.client_windows[client_index], (Vec2){.x = get_monitor_start_x(wm.focused_monitor), 0});

    if(hide_decoration)
        hide_client_decoration(&wm.client_windows[client_index]);
    redraw_client_decoration(&wm.client_windows[client_index]);
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
        XftTextExtents16(wm.display, client->decoration.closebutton_font.font, (FcChar16*)DECORATION_CLOSE_ICON, strlen(DECORATION_CLOSE_ICON), &extents);
        draw_str(DECORATION_CLOSE_ICON, client->decoration.closebutton_font, NULL,
                 (DECORATION_CLOSE_ICON_SIZE / 2.0f) - (extents.xOff / 2.0f), (DECORATION_TITLEBAR_SIZE / 2.0f) + (extents.height / 2.0f)); 
    }
    if(DECORATION_SHOW_MAXIMIZE_ICON) {
        x_offset += DECORATION_MAXIMIZE_ICON_SIZE;
        XClearWindow(wm.display, client->decoration.maximize_button);
        XGlyphInfo extents;
        XftTextExtents16(wm.display, client->decoration.maximize_button_font.font, (FcChar16*)DECORATION_MAXIMIZE_ICON, strlen(DECORATION_MAXIMIZE_ICON), &extents);
        draw_str(DECORATION_MAXIMIZE_ICON, client->decoration.maximize_button_font, NULL,
                 (DECORATION_MAXIMIZE_ICON_SIZE / 2.0f) - (extents.xOff / 2.0f), (DECORATION_TITLEBAR_SIZE / 2.0f) + (extents.height / 2.0f)); 
    }
    XClearWindow(wm.display, client->decoration.titlebar);
    char* window_name = NULL;
    XFetchName(wm.display, client->win, &window_name);
    if(window_name != NULL) {
        XGlyphInfo extents;
        XftTextExtents16(wm.display, client->decoration.titlebar_font.font, (FcChar16*)window_name, strlen(window_name), &extents);
        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), DECORATION_TITLE_COLOR);
        XFillRectangle(wm.display, client->decoration.titlebar, DefaultGC(wm.display, wm.screen), 0, 0, extents.xOff, DECORATION_TITLEBAR_SIZE);
        draw_str(window_name, client->decoration.titlebar_font, NULL, 0, (DECORATION_TITLEBAR_SIZE / 2.0f) + (extents.height / 2.0f));
        XFree(window_name);
        draw_design(client->decoration.titlebar, extents.xOff, DECORATION_TITLE_LABEL_DESIGN, DECORATION_TITLE_COLOR, DECORATION_DESIGN_WIDTH, DECORATION_TITLEBAR_SIZE);
    }

    XWindowAttributes attribs;
    XGetWindowAttributes(wm.display, client->frame, &attribs);
    draw_design(client->decoration.titlebar, attribs.width - x_offset, DECORATION_ICONS_LABEL_DESIGN, DECORATION_MAXIMIZE_ICON_COLOR, DECORATION_DESIGN_WIDTH, DECORATION_TITLEBAR_SIZE);
}

void establish_window_layout() {
    if(wm.current_layout == WINDOW_LAYOUT_FLOATING) return;
    // Retrieving the clients
    Client* clients[CLIENT_WINDOW_CAP]; 
    uint32_t client_count = 0;
    bool full = false;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].fullscreen) {
            unhide_bar();
            unhide_client_decoration(&wm.client_windows[i]);
        }
        if(client_count >= WINDOW_MAX_COUNT_LAYOUT) {
            full = true;
            wm.layout_full = true;
        }
        if(!full) {
            if(wm.client_windows[i].monitor_index == wm.focused_monitor && 
                wm.client_windows[i].layout.in &&
                wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
                clients[client_count] = &wm.client_windows[i];
                clients[client_count++]->layout.skip = false;
            }
            wm.layout_full = false;
        } else {
            wm.client_windows[i].layout.skip = true;
        }
    }
    if(client_count == 0) return;
    if(wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] == 0) {
        if(wm.current_layout == WINDOW_LAYOUT_TILED_MASTER)
            wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = Monitors[wm.focused_monitor].width / 2;
        else if(wm.current_layout == WINDOW_LAYOUT_HORIZONTAL_MASTER)
            wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] = Monitors[wm.focused_monitor].height / 2;
    }

    Client* master = clients[0];
    int32_t offset_bar_master = (master->monitor_index == wm.bar_monitor && SHOW_BAR && !wm.bar.hidden) ? BAR_SIZE + BAR_PADDING_Y : 0;

    if(client_count == 1) {
        resize_client(master, (Vec2){.x = Monitors[wm.focused_monitor].width - wm.window_gap * 2.3f, .y = (Monitors[wm.focused_monitor].height - wm.window_gap * 2.3f) - offset_bar_master});
        move_client(master, (Vec2){get_monitor_start_x(wm.focused_monitor) + wm.window_gap, wm.window_gap + offset_bar_master});
        master->fullscreen = false;
        XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);
        return;
    }
    if(wm.current_layout == WINDOW_LAYOUT_TILED_MASTER) {  
        // set master
        move_client(master, (Vec2){get_monitor_start_x(wm.focused_monitor) + wm.window_gap, wm.window_gap + offset_bar_master});
        resize_client(master, (Vec2){
            .x = wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] - wm.window_gap * 2.3f,
            .y = Monitors[wm.focused_monitor].height - (wm.window_gap * 2.3f) - offset_bar_master});
        master->fullscreen = false;
        XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);

        // set slaves
        int32_t last_y_offset = 0;
        for(uint32_t i = 1; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            int32_t offset_bar_size = 0;
            int32_t offset_bar_pos = 0;
            if(SHOW_BAR && !wm.bar.hidden && clients[i]->monitor_index == wm.bar_monitor) {
                if(i == client_count - 1) {
                    offset_bar_pos = BAR_SIZE + BAR_PADDING_Y;
                    if(client_count == 2) 
                        offset_bar_size = (BAR_SIZE + BAR_PADDING_Y);
                    else 
                        offset_bar_size = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                } else if(i == 1) {
                    offset_bar_pos = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                    offset_bar_size = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                } else {
                    offset_bar_pos = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                    offset_bar_size = 0;
                }
            }
            int32_t y_size =
                (int32_t)(Monitors[wm.focused_monitor].height / (client_count - 1)) 
                + clients[i]->layout.change
                - last_y_offset 
                - wm.window_gap * 2.3f
                - offset_bar_size;
            resize_client(clients[i], (Vec2){
                (Monitors[wm.focused_monitor].width 
                - wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]]) 
                - wm.window_gap * 2.3f, 

                y_size,
            });

            move_client(clients[i], (Vec2){
                get_monitor_start_x(wm.focused_monitor) 
                + wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] 
                + wm.window_gap,

                (Monitors[wm.focused_monitor].height - (int32_t)((Monitors[wm.focused_monitor].height / (client_count - 1) * i)))
                - ((i != client_count - 1) ? clients[i]->layout.change : 0)
                + wm.window_gap
                + offset_bar_pos
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
                    .y = attribs.height - clients[i]->layout.change
                });
                move_client(clients[i - 1], (Vec2){
                    .x = attribs.x,
                    .y = attribs.y + clients[i]->layout.change
                });
            }
            if(clients[i]->decoration.closebutton_font.font != NULL) {
                redraw_client_decoration(clients[i]);
            }
            last_y_offset = clients[i]->layout.change;
        }
    }   
    if(wm.current_layout == WINDOW_LAYOUT_HORIZONTAL_MASTER) {
        move_client(master, (Vec2){get_monitor_start_x(wm.focused_monitor) + wm.window_gap,
            (Monitors[wm.focused_monitor].height - wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]]) + wm.window_gap});
        resize_client(master, (Vec2){
            .x = Monitors[wm.focused_monitor].width - wm.window_gap * 2.3f,
            .y = wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]] - (wm.window_gap * 2.3f)});
        master->fullscreen = false;
        XSetWindowBorderWidth(wm.display, master->frame, WINDOW_BORDER_WIDTH);
        int32_t last_x_offset = 0;
        for(uint32_t i = 1; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            int32_t offset_bar = ((clients[i]->monitor_index == wm.bar_monitor && SHOW_BAR && !wm.bar.hidden) ? BAR_SIZE + BAR_PADDING_Y : 0);
            int32_t x_size = 
                (int32_t)(Monitors[wm.focused_monitor].width / (client_count - 1)) 
                + clients[i]->layout.change - last_x_offset - wm.window_gap * 2.3f;
            resize_client(clients[i], (Vec2){
                x_size,

                (Monitors[wm.focused_monitor].height 
                - wm.layout_master_size[wm.focused_monitor][wm.focused_desktop[wm.focused_monitor]]) 
                - wm.window_gap * 2.3f
                - offset_bar
            });

            move_client(clients[i], (Vec2){
                get_monitor_start_x(wm.focused_monitor) +
                (Monitors[wm.focused_monitor].width - (int32_t)((Monitors[wm.focused_monitor].width / (client_count - 1) * i)))
                - ((i != client_count -  1) ? clients[i]->layout.change : 0)
                + wm.window_gap,

                wm.window_gap
                + offset_bar
            });

            // The top window in the layout has to be treated diffrently 
            // for the y size offset. This is because 
            // there is no window on top of it which it can modify
            // the y size offset of. 
            if(i == client_count - 1) {
                XWindowAttributes attribs;
                XGetWindowAttributes(wm.display, clients[i - 1]->frame, &attribs);
                resize_client(clients[i - 1], (Vec2){
                    .x = attribs.width - clients[i]->layout.change,
                    .y = attribs.height 
                });
                move_client(clients[i - 1], (Vec2){
                    .x = attribs.x + clients[i]->layout.change,
                    .y = attribs.y 
                });
            }

            if(clients[i]->decoration.closebutton_font.font != NULL) {
                redraw_client_decoration(clients[i]);
            }
            last_x_offset = clients[i]->layout.change;
        }
    }
    if(wm.current_layout == WINDOW_LAYOUT_HORIZONTAL_STRIPES) {
        for(uint32_t i = 0; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            int32_t offset_bar_size = 0;
            int32_t offset_bar_pos = 0;
            if(SHOW_BAR && !wm.bar.hidden && clients[i]->monitor_index == wm.bar_monitor) {
                if(i == 0) {
                    offset_bar_pos = BAR_SIZE + BAR_PADDING_Y;
                    if(client_count == 1) 
                        offset_bar_size = (BAR_SIZE + BAR_PADDING_Y);
                    else 
                        offset_bar_size = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                } else if(i == client_count - 1) {
                    offset_bar_pos = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                    offset_bar_size = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                } else {
                    offset_bar_pos = (BAR_SIZE + BAR_PADDING_Y) / 2.0f;
                    offset_bar_size = 0;
                }
            }
            int32_t y_size =
                (int32_t)(Monitors[wm.focused_monitor].height / client_count) 
                - wm.window_gap * 2.3f
                - offset_bar_size;
            resize_client(clients[i], (Vec2){
                Monitors[wm.focused_monitor].width 
                - wm.window_gap * 2.3f, 

                y_size,
            });

            move_client(clients[i], (Vec2){
                get_monitor_start_x(wm.focused_monitor) 
                + wm.window_gap,

                (int32_t)(((Monitors[wm.focused_monitor].height / client_count) * i))
                + wm.window_gap
                + offset_bar_pos
            });
            if(clients[i]->decoration.closebutton_font.font != NULL) {
                redraw_client_decoration(clients[i]);
            }
        }
    }
    if(wm.current_layout == WINDOW_LAYOUT_VERTICAL_STRIPES) {
        for(uint32_t i = 0; i < client_count; i++) {
            if(clients[i]->monitor_index != wm.focused_monitor) continue;
            int32_t offset_bar = ((clients[i]->monitor_index == wm.bar_monitor && SHOW_BAR && !wm.bar.hidden) ? BAR_SIZE + BAR_PADDING_Y : 0);
            int32_t x_size = 
                (int32_t)(Monitors[wm.focused_monitor].width / client_count) 
                - wm.window_gap * 2.3f;
            resize_client(clients[i], (Vec2){
                x_size,

                Monitors[wm.focused_monitor].height 
                - wm.window_gap * 2.3f
                - offset_bar
            });

            move_client(clients[i], (Vec2){
                get_monitor_start_x(wm.focused_monitor) +
                (int32_t)((Monitors[wm.focused_monitor].width / client_count * i))
                + wm.window_gap,

                wm.window_gap
                + offset_bar
            });

            if(clients[i]->decoration.closebutton_font.font != NULL) {
                redraw_client_decoration(clients[i]);
            }
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
            wm.client_windows[i].layout.in &&
            wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
            if(wm.client_windows[i].win == client->win && wm.client_windows[i].frame == client->frame) {
                client_index = client_count;
            }
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    if(client_count <= 1) return;
    uint32_t new_index = client_index + 1;

    if(new_index >= client_count) {
        new_index = 0;
    }

    // swap the clients
    Client tmp = *clients[client_index];
    *clients[client_index] = *clients[new_index];
    *clients[new_index] = tmp;
    clients[client_index]->ignore_enter = true;
    establish_window_layout();
}

void cycle_client_layout_down(Client* client) {
    // Retrieving the clients in the layout
    Client* clients[CLIENT_WINDOW_CAP];
    uint32_t client_count = 0;
    uint32_t client_index = 0;
    for(uint32_t i = 0; i < wm.clients_count; i++) {
        if(wm.client_windows[i].monitor_index == wm.focused_monitor && 
            wm.client_windows[i].layout.in &&
            wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor]) {
            if(wm.client_windows[i].win == client->win && wm.client_windows[i].frame == client->frame) {
                client_index = client_count;
            }
            clients[client_count++] = &wm.client_windows[i];
        }
    }
    if(client_count <= 1) return;
    int32_t new_index = client_index - 1;

    if(new_index < 0) {
        new_index = client_count - 1;
    }
    // swap the clients
    Client tmp = *clients[client_index];
    *clients[client_index] = *clients[new_index];
    *clients[new_index] = tmp;
    clients[client_index]->ignore_enter = true;
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
                                     wm.root, get_monitor_start_x(wm.bar_monitor) + BAR_PADDING_X, BAR_PADDING_Y, 
                                     Monitors[wm.bar_monitor].width - (BAR_PADDING_X * 2.3f), BAR_SIZE, 
                                     BAR_BORDER_WIDTH,  BAR_BORDER_COLOR, BAR_COLOR);
    XSelectInput(wm.display, wm.bar.win, SubstructureRedirectMask | SubstructureNotifyMask); 
    XMapWindow(wm.display, wm.bar.win);

    XClassHint classHint;
    char* bar_name = "RagnarBar";
    classHint.res_name = bar_name;
    classHint.res_class = bar_name;
    XSetClassHint(wm.display, wm.bar.win, &classHint);
    XSetStandardProperties(wm.display, wm.bar.win, bar_name, bar_name, None, NULL, 0, NULL);

    wm.bar.font = font_create(FONT, FONT_COLOR, wm.bar.win);
    uint32_t xoffset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        
        BarButtons[i].win = XCreateSimpleWindow(wm.display, wm.root, get_monitor_start_x(wm.bar_monitor) + BarButtonLabelPos[wm.bar_monitor] + xoffset, BAR_PADDING_Y + WINDOW_BORDER_WIDTH, BAR_BUTTON_SIZE, 
                                                BAR_SIZE - WINDOW_BORDER_WIDTH, 0, 0x000000, BarButtons[i].color);
        XSelectInput(wm.display, BarButtons[i].win, ButtonPressMask); 
        XMapWindow(wm.display, BarButtons[i].win);
        XRaiseWindow(wm.display, BarButtons[i].win);
        xoffset += BAR_BUTTON_SIZE + BAR_BUTTON_PADDING;
    }

    // Allocate the colors of the bar commands
    for(uint32_t i = 0; i < BAR_COMMANDS_COUNT; i++) {
        XftColorAllocName(wm.display,DefaultVisual(wm.display,0),DefaultColormap(wm.display, 0), BarCommands[i].color, &BarCommands[i]._xcolor);
    }
    // Allocate the colors of the bar desktop icons
    for(uint32_t i = 0; i < DESKTOP_COUNT; i++) {
        XftColorAllocName(wm.display,DefaultVisual(wm.display,0),DefaultColormap(wm.display, 0), DesktopIcons[i].color, &DesktopIcons[i]._xcolor);
    }
    draw_bar_buttons();
    wm.bar.init = true;
    wm.bar.hidden = false;
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
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XMapWindow(wm.display, BarButtons[i].win);
    }
    raise_bar();
    wm.bar.hidden = false;
    draw_bar_buttons();
    draw_bar();
}


static void change_bar_monitor(int32_t monitor) {
    XMoveWindow(wm.display, wm.bar.win, get_monitor_start_x(monitor) + BAR_PADDING_X, BAR_PADDING_Y);
    XResizeWindow(wm.display, wm.bar.win, Monitors[wm.bar_monitor].width - 6 - (BAR_PADDING_X * 2.3f), BAR_SIZE);

    int32_t offset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XGlyphInfo extents;
        XftTextExtents16(wm.display, wm.bar.font.font, (FcChar16*)BarButtons[i].icon, strlen(BarButtons[i].icon), &extents);
        XMoveWindow(wm.display, BarButtons[i].win, get_monitor_start_x(monitor) + BarButtonLabelPos[wm.bar_monitor] + offset, BAR_PADDING_Y + WINDOW_BORDER_WIDTH);
        offset += BAR_BUTTON_SIZE + BAR_BUTTON_PADDING;
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
void draw_str(const char* str, FontStruct font, XftColor* color, int x, int y) {
    XftDrawStringUtf8(font.draw, color ? color : &font.color, font.font, x, y, (XftChar8 *)str, strlen(str));
}

void draw_design(Window win, int32_t xpos, BarLabelDesign design, uint32_t color, uint32_t design_width, uint32_t design_height) {
    switch (design) {
        case DESIGN_SHARP_RIGHT_UP: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos + design_width, .y = design_height}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          color);
            break;
        }
        case DESIGN_SHARP_LEFT_UP: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos - design_width, .y = design_height}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          color);
            break;
        }
        case DESIGN_SHARP_RIGHT_DOWN: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          (Vec2){.x = xpos + design_width, .y = 0}, 
                          color);
            break;
        }
        case DESIGN_SHARP_LEFT_DOWN: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          (Vec2){.x = xpos - design_width, .y = 0}, 
                          color);
            break;
        }
        case DESIGN_ARROW_LEFT: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos - design_width, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos, .y = design_height / 2.0f}, 
                          color);
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos - design_width, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          color);
            break;
        }
        case DESIGN_ARROW_RIGHT: {
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = 0}, 
                          (Vec2){.x = xpos + design_width, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos, .y = design_height / 2.0f}, 
                          color);
            draw_triangle(win, 
                          (Vec2){.x = xpos, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos + design_width, .y = design_height / 2.0f}, 
                          (Vec2){.x = xpos, .y = design_height}, 
                          color);
            break;
        }
        case DESIGN_ROUND_LEFT: {
            draw_half_circle(win, (Vec2){.x = xpos, .y = design_height / 2.0f}, design_height / 2.0f, color,true);
            break;
        }
        case DESIGN_ROUND_RIGHT: {
            draw_half_circle(win, (Vec2){.x = xpos + design_width, .y = design_height / 2.0f}, design_height / 2.0f, color,false);
            break;
        }
        case DESIGN_STRAIGHT: {
            XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), color);
            XFillRectangle(wm.display, win, DefaultGC(wm.display, wm.screen), xpos, 0, design_width, design_height);
            break;
        } 
    }   
}
void* update_ui() {
    while(true) { 
        usleep(UI_REFRESH_RATE * 1000000);
        XFlush(wm.display);
        draw_bar();
        if(SHOW_DECORATION) {
            for(uint32_t i = 0; i < wm.clients_count; i++) {
                if(wm.client_windows[i].desktop_index == wm.focused_desktop[wm.focused_monitor])
                    redraw_client_decoration(&wm.client_windows[i]);
            }
        }
    }
    return NULL;
}
void draw_bar() {
    if(!SHOW_BAR || wm.bar.hidden || !wm.bar.init) return;
    // Main Label
    XClearWindow(wm.display, wm.bar.win);
    uint32_t xoffset = 0;
    {
        for(uint32_t i = 0; i < BAR_COMMANDS_COUNT; i++) {
            // Passing the output of the given bar command to the text 
            get_cmd_output(BarCommands[i].cmd, BarCommands[i]._text);

            // Getting the  text with seperator
            char text[516] = {0};
            memcpy(text, BarCommands[i]._text, sizeof(text));
            if(strlen(BAR_COMMAND_SEPERATOR) != 0 && i != BAR_COMMANDS_COUNT - 1)
                strcat(text, " "BAR_COMMAND_SEPERATOR);

            // Insert a leading space
            int length = strlen(text);
            for (int i = length; i >= 0; i--) {
                text[i + 1] = text[i];
            }
            text[0] = ' ';

            // Getting the offset of the string (supports unicode)
            int32_t xoff = 0;
            bool is_unicode = false;
            for(uint32_t i = 0; i < strlen(text); i++) {
                XGlyphInfo extents;
                char str[2];
                str[0] = text[i];
                str[1] = '\0';
                if(str_unicode(str)) {
                    XftTextExtentsUtf16(wm.display, wm.bar.font.font, (FcChar8*)str, FcEndianBig, strlen(str), &extents);
                    is_unicode = true;
                } else {
                    XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)str, strlen(str), &extents);
                }
                xoff += extents.xOff;
            }
            // Adding the offset of the seperator to if the string is unicode
            if(is_unicode) {
                XGlyphInfo extents;
                if(str_unicode(BAR_COMMAND_SEPERATOR)) {
                    XftTextExtentsUtf16(wm.display, wm.bar.font.font, (FcChar8*)BAR_COMMAND_SEPERATOR, FcEndianBig, strlen(BAR_COMMAND_SEPERATOR), &extents);
                } else {
                    XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BAR_COMMAND_SEPERATOR, strlen(BAR_COMMAND_SEPERATOR), &extents);
                }
                xoff += extents.xOff;
            }
            // Drawing the background of the command
            XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), BarCommands[i].bg_color == -1 ? BAR_MAIN_LABEL_COLOR : BarCommands[i].bg_color);
            XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), xoffset, 0, xoff, BAR_SIZE);
            
            // Drawing the output of the command
            draw_str(text, wm.bar.font, &BarCommands[i]._xcolor, xoffset, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));
            xoffset += xoff;
        }
        draw_design(wm.bar.win, xoffset, BAR_MAIN_LABEL_DESIGN, BAR_MAIN_LABEL_COLOR, BAR_LABEL_DESIGN_WIDTH, BAR_SIZE);
        xoffset += BAR_LABEL_DESIGN_WIDTH;
    }


    // Info Label
    wm.bar.DESKTOP_LABEL_x = xoffset;
    xoffset = draw_bar_desktop_label();

    // Version label
    if(BAR_SHOW_VERSION_LABEL)
    {
        const char* text = "RagnarWM v"VERSION;
        uint32_t monitor_width = Monitors[wm.bar_monitor].width - (BAR_PADDING_X * 2.3f);
        XGlyphInfo extents;
        XftTextExtents16(wm.display, wm.bar.font.font, (FcChar16*)text, strlen(text), &extents);
        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), BAR_VERSION_LABEL_COLOR);
        XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), 
                       monitor_width - extents.xOff - BAR_LABEL_DESIGN_WIDTH, 0, extents.xOff + BAR_LABEL_DESIGN_WIDTH, BAR_SIZE);

        draw_str(text, wm.bar.font, NULL, monitor_width - extents.xOff - BAR_LABEL_DESIGN_WIDTH, (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));

        draw_design(wm.bar.win, monitor_width - extents.xOff - BAR_LABEL_DESIGN_WIDTH, BAR_VERSION_LABEL_DESIGN, BAR_DESKTOP_LABEL_COLOR, BAR_LABEL_DESIGN_WIDTH, BAR_SIZE);
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

int32_t draw_bar_desktop_label() {
    if(!BAR_SHOW_DESKTOP_LABEL) return 0;

    uint32_t xoffset = BarInfoLabelPos[wm.bar_monitor];

    XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), BAR_COLOR);
    XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), xoffset, 0, BAR_DESKTOP_LABEL_ICON_SIZE * DESKTOP_COUNT, BAR_SIZE);

    draw_design(wm.bar.win, xoffset, BAR_DESKTOP_LABEL_DESIGN_FRONT, BAR_DESKTOP_LABEL_COLOR, BAR_LABEL_DESIGN_WIDTH, BAR_SIZE);

    for(uint32_t i = 0; i < DESKTOP_COUNT; i++) {
        XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), ((int32_t)i == wm.focused_desktop[wm.focused_monitor]) ? BAR_DESKTOP_LABEL_SELECTED_COLOR : BAR_DESKTOP_LABEL_COLOR);
        XFillRectangle(wm.display, wm.bar.win, DefaultGC(wm.display, wm.screen), xoffset, 0, BAR_DESKTOP_LABEL_ICON_SIZE, BAR_SIZE);

        XGlyphInfo extents;
        if(str_unicode(DesktopIcons[i].icon)) {
            XftTextExtentsUtf16(wm.display, wm.bar.font.font, (FcChar8*)DesktopIcons[i].icon, FcEndianBig, strlen(DesktopIcons[i].icon), &extents);
        } else {
            XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)DesktopIcons[i].icon, strlen(DesktopIcons[i].icon), &extents);
        }
        draw_str(DesktopIcons[i].icon, wm.bar.font, &DesktopIcons[i]._xcolor, xoffset + ((BAR_DESKTOP_LABEL_ICON_SIZE / 2.0f) - (
                 ((str_unicode(DesktopIcons[i].icon) ? extents.width : extents.width / 2.0f)))), (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f));
        xoffset += BAR_DESKTOP_LABEL_ICON_SIZE;
    }
    draw_design(wm.bar.win, xoffset, BAR_DESKTOP_LABEL_DESIGN_BACK, BAR_DESKTOP_LABEL_COLOR, BAR_LABEL_DESIGN_WIDTH, BAR_SIZE);
    return xoffset;
}


static bool str_unicode(const char* str) {
    while (*str != '\0') {
        if ((unsigned char)(*str) > 127) {
            return true;
        }
        str++;
    }
    return false;
}
void draw_bar_buttons() {
    if(!SHOW_BAR || wm.bar.hidden) return;
    uint32_t xoffset = 0;
    for(uint32_t i = 0; i < BAR_BUTTON_COUNT; i++) {
        XRaiseWindow(wm.display, BarButtons[i].win);
        XClearWindow(wm.display, BarButtons[i].win);
        XGlyphInfo extents;
        if(str_unicode(BarButtons[i].icon)) {
            XftTextExtentsUtf16(wm.display, wm.bar.font.font, (FcChar8*)BarButtons[i].icon, FcEndianBig, strlen(BarButtons[i].icon), &extents);
        } else {
            XftTextExtents8(wm.display, wm.bar.font.font, (FcChar8*)BarButtons[i].icon, strlen(BarButtons[i].icon), &extents);
        }

        BarButtons[i].font = font_create(FONT, FONT_COLOR, BarButtons[i].win);
        draw_text_icon_color(BarButtons[i].icon, "", (Vec2){(BAR_BUTTON_SIZE / 2.0f) - ((str_unicode(BarButtons[i].icon)) ? (extents.width) : (extents.width / 2.0f)), (BAR_SIZE / 2.0f) + (FONT_SIZE / 2.0f)}, BarButtons[i].font, BarButtons[i].color, 0, BarButtons[i].win, BAR_SIZE);
        xoffset += BAR_BUTTON_SIZE + BAR_BUTTON_PADDING;
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
    XftTextExtents16(wm.display, font.font, (FcChar16*)text, strlen(text), &extents);
    XGlyphInfo extents_icon;
    XftTextExtents16(wm.display, font.font, (FcChar16*)icon, strlen(icon), &extents_icon);

    XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), color);
    XFillRectangle(wm.display, win, DefaultGC(wm.display, wm.screen), pos.x, 0, extents.xOff + extents_icon.xOff + rect_x_add, height);

    draw_str(icon, font, NULL, pos.x, pos.y);
    draw_str(text, font, NULL, pos.x + extents_icon.xOff, pos.y);

    return extents.xOff + extents_icon.xOff + rect_x_add;
}

void draw_half_circle(Window win, Vec2 pos, int32_t radius, uint32_t color, bool to_left) {
    XSetForeground(wm.display, DefaultGC(wm.display, wm.screen), color);

    if(to_left) {
        XFillArc(wm.display, win,DefaultGC(wm.display, wm.screen), pos.x - radius, pos.y - radius, radius * 2, radius * 2, 90 * 64, 180 * 64);
    } else {
        XRectangle rect;
        rect.x = pos.x + radius;
        rect.y = pos.y;
        rect.width = radius;
        rect.height = radius * 2;
        Region clipRegion = XCreateRegion();
        XUnionRectWithRegion(&rect, clipRegion, clipRegion);
        XFillArc(wm.display, win,DefaultGC(wm.display, wm.screen), pos.x - BAR_LABEL_DESIGN_WIDTH - radius, pos.y - radius, radius * 2, radius * 2, 90  * 64, -180 * 64);
    }
}

int main(void) {
    ragnar_init();
    pthread_t ui_thread;
    pthread_create(&ui_thread, NULL, update_ui, NULL);

    ragnar_run();
    ragnar_terminate();
}
