#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "lexer.h"

static ScanResult scanres_err(Error err) {
    return (ScanResult){ .ok = false, .res.err = err };
}

static ScanResult scanres_ok(ArrayList_Token tokens) {
    return (ScanResult){ .ok = true, .res.tokens = tokens };
}

static bool is_end(Lexer lexer) {
    return lexer.current[0] == '\0';
}

Lexer lexer_create(Allocator const allocator, String const source) {
    return (Lexer){
        .allocator = allocator,
        .source = source,

        .current = source.chars,
        .line = 1,
    };
}

ScanResult lexer_scan(Lexer const lexer) {
    // TODO

    // ArrayList_Token const tokens = {
    //     .length = 6,
    //     .array = lexer.allocator->calloc(6, sizeof(Token)),
    // };

    // if (tokens.array == NULL) {
    //     return scanres_err(err_create(ET_OUT_OF_MEMORY, "Couldn't allocate tokens for scanner"));
    // }

    // size_t idx = 0;
    // tokens.array[idx++] = (Token){
    //         .type = TT_VOID,
    //         .start = lexer.source.chars + 0,
    //         .length = 4,
    //         .line = 1,
    //     };
    // tokens.array[idx++] = (Token){
    //     .type = TT_IDENTIFIER,
    //     .start = lexer.source.chars + 5,
    //     .length = 4,
    //     .line = 1,
    // };
    // tokens.array[idx++] = (Token){
    //     .type = TT_LEFT_PAREN,
    //     .start = lexer.source.chars + 9,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.array[idx++] = (Token){
    //     .type = TT_RIGHT_PAREN,
    //     .start = lexer.source.chars + 10,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.array[idx++] = (Token){
    //     .type = TT_LEFT_BRACE,
    //     .start = lexer.source.chars + 12,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.array[idx++] = (Token){
    //     .type = TT_RIGHT_BRACE,
    //     .start = lexer.source.chars + 24,
    //     .length = 1,
    //     .line = 3,
    // };

    // return scanres_ok(tokens);

    return scanres_err(err_create(ET_UNIMPLEMENTED, "lexer_scan"));
}
