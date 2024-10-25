package std;

import libc/stdlib;

static Array<String> args;

@string_literal struct String {
	uint  length,
	char* bytes,
}

struct Array<T> {
	uint length,
	T*   data,
}

struct Result<T> {
	bool is_ok,
	// union {
		T      val,
		String err,
	// },
}

@range_literal struct Range {
	int from,
	int to,
	bool to_inclusive,
}

void assert(bool expr) {
	if !expr {
		exit(1);
	}
}

void exit(int code) {
	stdlib::exit(code);
}
