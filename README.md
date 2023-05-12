# RagnarWM

<img src="https://github.com/cococry/Ragnar/blob/main/branding/workflow.png" width="800"/>

## Overview

Ragnar is a minimal window manager for X written in C.
The goal of this project is to create a window manager that has minimal bloat but
is still accesable for non-technical people. 

## Features

- [x] Tiling layout
- [x] Multi-monitor support
- [x] Altering layout (Application switching, changing master size...)
- [x] Fullscreening windows
- [x] Multiple Desktops
- [x] Status Bar
- [ ] Auto Start Commands

## Running it

If you want to run Ragnar on your machine, 
clone this repo and type 
```console
sudo make clean install
```

Then, in your .xinitrc add:
```
exec ragnar
```

### IMPORTANT

In the [config.h](https://github.com/cococry/Ragnar/blob/main/config.h) file specify your monitor
setup in order for the WM to work as intended. Also, for configuration of the WM use the config.h file.

## Notes

For the best experience, i suggest to use a X compositer like picom. I am using [picom pijulius](https://github.com/pijulius/picom)
because i think it has very clean animation support. You can find my picom config [here](https://github.com/cococry/dotfiles/blob/main/picom/picom.conf)

This project is in early stage and not finished. So if you find bugs or have any problems, feel free to [submit an issue](https://github.com/cococry/Ragnar/issues). 
Especially multi monitor setups can be problematic with the current state of the code base.

## Usage

### Default Keybindings

| Keybind         |  Action     |
| ----------------|-------------|
| SUPER + Left Mouse/Middle Mouse | Select Window 
| SUPER + Enter | Open terminal |
| SUPER + W | Open Web-browser |
| SUPER + S | Open Application Launcher |
| SUPER + Q | Quit Application |
| SUPER + C | Quit WM |
| SUPER + Space | Add window to layout |
| SUPER + Up Arrow | Move layout up in window |
| SUPER + Down Arrow | Move layout down in window |
| SUPER + L | Increase master size in layout |
| SUPER + H | Decrease master size in layout |
| SUPER + J | Increase slave size in layout |
| SUPER + K | Decrease slave size in layout |
| SUPER + Plus | Increase gap size of windows |
| SUPER + Minus | Decrease gap size of windows |
| SUPER + F | Fullscreen selected window |
| SUPER + Shift + T | Set tiled master layout |
| SUPER + Shift + R | Set floating layout |
| SUPER + A | Cycle desktop down |
| SUPER + D | Cycle desktop up |
| SUPER + O | Cycle window one desktop down |
| SUPER + P | Cycle window one desktop down |
| SUPER + B | Cycle bar one monitor down |
| SUPER + N | Cycle bar one monitor up |
| SUPER + I | Toggle bar visibility |

