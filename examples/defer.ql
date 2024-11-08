import std/io;

void main() {
    defer io::println("10");
    defer io::println("9");

    io::println("1");

    {
	    defer io::println("4");
	    defer io::println("3");

	    io::println("2");
    }

    defer io::println("8");
    defer {
	    defer io::println("7");

	    io::println("6");
    }

    io::println("5");
}
