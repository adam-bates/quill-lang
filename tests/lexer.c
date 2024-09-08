#include <assert.h>
#include <stdio.h>

#include "../src/lib/lexer.h"

int main(void) {
    Allocator_s const allocator_s = allocator_create();
    Allocator const allocator = &allocator_s;

    String const src = c_str("void main() {\n\t// no-op\n}");

    Token const expected_tokens_arr[] = {
        {
            .type = TT_VOID,
            .start = src.chars + 0,
            .length = 4,
            .line = 1,
        },
        {
            .type = TT_IDENTIFIER,
            .start = src.chars + 5,
            .length = 4,
            .line = 1,
        },
        {
            .type = TT_LEFT_PAREN,
            .start = src.chars + 9,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_RIGHT_PAREN,
            .start = src.chars + 10,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_LEFT_BRACE,
            .start = src.chars + 12,
            .length = 1,
            .line = 1,
        },
        {
            .type = TT_RIGHT_BRACE,
            .start = src.chars + 24,
            .length = 1,
            .line = 3,
        },
    };
    ArrayList_Token const expected_tokens = {
        .length = 6,
        .array = (Token*)expected_tokens_arr,
    };

    Lexer const lexer = lexer_create(allocator, src);
    ScanResult const scan_res = lexer_scan(lexer);

    scanres_assert(scan_res);
    ArrayList_Token const tokens = scan_res.res.tokens;

    assert(tokens.length == expected_tokens.length);

    for (size_t i = 0; i < tokens.length; ++i) {
        Token const token = tokens.array[i];
        Token const expected_token = expected_tokens.array[i];

        assert(token.type == expected_token.type);
        assert(token.type == expected_token.type);
        assert(token.start == expected_token.start);
        assert(token.length == expected_token.length);
        assert(token.line == expected_token.line);
    }

    return EXIT_SUCCESS;
}

