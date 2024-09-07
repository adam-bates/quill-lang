#include <assert.h>
#include <stdio.h>

#include "../src/lib/lexer.h"

int main(void) {
    Allocator alloc = allocator_create();

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

    Lexer lexer = lexer_create(alloc, src);
    ScanResult scan_res = lexer_scan(lexer);

    scanres_assert(scan_res);
    Tokens tokens = scan_res.res.tokens;

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

