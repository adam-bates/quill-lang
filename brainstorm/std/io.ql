package std/io;

import libc/stdio;

// import ./std::String;

void println(String str);
void eprintln(String str);

---

@impl // <-- optional; tells compiler to ensure there is a matching declaration
void println(String str) {
	@ignore_unused let _ =
		fwriteln(stdio::stdout, str);
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
