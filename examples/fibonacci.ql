import std;
import std/conv;
import std/io;

void main() {
    let args = std::args;

    if args.length != 2 {
        CRASH "Usage: fibonacci [integer]";
    }

    let res = conv::parse_uint8(args.data[1]);
    if !res.is_ok {
        CRASH `Error parsing integer: {res.err}`;
    }
    uint8 n = res.val;

    foreach i in 0..n {
        io::println(`fib({i}) = {nth_fib(i)}`);
    }
}

uint64 nth_fib(uint8 n) {
    if n <= 1 {
        return 1;
    }

    uint64 mut a = 1;
    uint64 mut b = 2;
    uint64 mut tmp = a;

    foreach i in 2..n {
        tmp = a;
        a = b;
        b = tmp + a;
    }

    return b;
}
