#include "render.h"
#include "structs.h"
#include "config.h"

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <leif/leif.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void 
render_initgl(State* s) {
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


void 
render_setvsync(Window win, bool vsync, State* s) {
  // Check for GLX_EXT_swap_control extension
  const char *extensions = glXQueryExtensionsString(s->dsp, DefaultScreen(s->dsp));
  if (strstr(extensions, "GLX_EXT_swap_control") == NULL) {
    fprintf(stderr, "ragnar_render: GLX_EXT_swap_control extension is not available.\n");
    exit(1);
  }

  // Set swap interval to 0
  typedef void (*PFNGLXSWAPINTERVALEXT)(Display *, GLXDrawable, int);
  PFNGLXSWAPINTERVALEXT glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXT) glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");

  if (glXSwapIntervalEXT) {
    glXSwapIntervalEXT(s->dsp, win, (int32_t)vsync);
  } else {
    fprintf(stderr, "ragnar_render: failed to get address for glXSwapIntervalEXT.\n");
    exit(1);
  }
}
void 
render_setcontext(Window win, State* s) {
  if(!glXMakeCurrent(s->dsp, win, s->glcontext)) {
    fprintf(stderr, "ragnar_render: cannot make OpenGL context.\n");
    terminate(EXIT_FAILURE);
  }
}
void
render_clientdecoration(client* cl, State* s) {
  render_setcontext(cl->deco.win, s);
  {
    LfColor clearcolor = lf_color_from_hex(decorationcolor);
    vec4s zto = lf_color_to_zto(clearcolor); 
    glClearColor(zto.r, zto.g, zto.b, zto.a);
  }
  glClear(GL_COLOR_BUFFER_BIT);
  lf_begin(&cl->deco.lf);
  lf_text(&cl->deco.lf, "Hello");
  lf_end(&cl->deco.lf);
  glXSwapBuffers(s->dsp, cl->deco.win);
}
