package std;

import libc/stdlib;

static Array<String> args;

@string_literal struct String {
	uint  length,
	char* bytes,
}

struct Maybe<T> {
	bool is_some,
	T    val,
}

Maybe<T> some<T>(T val) {
	return .{
		.is_some = true,
		.val = val,
	};
}

Maybe<T> none<T>() {
	return .{ .is_some = false };
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

struct Array<T> {
	uint length,
	T*   data,
}

Maybe<T> arr_get<T>(Array<T>* arr, uint idx) {
	if idx >= arr->length {
		return none<T>();
	}

	return some<T>(arr->data[idx]);
}

void assert(bool expr) {
	if !expr {
		exit(1);
	}
}

T assert_some<T>(Maybe<T> maybe) {
	if !maybe.is_some {
		CRASH "Expected some, found none";
	}

	return maybe.val;
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
