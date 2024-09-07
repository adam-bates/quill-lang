#include <assert.h>
#include <stdio.h>

#include "../src/lib/scanner.h"

int main(void) {
    char const* src = "void main() {\n\t// no-op\n}";

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

    Scanner scanner = scanner_create(src);
    ScanResult scan_res = scanner_scan(scanner);

    assert(scan_res.ok);
    Tokens tokens = scan_res.res.val;

    assert(tokens.length == expected_tokens.length);

    for (size_t i = 0; i < tokens.length; ++i) {
        Token token = tokens.arr[i];
        Token expected_token = expected_tokens.arr[i];

        assert(token.type == expected_token.type);
        assert(token.type == expected_token.type);
        assert(token.start == expected_token.start);
        assert(token.length == expected_token.length);
        assert(token.line == expected_token.line);
    }

    return EXIT_SUCCESS;
}

