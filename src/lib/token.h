#ifndef snowy_token_h
#define snowy_token_h

#include "common.h"

typedef enum {
    TT_IDENTIFIER,

    // single-character tokens
    TT_LEFT_PAREN, TT_RIGHT_PAREN,
    TT_LEFT_BRACE, TT_RIGHT_BRACE,
    TT_LEFT_BRACKET, TT_RIGHT_BRACKET,
    TT_COMMA, TT_DOT, TT_SEMICOLON,
    TT_QUESTION, TT_CARET,

    // one or two character tokens
    TT_BANG, TT_BANG_EQUAL,
    TT_EQUAL, TT_EQUAL_EQUAL,
    TT_GREATER, TT_GREATER_EQUAL, TT_GREATER_GREATER,
    TT_LESS, TT_LESS_EQUAL, TT_LESS_LESS,
    TT_PLUS, TT_PLUS_EQUAL,
    TT_MINUS, TT_MINUS_EQUAL,
    TT_SLASH, TT_SLASH_EQUAL,
    TT_STAR, TT_STAR_EQUAL,
    TT_PIPE, TT_PIPE_PIPE,
    TT_AMPERSAND, TT_AMPERSAND_AMPERSAND,
    TT_COLON, TT_COLON_COLON,

    // literals
    TT_LITERAL_NUMBER,
    TT_LITERAL_CHAR,
    TT_LITERAL_STRING,
    TT_LITERAL_STRING_TEMPLATE_START, TT_LITERAL_STRING_TEMPLATE_CONT,

    // keywords
    TT_CASE,
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
    TT_UNION,
    TT_WHILE,

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
    int length;

    int line;
} Token;

typedef struct {
    size_t length;
    Token* arr;
} Tokens;

#endif
