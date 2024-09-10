import std::*;

int main(String[] args) {
    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [number]");
        return 1;
    }

    uint n = parse_uint(args[1]) catch err {
        CRASH `Error parsing uint: {err}`;
    };

    for i in 1..=n {
        let mut match = false;

        if i % 3 == 0 { io::printf("Fizz"); match = true; }
        if i % 5 == 0 { io::printf("Buzz"); match = true; }

        if !match { io::printf("%d", i); }

        io::println();
    }

    return 0;
}
