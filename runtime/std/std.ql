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

typedef Error = String;

struct Result<T> {
	bool is_ok,
	// union {
		T      val,
		Error err,
	// },
}

Result<T> res_ok<T>(T val) {
	return .{
		.is_ok = true,
		.val = val,
	};
}

Result<T> res_err<T>(Error err) {
	return .{
		.is_ok = false,
		.err = err,
	};
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

T assert_ok<T>(Result<T> res) {
	if !res.is_ok {
		CRASH `{res.err}`;
	}

	return res.val;
}

void exit(int code) {
	stdlib::exit(code);
}
