all: anitrack
anitrack: main.c
	cc -std=c99 -Wall -Wextra -Wpedantic -Werror -g3 \
	   -fsanitize=address,undefined \
	   -fno-omit-frame-pointer \
	   main.c -o anitrack
