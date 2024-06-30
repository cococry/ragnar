#include "render.h"
#include "structs.h"
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

void render_initgl(State* s) {
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

  int32_t fbcount;
  GLXFBConfig* fbconfs = glXChooseFBConfig(s->dsp, DefaultScreen(s->dsp), visattribs, &fbcount);
  if(!fbconfs) {
    fprintf(stderr, "ragnar_render: cannot retrieve an OpenGL framebuffer config.\n");
    terminate(EXIT_FAILURE);
  }

  GLXFBConfig fbconf = fbconfs[0];
  XFree(fbconfs);

  s->glvisual = glXGetVisualFromFBConfig(s->dsp, fbconf);
  if(!s->glvisual) {
    fprintf(stderr, "ragnar_render: no appropriate OpenGL visual found.\n" );
    terminate(EXIT_FAILURE);
  }

  s->glcontext = glXCreateNewContext(s->dsp, fbconf, GLX_RGBA_TYPE, NULL, True);
  if(!s->glcontext) {
    fprintf(stderr, "ragnar_render: cannot create an OpenGL context.\n" );
    terminate(EXIT_FAILURE);
  }
}
