#pragma once

/*
 * This header contains functions that are usable as callbacks 
 * for keybinds in the configuration file.
 * */ 

// Terminates the window manager with a successfull exit code
void terminate_success();
// Cycles the currently focused client 
void cyclefocus();
// Kills the currently focused client
void killfocus();
// Raises the currently focused client to the top of the window stack
void raisefocus();
// Toggles fullscreen mode on the currently focused client
void togglefullscreen();


// Cycles the desktop on the currently focused desktop one up 
void cycledesktopup(); 
// Cycles the desktop on the currently focused desktop one down 
void cycledesktopdown();
// Cycles the desktop of the currently focused client up
void cyclefocusdesktopup();
// Cycles the desktop of the currently focused client down
void cyclefocusdesktopdown();

// Switches to a given desktop (passthrough_data){.i = *desktop*}
void switchdesktop(passthrough_data data); 
// Switches the desktop of the currently focused client to a given desktop 
// (passthrough_data){.i = *desktop*}
void switchfocusdesktop(passthrough_data data);
// Runs a given command (passthrough_data){.cmd = *cmd_to_run*}
void runcmd(passthrough_data data);
