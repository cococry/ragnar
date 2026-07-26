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

# we do not need the make config anymore; logic unified.
echo "Successfully installed ragnarwm."
