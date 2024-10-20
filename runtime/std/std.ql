package std;

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

void assert(bool expr);
void exit(int code);
