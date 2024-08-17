CC = cc
CFLAGS = -O3 -ffast-math -Wall -Wextra -pedantic
CFLAGS += -isystem api/include

LDLIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lX11 -lX11-xcb -lGL -lm -lconfig

SRC = ./src/*.c ./src/ipc/*.c
BIN = ragnar

RAGNAR_API = api/lib/ragnar.a

PREFIX = /usr
BINDIR = $(PREFIX)/bin

.PHONY: all
all: $(RAGNAR_API)
	mkdir -p ./bin
	$(CC) -o bin/$(BIN) $(CFLAGS) $(SRC) $(LDLIBS)

$(RAGNAR_API):
	$(MAKE) -C api

SOURCE_DIR := ./cfg/ 
DEST_DIR := $(HOME)/.config/ragnarwm
CONFIG_FILE := $(DEST_DIR)/ragnar.cfg

.PHONY: config
config:
	@if [ ! -f "$(CONFIG_FILE)" ]; then \
		echo "Config file does not exist. Copying default config..."; \
		mkdir -p "$(DEST_DIR)"; \
		cp -r "./cfg/ragnar.cfg" "$(DEST_DIR)"; \
	else \
		echo "Config file already exists. Skipping copy."; \
	fi

.PHONY: install
install: 
	install -Dm755 bin/$(BIN) -t $(BINDIR)
	install -Dm755 ragnarstart -t $(BINDIR)
	cp -f ragnar.desktop $(PREFIX)/share/xsessions/

.PHONY: clean
clean:
	$(RM) bin/*

.PHONY: uninstall
uninstall:
	$(RM) $(BINDIR)/ragnar
	$(RM) $(PREFIX)/share/xsessions/ragnar.desktop
	$(RM) $(BINDIR)/ragnarstart
