#include <string.h>

#include "./compiler.h"
#include "../utils/utils.h"
#include "token.h"

typedef struct {
    char const* const pattern;
    TokenType const type;
} KeywordMatch;

static const size_t KEYWORD_MATCHES_LEN = 46;
static const KeywordMatch KEYWORD_MATCHES[KEYWORD_MATCHES_LEN] = {
    { "break", TT_BREAK },
    { "continue", TT_CONTINUE },
    { "CRASH", TT_CRASH },
    { "defer", TT_DEFER },
    { "else", TT_ELSE },
    { "enum", TT_ENUM },
    { "false", TT_FALSE },
    { "for", TT_FOR },
    { "foreach", TT_FOREACH },
    { "globaltag", TT_GLOBALTAG },
    { "if", TT_IF },
    { "import", TT_IMPORT },
    { "in", TT_IN },
    { "let", TT_LET },
    { "mut", TT_MUT },
    { "null", TT_NULL },
    { "package", TT_PACKAGE },
    { "return", TT_RETURN },
    { "sizeof", TT_SIZEOF },
    { "static", TT_STATIC },
    { "struct", TT_STRUCT },
    { "switch", TT_SWITCH },
    { "true", TT_TRUE },
    { "typedef", TT_TYPEDEF },
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

static Token lexer_token_error(Lexer const* const lexer, char* message) {
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
                switch (lexer_peek_next(lexer)) {
                    case '/': {
                        // a line comment goes until the end of the line
                        while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                            lexer_advance(lexer);
                        }
                        break;
                    }

                    case '*': {
                        lexer->multiline_comment_nest += 1;
                        lexer_advance(lexer); // '/'
                        lexer_advance(lexer); // '*'

                        while (true) {
                            if (lexer_is_at_end(lexer)) {
                                break;
                            }

                            if (lexer_peek(lexer) == '\n') {
                                lexer->line += 1;
                            }

                            if (lexer_peek(lexer) == '/' && lexer_peek_next(lexer) == '*') {
                                lexer->multiline_comment_nest += 1;
                                lexer_advance(lexer); // '/'
                                lexer_advance(lexer); // '*'
                                continue;
                            }

                            if (lexer_peek(lexer) == '*' && lexer_peek_next(lexer) == '/') {
                                lexer->multiline_comment_nest -= 1;
                                lexer_advance(lexer); // '*'
                                lexer_advance(lexer); // '/'

                                if (lexer->multiline_comment_nest == 0) {
                                    break;
                                } else {
                                    continue;
                                }
                            }

                            lexer_advance(lexer);
                        }
                        break;
                    }

                    default: return;
                }
                break;
            default:
                return;
        }
    }
}

static MaybeToken lexer_scan_keyword_maybe(Lexer* const lexer) {
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

static Token lexer_scan_keyword_or_identifier(Lexer* const lexer) {
    char c = lexer_peek(lexer);

    while (char_is_alpha(c) || char_is_digit(c)) {
        lexer_advance(lexer);
        c = lexer_peek(lexer);
    }

    // try keyword match
    MaybeToken const m_keyword = lexer_scan_keyword_maybe(lexer);
    if (m_keyword.ok) {
        return m_keyword.maybe.token;
    }

    // fallback to identifier
    return lexer_token_create(lexer, TT_IDENTIFIER);
}

static Token lexer_scan_at_or_compiler_directive(Lexer* const lexer) {
    bool is_compiler_directive = false;
    while (char_is_alpha(lexer_peek(lexer)) || char_is_digit(lexer_peek(lexer))) {
        is_compiler_directive = true;
        lexer_advance(lexer);
    }

    if (is_compiler_directive) {
        return lexer_token_create(lexer, TT_COMPILER_DIRECTIVE);
    } else {
        return lexer_token_create(lexer, TT_AT);
    }
}

static Token lexer_scan_number_or_identifier(Lexer* const lexer) {
    while (char_is_digit(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    if (char_is_alpha(lexer_peek(lexer))) {
        return lexer_scan_keyword_or_identifier(lexer);
    }

    // look for a fractional part
    if (lexer_peek(lexer) == '.' && char_is_digit(lexer_peek_next(lexer))) {
        // consume to "."
        lexer_advance(lexer);

        while (char_is_digit(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
    }

    return lexer_token_create(lexer, TT_LITERAL_NUMBER);
}

static Token lexer_scan_chars(Lexer* const lexer) {
    while (lexer_peek(lexer) != '\'' && !lexer_is_at_end(lexer)) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line += 1;
            return lexer_token_error(lexer, "Unterminated char(s)");
        }

        char const _ = lexer_advance(lexer);
    }

    if (lexer_is_at_end(lexer)) {
        return lexer_token_error(lexer, "Unterminated char(s)");
    }

    char _ = lexer_advance(lexer);

    return lexer_token_create(lexer, TT_LITERAL_CHAR);
}

static Token lexer_scan_string(Lexer* const lexer) {
    while (lexer_peek(lexer) != '"' && !lexer_is_at_end(lexer)) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line += 1;
            return lexer_token_error(lexer, "Unterminated string");
        }

        char const _ = lexer_advance(lexer);
    }

    if (lexer_is_at_end(lexer)) {
        return lexer_token_error(lexer, "Unterminated string");
    }

    char _ = lexer_advance(lexer);

    return lexer_token_create(lexer, TT_LITERAL_STRING);
}

static Token lexer_scan_template_string_start(Lexer* const lexer) {
    while (lexer_peek(lexer) != '{' && lexer_peek(lexer) != '`' && !lexer_is_at_end(lexer)) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line += 1;
        }

        if (lexer_peek(lexer) == '\\') {
            char const _ = lexer_advance(lexer);
        }

        char const _ = lexer_advance(lexer);
    }

    if (lexer_is_at_end(lexer)) {
        return lexer_token_error(lexer, "Unterminated template string");
    }

    char c = lexer_advance(lexer);

    if (c == '{') {
        lexer->template_string_nest += 1;
        return lexer_token_create(lexer, TT_LITERAL_STRING_TEMPLATE_START);
    } else {
        return lexer_token_create(lexer, TT_LITERAL_STRING_TEMPLATE_FULL);
    }
}

