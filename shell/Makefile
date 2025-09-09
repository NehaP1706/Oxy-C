all:
	gcc -Iinclude -std=c99 \
	-D_POSIX_C_SOURCE=200809L \
	-D_XOPEN_SOURCE=700 -Wall \
	-Wextra -Werror -Wno-unused-parameter \
	-fno-asm \
	src/util.c src/atomic.c src/bg.c src/display.c src/hop.c src/log.c src/parse.c src/pipe.c src/reveal.c src/sequential.c src/signal.c src/token.c src/shell.c -o shell.out

clean:
	rm shell.out *.txt