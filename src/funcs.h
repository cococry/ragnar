#pragma once 

#include "structs.h"
#include <stdarg.h>
#include <stdbool.h>

/* === FUNCTION DECLARATIONS === */

/**
 * @brief Sets up the WM state and the X server 
 *
 * @param s The window manager's state
 * This function establishes a connection to the X server,
 * sets up the root window and window manager keybindings.
 * The event mask of the root window is being cofigured to
 * listen to necessary events. 
 * After the configuration of the root window, all the specified
 * keybinds in config.h are grabbed by the window manager.
 */
void             setup(state_t* s);

/**
 * @brief Event loop of the window manager 
 *
 * This function polls for X server events and 
 * handles them accoringly by calling the associated event handler.
 */
void             loop(state_t* s);

/**
 * @brief Terminates the window manager 
 *
 * This function terminates the window manager by
 * giving up the connection to the X server and
 * exiting the program.
 */
void             terminate(state_t* s, int32_t exitcode);

/**
 * @brief Manages all windows that are avaiable on the 
 * X display.
 *
 * @param s The window manager' state
 */
void             managewins(state_t* s);

/**
 * @brief Creates a client from a given X windwo 
 *
 * @param s The window manager' state
 * @param win The window from which to create a client 
 */
client_t*       makeclient(state_t* s, xcb_window_t win);

/**
 * @brief Evaluates if a given point is inside a given area 
 *
 * @param p The point to check if it is inside the given area
 * @param area The area to check
 *
 * @return True if the point p is in the area, false if it is not in the area 
 */
bool             pointinarea(v2_t p, area_t a);

/**
 * @brief Returns the current cursor position on the X display 
 *
 * @param s The window manager's state
 * @param success Gets assigned whether or not the pointer query
 * was successfull.
 *
 * @return The cursor position as a two dimensional vector 
 */
v2_t             cursorpos(state_t* s, bool* success);

/**
 * @brief Returns the area (position and size) of a given window 
 *
 * @param s The window manager's state
 * @param win The window to get the area from 
 * @param success Gets assigned whether or not the geometry query
 * was successfull.
 *
 * @return The area of the given window 
 */

area_t           winarea(state_t* s, xcb_window_t win, bool* success);

/**
 * @brief Creates a X window with a trucolor visual 
 * and colormap.
 *
 * @param s The window manager's state
 * @param a The geometry of the window 
 * @param bw The border width of the window 
 *
 * @return The ID of the created X window 
 */
xcb_window_t 	   truecolorwindow(state_t* s, area_t a, uint32_t bw);

/**
 * @brief Gets the area (in float) of the overlap between two areas 
 *
 * @param a The inner area 
 * @param a The outer area 
 *
 * @return The overlap between the two areas 
 */
float            getoverlaparea(area_t a, area_t b);

/**
 * @brief Sets the border color of a given clients window 
 *
 * @param s The window manager's state
 * @param cl The client to set the border color of 
 * @param color The border color 
 */
void             setbordercolor(state_t* s, client_t* cl, uint32_t color);

/**
 * @brief Sets the border width of a given clients window 
 * and updates its 'borderwidth' variable 
 *
 * @param s The window manager's state
 * @param cl The client to set the border width of 
 * @param width The border width 
 */
void             setborderwidth(state_t* s, client_t* cl, uint32_t width);


/**
 * @brief Removes a given client from the linked list 
 * of clients of a given monitor (not deallocating the client)
 *
 * @param mon The monitor to remove the client from 
 * @param cl The client to remove 
 */
void              monremoveclient(monitor_t *mon, client_t *cl);

/**
 * @brief Adds a given client to the front of the linked list 
 * of clients of a given monitor 
 *
 * @param mon The monitor to add the client to 
 * @param cl The client to add 
 */
void            monaddclient(monitor_t *mon, client_t *cl);

/**
 * @brief Moves the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to move 
 * @param pos The position to move the client to 
 */
