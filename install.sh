#!/usr/bin/env bash
# api/ only needs building when you iterate on the IPC client lib:
#   make -C api && sudo make -C api install
set -e
# we also want this to work regardless if the user is root or noot
as_root() {
      if [ "$(id -u)" -eq 0 ]; then
              "$@"
      elif esc=$(command -v sudo) || esc=$(command -v doas); then
              "$esc" "$@"
      else
              echo "no sudo or doas, trying su" >&2
              su root -c "$*"
      fi
}

make
as_root make install

# lets the event loop ask for SCHED_RR, which is about input latency under
# load, not client framerate. optional: ragnar logs a warning and runs at
# normal priority without it. skipped when libcap is not installed.
if command -v setcap >/dev/null; then
        as_root setcap cap_sys_nice=ep /usr/bin/ragnar ||
                echo "setcap failed, ragnar will run at normal priority" >&2
else
        echo "no setcap (libcap), ragnar will run at normal priority" >&2
fi

# we do not need the make config anymore; logic unified.
echo "Successfully installed ragnarwm."