static Token lexer_scan_template_string_cont(Lexer* const lexer) {
    while (lexer_peek(lexer) != '{' && lexer_peek(lexer) != '`' && !lexer_is_at_end(lexer)) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line += 1;
        }

        if (lexer_peek(lexer) == '\\') {
            char const _ = lexer_advance(lexer);
        }

        char const _ = lexer_advance(lexer);
    }

    if (lexer_is_at_end(lexer)) {
        return lexer_token_error(lexer, "Unterminated string");
    }

    char c = lexer_advance(lexer);

    if (c == '`') {
        lexer->template_string_nest -= 1;
    }

    return lexer_token_create(lexer, TT_LITERAL_STRING_TEMPLATE_CONT);
}

static Token lexer_scan_token(Lexer* const lexer) {
    lexer_skip_whitespace(lexer);

    lexer->start = lexer->current;

    if (lexer_is_at_end(lexer)) {
        return lexer_token_create(lexer, TT_EOF);
    }

    char const c = lexer_advance(lexer);

    if (char_is_alpha(c)) {
        return lexer_scan_keyword_or_identifier(lexer);
    }

    if (char_is_digit(c)) {
        return lexer_scan_number_or_identifier(lexer);
    }

    if (c == '}' && lexer->template_string_nest > 0) {
        return lexer_scan_template_string_cont(lexer);
    }

    switch (c) {
        case '\'': return lexer_scan_chars(lexer);
        case '"': return lexer_scan_string(lexer);
        case '`': return lexer_scan_template_string_start(lexer);
        case '@': return lexer_scan_at_or_compiler_directive(lexer);

        case '(': return lexer_token_create(lexer, TT_LEFT_PAREN);
        case ')': return lexer_token_create(lexer, TT_RIGHT_PAREN);
        case '{': return lexer_token_create(lexer, TT_LEFT_BRACE);
        case '}': return lexer_token_create(lexer, TT_RIGHT_BRACE);
        case '[': return lexer_token_create(lexer, TT_LEFT_BRACKET);
        case ']': return lexer_token_create(lexer, TT_RIGHT_BRACKET);
        case ',': return lexer_token_create(lexer, TT_COMMA);
        case ';': return lexer_token_create(lexer, TT_SEMICOLON);
        case '?': return lexer_token_create(lexer, TT_QUESTION);
        case '%': return lexer_token_create(lexer, TT_PERCENT);

        case '!': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_BANG_EQUAL : TT_BANG
        );
        case '=': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_EQUAL_EQUAL : TT_EQUAL
        );
        case '+': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_PLUS_EQUAL : (
                lexer_match_char(lexer, '+') ? TT_PLUS_PLUS : TT_PLUS
            )
        );
        case '-': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_MINUS_EQUAL : (
                lexer_match_char(lexer, '>') ? TT_MINUS_GREATER : (
                    lexer_match_char(lexer, '-') ? (
                        lexer_match_char(lexer, '-') ? TT_MINUS_MINUS_MINUS : TT_MINUS_MINUS
                    ) : TT_MINUS
                )
            )
        );
        case '/': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_SLASH_EQUAL : TT_SLASH
        );
        case '*': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_STAR_EQUAL : TT_STAR
        );
        case '^': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_CARET_EQUAL : TT_CARET
        );
        case '>': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_GREATER_EQUAL : (
                lexer_match_char(lexer, '>') ? TT_GREATER_GREATER : TT_GREATER
            )
        );
        case '<': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_LESS_EQUAL : (
                lexer_match_char(lexer, '<') ? TT_LESS_LESS : TT_LESS
            )
        );
        case '|': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_PIPE_EQUAL : (
                lexer_match_char(lexer, '|') ? TT_PIPE_PIPE : TT_PIPE
            )
        );
        case '&': return lexer_token_create(lexer,
            lexer_match_char(lexer, '=') ? TT_AMPERSAND_EQUAL : (
                lexer_match_char(lexer, '&') ? TT_AMPERSAND_AMPERSAND : TT_AMPERSAND
            )
        );
        case ':': return lexer_token_create(lexer,
            lexer_match_char(lexer, ':') ? TT_COLON_COLON : TT_COLON
        );
        case '.': return lexer_token_create(lexer,
            lexer_match_char(lexer, '.') ? (
                lexer_match_char(lexer, '=') ? TT_DOT_DOT_EQUAL : TT_DOT_DOT
            ) : TT_DOT
        );
    }

    return lexer_token_error(lexer, "Unexpected character(s).");
}

Lexer lexer_create(Arena* const arena, String const source) {
    return (Lexer){
        .arena = arena,
        .start = source.chars,
        .current = source.chars,
        .line = 1,

        .template_string_nest = 0,
        .multiline_comment_nest = 0,
    };
}

ScanResult lexer_scan(Lexer* const lexer) {
    ArrayList_Token tokens = arraylist_token_create(lexer->arena);

    while (!lexer_is_at_end(lexer)) {
        Token const token = lexer_scan_token(lexer);
        arraylist_token_push(&tokens, token);
    }

    return scanres_ok(tokens);
}
