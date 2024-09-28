<img align="left" style="width:128px" src="https://github.com/cococry/ragnar/blob/main/branding/logo.png" width="128px">

**Ragnar is a feature-rich, straight-to-the-point dynamic window manager for X**

The goal of ragnarwm is to create a window manager that can be used as a solid foundation
for fully fledged desktop environments but also serve as a minimal daily driver. Ragnar 
contains about 5k lines of code which contain features like an IPC API, configuration file, 
tiling layouts, EWMH & ICCM implementation and much more key features.

Since Version 2.0, the window manager uses the **XCB API** for communicating with the X server,
which is far less bloated compared to the Xlib API.

<img src="https://github.com/cococry/ragnar/blob/main/branding/screenshot.png" width="500px">
---

# Installation

Installing ragnarwm is a process that involes two main steps. 

- Install build depdencies
```console
xcb-util, xcb-proto, xcb-util-keysyms, xcb-util-cursor, xcb-util-wm, xorg-server, xorg-xinit, mesa, libconfig
```

- Install the window manager
```console
$ ./install.sh
```

# Technical

### General
Ragnar uses **reparenting** to handle client windows which gives it the ability to easily create decoration
for client windows. 
Client windows are stored within a doubly linked list. 
As for the tiling fuctionality, **ragnar does not use some kind of tree datastructure**, instead, 
there are **preset layouts** which always resemble the same shape. This choice was made to keep the experience
smooth without thinking about where to place the spawning window. Layouts are still very customizable by changing the sizes of master and slave windows with a simple keybind. 

### IPC
The source code also contains an **abstracted IPC API** for creating plugins. The API works through
a **socket with binary data**.

### Config file
Ragnar uses libconfig to read an external configuration file for the window manager (/home/user/.config/ragnarwm.cfg).
This configuration is read on startup and can be reloaded while the WM is running (Typically through a keybind).

### Code Structure

- **funcs.h**
This file contains the documented function declarations that the window manager uses. Allthough,
it does **not** contain the callback functions that are fired via keybindings.

- **keycallbacks.h**
This file contains the documented function **definitions** of all callback functions that can be specified
in the configuration file.

- **ragnar.c**
This is the main translation unit that contains the definitions of all systematic window manager functions,
containg the application loop, event handling, calculating tiling layouts and more.

- **structs.h**
This file contains structures and function declaration that are used throughout the window manager's code.

- **config.h/config.c**
Those files handle loading the configuration file with libconfig.

- **ipc/**
The files socket.h and socket.c handle socket connections from clients via IPC.
