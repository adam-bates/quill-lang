build: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

build-release: setup
	gcc -std=c99 -O3 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

test-lexer: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/lexer_test tests/lexer.c src/lib/**/*.c
	.bin/lexer_test
	rm .bin/lexer_test

test: test-lexer

run: build
	mkdir -p ./.bin/tmp \
	&& .bin/quillc ./examples/hello.ql > ./.bin/tmp/main.c \
	&& cd ./.bin/tmp \
	&& gcc -std=c99 -o main ./main.c \
	&& clear \
	&& ./main \
	&& cd ../.. \

run-libc: build
	.bin/quillc ./brainstorm/libc/stdio.ql

setup:
	mkdir -p ./.bin

clean:
	rm -rf ./.bin
