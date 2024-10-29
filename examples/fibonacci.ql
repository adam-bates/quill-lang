import std;
import std/conv;
import std/io;

void main() {
    let args = std::args;

    if args.length != 2 {
        CRASH "Usage: fibonacci [integer]";
    }

    let res = conv::parse_uint(args.data[1]);
    if !res.is_ok {
        CRASH `Error parsing integer: {res.err}`;
    }
    uint n = res.val;

    foreach i in 0..n {
        io::println(`fib({i}) = {nth_fib(i)}`);
    }
}

// slow
/*
uint nth_fib(uint n) {
    if n <= 1 {
        uint x = 1;
        return x;
    }

    return nth_fib(n - 1) + nth_fib(n - 2);
}
*/

// fast
uint nth_fib(uint n) {
    if n <= 1 {
        uint x = 1;
        return x;
    }

    uint mut a = 1;
    uint mut b = 2;
    uint mut tmp = a;

    foreach i in 2..n {
        tmp = a;
        a = b;
        b = tmp + a;
    }

    return b;
}