void             moveclient(state_t* s, client_t* cl, v2_t pos, bool manage_mons);

/**
 * @brief Resizes the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to resize 
 * @param pos The new size of the clients window 
 */
void             resizeclient(state_t* s, client_t* cl, v2_t size);

/**
 * @brief Moves and resizes the window of a given client and updates its area.
 *
 * @param s The window manager's state
 * @param cl The client to move and resize
 * @param a The new area (position and size) for the client's window
 */
void             moveresizeclient(state_t* s, client_t* cl, area_t a);

/**
 * @brief Raises the window of a given client to the top of the stack
 *
 * @param s The window manager's state
 * @param cl The client to raise 
 */
void             raiseclient(state_t* s, client_t* cl);

/**
 * @brief Checks if a client has a WM_DELETE atom set 
 *
 * @param s The window manager's state
 * @param cl The client to check delete atom for
 *
 * @return Whether or not the given client has a WM_DELETE atom 
 */
bool             clienthasdeleteatom(state_t* s, client_t* cl);

/**
 * @brief Returns wether or not a client should be added 
 * to the tiling layout based on window properties:
 * (_NET_WM_WINDOW_TYPE, NET_WM_WINDOW_TYPE_NORMAL)
 *
 * @param s The window manager's state
 * @param cl The client to check if it should tile for 
 *
 * @return Whether or not a given client should tile 
 */
bool             clientshouldtile(state_t* s, client_t* cl);

/**
 * @brief Checks if a client is on a given monitor and if it is 
 * currently visible (on the current desktop) 
 *
 * @param cl The client to check visibility for 
 *
 * @return Whether or not the given client is visible and on the given monitor 
 */
bool             clientonscreen(state_t* s, client_t* cl, monitor_t* mon);

/**
 * @brief Retrieves the name of a given client (allocates memory) 
 *
 * @param s The window manager's state
 * @param cl The client to retrieve a name from  
 *
 * @return The name of the given client  
 */
char*            getclientname(state_t* s, client_t* cl);

/**
 * @brief Kills a given client by destroying the associated window and 
 * removing it from the linked list.
 *
 * @param s The window manager's state
 * @param cl The client to kill 
 */
void             killclient(state_t* s, client_t* cl);

/**
 * @brief Sets the current focus to a given client
 *
 * @param s The window manager's state
 * @param cl The client to focus
 */
void             focusclient(state_t* s, client_t* cl, bool upload_ewmh_desktops);

/**
 * @brief Sets the X input focus to a given client and handles EWMH atoms & 
 * focus event.
 *
 * @param s The window manager's state
 * @param cl The client to set the input focus to
 */
void             setxfocus(state_t* s, client_t* cl);

/**
 * @brief Creates a frame window for the content of a client window and decoration to live in.
 * The frame window encapsulates the client's window and decoration like titlebar.
 *
 * @param s The window manager's state
 * @param cl The client to create a frame window for 
 */
void             frameclient(state_t* s, client_t* cl);

/**
 * @brief Destroys and unmaps the frame window of a given client 
 * which consequently removes the client's window from the display
 *
 * @param s The window manager's state
 * @param cl The client to unframe 
 */
void             unframeclient(state_t* s, client_t* cl);

/**
 * @brief Removes the focus from client's window by setting the 
 * X input focus to the root window and unsetting the highlight color 
 * of the window's border.
 *
 * @param s The window manager's state
 * @param cl The client to unfocus
 */
void             unfocusclient(state_t* s, client_t* cl);

void             removescratchpad(state_t* s, uint32_t idx);

/**
 * @brief Notifies a given client window about it's configuration 
 * (geometry) by sending a configure notify event to it.
 *
 * @param s The window manager's state
 * @param cl The client to send a configure notify event to 
 */
void             configclient(state_t* s, client_t* cl);

/*
 * @brief Hides a given client by unframing it's window
 *
 * @param s The window manager's state
 * @param cl The client to hide 
 */
