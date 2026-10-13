all: debug

debug: main.c track.c mpv.c
	cc -std=c99 -g3 \
	   -Wall -Wextra -Wpedantic -Werror -Wshadow -Wformat=2 \
	   -fsanitize=address,undefined \
	   -fno-omit-frame-pointer \
	   main.c track.c mpv.c -o mpvtrack
release: main.c track.c mpv.c
	cc -std=c99 -DNDEBUG -O2 -flto -march=native \
	   -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 \
	   main.c track.c mpv.c -o mpvtrack
