#pragma once
#include "funcs.h"
#include <string.h>

/**
 * @brief Terminates the window manager with 
 * EXIT_SUCCESS exit code.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void terminate_successfully(state_t* s, passthrough_data_t data) { 
  (void)data;
  terminate(s, EXIT_SUCCESS);
}


/**
 * @brief Cycles the currently focused client one up
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cyclefocusup(state_t* s, passthrough_data_t data) {
  (void)data;
  // Return if there are no clients or no focus
  if (!s->clients || !s->focus)
    return;
  
  client_t* next = prevvisible(s, false);

  if(next) {
    focusclient(s, next);
    raiseclient(s, next);
  }
}

/**
 * @brief Cycles the currently focused client one down 
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cyclefocusdown(state_t* s, passthrough_data_t data) { 
  (void)data;
  if (!s->clients || !s->focus)
    return;
 
  client_t* next = nextvisible(s, false);

  // If there is a next client, just focus it
  if (next) {
    focusclient(s, next);
    raiseclient(s, next);
  }
}

/**
 * @brief Kills the currently focused window 
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void killfocus(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus) {
    return;
  }
  killclient(s, s->focus);
}



/**
 * @brief Toggles fullscreen mode on the currently focused client
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void togglefullscreen(state_t* s, passthrough_data_t data) { 
  (void)data;

  if(!s->focus) return;
  bool fs = !(s->focus->fullscreen);
  if(s->showtitlebars) {
    if(fs) {
      hidetitlebar(s, s->focus);
    } else {
      showtitlebar(s, s->focus);
    }
  }
  setfullscreen(s, s->focus, fs); 
}

/**
 * @brief Raises the currently focused client
 * 
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void raisefocus(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus) return;
  raiseclient(s, s->focus);
}

/**
 * @brief Cycles the currently selected desktop index one desktop up 
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void cycledesktopup(state_t* s, passthrough_data_t data) { 
  (void)data;

  int32_t newdesktop = mondesktop(s, s->monfocus)->idx;
  if(newdesktop + 1 < MAX_DESKTOPS) {
    newdesktop++;
  } else {
    newdesktop = 0;
  }
  switchdesktop(s, (passthrough_data_t){.i = newdesktop});
}

/**
 * @brief Cycles the currently selected desktop index one desktop down
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */ 
inline void cycledesktopdown(state_t* s, passthrough_data_t data) { 
  (void)data;

  int32_t newdesktop = mondesktop(s, s->monfocus)->idx;
  if(newdesktop - 1 >= 0) {
    newdesktop--;
  } else {
    newdesktop = MAX_DESKTOPS - 1;
  }
  switchdesktop(s, (passthrough_data_t){.i = newdesktop});
}

/*
 * Cycles the desktop, that the focused client is on, up
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 * */
inline void cyclefocusdesktopup(state_t* s, passthrough_data_t data) { 
  (void)data;

  if(!s->focus) return;
  int32_t new_desktop = s->focus->desktop;
  if(new_desktop + 1 < MAX_DESKTOPS) {
    new_desktop++;
  } else {
    new_desktop = 0;
  }
  switchclientdesktop(s, s->focus, new_desktop);
}

/*
 * Cycles the desktop, that the focused client is on, down 
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 * */
inline void cyclefocusdesktopdown(state_t* s, passthrough_data_t data){ 
  (void)data;

  if(!s->focus) return;
  int32_t new_desktop = s->focus->desktop;
  if(new_desktop - 1 >= 0) {
    new_desktop--;
  } else {
    new_desktop = MAX_DESKTOPS - 1;
  }
  switchclientdesktop(s, s->focus, new_desktop);
}

/**
 * @brief Switches the currently selected desktop index to the given 
 * index and notifies EWMH that there was a desktop change
 *
 * @param s The window manager's state
 * @param data The .i member is used as the desktop to switch to
 * */
