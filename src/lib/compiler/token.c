#include "./compiler.h"
#include "../utils/utils.h"
#include "token.h"

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

void arraylist_token_destroy(ArrayList_Token const list) {
    quill_free(list.allocator, list.array);
}

void arraylist_token_push(ArrayList_Token* const list, Token const token) {
    if (list->length >= list->capacity) {
        list->capacity = list->length * 2;
        list->array = quill_realloc(list->allocator, list->array, sizeof(Token) * list->capacity);
    }

    list->array[list->length] = token;
    list->length += 1;
}
