package std/ds;

import std::String;

static let DEFAULT_CAPACITY = 8;

struct StringBuffer {
	uint capacity,
	uint length,
	char* bytes,
}

StringBuffer strbuf_default() {
	return strbuf_create(DEFAULT_CAPACITY);
}

StringBuffer strbuf_create(uint capacity) {
	char* bytes = stdlib::calloc(capacity, sizeof(char));

	return .{
		.capacity = capacity,
		.length = 0,
		.bytes = bytes,
	};
}

void strbuf_reset(StringBuffer mut* sb) {
	CRASH "TODO";
}

void strbuf_append_str(StringBuffer mut* sb, String str) {
	CRASH "TODO";
}

void strbuf_append_int(StringBuffer mut* sb, int n) {
	CRASH "TODO";
}

void strbuf_append_uint(StringBuffer mut* sb, uint n) {
	CRASH "TODO";
}

void strbuf_append_char(StringBuffer mut* sb, char c) {
	CRASH "TODO";
}

void strbuf_append_chars(StringBuffer mut* sb, char* c) {
	CRASH "TODO";
}

String strbuf_as_str(StringBuffer sb) {
	CRASH "TODO";
}

---

void strbuf_grow(StringBuffer mut* sb, uint capacity) {
	char* ptr = stdlib::realloc(sb->bytes, capacity);
    sb->bytes = ptr;
    sb->capacity = capacity;
}