void             hideclient(state_t* s, client_t* cl);

/*
 * @brief Shows a given client by unframing it's window
 *
 * @param s The window manager's state
 * @param cl The client to show 
 */
void             showclient(state_t* s, client_t* cl);

/**
 * @brief Sends a given X event by atom to the window of a given client
 *
 * @param s The window manager's state
 * @param cl The client to send the event to
 * @param protocol The atom to store the event
 */
bool             raiseevent(state_t* s, client_t* cl, xcb_atom_t protocol);

/**
 * @brief Checks for window type of clients by EWMH and acts 
 * accoringly.
 *
 * @param cl The client to set/update the window type of
 * @param s The window manager's state
 */
void             setwintype(state_t* s, client_t* cl);

/**
 * @brief Sets/unsets urgency flag on a given client 
 * accoringly.
 *
 * @param s The window manager's state
 * @param cl The client to set/unset urgency on 
 * @param urgent The urgency of the client's window
 */
void             seturgent(state_t* s, client_t* cl, bool urgent);


/**
 * @brief Returns the next on-screen client after the 
 * given client.
 *
 * @param s The window manager's state
 * @param skip_floating Whether or not to skip floating clients
 */
client_t*	      nextvisible(state_t* s, bool skip_floating);

/**
 * @brief Gets the value of a given property on a 
 * window of a given client.
 *
 * @param s The window manager's state
 * @param cl The client to retrieve the property value from
 * @param prop The property to retrieve
 *
 * @return The value of the given property on the given window
 */
xcb_atom_t       getclientprop(state_t* s, client_t* cl, xcb_atom_t prop);

/**
 * @brief Puts a given client in or out of fullscreen
 * mode based on the input.
 *
 * @param s The window manager's state
 * @param cl The client to toggle fullscreen of 
 * @param fullscreen Whether the client should be set 
 * fullscreen or not
 */
void             setfullscreen(state_t* s, client_t* cl, bool fullscreen);

/**
 * @brief Switches the desktop of a given client and hides that client.
 *
 * @param s The window manager's state
 * @param cl The client to switch the desktop of
 * @param desktop The desktop to set the client of 
 */
void             switchclientdesktop(state_t* s, client_t* cl, int32_t desktop);

/**
 * @brief Switches the currently selected desktop index to the given 
 * index and notifies EWMH that there was a desktop change
 *
 * @param s The window manager's state
 * @param desktop Used as the desktop to switch to
 * */
void             switchmonitordesktop(state_t* s, int32_t desktop);

/**
 * @brief Returns how many client are currently in the layout on a 
 * given monitor.
 *
 * @param s The window manager's state
 * @param mon The monitor to count clients on 
 * */
uint32_t 	       numinlayout(state_t* s, monitor_t* mon);

/**
 * @brief Returns the current layout of the current virtual desktop on 
 * a given monitor
 *
 * @param s The window manager's state
 * @param mon The monitor to get the current layout of the current 
 * virtual desktop of.
 * */
layout_type_t    getcurlayout(state_t* s, monitor_t* mon);

/**
 * @brief Establishes the current tiling layout for the windows
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void             makelayout(state_t* s, monitor_t* mon);

/**
 * @brief Resets the size modifications of 
 * all client windows within the layout.
 *
 * @param s The window manager's state
 * @param mon The monitor on which the layout is 
 */
void             resetlayoutsizes(state_t* s, monitor_t* mon);

/**
 * @brief Establishes a tiled master layout for the windows that are 
 * currently visible.
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void             tiledmaster(state_t* s, monitor_t* mon);

/**
 * @brief Establishes a layout in which windows are 
 * layed out left to right as vertical stripes.
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void             verticalstripes(state_t* s, monitor_t* mon);

/**
 * @brief Establishes a layout in which windows are 
 * layed out top to bottom as horizontal stripes 
 *
 * @param s The window manager's state
 * @param mon The monitor to use as the frame of the layout 
 */
