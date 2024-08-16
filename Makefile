CC 			= cc
CFLAGS 	= -O3 -ffast-math -Wall -Wextra -pedantic
LIBS 		= -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lX11 -lX11-xcb -lGL -lleif -lclipboard -lm -lconfig
SRC 		= ./src/*.c ./src/ipc/*.c
BIN 		= ragnar

all:
	mkdir -p ./bin
	${CC} -o bin/${BIN} ${SRC} ${LIBS} ${CFLAGS}

SOURCE_DIR := ./cfg/ 
DEST_DIR := $(HOME)/.config/ragnarwm
CONFIG_FILE := $(DEST_DIR)/ragnar.cfg

config:
	@if [ ! -f "$(CONFIG_FILE)" ]; then \
		echo "Config file does not exist. Copying default config..."; \
		mkdir -p "$(DEST_DIR)"; \
		cp -r "./cfg/ragnar.cfg" "$(DEST_DIR)"; \
	else \
		echo "Config file already exists. Skipping copy."; \
	fi

install: 
	sudo cp -f bin/${BIN} /usr/bin
	sudo cp -f ragnar.desktop /usr/share/applications
	sudo cp -f ragnarstart /usr/bin
	sudo mkdir -p /usr/share/ragnarwm
	sudo cp -r ./fonts/ /usr/share/ragnarwm
	sudo cp -r ./icons/ /usr/share/ragnarwm
	sudo chmod 755 /usr/bin/ragnar

clean:
	rm -f bin/*

uninstall:
	sudo rm -f /usr/bin/ragnar
	sudo rm -f /usr/share/applications/ragnar.desktop
	sudo rm -f /usr/bin/ragnarstart
	sudo rm -r /usr/share/ragnarwm
