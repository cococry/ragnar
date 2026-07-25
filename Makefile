CC = cc
CFLAGS ?= -O3 -ffast-math
ALL_CFLAGS = $(CFLAGS) -Wall -Wextra -pedantic -isystem api/include

LDLIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lxcb-xfixes -lX11 -lX11-xcb -lGL -lm -lconfig -lxcb-util

SRC = ./src/*.c ./src/ipc/*.c
BIN = ragnar

PREFIX = /usr
BINDIR = $(PREFIX)/bin
SYSCONFDIR = /etc

.PHONY: all
all:
	mkdir -p ./bin
	$(CC) -o bin/$(BIN) $(ALL_CFLAGS) $(SRC) $(LDLIBS)

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
	install -Dm644 ragnar.desktop -t $(DESTDIR)$(PREFIX)/share/xsessions
	install -Dm644 cfg/ragnar.cfg -t $(DESTDIR)$(SYSCONFDIR)/ragnarwm

.PHONY: clean
clean:
	$(RM) bin/*

.PHONY: uninstall
uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(BIN)
	$(RM) $(DESTDIR)$(PREFIX)/share/xsessions/ragnar.desktop
	$(RM) -r $(DESTDIR)$(SYSCONFDIR)/ragnarwm
