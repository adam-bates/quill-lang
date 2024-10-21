import std;
import std/io;
import std/ds;

import libc/stdio;
import libc/stdlib;

static let sz = sizeof(char);

void main() {
    io::println("Hello, World");

    uint len = 15;

    uint mut i = 0;
    char mut* c = stdlib::malloc(sz * len);

    c[i] = 'H'; i += 1;
    c[i] = 'e'; i += 1;
    c[i] = 'l'; i += 1;
    c[i] = 'l'; i += 1;
    c[i] = 'o'; i += 1;
    c[i] = ','; i += 1;
    c[i] = ' '; i += 1;
    c[i] = 'w'; i += 1;
    c[i] = 'o'; i += 1;
    c[i] = 'r'; i += 1;
    c[i] = 'l'; i += 1;
    c[i] = 'd'; i += 1;
    c[i] = '!'; i += 1;
    c[i] = '\n'; i += 1;
    c[i] = '\0';

    // prints "Hello, world!" again
    stdio::fwrite(c, sz, len, stdio::stdout);
    stdlib::free(c);

    char n = '2';
    std::String str = "3, 4";
    uint number = 1337;

    // prints: "1, 2, 3, 4, 5 ... 1337 :)"
    io::println(`1, {n}, {str}, 5 ... {number} :)`);

    // prints "-> [-42] <-"
    f();

    bool x = false;
    while x {
        CRASH "unreachable";
    }
}

void f() {
    int foo = -42;
    io::println(`-> [{foo}] <-`);
}
