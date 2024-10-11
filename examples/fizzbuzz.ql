import std::*;
import std/conv;
import std/ds;
import std/io;

void main() {
    Array<String> args = std::args;

    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [integer]");
        std::exit(1);
    }
    String n_str = cstr_to_str(args.data[1]);

    Result<uint> res = conv::parse_uint(n_str);
    if !res.is_ok {
        CRASH `Error parser integer: {res.err}`;
    }
    uint n = res.val;

    ds::StringBuffer mut out = ds::strbuf_create();
    foreach i in 1..=n {
        defer ds::strbuf_reset(&out);
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(strbuf_as_str(out));
    }
}
