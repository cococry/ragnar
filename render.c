#include "render.h"
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>


XVisualInfo* 
getvisualinfo(Display* dsp) {
  static int visattribs[] = {
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_DOUBLEBUFFER, True,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8,
    GLX_DEPTH_SIZE, 24,
    None
  };

  int fbcount;
  GLXFBConfig* fbc = glXChooseFBConfig(dsp, DefaultScreen(dsp), visattribs, &fbcount);
  if (!fbc) {
    fprintf(stderr, "ragnar: failed to retrieve a framebuffer config.\n");
    return NULL;
  }


  XVisualInfo* vis = glXGetVisualFromFBConfig(dsp, fbc[0]);
  if (!vis) {
    fprintf(stderr, "ragnar: no appropriate visual found.\n");
    XFree(fbc);
    return NULL;
  }
  XFree(fbc);
  return vis;
}

GLXContext 
initgl(Display* dsp) {
  XVisualInfo* vis = getvisualinfo(dsp);

  GLXContext context = glXCreateContext(dsp, vis, NULL, GL_TRUE);
  if (!context) {
    fprintf(stderr, "ragnar: failed to create OpenGL context\n");
    XFree(vis);
    exit(EXIT_FAILURE);
    return (GLXContext){0};
  }

  XFree(vis);
  return context;
}
