import std::*;
import std/conv;
import std/ds;
import std/io;

import std::{
    *,
    /conv, /ds, /io,

    ErrorType::{
        UNSPECIFIED,
    }
};

int main(Array<String> args) {
    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [integer]");
        return 1;
    }
    String n_str = args.data[1];

    uint n = conv::parse_uint(n_str) catch err {
        CRASH `Error parsing integer: {err}`;
    };

    ds::StringBuffer mut out = ds::strbuf_create();
    for i in 1..=n {
        defer {
            ds::strbuf_reset(&out);
        }
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(strbuf_as_str(out));
    }

    return 0;
}
