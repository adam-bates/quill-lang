import std;
import std/conv;
import std/ds;
import std/io;

void main() {
    let args = std::args;

    if args.length != 2 {
        CRASH "Usage: fizzbuzz [integer]";
    }
    let n_res = conv::parse_uint(args.data[1]);

    uint n = std::assert_ok<uint>(n_res);

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
