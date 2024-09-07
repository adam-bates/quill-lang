setup:
	mkdir -p ./.bin

build: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/snowy src/bin/snowy.c src/lib/*.c

test-scanner: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/scanner_test tests/scanner.c src/lib/*.c
	.bin/scanner_test
	rm .bin/scanner_test

test: test-scanner

run: build
	.bin/snowy ./examples/hello.sny

clean:
	rm -rf ./.bin

