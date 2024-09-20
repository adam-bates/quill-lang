package std::io;

import libc::stdio;

import ./std::String;

void println(String str);
void eprintln(String str);

---

void println(String str) {
	@ignore_unused
	uint _ = fprintln(stdio::stdout, str);
}

@impl
void eprintln(String str) {
	@ignore_unused
	uint _ = fprintln(stdio::stderr, str);
}

uint fprintln(FILE mut* stream, String str) {
	uint mut bytes = stdio::fwrite(str.bytes, sizeof(char), str.length, stream);

	bytes += stdio::fwrite([]{'\n'}, sizeof(char), 1, stream);

	return bytes;
}
