import std::*;

ExitCode main(String[] args) {
    if args.length != 3 {
        io::eprintln("Usage: fibonacci [slow|fast] [integer]");
        return ExitCode::FAILURE;
    }

    let speed = switch str_to_lower(args.data[1]) {
        case "slow" { break Speed::SLOW; }
        case "fast" { break Speed::FAST; }

        else {
            io::eprintln("Usage: fibonacci [slow|fast] [integer]");
            return ExitCode::FAILURE;
        }
    };

    uint8 n = parse_uint8(args.data[2]) catch err {
        CRASH `Error parsing integer: {err}`;
    };

    uint64(uint8)* nth_fib_fnptr = switch speed {
        case Speed::SLOW { break nth_fib_slow; }
        case Speed::FAST { break nth_fib_fast; }
    };

    for i in 0..n {
        io::printf("fib(%d) = %d\n", i, nth_fib_fnptr(i));
    }

    return ExitCode::SUCCESS;
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

    for _ in 2..n {
        tmp = a;
        a = b;
        b = tmp + a;
    }

    return b;
}
