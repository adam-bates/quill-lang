#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "scanner.h"

static ScanResult scanres_err(Error err) {
    return (ScanResult){ .ok = false, .res.err = err };
}

static ScanResult scanres_ok(Tokens tokens) {
    return (ScanResult){ .ok = true, .res.tokens = tokens };
}

Scanner scanner_create(char const* source) {
    return (Scanner){
        .source = source,
        .current = source,
        .line = 1,
    };
}

ScanResult scanner_scan(Scanner scanner) {
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
    //         .start = scanner.source + 0,
    //         .length = 4,
    //         .line = 1,
    //     };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_IDENTIFIER,
    //     .start = scanner.source + 5,
    //     .length = 4,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_LEFT_PAREN,
    //     .start = scanner.source + 9,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_RIGHT_PAREN,
    //     .start = scanner.source + 10,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_LEFT_BRACE,
    //     .start = scanner.source + 12,
    //     .length = 1,
    //     .line = 1,
    // };
    // tokens.arr[idx++] = (Token){
    //     .type = TT_RIGHT_BRACE,
    //     .start = scanner.source + 24,
    //     .length = 1,
    //     .line = 3,
    // };

    // return scanres_ok(tokens);

    return scanres_err(err_create(ET_UNIMPLEMENTED, "scanner_scan"));
}
