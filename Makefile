CC = cc
# ?= so an exported CFLAGS (makepkg, distro build) replaces this wholesale.
# geometry here is ratios and pixel rects cast back to ints, so -ffast-math
# bought nothing and -ffinite-math-only would let divisions by a zero-sized
# monitor fold away instead of showing up.
CFLAGS ?= -O2
WARN_CFLAGS = -Wall -Wextra -pedantic -isystem api/include
ALL_CFLAGS = $(CFLAGS) $(CPPFLAGS) $(WARN_CFLAGS)

# -O1 : for readable ASAN traces. Separate from CFLAGS because that is ?=
# and a distro build must not pick these up.
DEBUG_CFLAGS = -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined
ALL_DEBUG_CFLAGS = $(DEBUG_CFLAGS) $(CPPFLAGS) $(WARN_CFLAGS)

LDLIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-xfixes -lX11 -lX11-xcb -lconfig

SRC = ./src/*.c ./src/ipc/*.c
BIN = ragnar

PREFIX = /usr
BINDIR = $(PREFIX)/bin
SYSCONFDIR = /etc

.PHONY: all
all:
	mkdir -p ./bin
	$(CC) -o bin/$(BIN) $(ALL_CFLAGS) $(LDFLAGS) $(SRC) $(LDLIBS)

# ASan + UBSan + LSan build, overwrites bin/ragnar. LSan only reports on a
# normal exit, so quit ragnar through its own keybind: a SIGKILL prints
# nothing. .PHONY matters here, the ./debug Xephyr script shares the name
# and make would otherwise call the target up to date.
#   make debug && ./debug
.PHONY: debug
debug:
	mkdir -p ./bin
	$(CC) -o bin/$(BIN) $(ALL_DEBUG_CFLAGS) $(LDFLAGS) $(SRC) $(LDLIBS)

# client-side IPC library. the WM links none of it, only the headers under
# api/include are needed to build. opt-in, for writing external clients.
.PHONY: api
api:
	$(MAKE) -C api

.PHONY: install-api
install-api:
	$(MAKE) -C api install

# no user config target: ragnar reads $HOME/.config/ragnarwm/ragnar.cfg first
# and falls back to the copy installed below. an override is a plain cp, and
# writing $HOME from a root install lands in the wrong home.
# mirrors PKGBUILD logic, and is "standard practice" for config files.
.PHONY: install
install:
	install -Dm755 bin/$(BIN) -t $(DESTDIR)$(BINDIR)
	install -Dm644 cfg/ragnar.cfg -t $(DESTDIR)$(SYSCONFDIR)/ragnarwm

# the shipped cfg is the only fallback a fresh install has: readconfig
# terminates when neither the user nor the global path parses, so a syntax
# error here means a fresh install cannot start at all.
.PHONY: check
check:
	mkdir -p ./bin
	$(CC) -o bin/config_check $(ALL_CFLAGS) $(LDFLAGS) cfg/config_check.c -lconfig
	./bin/config_check cfg/ragnar.cfg

# makepkg's $srcdir defaults to ./src, which collides with this project's
# own src/. build out of tree so the clone never lands in the worktree.
BUILDDIR ?= /tmp/makepkg
SRCDEST  ?= $(HOME)/.cache/makepkg/sources

.PHONY: package
package:
	BUILDDIR=$(BUILDDIR) SRCDEST=$(SRCDEST) makepkg -f

.PHONY: clean
clean:
	$(RM) bin/*

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(BIN)
	$(RM) -r $(DESTDIR)$(SYSCONFDIR)/ragnarwm
