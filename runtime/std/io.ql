package std/io;

import libc/stdio;

import std::String;

void print(String str);
void println(String str);

void eprint(String str);
void eprintln(String str);

---

@impl
void print(String str) {
	@ignore_unused let _ =
		stdio::fwrite(str.bytes, sizeof(char), str.length, stdio::stdout);
}

@impl
void println(String str) {
	@ignore_unused let _ =
		fwriteln(stdio::stdout, str);
}

@impl
void eprint(String str) {
	@ignore_unused let _ =
		stdio::fwrite(str.bytes, sizeof(char), str.length, stdio::stderr);
}

@impl
void eprintln(String str) {
	@ignore_unused let _ =
		fwriteln(stdio::stderr, str);
}

uint fwriteln(stdio::FILE mut* stream, String str) {
	uint mut bytes = stdio::fwrite(str.bytes, sizeof(char), str.length, stream);

	bytes += stdio::fwrite(&'\n', sizeof(char), 1, stream);

	return bytes;
}
