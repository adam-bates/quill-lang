package std/ds;

import std;
import libc/stdlib;
import libc/string;

@string_template struct StringBuffer {
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

void strbuf_free(StringBuffer sb) {
	stdlib::free(sb.bytes);
}

void strbuf_reset(StringBuffer mut* sb) {
	uint i = 0;
	while i < sb->length {
        sb->bytes[i] = '\0';
		i += 1;
    }
    sb->length = 0;
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
	std::String str = .{
		.bytes = chars,
		.length = len,
	};

    strbuf_append_str(sb, str);
}

void strbuf_append_str(StringBuffer mut* sb, std::String str) {
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
    if n == 0 {
        strbuf_append_char(sb, '0');
        return;
    }

    if n < 0 {
        strbuf_append_char(sb, '-');
    }

    strbuf_append_uint(sb, stdlib::llabs(n));
}

void strbuf_append_uint(StringBuffer mut* sb, uint input) {
    if input == 0 {
        strbuf_append_char(sb, '0');
        return;
    }

    uint n = input;
    int cursor = 0;
    char[32] digits_stack = []{0};
    while n > 0 {
        std::assert(cursor < 32);
        char c = '0' + (n % 10);
        n = (n - (n % 10)) / 10;
        digits_stack[cursor++] = c;
    }

    while cursor >= 0 {
        char c = digits_stack[--cursor];
        if c {
            strbuf_append_char(sb, c);
        }
    }
}

void strbuf_append_bool(StringBuffer mut* sb, bool input) {
	if input {
		strbuf_append_str(sb, "true");
	} else {
		strbuf_append_str(sb, "false");
	}
}

std::String strbuf_as_str(StringBuffer sb) {
	return .{
		.length = sb.length,
		.bytes = sb.bytes,
	};
}

---

void strbuf_grow(StringBuffer mut* sb, uint capacity) {
	char* ptr = stdlib::calloc(capacity, sizeof(char));
	uint i = 0;
	while i < sb->length {
		ptr[i] = sb->bytes[i];
		i += 1;
	}
	while i < capacity {
		ptr[i] = '\0';
		i += 1;
	}
	stdlib::free(sb->bytes);
    sb->bytes = ptr;
    sb->capacity = capacity;
}
