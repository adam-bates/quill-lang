build: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

build-release: setup
	gcc -std=c99 -O3 -Wall -Wextra -pedantic -I./src/lib -o .bin/quillc src/bin/quillc.c src/lib/**/*.c

test-lexer: setup
	gcc -std=c99 -Wall -Wextra -pedantic -I./src/lib -o .bin/lexer_test tests/lexer.c src/lib/**/*.c
	.bin/lexer_test
	rm .bin/lexer_test

test: test-lexer

example-hello: clean build
	mkdir -p ./.bin/tmp \
	&& .bin/quillc ./examples/hello.ql -D=./.bin/tmp -lstd=./runtime/std/std.ql ./runtime/std/io.ql -llibc=./runtime/libc/stdio.ql \
	&& cd ./.bin \
	&& gcc -std=c99 -o main -I./tmp ./tmp/*.c  \
	&& clear \
	&& ./main \
	&& cd ..

example-fizzbuzz: clean build
	mkdir -p ./.bin/tmp \
	&& .bin/quillc ./examples/fizzbuzz.ql -D=./.bin/tmp -lstd=./runtime/std/std.ql ./runtime/std/io.ql ./runtime/std/ds.ql ./runtime/std/conv.ql -llibc=./runtime/libc/stdio.ql \
	&& cd ./.bin \
	&& gcc -std=c99 -o main -I./tmp ./tmp/*.c  \
	&& clear \
	&& ./main \
	&& cd ..

setup:
	mkdir -p ./.bin

clean:
	rm -rf ./.bin
