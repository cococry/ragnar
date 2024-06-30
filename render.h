#pragma once

#include "structs.h"

// Initializes OpenGL context
void render_initgl(State* s);
// Sets the swap interval (vsync) of a given window
void render_setvsync(Window win, bool vsync, State* s);
// Sets the OpenGL context to a given window
void render_setcontext(Window win, State* s);
// Renders the decoration of a given client with OpenGL
void render_clientdecoration(client* cl, State* s);
