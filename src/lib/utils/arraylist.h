#ifndef quill_arraylist_h
#define quill_arraylist_h

#include "./base.h"
#include "./arena.h"

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    void** data;
} ArrayList_Ptr;

ArrayList_Ptr arraylist_ptr_create(Arena* arena);
ArrayList_Ptr arraylist_ptr_create_with_capacity(Arena* arena, size_t const capacity);

void arraylist_ptr_push(ArrayList_Ptr* list, void* ptr);

void arraylist_ptr_reset(ArrayList_Ptr* list);

#endif