inline void switchdesktop(state_t* s, passthrough_data_t data){ 
  if(!s->monfocus) return;
  if(data.i == (int32_t)mondesktop(s, s->monfocus)->idx) return;

  // Create the desktop if it was not created yet
  if(!strinarr(s->monfocus->activedesktops, s->monfocus->desktopcount, desktopnames[data.i])) {
    createdesktop(s, data.i, s->monfocus);
  }

  uint32_t desktopidx = 0;
  for(uint32_t i = 0; i < s->monfocus->desktopcount; i++) {
    if(strcmp(s->monfocus->activedesktops[i], desktopnames[data.i]) == 0) {
      desktopidx = i;
      break;
    }
  }
  // Notify EWMH for desktop change
  xcb_change_property(s->con, XCB_PROP_MODE_REPLACE, s->root, s->ewmh_atoms[EWMHcurrentDesktop],
      XCB_ATOM_CARDINAL, 32, 1, &desktopidx);


  for (client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->mon != s->monfocus) continue;
    // Hide the clients on the current desktop
    if(cl->desktop == mondesktop(s, s->monfocus)->idx) {
      hideclient(s, cl);
      // Show the clients on the desktop we want to switch to
    } else if((int32_t)cl->desktop == data.i) {
      showclient(s, cl);
    }
  }

  // Unfocus all selected clients
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    unfocusclient(s, cl);
  }

  mondesktop(s, s->monfocus)->idx = data.i;
  makelayout(s, s->monfocus);

  logmsg(LogLevelTrace, "Switched virtual desktop on monitor %i to %i",
      s->monfocus->idx, data.i);
}


/**
 * @brief Switches the desktop of the currently selected client to a given index 
 *
 * @param s The window manager's state
 * @param data The .i member is used as the desktop to switch to
 * */
inline void switchfocusdesktop(state_t* s, passthrough_data_t data){ 
  if(!s->focus) return;
  switchclientdesktop(s, s->focus, data.i);
}

/**
 * @brief Runs a given command by forking the process and using execl.
 *
 * @param cmd The command to run 
 */
inline void runcmd(state_t* s, passthrough_data_t data) { 
  (void)s;
  if (data.cmd == NULL) {
    return;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Child process
    execl("/bin/sh", "sh", "-c", data.cmd, (char *)NULL);
    // If execl fails
    logmsg(LogLevelError, "failed to execute command '%s'.", data.cmd);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    // Parent process
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    // Fork failed
    perror("fork");
    logmsg(LogLevelError, "failed to execute command '%s'.", data.cmd);
    return;
  }
}

