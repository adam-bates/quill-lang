#include <stdbool.h>
#include <string.h>

#include "arena_alloc.h"
#include "string.h"

#define INITIAL_STRINGS_CAPACITY 4

String c_str(char* const chars) {
    size_t const length = strlen(chars);

    return (String){
        .length = length,
        .chars = chars,
    };
}

bool str_eq(String str1, String str2) {
    if (str1.length != str2.length) {
        return false;
    }

    return strncmp(str1.chars, str2.chars, str1.length) == 0;
}

void strs_remove(Strings* const strs, size_t const idx) {
    if (idx >= strs->length) {
        return;
    }

    for (size_t i = idx + 1; i < strs->length; ++i) {
        strs->strings[i - 1] = strs->strings[i];
    }
    strs->strings[idx] = (String){0};

    strs->length -= 1;
}



ArrayList_String arraylist_string_create_with_capacity(Arena* const arena, size_t const capacity) {
    String* array = arena_calloc(arena, capacity, sizeof *array);
    
    return (ArrayList_String){
        .arena = arena,
    
        .capacity = capacity,
        .length = 0,

        .array = array,
    };
}

ArrayList_String arraylist_string_create(Arena* const arena) {
    return arraylist_string_create_with_capacity(arena, INITIAL_STRINGS_CAPACITY);
}

void arraylist_string_push(ArrayList_String* const list, String const string) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->array = arena_realloc(
            list->arena,
            list->array,
            sizeof(String) * prev_cap,
            sizeof(String) * list->capacity
        );
    }

    list->array[list->length] = string;
    list->length += 1;
}
