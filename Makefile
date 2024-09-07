build:
	mkdir -p ./.bin
	gcc -std=c99 -Wall -Wextra -pedantic ./src/*.c -o ./.bin/snowy

run: build
	./.bin/snowy input.sny

clean:
	rm -rf ./.bin

