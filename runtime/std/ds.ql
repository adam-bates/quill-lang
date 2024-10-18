package std/ds;

import libc/string;

import std::String;

struct StringBuffer {
	uint capacity,
	uint length,
	char* bytes,
}

StringBuffer strbuf_default() {
	return strbuf_create(8);
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
	// TODO: loop on length, set to '\0'

	sb->length = 0;

	CRASH "TODO";
}

void strbuf_append_char(StringBuffer mut* sb, char c) {
    if sb->length >= sb->capacity {
        strbuf_grow(sb, sb->length * 2);
    }

    sb->bytes[sb->length] = c;
    sb->length += 1;
}

void strbuf_append_chars(StringBuffer mut* sb, char* chars) {
    uint len = string::strlen(chars);
	String str = .{
		.bytes = chars,
		.length = len,
	};

    strbuf_append_str(sb, str);
}

void strbuf_append_str(StringBuffer mut* sb, String str) {
    if sb->length + str.length >= sb->capacity {
        if sb->length >= str.length {
            strbuf_grow(sb, sb->length * 2);
        } else {
            strbuf_grow(sb, sb->length + str.length);
        }
    }

    string::strncpy(sb->bytes + sb->length, str.bytes, str.length);
    sb->length += str.length;
}

void strbuf_append_int(StringBuffer mut* sb, int n) {
	CRASH "TODO";
}

void strbuf_append_uint(StringBuffer mut* sb, uint n) {
	CRASH "TODO";
}

String strbuf_as_str(StringBuffer sb) {
	return .{
		.length = sb.length,
		.bytes = sb.bytes,
	};
}

---

void strbuf_grow(StringBuffer mut* sb, uint capacity) {
	char* ptr = stdlib::realloc(sb->bytes, capacity);
    sb->bytes = ptr;
    sb->capacity = capacity;
}
