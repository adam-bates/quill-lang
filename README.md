# Quill Programming Langauge

Simplified c, with some modern syntax, and a couple of modern features.

Notable differences from c:
- Data is constant & immutable by default. Mutable data must be marked as `mut`.
- Namespacing: `std::io::printf("Hello, world!");`
- Generics: `std::ds::ArrayList<char>`
- Type-inferencing: `let x = true; // x is a bool`
- Dedicated syntax for optional/nullable data: `int?` vs `int`
- for-each loops: `for n in 0..10 { }`
- break-data turns blocks into expressions: `int x = { break 1; };`
- Strings & Arrays encode the length. c-style strings and arrays can only be used as pointers.
- No macros, no metaprogramming. Sorry, not sorry!

## Examples

### Hello world
```c
import std::io;

void main() {
    io::println("Hello, world!");
}
```

### Fizzbuzz
```c
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
```

## Why does this exist?

It's the langauge I want to write code in.
