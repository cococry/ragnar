<p align="left">
  <img src="https://github.com/cococry/ragnar/blob/main/branding/logo.png" width="128px" alt="Ragnar logo">
</p>

# Ragnar

**Ragnar is a feature-rich, straight-to-the-point dynamic window manager for X11.**

Ragnar is designed to work as both a foundation for full desktop environments and as a minimal daily-driver window manager. The project stays compact at roughly 5,000 lines of code while still providing core window manager features such as:

- IPC support
- External configuration
- Tiling layouts
- EWMH and ICCCM support
- Reparenting-based window decoration
- Runtime configuration reloads

Since version 2.0, Ragnar uses the **XCB API** to communicate with the X server. XCB provides a lower-level alternative to Xlib and gives Ragnar more direct control over X11 interactions.

<p align="left">
  <img src="https://github.com/cococry/ragnar/blob/main/branding/screenshot.png" width="500px" alt="Ragnar screenshot">
</p>

---

## Installation

Installing Ragnar involves two steps.

### 1. Install build dependencies

Install the following dependencies:

```console
libxcb \
libx11 \
xcb-util-keysyms \
xcb-util-cursor \
xcb-util-wm \
xorg-server \
xorg-xinit \
libconfig
```

### 2. Install Ragnar

Run the installation script from the project root:

```console
./install.sh
```

---

## Technical Overview

### Window Management

Ragnar uses **reparenting** to manage client windows. This allows the window manager to create and control decorations around client windows.

Client windows are stored in a doubly linked list. For tiling, Ragnar does not use a tree-based layout system. Instead, it provides preset layouts with predictable structure. This keeps window placement simple and avoids requiring the user to manually decide where each new window should appear.

Layouts can still be adjusted at runtime by changing the size of master and slave areas through keybindings.

### IPC

Ragnar includes an abstracted **IPC API** for plugin development and external control.

The IPC system communicates through a socket using binary data. This allows external programs to interact with the window manager in a structured way.

### Configuration

Ragnar uses `libconfig` to load an external configuration file:

```console
# by default, system-wide:
/etc/ragnarwm/ragnar.cfg
# or mkdir -p and cp the default per user.
~/.config/ragnarwm/ragnar.cfg
```

The configuration is loaded on startup and can be reloaded while the window manager is running, typically through a keybinding.

You can add to `.xinitrc`:

`exec path/to/ragnar` Then simply; `startx`

> By default it uses `alacritty` (Super+Return) and `dmenu` (Super+S), if you haven't edited these yet.
> Both of these need fonts file; for instance `ttf-dejavu`

You can also for example add: `polybar &` before the `exec ragnar` line.

---

## Code Structure

### `funcs.h`

Contains documented function declarations used throughout the window manager.

This file does not contain callback functions triggered by keybindings.

### `keycallbacks.h`

Contains documented definitions of callback functions that can be assigned to keybindings in the configuration file.

### `ragnar.c`

The main translation unit of the window manager.

It contains the core implementation, including:

- The application loop
- Event handling
- Window management logic
- Layout calculations
- System-level window manager behavior

### `structs.h`

Contains shared structures and function declarations used across the codebase.

### `config.h` / `config.c`

Handles loading and parsing the Ragnar configuration file through `libconfig`.

### `ipc/socket.h` / `ipc/socket.c`

Implements socket handling for IPC clients.
