CC=gcc
CFLAGS=
all: lib/ragnar.a
lib/ragnar.a: lib/api.o
	ar cr lib/libragnar.a lib/*.o
lib/api.o: lib
	${CC} -c api.c -o lib/api.o ${CFLAGS}

lib:
	mkdir lib
clean:
	rm -r ./lib

install: all
	cp lib/libragnar.a /usr/local/lib/ 
	cp -r include/ragnar /usr/local/include

uninstall:
	rm -f /usr/local/lib/libragnar.a
	rm -rf /usr/local/include/ragnar/

.PHONY: all test clean
