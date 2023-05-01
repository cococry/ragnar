# XRagnar

<img src="https://github.com/cococry/Xragnar/blob/main/branding/workflow.png" width="800"/>

## Overview

XRagnar is a minimal window manager for X written in C.
The goal of this project is to create a window manager that has minimal bloat but
is still accesable for non-technical people. 

## Features

- [x] Tiling layout
- [x] Multi-monitor support
- [x] Altering layout (Application switching, changing master size...)
- [x] Fullscreening windows
- [x] Multiple Desktops
- [ ] Status Bar

## Running it

If you want to run Xragnar on your machine, 
clone this repo and type 
```console
sudo make clean install
```

Then, i your .xinitrc add:
```
exec xragnar
```

### IMPORTANT

In the [config.h](https://github.com/cococry/Xragnar/blob/main/config.h) file specify your monitor
setup in order for the WM to work as intended. Also, for configuration of the WM use the config.h file.

## Notes

For the best experience, i suggest to use a X compositer like picom. I am using [picom pijulius](https://github.com/pijulius/picom)
because i think it has very clean animation support. 

This project is in early stage and not finished. So if you find bugs or have any problems, feel free to [submit an issue](https://github.com/cococry/Xragnar/issues). 
Especially multi monitor setups can be problematic at with current state of the code base.

## Usage

### Default Keybindings

| Keybind         |  Action     |
| ----------------|-------------|
| ALT + Left Mouse/Middle Mouse | Select Window |
| ALT + Enter | Open terminal |
| ALT + W | Open Web-browser |
| ALT + Q | Quit Application |
| ALT + C | Quit WM |
| ALT + Space | Add window to layout |
| ALT + Up Arrow | Move layout up in window |
| ALT + Down Arrow | Move layout down in window |
| ALT + L | Increase master size in layout |
| ALT + H | Decrease master size in layout |
| ALT + J | Increase slave size in layout |
| ALT + K | Decrease slave size in layout |
| ALT + Plus | Increase gap size of windows |
| ALT + Minus | Decrease gap size of windows |
| ALT + F | Fullscreen selected window |
| ALT + Shift + T | Set tiled master layout |
| ALT + Shift + R | Set floating layout |
| ALT + A | Cycle desktop down |
| ALT + D | Cycle desktop up |
| ALT + O | Cycle window one desktop down |
| ALT + P | Cycle window one desktop down |
