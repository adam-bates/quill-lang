#include "./arraylist.h"
#include "./arena_alloc.h"

ArrayList_Ptr arraylist_ptr_create_with_capacity(Arena* arena, size_t const capacity) {
    void** const ptr = arena_calloc(arena, capacity, sizeof *ptr);

    return (ArrayList_Ptr){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .data = ptr,
    };
}

ArrayList_Ptr arraylist_ptr_create(Arena* arena) {
  return arraylist_ptr_create_with_capacity(arena, 8);
}

static void arraylist_ptr_grow(ArrayList_Ptr* list, size_t const capacity) {
    void** ptr = arena_realloc(list->arena, list->data, list->capacity * sizeof(uintptr_t), capacity * sizeof(uintptr_t));
    list->data = ptr;
    list->capacity = capacity;
}

void arraylist_ptr_push(ArrayList_Ptr* list, void* ptr) {
    if (list->length >= list->capacity) {
        arraylist_ptr_grow(list, list->length * 2);
    }

    list->data[list->length] = ptr;
    list->length += 1;
}

void arraylist_ptr_reset(ArrayList_Ptr* list) {
    for (size_t i = 0; i < list->length; ++i) {
        list->data[i] = NULL;
    }
    list->length = 0;
}

