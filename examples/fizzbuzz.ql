import std::*;

int main(String[] args) {
    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [number]");
        return 1;
    }

    Result<uint> res = parse_uint(args[1]);
    if !res {
        CRASH `Error parsing as uint: {res.err}`;
    }
    uint n = res.val;

    for i in 1..=n {
        let mut match = false;

        if i % 3 == 0 { io::printf("Fizz"); match = true; }
        if i % 5 == 0 { io::printf("Buzz"); match = true; }

        if !match { io::printf("%d", i); }

        io::println();
    }

    return 0;
}
