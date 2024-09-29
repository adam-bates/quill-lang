#ifndef quill_string_buffer_h
#define quill_string_buffer_h

#include "./arena.h"
#include "./base.h"

typedef struct {
    Arena* arena;
    size_t      capacity;
    size_t      length;
    char* chars;
} StringBuffer;

StringBuffer strbuf_create(Arena* arena);
StringBuffer strbuf_create_with_capacity(Arena* arena, size_t const capacity);

void strbuf_append_char(StringBuffer* strbuf, char const c);
void strbuf_append_chars(StringBuffer* strbuf, char const* const chars);
void strbuf_append_str(StringBuffer* strbuf, String const str);

void strbuf_reset(StringBuffer* strbuf);

String strbuf_to_str(StringBuffer strbuf);
String strbuf_to_strcpy(StringBuffer strbuf);

#endif
