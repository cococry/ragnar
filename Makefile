CC = cc
MACROS = -DWLR_USE_UNSTABLE
CFLAGS = -Wall -Wextra `pkg-config --cflags pixman-1 wlroots --libs wlroots`
LIBS = -lwayland-server -lxkbcommon -lm
INCS = -Isrc
SRC = ./src/*.c
BIN_NAME = ragnar 

all: build

build:
	${CC} -o ${BIN_NAME} ${SRC} ${LIBS} ${CFLAGS} ${MACROS} ${INCS}

clean:
		rm -f ${BIN_NAME}

install:
	cp ${BIN_NAME} /usr/bin/
