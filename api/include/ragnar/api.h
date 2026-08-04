#pragma once 

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * IPC socket path, computed rather than fixed. XDG_RUNTIME_DIR scopes it
 * per user and DISPLAY per X server: one hardcoded path meant a second
 * instance (a nested Xephyr session, a second seat) unlinked the running
 * WM's socket out from under it at startup
 *
 * static inline in the public header on purpose. The WM links none of
 * api.c and only includes these headers, so this is the single place both
 * sides can share the path and not drift apart
 */
static inline void
rg_socket_path(char* buf, size_t len) {
  const char* dir = getenv("XDG_RUNTIME_DIR");
  if(!dir) {
    dir = "/tmp";
  }
  const char* display = getenv("DISPLAY");
  if(!display) {
    display = ":0";
  }
  // DISPLAY is ":1" or ":0.0"; drop the leading colon so the result reads
  // as ragnar-1.socket rather than ragnar-:1.socket
  if(*display == ':') {
    display++;
  }
  snprintf(buf, len, "%s/ragnar-%s.socket", dir, display);
}

typedef int32_t RgWindow;

#define RG_INVALID_WINDOW -1

typedef enum {
  RgCommandTerminate,
  RgCommandGetWindows,
  RgCommandKillWindow,
  RgCommandFocusWindow,
  RgCommandNextWindow,
  RgCommandFirstWindow,
  RgCommandGetFocus,
  RgCommandGetMonitorFocus,
  RgCommandGetCursor,
  RgCommandGetWindowArea,
  RgCommandReloadConfig,
  RgCommandSwitchDesktop,
} RgCommandType;

typedef struct {
  float x, y;
} Rgv2;

typedef struct {
  Rgv2 pos, size;
} RgArea;

void rg_set_trace_logging(bool logging);

int32_t rg_cmd_terminate(uint32_t exitcode);

int32_t rg_cmd_get_windows(RgWindow** wins, uint32_t* numwins);

int32_t rg_cmd_kill_window(RgWindow win);

int32_t rg_cmd_focus_window(RgWindow win);

int32_t rg_cmd_next_window(RgWindow win, RgWindow* next);

int32_t rg_cmd_first_window(RgWindow* first);

int32_t rg_cmd_get_focus(RgWindow* focus);

int32_t rg_cmd_get_monitor_focus(int32_t* idx);

int32_t rg_cmd_get_cursor(Rgv2* cursor);

int32_t rg_cmd_get_window_area(RgWindow win, RgArea* area);

int32_t rg_cmd_reload_config(void);

int32_t rg_cmd_switch_desktop(uint32_t desktop_id);

