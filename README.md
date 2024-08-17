<img align="left" style="width:128px" src="https://github.com/cococry/ragnar/blob/main/branding/logo.png" width="128px">

**Ragnar is a feature-rich, straight-to-the-point dynamic window manager for X**

The goal of ragnarwm is to create a window manager that can be used as a solid foundation
for fully fledged desktop environments but also serve as a minimal daily driver. Ragnar 
contains about 5k lines of code which contain features like an IPC API, configuration file, 
tiling layouts, EWMH & ICCM implementation and much more key features.

---

# Installation

Installing ragnarwm is a process that involes two main steps. 

- Install build depdencies
```console
xcb-util, xcb-proto, xcb-util-keysyms, xcb-util-cursor, xcb-util-wm, xorg-server, xorg-xinit, mesa, libconfig
```

- Build the window manager
```console
$ ./install.sh
```
