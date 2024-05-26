#include "def.h"
#include <stdlib.h>
#include <wlr/util/log.h>

void terminate(const char* msg) {
  wlr_log(WLR_ERROR, "%s | Terminating.", msg);
  exit(1);
} 
