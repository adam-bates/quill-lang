# Quill Programming Langauge

Iteration on c. Aiming for quality-of-life improvements, keeping a lean language, and removing c weirdness.

Some notable differences from c:
- Data is constant & immutable by default. Mutable data must be marked as `mut`.
- Namespacing: `std::io::printf("Hello, world!");`
- Generics: `HashTable<String, int>`
- Type-inferencing: `let x = true; // x is a bool`
- for-each loops: `for n in 0..10 { }`
- break-data turns blocks into expressions: `int x = { break 1; };`
- Standard library supplies fat strings, fat arrays, optionals, result types, and much more.
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

int main(Array<String> args) {
    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [integer]");
        return 1;
    }
    String n_str = args.data[1];

    uint n = conv::parse_uint(n_str) catch err {
        CRASH `Error parsing integer: {err}`;
    };

    ds::StringBuffer mut out = ds::strbuf_create();
    for i in 1..=n {
        defer ds::strbuf_reset(&out);
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(strbuf_as_str(out));
    }

    return 0;
}
```

## Why does this exist?

For fun, learning, and because it's the langauge I want to use.

It will probably never be ready for you or others to use, and that's ok. If you like what you see here, I recommend checking out Zig and C3. Both of which also attempt to iterate on C with modern features, a lean grammar, and still allowing full control of your program.

## Using the language

For now, there is only a compiler which handles single files. I'll improve the tooling later.

Download this repo, then build the compiler using:
```
make
```

You can run the repo's tests using:
```
make test
```

Now, assuming you have a file called `my_program.ql`, you can compile it using:
```
.bin/quillc my_program.ql
```