void             horizontalstripes(state_t* s, monitor_t* mon);

/**
 * @brief Swaps two clients within the linked list of clients 
 *
 * @param s The window manager's state 
 * @param c1 The client to swap with c2 
 * @param c2 The client to swap with c1
 */
void 		         swapclients(state_t* s, client_t* c1, client_t* c2);

/**
 * @brief Uploads the active desktop names and sends them to EWMH 
 * @param s The window manager's state
 */
void             uploaddesktopnames(state_t* s, monitor_t* mon);

/**
 * @brief Uploads the active desktop names, current desktop and number of desktops 
 * and sends them to EWMH 
 * @param s The window manager's state
 */
void             updateewmhdesktops(state_t* s, monitor_t* mon);

/**
 * @brief Creates a new virtual desktop and notifies EWMH about it.
 * @param s The window manager's state 
 * @param idx The index of the virtual desktop
 * @param mon The monitor that the virtual desktop is on
 */
void             createdesktop(state_t* s, uint32_t idx, monitor_t* mon);

/**
 * @brief Initializes all important atoms for EWMH &
 * NetWM compatibility.
 *
 * @param s The window manager's state 
 * */
void             setupatoms(state_t* s);

/**
 * @brief Grabs all the keybinds specified in config.h for the window 
 * manager. The function also ungrabs all previously grabbed keys
 *
 * @param s The window manager's state 
 * */
void             grabkeybinds(state_t* s);

/**
 * @brief Loads and sets the default cursor image of the window manager.
 * The default image is the left facing pointer.
 * */
void             loaddefaultcursor(state_t* s);

/**
 * @brief Takes in a size for a client window and adjusts it 
 * if it does not meet the requirements of the client's hints.
 * The adjusted value is returned.
 *
 * @param s The window manager's state
 * @param cl The client of which to get the adjusted size of
 * @param size The size before applying hints 
 *
 * @return The size after applying hints
 * */
v2_t            applysizehints(state_t* s, client_t* cl, v2_t size);


/**
 * @brief Returns the layering order in EWMH that is 
 * set for a given client (checks for macros like 
 * _NET_WM_STATE_ABOVE, _NET_WM_STATE_BELOW)
 *
 * @param s the state of the window manager  
 * @param cl the client for checking the layering order 
 *
 * @return The layering order enum for the given client 
 */ 
layering_order_t clientlayering(state_t* s, client_t* cl);

/**
 * @brief Adds a given client to the current tiling layout
 *
 * @param s The window manager's state
 * @param cl The client to add to the layout 
 * */
void            addtolayout(state_t* s, client_t* cl);

/**
 * @brief Returns the number of master- and slave windows 
 * within a given layout
 *
 * @param s The window manager's state
 * @param mon The monitor on which the layout is 
 * @param nmaster The number of master windows [out] 
 * @param nslaves The number of slave windows [out] 
 * */
void            enumartelayout(state_t* s, monitor_t* mon, uint32_t* nmaster, uint32_t* nslaves);

/**
 * @brief Checks if a given client is a master window 
 * within layouts that can have master windows.
 *
 * @param s The window manager's state
 * @param cl The client to check if it is a master window 
 * @param mon The monitor on which the layout is 
 * */
bool            isclientmaster(state_t* s, client_t* cl, monitor_t* mon);

/**
 * @brief Removes a given client from the current tiling layout
 *
 * @param s The window manager's state
 * @param cl The client to remove from the layout 
 * */
void            removefromlayout(state_t* s, client_t* cl);


