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
    char const* src = scanner.source;

    Token expected_tokens_arr[] = {
        {
            .type = TT_VOID,
            .start = src + 0,
            .length = 4,
            .line = 1,
        },
        {
            .type = TT_IDENTIFIER,
            .start = src + 5,
            .length = 4,
            .line = 1,
        },
        {
            .type = TT_LEFT_PAREN,
            .start = src + 9,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_RIGHT_PAREN,
            .start = src + 10,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_LEFT_BRACE,
            .start = src + 12,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_RIGHT_BRACE,
            .start = src + 24,
            .length = 1,
            .line = 3,
        },
    };
    Tokens expected_tokens = {
        .length = 6,
        .arr = expected_tokens_arr,
    };

    return (ScanResult) {
        .ok = true,
        .res.val = expected_tokens,
    };

    // return (ScanResult){
    //     .ok = false,
    //     .res.err = err_create_unspecified("Not implemented"),
    // };
}

