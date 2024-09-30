#include "./compiler.h"
#include "../utils/utils.h"
#include "token.h"

#define INITIAL_TOKENS_CAPACITY 64

ArrayList_Token arraylist_token_create_with_capacity(Arena* const arena, size_t const capacity) {
    Token* array = arena_calloc(arena, capacity, sizeof(Token));
    
    return (ArrayList_Token){
        .arena = arena,
    
        .capacity = capacity,
        .length = 0,

        .array = array,
    };
}

ArrayList_Token arraylist_token_create(Arena* const arena) {
    return arraylist_token_create_with_capacity(arena, INITIAL_TOKENS_CAPACITY);
}

void arraylist_token_push(ArrayList_Token* const list, Token const token) {
    if (list->length >= list->capacity) {
        list->capacity = list->length * 2;
        list->array = arena_realloc(
            list->arena,
            list->array,
            sizeof(Token) * list->capacity,
            sizeof(Token) * list->capacity
        );
    }

    list->array[list->length] = token;
    list->length += 1;
}
