install:
	gcc xragnar.c -o xragnar -lX11 -lXcursor -O3 -ffast-math -Wall -Wextra
	cp xragnar /usr/bin/xragnar
clean:
	rm -f xragnar
	rm -f /usr/bin/xragnar
