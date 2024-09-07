build: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/snowy src/bin/snowy.c src/lib/*.c

test-lexer: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/lexer_test tests/lexer.c src/lib/*.c
	.bin/lexer_test
	rm .bin/lexer_test

test: test-lexer

run: build
	.bin/snowy ./examples/hello.sny

setup:
	mkdir -p ./.bin

clean:
	rm -rf ./.bin

