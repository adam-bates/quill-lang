# Quill Programming Langauge
Note: the compiler is a work-in-progress. Not everything is implemented yet.

Iteration on c. Aiming for quality-of-life improvements, keeping a lean language, and removing c weirdness.

Some notable differences from c:
- Data is immutable by default. Mutable data is marked as `mut`.
- Namespacing: `io::println("hello")`
- Generics: `HashTable<String, int>`
- Type-inferencing: `let x = true;`
- for-each loops: `foreach n in 0..10`
- Standard library supplies fat strings, fat arrays, optionals, result types, and much more.
- No macros, no metaprogramming. Sorry, not sorry!

## Examples

### Hello world
```c
import std/io;

void main() {
    io::println("Hello, world!");
}
```

### Fizzbuzz
```c
import std;
import std/conv;
import std/ds;
import std/io;

void main() {
    let args = std::args;

    if args.length != 2 {
        io::eprintln("Usage: fizzbuzz [integer]");
        std::exit(1);
    }
    let n_str = args.data[1];

    let res = conv::parse_uint(n_str);
    if !res.is_ok {
        CRASH `Error parser integer: {res.err}`;
    }
    uint n = res.val;

    let mut out = ds::strbuf_default();
    foreach i in 1..=n {
        defer ds::strbuf_reset(&out);
    
        if i % 3 == 0 { ds::strbuf_append_str(&out, "Fizz"); }
        if i % 5 == 0 { ds::strbuf_append_str(&out, "Buzz"); }

        if out.length == 0 { ds::strbuf_append_uint(&out, i); }

        ds::strbuf_append_char(&out, '\n');
        io::print(ds::strbuf_as_str(out));
    }
}
```

## Why does this exist?

For fun, learning, and because it's the langauge I want to use.

It will probably never be ready for you or others to use, and that's ok. If you like what you see here, I recommend checking out Zig and C3. Both of which also attempt to iterate on C with modern features, a lean grammar, and still allowing full control of your program.

## Using the language

For now, there is only a barebones compiler which outputs generated c files. I'll improve the tooling later.

Download this repo, then build the compiler using:
```
make
```

You can run the repo's tests using:
```
make test
```

The hello-world example can be run using:
```
make example-hello
```
