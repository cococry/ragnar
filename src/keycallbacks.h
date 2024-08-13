#pragma once
#include "funcs.h"
#include "structs.h"
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
 * @brief Cycles the currently focused client one down 
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cyclefocusdown(state_t* s, passthrough_data_t data) { 
  (void)data;
  if (!s->clients || !s->focus)
    return;

  s->ignore_enter_layout = true;

  client_t* new_focus = nextvisible(s, false);

  focusclient(s, new_focus);
  raiseclient(s, s->focus);
}

/**
 * @brief Kills the currently focused window 
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void killfocus(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus || s->focus->is_scratchpad) {
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
  if(newdesktop + 1 < (int32_t)s->config.maxdesktops) {
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
    newdesktop = s->config.maxdesktops - 1;
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
  if(new_desktop + 1 < (int32_t)s->config.maxdesktops) {
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
inline void cyclefocusdesktopdown(state_t* s, passthrough_data_t data) { 
  (void)data;

  if(!s->focus) return;
  int32_t new_desktop = s->focus->desktop;
  if(new_desktop - 1 >= 0) {
    new_desktop--;
  } else {
    new_desktop = s->config.maxdesktops - 1;
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
inline void switchdesktop(state_t* s, passthrough_data_t data) { 
  if(!s->monfocus) return;
  if(data.i == (int32_t)mondesktop(s, s->monfocus)->idx) return;

  // Create the desktop if it was not created yet
  if(!strinarr(s->monfocus->activedesktops, s->monfocus->desktopcount, s->config.desktopnames[data.i])) {
    createdesktop(s, data.i, s->monfocus);
  }

  uint32_t desktopidx = 0;
  for(uint32_t i = 0; i < s->monfocus->desktopcount; i++) {
    if(strcmp(s->monfocus->activedesktops[i], s->config.desktopnames[data.i]) == 0) {
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

  s->lastdesktop = *mondesktop(s, s->monfocus);

  mondesktop(s, s->monfocus)->idx = data.i;
  makelayout(s, s->monfocus);

  logmsg(s, LogLevelTrace, "Switched virtual desktop on monitor %i to %i",
      s->monfocus->idx, data.i);

  s->ignore_enter_layout = false;
}


/**
 * @brief Switches the desktop of the currently selected client to a given index 
 *
 * @param s The window manager's state
 * @param data The .i member is used as the desktop to switch to
 * */
inline void switchfocusdesktop(state_t* s, passthrough_data_t data) { 
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
    logmsg(s, LogLevelError, "failed to execute command '%s'.", data.cmd);
    _exit(EXIT_FAILURE);
  } else if (pid > 0) {
    // Parent process
    int32_t status;
    waitpid(pid, &status, 0);
    return;
  } else {
    // Fork failed
    perror("fork");
    logmsg(s, LogLevelError, "failed to execute command '%s'.", data.cmd);
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

  if(s->focus->is_scratchpad) return;

  s->focus->floating = false;

  resetlayoutsizes(s, s->monfocus);
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
inline void settiledmaster(state_t* s, passthrough_data_t data) { 
  (void)data;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, s->monfocus) && !cl->is_scratchpad) {
      cl->floating = false;
    }
  }
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  s->monfocus->layouts[deskidx].curlayout = LayoutTiledMaster;

  resetlayoutsizes(s, s->monfocus);

  makelayout(s, s->monfocus);
}

/**
 * @brief Sets the current layout to vertical stripes 
 * and adds every on-screen client to the layout
 * layout.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void setverticalstripes(state_t* s, passthrough_data_t data) { 
  (void)data;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, s->monfocus) && !cl->is_scratchpad) {
      cl->floating = false;
    }
  }
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  s->monfocus->layouts[deskidx].curlayout = LayoutVerticalStripes;

  resetlayoutsizes(s, s->monfocus);

  makelayout(s, s->monfocus);
}

/**
 * @brief Sets the current layout to horizontal stripes 
 * and adds every on-screen client to the layout
 * layout.
 *
 * @param s The window manager's state 
 * @param data The data to use for the function (unused here)
 */
inline void sethorizontalstripes(state_t* s, passthrough_data_t data) { 
  (void)data;

  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, s->monfocus) && !cl->is_scratchpad) {
      cl->floating = false;
    }
  }
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  s->monfocus->layouts[deskidx].curlayout = LayoutHorizontalStripes;

  resetlayoutsizes(s, s->monfocus);

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
inline void updatebarslayout(state_t* s, passthrough_data_t data) { 
  (void)data;
  // Gather strut information 
  s->nwinstruts = 0;
  getwinstruts(s, s->root);
  makelayout(s, s->monfocus);
}

