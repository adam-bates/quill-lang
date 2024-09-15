#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "./arena.h"
#include "../utils/utils.h"

#define REGION_DEFAULT_CAPACITY (8 * 1024)

static struct Region *new_region(size_t const capacity) {
    size_t const size_bytes = (sizeof(uintptr_t) * capacity) + sizeof(struct Region);
    struct Region* region = malloc(size_bytes);

    if (region != NULL) {
        region->next = NULL;
        region->length = 0;
        region->capacity = capacity;
    }

    return region;
}

static void free_region(struct Region* region) {
    free(region);
}

void* arena_alloc(Arena* arena, size_t const size_bytes) {
    size_t const size = (size_bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    if (arena->tail == NULL) {
        assert(arena->head == NULL);

        size_t capacity = REGION_DEFAULT_CAPACITY;
        if (capacity < size) {
            capacity = size;
        }

        struct Region* region = new_region(capacity);
        assert(region != NULL);

        arena->head = region;
        arena->tail = region;
    }

    while (
        arena->tail->length + size > arena->tail->capacity
        && arena->tail->next != NULL
    ) {
        arena->tail = arena->tail->next;
    }

    if (arena->tail->length + size > arena->tail->capacity) {
        assert(arena->tail->next == NULL);

        size_t capacity = REGION_DEFAULT_CAPACITY;
        if (capacity < size) {
            capacity = size;
        }

        struct Region* region = new_region(capacity);
        assert(region != NULL);

        arena->tail->next = region;
        arena->tail = arena->tail->next;
    }

    void* const ptr = &arena->tail->data[arena->tail->length];
    arena->tail->length += size;
    return ptr;
}

void* arena_realloc(Arena* arena, void* const old_ptr, size_t const old_size, size_t const new_size) {
    if (new_size <= old_size) {
        return old_ptr;
    }

    void* new_ptr = arena_alloc(arena, new_size);

    char* new_ptr_c = (char*)new_ptr;
    char* old_ptr_c = (char*)old_ptr;
    for (size_t i = 0; i < old_size; ++i) {
        new_ptr_c[i] = old_ptr_c[i];
    }

    return new_ptr;
}

String arena_strcpy(Arena* arena, String const str) {
    size_t const n = str.length;
    char* const cpy = arena_alloc(arena, n + 1);

    memcpy(cpy, str.chars, n);
    cpy[n] = '\0';

    return (String){
        .length = n,
        .chars = cpy,
    };
}

void* arena_memcpy(Arena* arena, void const* const data, size_t const size) {
    return memcpy(arena_alloc(arena, size), data, size);
}

StringBuffer arena_sprintf(Arena* arena, char const* fmt, ...) {
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    assert(n >= 0);
    char* ptr = arena_alloc(arena, n + 1);

    va_start(args, fmt);
    vsnprintf(ptr, n + 1, fmt, args);
    va_end(args);

    return (StringBuffer){
        .arena = arena,
        .capacity = n,
        .length = n,
        .chars = ptr,
    };
}

void arena_reset(Arena* arena) {
    struct Region* region = arena->head;

    while (region != NULL) {
        region->length = 0;
        region = region->next;
    }

    arena->tail = arena->head;
}

void arena_free(Arena* arena) {
    struct Region* region = arena->head;

    while (region) {
        struct Region* r = region;
        region = region->next;
        free_region(r);
    }

    arena->head = NULL;
    arena->tail = NULL;
}

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
}

void strbuf_append_chars(StringBuffer* strbuf, char const* const chars) {
    size_t const n = strlen(chars);

    strbuf_append_str(strbuf, (String){ .chars = chars, .length = n });
}

void strbuf_reset(StringBuffer* strbuf) {
    for (size_t i = 0; i < strbuf->length; ++i) {
        strbuf->chars[i] = '\0';
    }
    strbuf->length = 0;
}
