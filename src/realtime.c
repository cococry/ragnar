// SCHED_RESET_ON_FORK
#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <string.h>

#include "structs.h"
#include "funcs.h"

/**
 * @brief Raises the window manager's event loop to real-time
 * round-robin scheduling.
 *
 * Ragnar is blocked in xcb_wait_for_event() almost all of the time and
 * only wakes on input or window changes, so what it competes for is
 * wake-up latency, not throughput. Under load a SCHED_OTHER process can
 * wait milliseconds for a slice, which is felt as late keybinds and
 * stuttering interactive moves and resizes.
 *
 * This does nothing for the framerate of clients. Under X11 the process
 * in the path of every frame is the X server, not the window manager.
 *
 * SCHED_RESET_ON_FORK is what keeps the priority contained: the kernel
 * puts every child back on SCHED_OTHER at nice 0 across fork, so the
 * terminals and browsers started by runcmd keybinds never inherit
 * real-time priority. sway does the same containment with a
 * pthread_atfork handler; the kernel flag needs no handler and holds
 * across exec.
 *
 * pid 0 is the calling thread, so the IPC thread started in setup()
 * stays on SCHED_OTHER.
 *
 * Needs CAP_SYS_NICE or a nonzero RLIMIT_RTPRIO. Failing is not fatal,
 * ragnar carries on at normal priority.
 *
 * @param s The window manager's state
 */
void
setrrscheduling(state_t* s) {
  int32_t prio = sched_get_priority_min(SCHED_RR);
  if(prio == -1) {
    logmsg(s, LogLevelWarn, "no SCHED_RR priority range: %s",
           strerror(errno));
    return;
  }

  struct sched_param param;
  memset(&param, 0, sizeof(param));
  param.sched_priority = prio;

  if(sched_setscheduler(0, SCHED_RR | SCHED_RESET_ON_FORK, &param) != 0) {
    logmsg(s, LogLevelWarn,
           "could not set SCHED_RR priority %i: %s "
           "(needs cap_sys_nice or RLIMIT_RTPRIO)", prio, strerror(errno));
    return;
  }

  logmsg(s, LogLevelTrace, "event loop running at SCHED_RR priority %i.",
         prio);
}
