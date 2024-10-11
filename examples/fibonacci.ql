import std;
import std/conv;
import std/io;

void main() {
    Array<String> args = std::args;

    if args.length != 3 {
        io::eprintln("Usage: fibonacci [slow|fast] [integer]");
        std::exit(std::ExitCode::FAILURE);
        return;
    }

    Speed speed;
    switch str_to_lower(args.data[1]) {
        case "slow" { speed = Speed::SLOW; }
        case "fast" { speed = Speed::FAST; }

        else {
            io::eprintln("Usage: fibonacci [slow|fast] [integer]");
            std::exit(1);
            return;
        }
    };

    Result<uint8> res = conv::parse_uint8(args.data[2]);
    if !res.is_ok {
        CRASH `Error parsing integer: {res.err}`;
    }
    uint8 n = res.val;

    uint64(uint8)* nth_fib_fnptr = null;
    switch speed {
        case Speed::SLOW { nth_fib_fnptr = nth_fib_slow; }
        case Speed::FAST { nth_fib_fnptr = nth_fib_fast; }
    };

    foreach i in 0..n {
        io::printf("fib(%d) = %d\n", i, nth_fib_fnptr(i));
    }
}

enum Speed {
    SLOW,
    FAST,
}

uint64 nth_fib_slow(uint8 n) {
    if n <= 1 { return 1; }

    return nth_fib_slow(n - 1) + nth_fib_slow(n - 2);
}

uint64 nth_fib_fast(uint8 n) {
    if n <= 1 { return 1; }

    uint64 mut a = 1;
    uint64 mut b = 2;
    uint64 mut tmp = a;

    for (uint8 i = 2; i < n; ++i) {
        tmp = a;
        a = b;
        b = tmp + a;
    }

    return b;
}