/**
 * @brief Handles a map request event on the X server by 
 * adding the mapped window to the linked list of clients and 
 * setting up necessary stuff.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evmaprequest(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X unmap event by unmapping the window 
 * and removing the associated client from the linked list.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evunmapnotify(state_t* s, xcb_generic_event_t* ev);


/**
 * @brief Handles an X destroy event by unframing the client 
 * associated with the event and removing it from the list of 
 * clients.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evdestroynotify(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X enter-window event by focusing the window
 * that's associated with the enter event.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             eventernotify(state_t* s, xcb_generic_event_t* ev);


/**
 * @brief Handles a X key press event by checking if the pressed 
 * key (and modifiers) match any window manager keybind and then executing
 * that keybinds function.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evkeypress(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X button press event by focusing the client 
 * associated with the pressed window and setting cursor and window grab positions
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evbuttonpress(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X motion notify event by moving the clients window if left mouse 
 * button is held and resizing the clients window if right mouse is held. 
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evmotionnotify(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a Xorg configure request by configuring the client that 
 * is associated with the window how the event requested it.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evconfigrequest(state_t* s, xcb_generic_event_t* ev);

/*
 * @brief Handles a X configure notify event. If the root window 
 * recieved a configure notify event, the list of monitors is refreshed.
 *
 * @param s The window manager's state
 * @param ev The generic event 
 * */
