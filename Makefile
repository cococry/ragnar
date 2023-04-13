install:
	gcc xragnar.c -o xragnar -lX11
	cp xragnar /usr/bin/xragnar
clean:
	rm -f xragnar
	rm -f /usr/bin/xragnar
