#pragma once

#include "structs.h"

/* ===================== CORE  ===================== */

/**
 * @brief Sets up the WM state and the X server 
 *
 * This function establishes a s.conection to the X server,
 * sets up the root window and window manager keybindings.
 * The event mask of the root window is being cofigured to
 * listen to necessary events. 
 * After the configuration of the root window, all the specified
 * keybinds in config.h are grabbed by the window manager.
 */
void setup();
/**
 * @brief Event loop of the window manager 
 *
 * This function waits for X server events and 
 * handles them accoringly by calling the associated event handler.
 */
void loop();
/**
 * @brief Terminates the window manager 
 *
 * This function terminates the window manager by
 * diss.conecting the s.conection to the X server and
 * exiting the program.
 */
void terminate(int32_t exitcode);
/**
 * @brief Creates WM atoms & sets up properties for EWMH. 
 * */
void setupatoms();
/**
 * @brief Grabs all the keybinds specified in config.h for the window 
 * manager. The function also ungrabs all previously grabbed keys.
 * */
void grabkeybinds();
/**
 * @brief Loads and sets the default cursor image of the window manager.
 * The default image is the left facing pointer.
 * */
void loaddefaultcursor();

/**
 * @brief Creates an GLX Context and sets up OpenGL visual. 
 * */
void initglcontext();

/* ===================== UTIL ===================== */

/**
 * @brief Evaluates if a given point is inside a given area 
 *
 * @param p The point to check if it is inside the given area
 * @param area The area to check
 *
 * @return If the point p is in the area, false if it is not in the area 
 */
bool pointinarea(v2 p, area a);
/**
 * @brief Returns the current cursor position on the X display 
 *
 * @param success Gets assigned whether or not the pointer query
 * was successfull.
 *
 * @return The cursor position as a two dimensional vector 
 */
v2 cursorpos(bool* success);
/**
 * @brief Returns the area (position and size) of a given window 
 *
 * @param win The window to get the area from 
 * @param success Gets assigned whether or not the geometry query
 * was successfull.
 *
 * @return The area of the given window 
 */
area winarea(xcb_window_t win, bool* success);
/**
 * @brief Gets the area (in float) of the overlap between two areas 
 *
 * @param a The inner area 
 * @param a The outer area 
 *
 * @return The overlap between the two areas 
 */
float getoverlaparea(area a, area b);

/**
 * @brief Returns the keysym of a given key code. 
 * Returns 0 if there is no keysym for the given keycode.
 *
 * @param keycode The keycode to get the keysym from 
 *
 * @return The keysym of the given keycode (0 if no keysym associated) 
 */
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
/**
 * @brief Returns the keycode of a given keysym. 
 * Returns NULL if there is no keycode for the given keysym.
 *
 * @param keysym The keysym to get the keycode from 
 *
 * @return The keycode of the given keysym (NULL if no keysym associated) 
 */
xcb_keycode_t* getkeycodes(xcb_keysym_t keysym);
/**
 * @brief Retrieves an intern X atom by name.
 *
 * @param atomstr The string (name) of the atom to retrieve
 *
 * @return The XCB atom associated with the given atom name.
 * Returns XCB_ATOM_NONE if the given name is not associated with 
 * any atom.
 * */
xcb_atom_t getatom(const char* atomstr);
/**
 * @brief Updates the client list EWMH atom tothe current list of clients.
 * */
void ewmh_updateclients();
/**
 * @brief Signal handler for SIGCHLD to avoid zombie processes
 * */
void sigchld_handler(int32_t signum);

/* ============== CLIENT INTERACTION =============== */

/**
 * @brief Adds a client window to the linked list of clients.
 *
 * @param win The window to create a client from and add it 
 * to the clients
 *
 * @return The newly created client
 */
client* addclient(xcb_window_t win);
/**
 * @brief Removes a given client from the list of clients
 * by window.
 *
 * @param win The window of the client to remove
 */
void releaseclient(xcb_window_t win);
/**
 * @brief Returns the associated client from a given window.
 * Returns NULL if there is no client associated with the window.
 *
 * @param win The window to get the client from
 *
 * @return The client associated with the given window (NULL if no associated client)
 */
