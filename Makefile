CC = cc

# includes and flags
CFLAGS = -O3 -ffast-math -Wall -Wextra -pedantic
LIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lX11 -lX11-xcb -lGL -lleif -lclipboard -lm

SRC = ragnar.c config.h keycallbacks.h 
OBJ = ${SRC:.c=.o}

all: ragnar

print_options:
	@echo ragnar build options:
	@echo "CFLAGS = ${CFLAGS}"
	@echo "LIBS   = ${LIBS}"
	@echo "INCS   = ${INCS}"
	@echo "CC     = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} ${LIBS} ${INCS} $<

ragnar: ${OBJ}
	${CC} -o $@ ${OBJ} ${LIBS} ${INCS}

install: ragnar
	sudo cp -f ragnar /usr/bin
	sudo cp -f ragnar.desktop /usr/share/applications
	sudo cp -f ragnarstart /usr/bin
	sudo mkdir -p /usr/share/ragnarwm
	sudo cp -r ./fonts/ /usr/share/ragnarwm
	sudo cp -r ./icons/ /usr/share/ragnarwm
	sudo chmod 755 /usr/bin/ragnar

clean:
	rm -f ragnar ragnar.o 

uninstall:
	sudo rm -f /usr/bin/ragnar
	sudo rm -f /usr/share/applications/ragnar.desktop
	sudo rm -f /usr/bin/ragnarstart
	sudo rm -r /usr/share/ragnarwm
