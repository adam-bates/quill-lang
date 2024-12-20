import std;
import std/conv;
import std/ds;
import std/io;

void main() {
    let args = std::args;

    if args.length != 2 {
        let prog = args.data[0];
        CRASH `Usage: {prog} [integer]`;
    }

    std::Result<uint> res = conv::parse_uint(args.data[1]);

    // instead of std::assert_ok<uint>res)
    if !res.is_ok {
        CRASH `Error parsing integer: {res.err}`;
    }
    uint n = res.val;

    // each iteration reuses a std/ds::StringBuffer
    // that way building the string has minimal allocations
    let mut out = ds::strbuf_default();
    foreach i in 1..=n {
        defer ds::strbuf_reset(&out); // maintains capacity
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(ds::strbuf_as_str(out));
    }
}