client* clientfromwin(xcb_window_t win);
/**
 * @brief Returns the associated client from a given decoration.
 * Returns NULL if there is no client associated with the decoration.
 *
 * @param win The decoration to get the client from
 *
 * @return The client associated with the given decoration (NULL if no associated client)
 */
client* clientfromdecoration(xcb_window_t decoration);
/**
 * @brief Sets the border color of a given clients window 
 *
 * @param cl The client to set the border color of 
 * @param color The border color 
 */
void setbordercolor(client* cl, uint32_t color);
/**
 * @brief Sets the border width of a given clients window 
 * and updates its 'borderwidth' variable 
 *
 * @param cl The client to set the border width of 
 * @param width The border width 
 */
void setborderwidth(client* cl, uint32_t width);
/**
 * @brief Moves the window of a given client and updates its area.
 *
 * @param cl The client to move 
 * @param pos The position to move the client to 
 */
void moveclient(client* cl, v2 pos);
/**
 * @brief Resizes the window of a given client and updates its area.
 *
 * @param cl The client to resize 
 * @param pos The new size of the clients window 
 */
void resizeclient(client* cl, v2 size);
/**
 * @brief Moves and resizes the window of a given client and updates its area.
 *
 * @param cl The client to move and resize
 * @param a The new area (position and size) for the client's window
 */
void moveresizeclient(client* cl, area a);
/**
 * @brief Raises the window of a given client to the top of the stack
 *
 * @param cl The client to raise 
 */
void raiseclient(client* cl);
/**
 * @brief Kills a given client by destroying the associated window and 
 * removing it from the linked list.
 *
 * @param cl The client to kill 
 */
void killclient(client* cl);
/**
 * @brief Sets the current focus to a given client
 *
 * @param cl The client to focus
 */
void focusclient(client* cl);
/**
 * @brief Sets the X input focus to a given client and handles EWMH atoms & 
 * focus event.
 *
 * @param cl The client to set the input focus to
 */
void setxfocus(client* cl);
/**
 * @brief Unfocuses a given client and resets input foucs to root  
 *
 * @param cl The client to unfocus
 */
void unfocusclient(client* cl);
/*
 * @brief Hides a given client by unmapping it's window
 * @param cl The client to hide 
 */
void hideclient(client* cl);
/*
 * @brief Shows a given client by mapping it's window
 * @param cl The client to show 
 */
void showclient(client* cl);
/*
 * @brief Updates a client's geometry and decoration 
 */
void updateclient(client* cl);
/**
 * @brief Sends a given X event by atom to the window of a given client
 *
 * @param cl The client to send the event to
 * @param protocol The atom to store the event
 */

bool raiseevent(client* cl, xcb_atom_t protocol);
/**
 * @brief Checks for window type of clients by EWMH and acts accoringly.
 * If a client requested fullscreen, for example, the window manager fullscreens 
 * the client.
 *
 * @param cl The client to set/update the window type of
 */
void setwintype(client* cl);
/**
 * @brief Sets/unsets urgency hint on a given client 
 *
 * @param cl The client to update urgency hints on
 * @param urgent Defines if urgency should be set or unset
 */
void seturgent(client* cl, bool urgent);
/**
 * @brief Gets the value of a given property on a 
 * window of a given client.
 *
 * @param cl The client to retrieve the property value from
 * @param prop The property to retrieve
 *
 * @return The value of the given property on the given window
 */
xcb_atom_t getclientprop(client* cl, xcb_atom_t prop);
/**
 * @brief Puts a given client in or out of fullscreen
 * mode based on the input.
 *
 * @param cl The client to toggle fullscreen of 
 * @param fullscreen Whether the client should be set 
 * fullscreen or not
 */
void setfullscreen(client* cl, bool fullscreen);
/**
 * @brief Switches the desktop of a given client and hides that client.
 * @param cl The client to switch the desktop of
 * @param desktop The desktop to set the client of 
 */
void switchclientdesktop(client* cl, int32_t desktop);
/**
 * @brief Returns the window title/name of a given client (Allocates memory) 
 *
 * @param cl The client to get the window title from 
 *
 * @return The name of the given client 
 */
char* getclientname(client* cl);

/* ============ DECORATION ============= */

