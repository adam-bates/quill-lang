#include <assert.h>
#include <stdio.h>

#include "../src/lib/compiler/lexer.h"

#define assert_true(expr) \
    if (!(expr)) { fprintf(stderr, "Test failed: \"%s\"\n", test_name); } \
    assert(expr)

#define assert_eq_i32(expected, actual) \
    if (expected != actual) { \
        fprintf(stderr, "Test failed: \"%s\" ... Expected [%d] but got [%d] \n", test_name, expected, actual); \
    } \
    assert(expected == actual)

#define assert_eq_u64(expected, actual) \
    if (expected != actual) { \
        fprintf(stderr, "Test failed: \"%s\" ... Expected [%lu] but got [%lu] \n", test_name, expected, actual); \
    } \
    assert(expected == actual)

#define assert_eq_char_ptr(expected, actual) \
    if (expected != actual) { \
        fprintf(stderr, "Test failed: \"%s\" ... Expected '%c' @%lu but got '%c' @%lu \n", test_name, *expected, (size_t)expected, *actual, (size_t)actual); \
    } \
    assert(expected == actual)

void test_lexer(
    char const* const test_name,
    Allocator const allocator,
    String const src,
    ArrayList_Token expected_tokens
) {
    Lexer lexer = lexer_create(allocator, src);
    ScanResult const scan_res = lexer_scan(&lexer);

    scanres_assert(scan_res);
    ArrayList_Token const tokens = scan_res.res.tokens;

    assert_eq_u64(expected_tokens.length, tokens.length);

    for (size_t i = 0; i < tokens.length; ++i) {
        Token const token = tokens.array[i];
        Token const expected_token = expected_tokens.array[i];

        assert_eq_i32(expected_token.type, token.type);
        assert_eq_char_ptr(expected_token.start, token.start);
        assert_eq_u64(expected_token.length, token.length);
        assert_eq_u64(expected_token.line, token.line);
    }

    arraylist_token_destroy(tokens);
}

int main(void) {
    MaybeAllocator const m_allocator = allocator_create();
    if (!m_allocator.ok) {
        fprintf(stderr, "Couldn't initialize allocator");
        return EXIT_FAILURE;
    }
    Allocator const allocator = m_allocator.maybe.allocator;

    {
        String src;
        test_lexer("test empty program",
            allocator,
            src = c_str("void main() {\n\t// no-op\n}\n"),
            (ArrayList_Token){
                .length = 7,
                .array = (Token[]){
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
                    {
                        .type = TT_EOF,
                        .start = src.chars + src.length,
                        .length = 0,
                        .line = 4,
                    },
                },
            }
        );
    }


    {
        String src;
        test_lexer("test hello world",
            allocator,
            src = c_str("import std::io;\n\nvoid main() {\n\tio::println(\"Hello, world!\");\n}\n"),
            (ArrayList_Token){
                .length = 19,
                .array = (Token[]){
                    {
                        .type = TT_IMPORT,
                        .start = src.chars + 0,
                        .length = 6,
                        .line = 1,
                    },
                    {
                        .type = TT_IDENTIFIER,
                        .start = src.chars + 7,
                        .length = 3,
                        .line = 1,
                    },
                    {
                        .type = TT_COLON_COLON,
                        .start = src.chars + 10,
                        .length = 2,
                        .line = 1,
                    },
                    {
                        .type = TT_IDENTIFIER,
                        .start = src.chars + 12,
                        .length = 2,
                        .line = 1,
                    },
                    {
                        .type = TT_SEMICOLON,
                        .start = src.chars + 14,
                        .length = 1,
                        .line = 1,
                    },
                    {
                        .type = TT_VOID,
                        .start = src.chars + 17,
                        .length = 4,
                        .line = 3,
                    },
                    {
                        .type = TT_IDENTIFIER,
                        .start = src.chars + 22,
                        .length = 4,
                        .line = 3,
                    },
                    {
                        .type = TT_LEFT_PAREN,
                        .start = src.chars + 26,
                        .length = 1,
                        .line = 3,
                    },
                    {
                        .type = TT_RIGHT_PAREN,
                        .start = src.chars + 27,
                        .length = 1,
                        .line = 3,
                    },
                    {
                        .type = TT_LEFT_BRACE,
                        .start = src.chars + 29,
                        .length = 1,
                        .line = 3,
                    },
                    {
                        .type = TT_IDENTIFIER,
                        .start = src.chars + 32,
                        .length = 2,
                        .line = 4,
                    },
                    {
                        .type = TT_COLON_COLON,
                        .start = src.chars + 34,
                        .length = 2,
                        .line = 4,
                    },
                    {
                        .type = TT_IDENTIFIER,
                        .start = src.chars + 36,
                        .length = 7,
                        .line = 4,
                    },
                    {
                        .type = TT_LEFT_PAREN,
                        .start = src.chars + 43,
                        .length = 1,
                        .line = 4,
                    },
                    {
                        .type = TT_LITERAL_STRING,
                        .start = src.chars + 44,
                        .length = 15,
                        .line = 4,
                    },
                    {
                        .type = TT_RIGHT_PAREN,
                        .start = src.chars + 59,
                        .length = 1,
                        .line = 4,
                    },
                    {
                        .type = TT_SEMICOLON,
                        .start = src.chars + 60,
                        .length = 1,
                        .line = 4,
                    },
                    {
                        .type = TT_RIGHT_BRACE,
                        .start = src.chars + 62,
                        .length = 1,
                        .line = 5,
                    },
                    {
                        .type = TT_EOF,
                        .start = src.chars + src.length,
                        .length = 0,
                        .line = 6,
                    },
                },
            }
        );
    }

    return EXIT_SUCCESS;
}

