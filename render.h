#pragma once

#include <xcb/glx.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <X11/Xlib.h>

/**
 * @brief Retrieves the visual info of a given X display
 * @param dsp The X display to get the visual info from
 * @return the retrieved visual info (NULL on error)
 */
XVisualInfo* getvisualinfo(Display* dsp);

/**
 * @brief Initializes an OpenGL context for the X server 
 * @param dsp The X display to initialize the GL context for 
 * @return The created GL context ((GLContext){0} on error)
 */
GLXContext initgl(Display* dsp);
