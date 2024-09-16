build: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

release: setup
	gcc -std=c99 -O3 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

test-lexer: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/lexer_test tests/lexer.c src/lib/**/*.c
	.bin/lexer_test
	rm .bin/lexer_test

test: test-lexer

run: build
	.bin/quillc ./examples/hello.ql

setup:
	mkdir -p ./.bin

clean:
	rm -rf ./.bin
