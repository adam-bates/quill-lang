package std/io;

import libc/stdio;

import std;

void println(std::String str);
void eprintln(std::String str);

---

@impl // <-- optional; tells compiler to ensure there is a matching declaration
void println(std::String str) {
	@ignore_unused let _ =
		fwriteln(stdio::stdout, str);
}

@impl
void eprintln(std::String str) {
	@ignore_unused let _ =
		fwriteln(stdio::stderr, str);
}

uint fwriteln(stdio::FILE mut* stream, std::String str) {
	uint mut bytes = stdio::fwrite(str.bytes, sizeof(char), str.length, stream);

	// bytes += stdio::fwrite(&'\n', sizeof(char), 1, stream);

	return bytes;
}
