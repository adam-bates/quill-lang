package std;

@string_literal struct String {
	uint  length,
	char* bytes,
}

struct Result<T> {
	bool is_ok,
}

void assert(bool expr);
