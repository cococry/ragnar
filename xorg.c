#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

int main() {
    Display *display;
    Window window;
    XEvent event;
    int screen;
    Colormap colormap;
    XColor color;
    unsigned long gray_pixel;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        return 1; // Unable to open display
    }

    screen = DefaultScreen(display);
    colormap = DefaultColormap(display, screen);

    // Define the gray color
    if (!XParseColor(display, colormap, "gray", &color)) {
        return 1; // Unable to parse color
    }

    if (!XAllocColor(display, colormap, &color)) {
        return 1; // Unable to allocate color
    }

    gray_pixel = color.pixel;

    // Create the window
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 800, 600, 1,
                                 BlackPixel(display, screen), gray_pixel);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    // Main event loop
    while (1) {
        XNextEvent(display, &event);
        if (event.type == Expose) {
            // Handle expose events
        } else if (event.type == KeyPress) {
            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}