/**
 * @brief Cycles the focused window down in the layout list by
 * swapping it with the client after the it.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void cycledownlayout(state_t* s, passthrough_data_t data) { 
  (void)data;
 
  s->ignore_enter_layout = true;
  client_t* next = nextvisible(s, true);

  swapclients(s, s->focus, next);
  resetlayoutsizes(s, s->monfocus); 
  makelayout(s, s->monfocus);
}

/**
 * @brief Increments the number of windows that are seen as master windows
 * in master-slave layouts
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void addmasterlayout(state_t* s, passthrough_data_t data) { 
  (void)data;

  uint32_t nlayout = numinlayout(s, s->monfocus);
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(nlayout - (layout->nmaster + 1) >= 1 && nlayout > 1) {
    layout->nmaster++;
  }

  resetlayoutsizes(s, s->monfocus);
  makelayout(s, s->monfocus);
}

/**
 * @brief Decrements the number of windows that are seen as master windows
 * in master-slave layouts
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void removemasterlayout(state_t* s, passthrough_data_t data) { 
  (void)data;

  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->nmaster - 1 >= 1) {
    layout->nmaster--;
  }
  
  resetlayoutsizes(s, s->monfocus);
  makelayout(s, s->monfocus);
}

/**
 * @brief Increments the area that the master window takes 
 * up by 'layoutmasterarea_step'. (in %)
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void incmasterarealayout(state_t* s, passthrough_data_t data) { 
  (void)data;

  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->masterarea 
      + s->config.layoutmasterarea_step <= s->config.layoutmasterarea_max) {
    layout->masterarea += s->config.layoutmasterarea_step;
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
inline void decmasterarealayout(state_t* s, passthrough_data_t data) { 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->mastermaxed) return;

  if(layout->masterarea 
      - s->config.layoutmasterarea_step >= s->config.layoutmasterarea_min) {
    layout->masterarea -= s->config.layoutmasterarea_step;
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
inline void incgapsizelayout(state_t* s, passthrough_data_t data) { 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->gapsize + s->config.winlayoutgap_step <= s->config.winlayoutgap_max) {
    layout->gapsize += s->config.winlayoutgap_step;
  }
  makelayout(s, s->monfocus);
}

/**
 * @brief Increments the height of a client within layouts 
 * in which can be stacked on top of each other.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void inclayoutsizefocus(state_t* s, passthrough_data_t data) {
  (void)data;
  if(!s->focus) return;

  bool horizontal = false;
  layout_type_t curlayout = getcurlayout(s, s->monfocus);
  // Only layouts where clients are stacked on top of each other
  if( curlayout == LayoutTiledMaster ||
      curlayout == LayoutHorizontalStripes) {
    horizontal = true;
  } 

  uint32_t nmaster = 0, nslaves = 0;
  enumartelayout(s, s->monfocus, &nmaster, &nslaves);

  bool ismaster = isclientmaster(s, s->focus, s->monfocus);

  // The last client cannot change size itself as it is only 
  // influenced by the client ontop of it
  if(!s->focus->next || (s->focus->next && s->focus->next->desktop != s->focus->desktop)
      || (s->focus->next && s->focus->next->mon != s->focus->mon)) return;

  uint32_t i = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx
      || cl->mon != s->monfocus) continue;
    if(i == nmaster - 1 && cl == s->focus) {
      return;
    }
    i++;
  }

  // If the height of the window influenced by the focus is smaller 
  // than the minimum height a window can be in the layout, return
  float nextsize = horizontal ? s->focus->next->area.size.y : s->focus->next->area.size.x;
  if(nextsize - s->config.layoutsize_step < s->config.layoutsize_min) {
    if(horizontal) {
      s->focus->next->area.size.y = s->config.layoutsize_min;
    } else {
      s->focus->next->area.size.x = s->config.layoutsize_min;
    }
    return;
  }
  
  s->ignore_enter_layout = true;

  // If the window is a master and there are more than one master 
  // or the window is a slave and there are more than one slave,
  // the clients height can be changed
  if((nmaster > 1 && ismaster) || (nslaves > 1 && !ismaster)) {
    s->focus->layoutsizeadd += s->config.layoutsize_step;
  }

  // Recreate the layout
  makelayout(s, s->monfocus);
}

inline void declayoutsizefocus(state_t* s, passthrough_data_t data) {
  (void)data;
  if(!s->focus) return;

  bool horizontal = false;
  layout_type_t curlayout = getcurlayout(s, s->monfocus);
  // Only layouts where clients are stacked on top of each other
  if( curlayout == LayoutTiledMaster ||
      curlayout == LayoutHorizontalStripes) {
    horizontal = true;
  } 

  uint32_t nmaster = 0, nslaves = 0;
  enumartelayout(s, s->monfocus, &nmaster, &nslaves);

  bool ismaster = isclientmaster(s, s->focus, s->monfocus);

  // The last client cannot change size itself as it is only 
  // influenced by the client ontop of it
  if(!s->focus->next || (s->focus->next && s->focus->next->desktop != s->focus->desktop)
      || (s->focus->next && s->focus->next->mon != s->focus->mon)) return;
  
  uint32_t i = 0;
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(cl->floating || cl->desktop != mondesktop(s, cl->mon)->idx
      || cl->mon != s->monfocus) continue;
    if(i == nmaster - 1 && cl == s->focus) {
      return;
    }
    i++;
  }

  // If the height of the focus  is smaller 
  // than the minimum height a window can be in the layout, return
  float size = horizontal ? s->focus->area.size.y : s->focus->area.size.x;
  if(size - s->config.layoutsize_step < s->config.layoutsize_min) {
    if(horizontal) {
      s->focus->area.size.y = s->config.layoutsize_min;
    } else {
      s->focus->area.size.x = s->config.layoutsize_min;
    }
    return;
  }

  s->ignore_enter_layout = true;

  // If the window is a master and there are more than one master 
  // or the window is a slave and there are more than one slave,
  // the clients height can be changed
  if((nmaster > 1 && ismaster) || (nslaves > 1 && !ismaster)) {
    s->focus->layoutsizeadd -= s->config.layoutsize_step;
  }

  // Recreate the layout
  makelayout(s, s->monfocus);
}

/**
 * @brief Decrements the size of the gaps between windows 
 * in layouts in the current layout.
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void decgapsizelayout(state_t* s, passthrough_data_t data) { 
  (void)data;
  uint32_t deskidx = mondesktop(s, s->monfocus)->idx;
  layout_props_t* layout = &s->monfocus->layouts[deskidx];

  if(layout->gapsize - s->config.winlayoutgap_step >= 0) {
    layout->gapsize -= s->config.winlayoutgap_step;
  }
  makelayout(s, s->monfocus);
}


/**
 * @brief Toggles the titlebar of a the focused client 
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */
inline void togglefocustitlebar(state_t* s, passthrough_data_t data) { 
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
inline void toggletitlebars(state_t* s, passthrough_data_t data) { 
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
inline void movefocusup(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){pos.x,
    MIN(MAX(pos.y - s->config.keywinmove_step, s->monfocus->area.pos.y), 
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
inline void movefocusdown(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){pos.x,
    MIN(MAX(pos.y + s->config.keywinmove_step, s->monfocus->area.pos.y), 
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
inline void movefocusleft(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){MIN(MAX(pos.x - s->config.keywinmove_step, s->monfocus->area.pos.x), 
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
inline void movefocusright(state_t* s, passthrough_data_t data) { 
  (void)data;
  if(!s->focus->floating) return;

  v2_t pos = s->focus->area.pos;
  v2_t dest = (v2_t){MIN(MAX(pos.x + s->config.keywinmove_step, s->monfocus->area.pos.x), 
      s->monfocus->area.pos.x + s->monfocus->area.size.x 
      - s->focus->area.size.x), pos.y};
  moveclient(s, s->focus, dest);
  s->ignore_enter_layout = true;
}



/**
 * @brief Cycles the currently focused client one monitor 
 * down (to the left).
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */ 

inline void cyclefocusmonitordown(state_t* s, passthrough_data_t data) {
  (void)data;
  if(!s->focus) return;
  
  s->ignore_enter_layout = true;

  monitor_t* prevmon = NULL;
  {
    monitor_t* temp = s->monitors;

    while (temp && temp->next != s->focus->mon) {
      temp = temp->next;
    }

    if (temp && temp->next == s->focus->mon) {
      prevmon = temp;
    }
  }

  if(!prevmon) return;

  area_t afocusmon = s->focus->mon->area;

  bool fs = s->focus->fullscreen;
  bool floating = s->focus->floating;
  if(fs) {
    setfullscreen(s, s->focus, false);
  }
  
  // Unset fullscreen for all clients on the 
  // previous monitor
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, prevmon)) {
      if(cl->fullscreen) {
        setfullscreen(s, cl, false);
        cl->floating = floating;
      }
    }
  }

  // Moving
  {
    v2_t relpos;
    relpos.x = s->focus->area.pos.x - afocusmon.pos.x;
    relpos.y = s->focus->area.pos.y - afocusmon.pos.y;

    float normx = relpos.x / afocusmon.size.x; 
    float normy = relpos.y / afocusmon.size.y; 

    v2_t dest;
    dest.x = prevmon->area.pos.x + (normx * prevmon->area.size.x); 
    dest.y = prevmon->area.pos.y + (normy * prevmon->area.size.y); 

    removefromlayout(s, s->focus);
    makelayout(s, s->focus->mon);
    s->focus->floating = floating;

    moveclient(s, s->focus, dest);
  }

  // Resizing
  {
    float scalex = prevmon->area.size.x / afocusmon.size.x;
    float scaley = prevmon->area.size.y / afocusmon.size.y;

    v2_t dest;
    dest.x = s->focus->area.size.x * scalex;
    dest.y = s->focus->area.size.y * scaley;

    resizeclient(s, s->focus, dest);
  }

  makelayout(s, prevmon);

  if(fs) {
    setfullscreen(s, s->focus, true);
  }


  s->monfocus = cursormon(s);
  s->focus->mon = prevmon;
}

/**
 * @brief Cycles the currently focused client one monitor 
 * down (to the left).
 *
 * @param s The window manager's state
 * @param data The data to use for the function (unused here)
 */ 

inline void cyclefocusmonitorup(state_t* s, passthrough_data_t data) {
  (void)data;
  if(!s->focus) return;

  s->ignore_enter_layout = true;

  monitor_t* nextmon = s->focus->mon->next;

  if(!nextmon) return;

  area_t afocusmon = s->focus->mon->area;

  bool fs = s->focus->fullscreen;
  bool floating = s->focus->floating;

  if(fs) {
    setfullscreen(s, s->focus, false);
  }
 
  // Unset fullscreen for all clients on the 
  // next monitor
  for(client_t* cl = s->clients; cl != NULL; cl = cl->next) {
    if(clientonscreen(s, cl, nextmon)) {
      if(cl->fullscreen) {
        setfullscreen(s, cl, false);
        cl->floating = floating;
      }
    }
  }

  // Moving
  {
    v2_t relpos;
    relpos.x = s->focus->area.pos.x - afocusmon.pos.x;
    relpos.y = s->focus->area.pos.y - afocusmon.pos.y;

    float normx = relpos.x / afocusmon.size.x; 
    float normy = relpos.y / afocusmon.size.y; 

    v2_t dest;
    dest.x = nextmon->area.pos.x + (normx * nextmon->area.size.x); 
    dest.y = nextmon->area.pos.y + (normy * nextmon->area.size.y); 

    removefromlayout(s, s->focus);
    makelayout(s, s->focus->mon);
    s->focus->floating = floating;

    moveclient(s, s->focus, dest);
  }
  // Resizing
  {
    float scalex = nextmon->area.size.x / afocusmon.size.x;
    float scaley = nextmon->area.size.y / afocusmon.size.y;

    v2_t dest;
    dest.x = s->focus->area.size.x * scalex;
    dest.y = s->focus->area.size.y * scaley;

    resizeclient(s, s->focus, dest);
  }

  makelayout(s, nextmon);

  if(fs) {
    setfullscreen(s, s->focus, true);
  }

  s->monfocus = cursormon(s);
  s->focus->mon = nextmon;
}

inline void togglescratchpad(state_t* s, passthrough_data_t data) {
  if(s->scratchpads[data.i].needs_restart) {
    s->mapping_scratchpad_index = data.i;
    runcmd(s, data);
    return;
  }

  client_t* cl = clientfromwin(s, s->scratchpads[data.i].win);

  if(s->scratchpads[data.i].hidden) {
    showclient(s, cl);
    raiseclient(s, cl);
  } else {
    hideclient(s, cl);
  }

  s->scratchpads[data.i].hidden = !s->scratchpads[data.i].hidden;
}
