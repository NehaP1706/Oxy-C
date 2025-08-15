run:
	gcc -Iinclude -std=c99 \
	-D_POSIX_C_SOURCE=200809L \
	-D_XOPEN_SOURCE=700 -Wall \
	-Wextra -Werror -Wno-unused-parameter \
	-fno-asm \
	include/shell.h src/functions.c src/shell.c -o run.out
	gcc src/run.c -o shell.out