#ifndef quillc_arena_h
#define quillc_arena_h

#include "./base.h"

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

void* arena_alloc(Arena* arena, size_t const size_bytes);
void* arena_realloc(Arena* arena, void* const ptr, size_t const old_size, size_t const new_size);
char* arena_strcpy(Arena* arena, char const* str);
void* arena_memcpy(Arena* arena, void const* const data, size_t const size);

char* arena_sprintf(Arena* arena, char const* fmt, ...);

void arena_reset(Arena* arena);
void arena_free(Arena* arena);

#endif