/**
 * @brief Sets up the decoration window of a given client and reparents it the 
 * client's frame window. Sets OpenGL context to the newly created window 
 * and renders the decoration UI. If no leif context is initialized, the function 
 * initializes it as leif needs a OpenGL context to be initialized.
 *
 * @param cl The client to setup a decoration window for
 */
void setupdecoration(client* cl);
/**
 * @brief Updates the decoration window of the given client by resizing it
 * to the client's size.
 *
 * @param cl The client to update decoration of 
 */
void updatedecoration(client* cl);
/**
 * @brief Renders the decoration UI of a given client on it's decoration window.
 * The rendering is done with OpenGL and libleif. 
 *
 * @param cl The client to render decoration of 
 */
void renderdecoration(client* cl);


/* ========= EVENT HANDLERS ========== */
/**
 * @brief Handles a map request event on the X server by 
 * adding the mapped window to the linked list of clients and 
 * setting up necessary stuff.
 *
 * @param ev The generic event 
 */
void evmaprequest(xcb_generic_event_t* ev);
/**
 * @brief Handles an X unmap event by unmapping the window 
 * and removing the associated client from the linked list.
 *
 * @param ev The generic event 
 */
void evunmapnotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X enter-window event by focusing the window
 * that's associated with the enter event.
 *
 * @param ev The generic event 
 */
void eventernotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X key press event by checking if the pressed 
 * key (and modifiers) match any window manager keybind and then executing
 * that keybinds function.
 *
 * @param ev The generic event 
 */
void evkeypress(xcb_generic_event_t* ev);
/**
 * @brief Handles an X button press event by focusing the client 
 * associated with the pressed window and setting cursor and window grab positions.
 *
 * @param ev The generic event 
 */
void evbuttonpress(xcb_generic_event_t* ev);
/**
 * @brief Handles an X motion notify event by moving the clients window if left mouse 
 * button is held and resizing the clients window if right mouse is held. 
 *
 * @param ev The generic event 
 */
void evmotionnotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X configure request by configuring the client that 
 * is associated with the window how the event requested it.
 *
 * @param ev The generic event 
 */
void evconfigrequest(xcb_generic_event_t* ev);
/*
 * @brief Handles an X configure notify event. If the root window 
 * recieved a configure notify event, the list of monitors is refreshed.
 * */
void evconfignotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X property notify event for window properties by handling various Extended Window Manager Hints (EWMH). 
 *
 * @param ev The generic event 
 */
void evpropertynotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X client message event by setting fullscreen or 
 * urgency (based on the event request).
 *
 * @param ev The generic event 
 */
void evclientmessage(xcb_generic_event_t* ev);
/**
 * @brief Handles an X destroy window event by destroying
 * the client's decoration.
 *
 * @param ev The generic event 
 */
void evdestroynotify(xcb_generic_event_t* ev);
/**
 * @brief Handles an X expose event by re-rendering decoration UI 
 * if the window that raised the event is a decoration window. *
 *
 * @param ev The generic event 
 */
void evexpose(xcb_generic_event_t* ev);

/* ============= MONITORS ============= */
/**
 * @brief Adds a monitor area to the linked list of monitors.
 *
 * @param a The area of the monitor to create 
 *
 * @return The newly created monitor 
 */
monitor* addmon(area a, uint32_t idx); 
/**
 * @brief Returns the monitor within the list of monitors 
 * that matches the given area. 
 * Returns NULL if there is no monitor associated with the area.
 *
 * @param a The area to find within the monitors 
 *
 * @return The monitor associated with the given area (NULL if no associated monitor)
 */
monitor* monbyarea(area a);
/**
 * @brief Returns the monitor in which a given client is contained in. 
 * any registered monitor.
 *
 * @param cl The client to get the monitor of 
 *
 * @return The monitor that the given client is contained in.
 * Returns the first monitor if the given client is not contained within
 */
monitor* clientmon(client* cl);
/**
 * @brief Returns the monitor under the cursor 
 *
 * @return The monitor that is under the cursor.
 * Returns the first monitor if there is no monitor under the cursor. 
 */
monitor* cursormon();
/**
 * @brief Retrieves the currently active monitor setup with XRandr and syncs it 
 * with the internal list of monitors
 *
 * @return The number of monitors that were enumarted by XRandr
 */
uint32_t updatemons();