void             evconfignotify(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X property notify event for window properties by 
 * handling various Extended Window Manager Hints (EWMH). 
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evpropertynotify(state_t* s, xcb_generic_event_t* ev);

/**
 * @brief Handles a X client message event by setting fullscreen or 
 * urgency (based on the event request).
 *
 * @param s The window manager's state
 * @param ev The generic event 
 */
void             evclientmessage(state_t* s, xcb_generic_event_t* ev);


/**
 * @brief Adds a client window to the linked list of clients
 *
 * @param s The window manager's state
 * @param win The window to create a client from and add it 
 * to the clients
 *
 * @return The newly created client
 */
client_t*        addclient(state_t* s, client_t** clients, xcb_window_t win);

/**
 * @brief Removes a given client from the list of clients
 * by window.
 *
 * @param s The window manager's state
 * @param win The window of the client to remove
 */
void             releaseclient(state_t* s, xcb_window_t win);

/**
 * @brief Returns the associated client from a given window.
 * Returns NULL if there is no client associated with the window.
 *
 * @param s The window manager's state
 * @param win The window to get the client from
 *
 * @return The client associated with the given window (NULL if no associated client)
 */
client_t*        clientfromwin(state_t* s, xcb_window_t win);


/**
 * @brief Returns a filtered linked list of all clients 
 * that are currently visible on the given monitor
 *
 * @param s The window manager's state
 * @param mon The monitor to get visible clients off
 * @param tiled Whether or not to ignore floating clients
 *
 * @return A filtered linked list with all visible clients on 
 * the given monitor
 */
client_t*        visibleclients(state_t* s, monitor_t* mon, bool tiled);

/**
**
 * @brief Adds a monitor area to the linked list of monitors
 *
 * @param s The window manager's state 
 * @param a The area of the monitor to create 
 *
 * @return The newly created monitor 
 */
monitor_t*       addmon(state_t* s, area_t a, uint32_t idx); 

/**
 * @brief Returns the monitor within the list of monitors 
 * that matches the given area. 
 * Returns NULL if there is no monitor associated with the area.
 *
 * @param s The window manager's state
 * @param a The area to find within the monitors 
 *
 * @return The monitor associated with the given area (NULL if no associated monitor)
 */
monitor_t*       monbyarea(state_t* s, area_t a);


/**
 * @brief Returns the monitor in which a given client is contained in. 
 * any registered monitor.
 *
 * @param s The window manager's state
 * @param cl The client to get the monitor of 
 *
 * @return The monitor that the given client is contained in.
 * Returns the first monitor if the given client is not contained within
 */
monitor_t*       clientmon(state_t* s, client_t* cl);

/**
 * @brief Returns the currently selected virtual desktop on 
 * a given monitor
 *
 * @param s The window manager's state
 * @param mon The monitor to get the selected desktop of
 *
 * @return The selected virtual desktop on the given monitor 
 */
desktop_t*       mondesktop(state_t* s, monitor_t* mon);

/**
 * @brief Returns the monitor under the cursor 
 * Returns the first monitor if there is no monitor under the cursor. 
 *
 * @param s The window manager's state
 *
 * @return The monitor that is under the cursor.
 */
monitor_t*       cursormon(state_t* s);

/**
 * @brief Gets all screens registed by xrandr and adds newly registered 
 * monitors to the linked list of monitors in the window manager.
 *
 * @param s The window manager's state 
 *
 * @return The number of newly registered monitors 
 */
uint32_t         updatemons(state_t* s);

/**
 * @brief Returns the keysym of a given key code. 
 * Returns 0 if there is no keysym for the given keycode.
 *
 * @param s The window manager's state
 * @param keycode The keycode to get the keysym from 
 *
 * @return The keysym of the given keycode (0 if no keysym associated) 
 */
xcb_keysym_t     getkeysym(state_t* s, xcb_keycode_t keycode);

/**
 * @brief Returns the keycode of a given keysym. 
 * Returns NULL if there is no keycode for the given keysym.
 *
 * @param s The window manager's state
 * @param keysym The keysym to get the keycode from 
 *
 * @return The keycode of the given keysym (NULL if no keysym associated) 
 */
xcb_keycode_t*   getkeycodes(state_t* s, xcb_keysym_t keysym);

/**
 * @brief Reads the _NET_WM_STRUT_PARTIAL atom of a given window and 
 * uses the gathered strut information to populate a strut_t.
 *
 * @param s The window manager's state
 * @param win The window to retrieve the strut of 
 *
 * @return The strut of the window 
 */
strut_t          readstrut(state_t* s, xcb_window_t win);

/**
 * @brief Gathers the struts of all subwindows of a given window
 *
 * @param s The window manager's state
 * @param win The window to use as a root 
 */
void             getwinstruts(state_t* s, xcb_window_t win);

/**
 * @brief Retrieves an intern X atom by name.
 *
 * @param s The window manager's state
 * @param atomstr The string (name) of the atom to retrieve
 *
 * @return The XCB atom associated with the given atom name.
 * Returns XCB_ATOM_NONE if the given name is not associated with 
 * any atom.
 * */
xcb_atom_t       getatom(state_t* s, const char* atomstr);

/**
 * 
 * @brief Updates the client list EWMH atom tothe current list of clients
 *
 * @param s The window manager's state 
 * */
void             ewmh_updateclients(state_t* s);

/**
 * @brief Signal handler for SIGCHLD to avoid zombie processes
 * */
void             sigchld_handler(int32_t signum);

/**
 * @brief Checks if a given string is within a given 
 * array of strings
 * @param array Haystack
 * @param count Number of items within the haystack
 * @param target Needle
 *
 * @return Wether or not the given string was found in the 
 * given array
 * */
bool             strinarr(char* array[], int count, const char* target);

/*
 * @brief Compares two strings 
 * @param a The first string to compare
 * @param b The second string to compare
 * @return See 'man strcmp' */
int32_t          compstrs(const void* a, const void* b);


/**
 * @brief Logs a given message to stdout/stderr that differs based 
 * on the log level given. This function also logs the given message to 
 * the logfile specified in the config if 'shouldlogtofile' is enabled. 
 * config.
 * @param lvl The log level 
 * @param fmt The format string
 * @param ... The variadic arguments */
void             logmsg(state_t* s, log_level_t lvl, const char* fmt, ...);

/**
 * @brief Logs a given formatted message to the log file specified in the
 * config. 
 * @param fmt The format string
 * @param ... The variadic arguments */
void 		         logtofile(log_level_t lvl, state_t* s, const char* fmt, va_list args); 


/**
 * @brief Returns the output of a given command 
 *
 * @param cmd The command to get the output of 
 *
 * @return The output of the given command */ 
char* 		       cmdoutput(const char* cmd);

