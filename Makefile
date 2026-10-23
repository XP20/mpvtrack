CC = cc
PREFIX ?= $(HOME)/.local

CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
DEBUG = -g3 -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
RELEASE = -DNDEBUG -O2 -flto -march=native
SRC = main.c track.c mpv.c json.c

.PHONY: all debug release install uninstall clean
all: release
debug: $(SRC)
	$(CC) $(CFLAGS) $(DEBUG) $(SRC) -o mpvtrack
release: $(SRC)
	$(CC) $(CFLAGS) $(RELEASE) $(SRC) -o mpvtrack
install:
	install -Dm755 mpvtrack $(PREFIX)/bin/mpvtrack
uninstall:
	rm -f $(PREFIX)/bin/mpvtrack
clean:
	rm -f mpvtrack
