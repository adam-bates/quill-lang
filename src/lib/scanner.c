#include <stdlib.h>

#include "common.h"
#include "error.h"
#include "scanner.h"

Scanner scanner_create(char const* source) {
    // TODO

    return (Scanner){
        .source = source,
    };
}

ScanResult scanner_scan(Scanner scanner) {
    // TODO

    Tokens tokens = {
        .length = 6,
        .arr = calloc(6, sizeof(Token)),
    };

    size_t idx = 0;
    tokens.arr[idx++] = (Token){
            .type = TT_VOID,
            .start = scanner.source + 0,
            .length = 4,
            .line = 1,
        };
    tokens.arr[idx++] = (Token){
        .type = TT_IDENTIFIER,
        .start = scanner.source + 5,
        .length = 4,
        .line = 1,
    };
    tokens.arr[idx++] = (Token){
        .type = TT_LEFT_PAREN,
        .start = scanner.source + 9,
        .length = 1,
        .line = 1,
    };
    tokens.arr[idx++] = (Token){
        .type = TT_RIGHT_PAREN,
        .start = scanner.source + 10,
        .length = 1,
        .line = 1,
    };
    tokens.arr[idx++] = (Token){
        .type = TT_LEFT_BRACE,
        .start = scanner.source + 12,
        .length = 1,
        .line = 1,
    };
    tokens.arr[idx++] = (Token){
        .type = TT_RIGHT_BRACE,
        .start = scanner.source + 24,
        .length = 1,
        .line = 3,
    };

    return (ScanResult){ .ok = true, .res.val = tokens };

    // return (ScanResult){
    //     .ok = false,
    //     .res.err = err_create_unspecified("Not implemented"),
    // };
}

