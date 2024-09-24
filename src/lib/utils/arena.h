#ifndef quill_arena_h
#define quill_arena_h

#include "./base.h"
#include "./string.h"

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

#endif
