#include <stdio.h> // for debugging
#include <string.h>

#include "./compiler.h"
#include "../utils/utils.h"
#include "token.h"

typedef struct {
    char const* const pattern;
    TokenType const type;
} KeywordMatch;

static const size_t KEYWORD_MATCHES_LEN = 40;
static const KeywordMatch KEYWORD_MATCHES[KEYWORD_MATCHES_LEN] = {
    { "case", TT_CASE },
    { "CRASH", TT_CRASH },
    { "else", TT_ELSE },
    { "enum", TT_ENUM },
    { "false", TT_FALSE },
    { "for", TT_FOR },
    { "globaltag", TT_GLOBALTAG },
    { "if", TT_IF },
    { "import", TT_IMPORT },
    { "in", TT_IN },
    { "let", TT_LET },
    { "mut", TT_MUT },
    { "null", TT_NULL },
    { "return", TT_RETURN },
    { "static", TT_STATIC },
    { "struct", TT_STRUCT },
    { "switch", TT_SWITCH },
    { "true", TT_TRUE },
    { "union", TT_UNION },
    { "while", TT_WHILE },

    // built-in types
    { "void", TT_VOID },
    { "bool", TT_BOOL },
    { "char", TT_CHAR },
    { "int", TT_INT },
    { "int8", TT_INT8 },
    { "int16", TT_INT16 },
    { "int32", TT_INT32 },
    { "int64", TT_INT64 },
    { "int128", TT_INT128 },
    { "uint", TT_UINT },
    { "uint8", TT_UINT8 },
    { "uint16", TT_UINT16 },
    { "uint32", TT_UINT32 },
    { "uint64", TT_UINT64 },
    { "uint128", TT_UINT128 },
    { "float", TT_FLOAT },
    { "float16", TT_FLOAT16 },
    { "float32", TT_FLOAT32 },
    { "float64", TT_FLOAT64 },
    { "float128", TT_FLOAT128 },
};

typedef struct {
    bool ok;
    union {
        Token token;
        void* _;
    } maybe;
} MaybeToken;
static MaybeToken const MAYBETOKEN_NONE = { .ok = false, .maybe._ = NULL };
static MaybeToken maybetoken_none(void) { return MAYBETOKEN_NONE; }
static MaybeToken maybetoken_some(Token const token) {
    return (MaybeToken){ .ok = true, .maybe.token = token };
}

static ScanResult scanres_err(Error const err) {
    return (ScanResult){ .ok = false, .res.err = err };
}
static ScanResult scanres_ok(ArrayList_Token const tokens) {
    return (ScanResult){ .ok = true, .res.tokens = tokens };
}

static bool char_is_alpha(char const c) {
    return
        ('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        (c == '_')
    ;
}

static bool char_is_digit(char const c) {
    return '0' <= c && c <= '9';
}

static Token lexer_token_create(Lexer const* const lexer, TokenType const type) {
    return (Token) {
        .type = type,
        .start = lexer->start,
        .length = lexer->current - lexer->start,
        .line = lexer->line,
    };
}

static Token lexer_token_error(Lexer const* const lexer, char const* message) {
    return (Token) {
        .type = TT_ERROR,
        .start = message,
        .length = strlen(message),
        .line = lexer->line,
    };
}

static bool lexer_is_at_end(Lexer const* const lexer) {
    return *lexer->current == '\0';
}

static char lexer_peek(Lexer const* const lexer) {
    return *lexer->current;
}

static char lexer_peek_next(Lexer const* const lexer) {
    if (lexer_is_at_end(lexer)) {
        return '\0';
    }
    return lexer->current[1];
}

static char lexer_advance(Lexer* const lexer) {
    char const current = *lexer->current;
    lexer->current += 1;
    return current;
}

static bool lexer_match_char(Lexer* const lexer, char const expected) {
    if (lexer_is_at_end(lexer)) {
        return false;
    }

    if (*lexer->current != expected) {
        return false;
    }

    lexer->current += 1;
    return true;
}

static void lexer_skip_whitespace(Lexer* const lexer) {
    while (true) {
        switch (lexer_peek(lexer)) {
            case ' ':
            case '\r':
            case '\t':
                lexer_advance(lexer);
                break;
            case '\n':
                lexer->line += 1;
                lexer_advance(lexer);
                break;
            case '/':
                if (lexer_peek_next(lexer) != '/') {
                    return;
                }

                // a line comment goes until the end of the line
                while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                    lexer_advance(lexer);
                }

                break;
            default:
                return;
        }
    }
}

