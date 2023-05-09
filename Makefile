install:
	gcc xragnar.c -o xragnar -lX11 -lXcursor -lXft -O3 -ffast-math -Wall -Wextra
	cp xragnar /usr/bin/xragnar
clean:
	rm -f xragnar
	rm -f /usr/bin/xragnar
bar:
	cp xragbar/* /usr/bin
