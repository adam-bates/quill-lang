#ifndef quill_arena_h
#define quill_arena_h

#include "./base.h"
#include "./strings.h"

// Arena design & implementation from Tsoding: https://github.com/tsoding/arena/blob/master/arena.h

struct Region {
    struct Region *next;
    size_t capacity;
    size_t length;
    uintptr_t data[];
};

typedef struct {
    struct Region* head;
    struct Region* tail;
} Arena;

typedef struct {
    Arena* arena;
    size_t      capacity;
    size_t      length;
    char* chars;
} StringBuffer;

void* arena_alloc(Arena* arena, size_t const size_bytes);
void* arena_realloc(Arena* arena, void* const ptr, size_t const old_size, size_t const new_size);
String arena_strcpy(Arena* arena, String const str);
void* arena_memcpy(Arena* arena, void const* const data, size_t const size);

StringBuffer arena_sprintf(Arena* arena, char const* fmt, ...);

void arena_reset(Arena* arena);
void arena_free(Arena* arena);

StringBuffer strbuf_create(Arena* arena);
StringBuffer strbuf_create_with_capacity(Arena* arena, size_t const capacity);

void strbuf_append_char(StringBuffer* strbuf, char const c);
void strbuf_append_chars(StringBuffer* strbuf, char const* const chars);
void strbuf_append_str(StringBuffer* strbuf, String const str);

void strbuf_reset(StringBuffer* strbuf);

#endif
