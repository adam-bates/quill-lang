import std;
import std/conv;
import std/ds;
import std/io;

void main() {
    std::Array<std::String> args = std::args;

    if args.length != 2 {
        CRASH "Usage: fizzbuzz [integer]";
    }

    std::Result<uint> res = conv::parse_uint(args.data[1]);
    if !res.is_ok {
        CRASH `Error parser integer: {res.err}`;
    }
    uint n = res.val;

    let mut out = ds::strbuf_default();
    foreach i in 1..=n {
        defer ds::strbuf_reset(&out);
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(ds::strbuf_as_str(out));
    }
}
