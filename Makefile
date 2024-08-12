CC 			= cc
CFLAGS 	= -O3 -ffast-math -Wall -Wextra -pedantic
LIBS 		= -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lX11 -lX11-xcb -lGL -lleif -lclipboard -lm -lconfig
SRC 		= ./src/*.c
BIN 		= ragnar

all:
	mkdir -p ./bin
	${CC} -o bin/${BIN} ${SRC} ${LIBS} ${CFLAGS}
	@if [ -f "~/.config/ragnarwm/" ]; then \
		mkdir -p ~/.config/ragnarwm; \
		cp ./cfg/ragnar.cfg ~/.config/ragnarwm; \
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
