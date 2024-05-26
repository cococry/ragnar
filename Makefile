CC = cc
MACROS = -DWLR_USE_UNSTABLE
CFLAGS = -Wall -Wextra `pkg-config --cflags pixman-1 wlroots --libs wlroots`
LIBS = -lwayland-server
INCS = -Isrc
SRC = ./src/*.c
BIN_NAME = tica

all: build

build:
	${CC} -o ${BIN_NAME} ${SRC} ${CFLAGS} ${MACROS} ${LIBS}  ${INCS}

clean:
		rm -f ${BIN_NAME}

install:
	cp ${BIN_NAME} /usr/bin/
