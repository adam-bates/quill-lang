#include "./compiler.h"
#include "../utils/utils.h"

#define INITIAL_TOKENS_CAPACITY 64

ArrayList_Token arraylist_token_create_with_capacity(Allocator const allocator, size_t const capacity) {
    Token* array = quill_calloc(allocator, capacity, sizeof(Token));
    
    return (ArrayList_Token){
        .allocator = allocator,
    
        .capacity = capacity,
        .length = 0,

        .array = array,
    };
}

ArrayList_Token arraylist_token_create(Allocator const allocator) {
    return arraylist_token_create_with_capacity(allocator, INITIAL_TOKENS_CAPACITY);
}

void arraylist_token_push(ArrayList_Token* const list, Token const token) {
    if (list->length >= list->capacity) {
        list->capacity = list->length * 2;
        list->array = quill_realloc(list->allocator, list->array, sizeof(Token) * list->capacity);
    }

    list->array[list->length] = token;
    list->length += 1;
}

ArrayListResult_Token arraylist_token_set(ArrayList_Token* const list, size_t const idx, Token const token) {
    bool has_value = true;
    
    if (list->length <= idx) {
        if (list->capacity <= idx) {
            if (list->capacity * 2 <= idx) {
                list->capacity = list->length + idx;
            } else {
                list->capacity *= 2;
            }

            list->array = quill_realloc(list->allocator, list->array, sizeof(Token) * list->capacity);
        }

        has_value = false;
        list->length = idx + 1;
    }

    Token const prev = list->array[idx];

    list->array[idx] = token;

    return (ArrayListResult_Token) {
        .ok = has_value,
        .res.token = prev,
    };

}

void arraylist_token_insert(ArrayList_Token* const list, size_t const idx, Token const token) {
    if (list->length <= idx) {
        arraylist_token_set(list, idx, token);
        return;
    }

    if (list->length >= list->capacity) {
        list->capacity = list->length * 2;
        list->array = quill_realloc(list->allocator, list->array, sizeof(Token) * list->capacity);
    }

    for (size_t i = list->length - 1; i >= idx; --i) {
        list->array[i + 1] = list->array[i];
    }

    list->array[idx] = token;
}

