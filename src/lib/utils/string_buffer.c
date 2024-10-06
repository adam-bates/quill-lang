#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "./arena_alloc.h"
#include "./string_buffer.h"

StringBuffer strbuf_create_with_capacity(Arena* arena, size_t const capacity) {
    char* const ptr = arena_alloc(arena, capacity);

    return (StringBuffer){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .chars = ptr,
    };
}

StringBuffer strbuf_create(Arena* arena) {
    return strbuf_create_with_capacity(arena, 8);
}

static void strbuf_grow(StringBuffer* strbuf, size_t const capacity) {
    char* ptr = arena_realloc(strbuf->arena, strbuf->chars, strbuf->capacity, capacity);
    strbuf->chars = ptr;
    strbuf->capacity = capacity;
}

void strbuf_append_char(StringBuffer* strbuf, char const c) {
    if (strbuf->length >= strbuf->capacity) {
        strbuf_grow(strbuf, strbuf->length * 2);
    }

    strbuf->chars[strbuf->length] = c;
    strbuf->length += 1;
}

void strbuf_append_str(StringBuffer* strbuf, String const str) {
    if (strbuf->length + str.length >= strbuf->capacity) {
        if (strbuf->length >= str.length) {
            strbuf_grow(strbuf, strbuf->length * 2);
        } else {
            strbuf_grow(strbuf, strbuf->length + str.length);
        }
    }

    strncpy(strbuf->chars + strbuf->length, str.chars, str.length);
    strbuf->length += str.length;
}

void strbuf_append_chars(StringBuffer* strbuf, char const* const chars) {
    size_t const n = strlen(chars);

    strbuf_append_str(strbuf, (String){ .chars = chars, .length = n });
}

void strbuf_append_uint(StringBuffer* strbuf, size_t in) {
    if (in == 0) {
        strbuf_append_char(strbuf, '0');
        return;
    }

    size_t n = in;
    int cursor = 0;
    char digits_stack[32] = {0};
    while (n > 0) {
        assert(cursor < 32);
        char c = '0' + (n % 10);
        n = (n - (n % 10)) / 10;
        digits_stack[cursor++] = c;
    }

    while (cursor >= 0) {
        char c = digits_stack[--cursor];
        if (c) {
            strbuf_append_char(strbuf, c);
        }
    }
}

void strbuf_reset(StringBuffer* strbuf) {
    for (size_t i = 0; i < strbuf->length; ++i) {
        strbuf->chars[i] = '\0';
    }
    strbuf->length = 0;
}

String strbuf_to_str(StringBuffer strbuf) {
    return (String){
        .chars = strbuf.chars,
        .length = strbuf.length,
    };
}

String strbuf_to_strcpy(StringBuffer strbuf) {
    return arena_strcpy(strbuf.arena, strbuf_to_str(strbuf));
}