static MaybeToken lexer_scan_maybetoken_keyword(Lexer* const lexer) {
    for (size_t i = 0; i < KEYWORD_MATCHES_LEN; ++i) {
        KeywordMatch const keyword = KEYWORD_MATCHES[i];

        size_t const keyword_len = strlen(keyword.pattern);

        if ((size_t)(lexer->current - lexer->start) == keyword_len
            && memcmp(lexer->start, keyword.pattern, keyword_len) == 0
        ) {
            return maybetoken_some(lexer_token_create(lexer, keyword.type));
        }
    }

    return maybetoken_none();
}

static Token lexer_scan_token_keyword_or_identifier(Lexer* const lexer) {
    char c = lexer_peek(lexer);

    while (char_is_alpha(c) || char_is_digit(c)) {
        lexer_advance(lexer);
        c = lexer_peek(lexer);
    }

    // try keyword match
    MaybeToken const m_keyword = lexer_scan_maybetoken_keyword(lexer);
    if (m_keyword.ok) {
        return m_keyword.maybe.token;
    }

    // fallback to identifier
    return lexer_token_create(lexer, TT_IDENTIFIER);
}

static Token lexer_scan_token(Lexer* const lexer) {
    lexer_skip_whitespace(lexer);

    lexer->start = lexer->current;

    if (lexer_is_at_end(lexer)) {
        return lexer_token_create(lexer, TT_EOF);
    }

    char const c = lexer_advance(lexer);

    if (char_is_alpha(c)) {
        return lexer_scan_token_keyword_or_identifier(lexer);
    }

    if (char_is_digit(c)) {
        printf("digit: %c\n", c);
        // TODO: number
    }

    if (c == ':' && lexer_peek(lexer) == ':') {
        lexer_advance(lexer);
        return lexer_token_create(lexer, TT_COLON_COLON);
    }

    if (c == '"') {
        char cc = lexer_peek(lexer);
        while (cc != '"') {
            cc = lexer_advance(lexer);
        }

        return lexer_token_create(lexer, TT_LITERAL_STRING);
    }

    switch (c) {
        case ':': return lexer_token_create(lexer, TT_COLON);
        case ';': return lexer_token_create(lexer, TT_SEMICOLON);
        case '(': return lexer_token_create(lexer, TT_LEFT_PAREN);
        case ')': return lexer_token_create(lexer, TT_RIGHT_PAREN);
        case '{': return lexer_token_create(lexer, TT_LEFT_BRACE);
        case '}': return lexer_token_create(lexer, TT_RIGHT_BRACE);
        case '"': return lexer_token_create(lexer, TT_LITERAL_STRING);
        case ',': return lexer_token_create(lexer, TT_COMMA);
        case '!': return lexer_token_create(lexer, TT_BANG);
        default: exit(1); // TODO
    }

    // TODO
}

Lexer lexer_create(Allocator const allocator, String const source) {
    return (Lexer){
        .allocator = allocator,
        .start = source.chars,
        .current = source.chars,
        .line = 1,
    };
}

ScanResult lexer_scan(Lexer* const lexer) {
    ArrayList_Token tokens = arraylist_token_create(lexer->allocator);

    while (!lexer_is_at_end(lexer)) {
        Token const token = lexer_scan_token(lexer);

        // {
        //     char* token_str = quill_calloc(lexer->allocator, token.length, sizeof(char));
        //     strncpy(token_str, token.start, token.length);
        //     printf("Token(%d): %s\n", token.type, token_str);
        //     quill_free(lexer->allocator, token_str);
        // }

        arraylist_token_push(&tokens, token);
    }

    return scanres_ok(tokens);
}
