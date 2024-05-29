WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)
LIBS=-lwayland-server `pkg-config --cflags --libs xkbcommon wlroots-0.18`

xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

ragnar: ragnar.c xdg-shell-protocol.h
	$(CC) $(CFLAGS) -g -Werror -I. -DWLR_USE_UNSTABLE -o $@ $< $(LIBS)

clean:
	rm -f ragnar xdg-shell-protocol.h xdg-shell-protocol.c

.DEFAULT_GOAL=ragnar
.PHONY: clean
