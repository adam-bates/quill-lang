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
        let match = false;

        if n % 3 == 0 { io::printf("Fizz"); match = true; }
        if n % 5 == 0 { io::printf("Buzz"); match = true; }

        if !match { io::printf("%d", n); }

        io::println();
    }

    return 0;
}
