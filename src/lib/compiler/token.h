#ifndef quillc_token_h
#define quillc_token_h

#include "../utils/utils.h"

typedef enum {
    TT_IDENTIFIER,
    TT_COMPILER_DIRECTIVE,

    // single-character tokens
    TT_LEFT_PAREN, TT_RIGHT_PAREN,     // ()
    TT_LEFT_BRACE, TT_RIGHT_BRACE,     // {}
    TT_LEFT_BRACKET, TT_RIGHT_BRACKET, // []
    TT_COMMA, TT_DOT,
    TT_SEMICOLON, TT_QUESTION, TT_AT,

    // one or two character tokens
    TT_BANG, TT_BANG_EQUAL,
    TT_EQUAL, TT_EQUAL_EQUAL,
    TT_PLUS, TT_PLUS_EQUAL, TT_PLUS_PLUS,
    TT_MINUS, TT_MINUS_EQUAL, TT_MINUS_MINUS, TT_MINUS_MINUS_MINUS,
    TT_SLASH, TT_SLASH_EQUAL,
    TT_STAR, TT_STAR_EQUAL,
    TT_CARET, TT_CARET_EQUAL,
    TT_GREATER, TT_GREATER_EQUAL, TT_GREATER_GREATER,
    TT_LESS, TT_LESS_EQUAL, TT_LESS_LESS,
    TT_PIPE, TT_PIPE_EQUAL, TT_PIPE_PIPE,
    TT_AMPERSAND, TT_AMPERSAND_EQUAL, TT_AMPERSAND_AMPERSAND,
    TT_COLON, TT_COLON_COLON,

    // literals
    TT_LITERAL_NUMBER,
    TT_LITERAL_CHAR,
    TT_LITERAL_STRING,
    TT_LITERAL_STRING_TEMPLATE_START, TT_LITERAL_STRING_TEMPLATE_CONT,
    TT_LITERAL_STRING_TEMPLATE_FULL,

    // keywords
    TT_CRASH,
    TT_ELSE,
    TT_ENUM,
    TT_FALSE,
    TT_FOR,
    TT_GLOBALTAG,
    TT_IF,
    TT_IMPORT,
    TT_IN,
    TT_LET,
    TT_MUT,
    TT_NULL,
    TT_RETURN,
    TT_STATIC,
    TT_STRUCT,
    TT_SWITCH,
    TT_TRUE,
    TT_TYPEDEF,
    TT_UNION,
    TT_WHILE,
    TT_DEFER,

    // built-in types
    TT_VOID,
    TT_BOOL,
    TT_CHAR,
    // (signed integers)
    TT_INT,   TT_INT8,  TT_INT16,
    TT_INT32, TT_INT64, TT_INT128,
    // (unsigned integers)
    TT_UINT,   TT_UINT8,  TT_UINT16,
    TT_UINT32, TT_UINT64, TT_UINT128,
    // (floating-point numbers)
    TT_FLOAT,   TT_FLOAT16,  TT_FLOAT32,
    TT_FLOAT64, TT_FLOAT128,

    // ephemeral
    TT_ERROR,
    TT_EOF,

    TT_COUNT
} TokenType;

typedef struct {
    TokenType type;

    char const* start;
    size_t length;

    size_t line;
} Token;

typedef struct {
    Allocator const allocator;

    size_t capacity;
    size_t length;
    Token* array;
} ArrayList_Token;

typedef struct {
    bool const ok;
    union {
        Error const err;
        Token const token;
    } const res;
} ArrayListResult_Token;

ArrayList_Token arraylist_token_create(Allocator const allocator);
ArrayList_Token arraylist_token_create_with_capacity(Allocator const allocator, size_t const capacity);
void arraylist_token_destroy(ArrayList_Token const list);

void arraylist_token_push(ArrayList_Token* const list, Token const token);

#endif
