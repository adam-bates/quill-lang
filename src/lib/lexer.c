#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "lexer.h"

static ScanResult scanres_err(Error err) {
    return (ScanResult){ .ok = false, .res.err = err };
}

static ScanResult scanres_ok(Tokens tokens) {
    return (ScanResult){ .ok = true, .res.tokens = tokens };
}

Lexer lexer_create(char const* source) {
    return (Lexer){
        .source = source,
        .current = source,
        .line = 1,
    };
}

ScanResult lexer_scan(Lexer lexer) {
    // TODO

    // Tokens tokens = {
    //     .length = 6,
    //     .arr = calloc(6, sizeof(Token)),
    // };

    // if (tokens.arr == NULL) {
    //     return (ScanResult) {
    //         .ok = false,
    //         .res.err = err_create(ET_OUT_OF_MEMORY, "Couldn't allocate tokens for scanner"),
    //     };
    // }

    // size_t idx = 0;
    // tokens.arr[idx++] = (Token){
    //         .type = TT_VOID,
    //         .start = lexer.source + 0,
    //         .length = 4,
    //         .line = 1,
    //     };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_IDENTIFIER,
    //     .start = lexer.source + 5,
    //     .length = 4,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_LEFT_PAREN,
    //     .start = lexer.source + 9,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_RIGHT_PAREN,
    //     .start = lexer.source + 10,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_LEFT_BRACE,
    //     .start = lexer.source + 12,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_RIGHT_BRACE,
    //     .start = lexer.source + 24,
    //     .length = 1,
    //     .line = 3,
    // };

    // return scanres_ok(tokens);

    return scanres_err(err_create(ET_UNIMPLEMENTED, "lexer_scan"));
}
