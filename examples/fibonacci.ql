import std;
import std/conv;
import std/io;

void main() {
    // std::args is type std::Array<std::String>
    // it contains the arguments passed in to the program (ie. argc, argv in c)
    let args = std::args;

    // parse cli arg as uint8
    if args.length != 2 {
        let prog = args.data[0];
        CRASH `Usage: {prog} [integer]`;
    }
    let res = conv::parse_uint8(args.data[1]);

    let n = std::assert_ok<uint8>(res);

    foreach i in 0..n {
        io::println(`fib({i}) = {nth_fib(i)}`);
    }
}

uint64 nth_fib(uint8 n) {
    if n <= 1 {
        return 1;
    }

    let mut a = 1;
    let mut b = 2;
    let mut tmp = a;

    foreach i in 2..n {
        tmp = a;
        a = b;
        b = tmp + a;
    }

    return b;
}
