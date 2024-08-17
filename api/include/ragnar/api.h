#pragma once 

#include <stdbool.h>
#include <stdint.h>

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

