#ifndef quill_arena_alloc_h
#define quill_arena_alloc_h

#include "./arena.h"
#include "./string_buffer.h"

// Arena design & implementation from Tsoding: https://github.com/tsoding/arena/blob/master/arena.h

void* arena_alloc(Arena* arena, size_t const size_bytes);
void* arena_realloc(Arena* arena, void* const ptr, size_t const old_size, size_t const new_size);
String arena_strcpy(Arena* arena, String const str);
void* arena_memcpy(Arena* arena, void const* const data, size_t const size);

StringBuffer arena_sprintf(Arena* arena, char const* fmt, ...);

void arena_reset(Arena* arena);
void arena_free(Arena* arena);

#endif
