CC = cc

# includes and flags
CFLAGS = -O3 -ffast-math -Wall -Wextra -pedantic
LIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lxcb-composite -lxcb-ewmh -lX11 -lX11-xcb -lGL -lleif -lclipboard -lm 

SRC = ragnar.c
OBJ = ${SRC:.c=.o}

all: ragnar print_options 

print_options:
	@echo ragnar build options:
	@echo "CFLAGS = ${CFLAGS}"
	@echo "LIBS   = ${LIBS}"
	@echo "INCS   = ${INCS}"
	@echo "CC     = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} ${LIBS} ${INCS} $<

${OBJ}: 

ragnar: ${OBJ}
	${CC} -o $@ ${OBJ} ${LIBS} ${INCS}

install:
	cp -f ragnar /usr/bin
	mkdir -p /usr/share/xsessions
	cp -f ragnar.desktop /usr/share/xsessions
	cp -f ragnarstart /usr/bin
	mkdir -p /usr/share/ragnarwm
	cp -r ./fonts/ /usr/share/ragnarwm
	cp -r ./icons/ /usr/share/ragnarwm
	chmod 755 /usr/bin/ragnar

clean:
	rm -f ragnar ${OBJ}

uninstall:
	rm -f /usr/bin/ragnar
	rm -f /usr/share/xsessions/ragnar.desktop
	rm -f /usr/bin/ragnarstart

.PHONY: all print_options clean install uninstall freetype
