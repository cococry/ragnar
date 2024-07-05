CC = cc

# includes and flags
CFLAGS = -O3 -ffast-math -Wall -Wextra -pedantic
LIBS = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-cursor -lxcb-randr -lX11 -lX11-xcb -lGL

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
	cp -f ragnar.desktop /usr/share/applications
	cp -f ragnarstart /usr/bin
	chmod 755 /usr/bin/ragnar

clean:
	rm -f ragnar ${OBJ}

uninstall:
	rm -f /usr/bin/ragnar
	rm -f /usr/share/applications/ragnar.desktop
	rm -f /usr/bin/ragnarstart

.PHONY: all print_options clean install uninstall freetype