/**
 * @brief Adds the currently focused client to the current 
 * layout.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void addfocustolayout(state_t* s, passthrough_data_t data) { 
  (void)data;

  s->focus->floating = false;
  makelayout(s, s->monfocus);
}

/**
 * @brief Sets the current layout to tiled master 
 * and adds every on-screen client to the layout
 * layout.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void settiledmaster(state_t* s, passthrough_data_t data){ 
  (void)data;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, s->monfocus)) {
      cl->floating = false;
    }
  }
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  s->monfocus->layouts[deskidx].curlayout = LayoutTiledMaster;
  makelayout(s, s->monfocus);
}

/**
 * @brief Sets the current layout to floating 
 * and removes every on-screen client to the layout
 * layout.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void setfloatingmode(state_t* s, passthrough_data_t data) { 
  (void)data;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, s->monfocus)) {
      cl->floating = true;
    }
  }
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  s->monfocus->layouts[deskidx].curlayout = LayoutFloating;
  makelayout(s, s->monfocus);
}

/**
 * @brief Updates the inernal list of window struts that 
 * the layout knows about.
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void updatebarslayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  // Gather strut information 
  s->nwinstruts = 0;
  getwinstruts(s, s->root);
  makelayout(s, s->monfocus);
}


/**
 * @brief Cycles the focused window up in the layout list by
 * swapping it with the client before the it.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cycleuplayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  client_t* prev = prevvisible(s, true);
  s->ignore_enter_layout = true;
  swapclients(s, s->focus, prev);
  makelayout(s, s->monfocus);
}

/**
 * @brief Cycles the focused window down in the layout list by
 * swapping it with the client after the it.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cycledownlayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  client_t* next = nextvisible(s, true);
  s->ignore_enter_layout = true;
  swapclients(s, s->focus, next);
  makelayout(s, s->monfocus);
}

/**
 * @brief Increments the number of windows that are seen as master windows
 * in master-slave layouts
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void addmasterlayout(state_t* s, passthrough_data_t data){ 
  (void)data;

  uint32_t nlayout = numinlayout(s, s->monfocus);
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(nlayout - (layout->nmaster + 1) >= 1 && nlayout > 1) {
    layout->nmaster++;
  }
  makelayout(s, s->monfocus);
}


/**
 * @brief Decrements the number of windows that are seen as master windows
 * in master-slave layouts
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void removemasterlayout(state_t* s, passthrough_data_t data){ 
  (void)data;

  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->nmaster - 1 >= 1) {
    layout->nmaster--;
  }
  makelayout(s, s->monfocus);
}

/**
 * @brief Increments the area that the master window takes 
 * up by 'layoutmasterarea_step'. (in %)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void incmasterarealayout(state_t* s, passthrough_data_t data){ 
  (void)data;

  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->masterarea 
      + layoutmasterarea_step <= layoutmasterarea_max) {
    layout->masterarea += layoutmasterarea_step;
  }
  makelayout(s, s->monfocus);
}

/**
 * @brief Decrements the area that the master window takes 
 * up by 'layoutmasterarea_step'. (in %)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void decmasterarealayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->masterarea 
      - layoutmasterarea_step >= layoutmasterarea_min) {
    layout->masterarea -= layoutmasterarea_step;
  }
  makelayout(s, s->monfocus);
}


/**
 * @brief Increments the size of the gaps between windows 
 * in layouts in the current layout.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void incgapsizelayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->gapsize + winlayoutgap_step <= winlayoutgap_max) {
    layout->gapsize += winlayoutgap_step;
  }
  makelayout(s, s->monfocus);
}

/**
 * @brief Decrements the size of the gaps between windows 
 * in layouts in the current layout.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void decgapsizelayout(state_t* s, passthrough_data_t data){ 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->gapsize - winlayoutgap_step >= 0) {
    layout->gapsize -= winlayoutgap_step;
  }
  makelayout(s, s->monfocus);
}


/**
 * @brief Toggles the titlebar of a the focused client 
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void togglefocustitlebar(state_t* s, passthrough_data_t data){ 
  (void)data;
  if(s->focus->showtitlebar) {
    hidetitlebar(s, s->focus);
  } else {
    showtitlebar(s, s->focus);
  }
}


/**
 * @brief Toggles the titlebar of every client window 
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void toggletitlebars(state_t* s, passthrough_data_t data){ 
  (void)data;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(s->showtitlebars) {
      hidetitlebar(s, cl);
    } else {
      showtitlebar(s, cl);
    }
  }
  s->showtitlebars = !s->showtitlebars;
}


/**
 * @brief Moves the focused client up by 'keywinmove_step' 
 * (in px)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void movefocusup(state_t* s, passthrough_data_t data){ 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){pos.x,
    MIN(MAX(pos.y - keywinmove_step, s->monfocus->area.pos.y), 
        s->monfocus->area.pos.y + s->monfocus->area.size.y
        - s->focus->area.size.y)};
  moveclient(s, s->focus, dest);
  s->ignore_enter_layout = true;
}

/**
 * @brief Moves the focused client down by 'keywinmove_step' 
 * (in px)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void movefocusdown(state_t* s, passthrough_data_t data){ 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){pos.x,
    MIN(MAX(pos.y + keywinmove_step, s->monfocus->area.pos.y), 
        s->monfocus->area.pos.y + s->monfocus->area.size.y 
        - s->focus->area.size.y)};
  moveclient(s, s->focus, dest);
  s->ignore_enter_layout = true;
}

/**
 * @brief Moves the focused client left by 'keywinmove_step' 
 * (in px)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void movefocusleft(state_t* s, passthrough_data_t data){ 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){MIN(MAX(pos.x - keywinmove_step, s->monfocus->area.pos.x), 
      s->monfocus->area.pos.x + s->monfocus->area.size.x 
      - s->focus->area.size.x), pos.y};
  moveclient(s, s->focus, dest);
  s->ignore_enter_layout = true;
}

/**
 * @brief Moves the focused client right by 'keywinmove_step' 
 * (in px)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void movefocusright(state_t* s, passthrough_data_t data){ 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){MIN(MAX(pos.x + keywinmove_step, s->monfocus->area.pos.x), 
      s->monfocus->area.pos.x + s->monfocus->area.size.x 
      - s->focus->area.size.x), pos.y};
  moveclient(s, s->focus, dest);
  s->ignore_enter_layout = true;
}

