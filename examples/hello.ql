import std/io;
import std/ds;

import libc/stdio;
import libc/stdlib;

static let sz = sizeof(char);

void main() {
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

    stdio::fwrite(c, sz, len, stdio::stdout);

    stdlib::free(c);

    char n = '2';
    io::println(`1, {n}`);
}
