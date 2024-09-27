#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../utils/utils.h"
#include "./compiler.h"
#include "./ast.h"
#include "./token.h"

typedef enum {
    PRS_OK,
    PRS_ERROR,
    PRS_NONE,
} ParseResultStatus;

typedef struct {
    ParseResultStatus status;
    ASTNode node;
} ParseResult;

static ParseResult parser_parse_expr(Parser* const parser, LL_Directive const directives);

typedef struct {
    char const* const pattern;
    DirectiveType const type;
} DirectiveMatch;

typedef struct {
    bool is_some;
    union {
        DirectiveType val;
        void* _;
    } maybe;
} Maybe_DirectiveType;

static const size_t DIRECTIVE_MATCHES_LEN = 5;
static const DirectiveMatch DIRECTIVE_MATCHES[DIRECTIVE_MATCHES_LEN] = {
    { "@c_header", DT_C_HEADER },
    { "@c_restrict", DT_C_RESTRICT },
    { "@c_FILE", DT_C_FILE },
    { "@ignore_unused", DT_IGNORE_UNUSED },
    { "@impl", DT_IMPL },
    // TODO
};

void debug_token_type(TokenType token_type) {
    switch (token_type) {
        case TT_IDENTIFIER: printf("identifier"); break;
        case TT_COMPILER_DIRECTIVE: printf("compiler_directive"); break;
        
        case TT_LEFT_PAREN: printf("("); break;
        case TT_RIGHT_PAREN: printf(")"); break;
        case TT_LEFT_BRACE: printf("{"); break;
        case TT_RIGHT_BRACE: printf("}"); break;
        case TT_LEFT_BRACKET: printf("["); break;
        case TT_RIGHT_BRACKET: printf("]"); break;
        case TT_COMMA: printf(","); break;
        case TT_DOT: printf("."); break;
        case TT_TILDE: printf("~"); break;
        case TT_SEMICOLON: printf(";"); break;
        case TT_QUESTION: printf("?"); break;
        case TT_AT: printf("@"); break;

        case TT_BANG: printf("!"); break;
        case TT_BANG_EQUAL: printf("!="); break;
        case TT_EQUAL: printf("="); break;
        case TT_EQUAL_EQUAL: printf("=="); break;
        case TT_PLUS: printf("+"); break;
        case TT_PLUS_EQUAL: printf("+="); break;
        case TT_PLUS_PLUS: printf("++"); break;
        case TT_MINUS: printf("-"); break;
        case TT_MINUS_EQUAL: printf("-="); break;
        case TT_MINUS_MINUS: printf("--"); break;
        case TT_MINUS_MINUS_MINUS: printf("---"); break;
        case TT_SLASH: printf("/"); break;
        case TT_SLASH_EQUAL: printf("/="); break;
        case TT_STAR: printf("*"); break;
        case TT_STAR_EQUAL: printf("*="); break;
        case TT_CARET: printf("^"); break;
        case TT_CARET_EQUAL: printf("^="); break;
        case TT_GREATER: printf(">"); break;
        case TT_GREATER_EQUAL: printf(">="); break;
        case TT_GREATER_GREATER: printf(">>"); break;
        case TT_LESS: printf("<"); break;
        case TT_LESS_EQUAL: printf("<="); break;
        case TT_LESS_LESS: printf("<<"); break;
        case TT_PIPE: printf("|"); break;
        case TT_PIPE_EQUAL: printf("|="); break;
        case TT_PIPE_PIPE: printf("||"); break;
        case TT_AMPERSAND: printf("&"); break;
        case TT_AMPERSAND_EQUAL: printf("&="); break;
        case TT_AMPERSAND_AMPERSAND: printf("&&"); break;
        case TT_COLON: printf(":"); break;
        case TT_COLON_COLON: printf("::"); break;

        case TT_LITERAL_NUMBER: printf("literal_number"); break;
        case TT_LITERAL_CHAR: printf("literal_char"); break;
        case TT_LITERAL_STRING: printf("literal_string"); break;
        case TT_LITERAL_STRING_TEMPLATE_START: printf("literal_string_template_start"); break;
        case TT_LITERAL_STRING_TEMPLATE_CONT: printf("literal_string_template_cont"); break;
        case TT_LITERAL_STRING_TEMPLATE_FULL: printf("literal_string_template_full"); break;

        case TT_CRASH: printf("crash"); break;
        case TT_ELSE: printf("else"); break;
        case TT_ENUM: printf("enum"); break;
        case TT_FALSE: printf("false"); break;
        case TT_FOR: printf("for"); break;
        case TT_GLOBALTAG: printf("globaltag"); break;
        case TT_IF: printf("if"); break;
        case TT_IMPORT: printf("import"); break;
        case TT_IN: printf("in"); break;
        case TT_LET: printf("let"); break;
        case TT_MUT: printf("mut"); break;
        case TT_NULL: printf("null"); break;
        case TT_PACKAGE: printf("package"); break;
        case TT_RETURN: printf("return"); break;
        case TT_SIZEOF: printf("sizeof"); break;
        case TT_STATIC: printf("static"); break;
        case TT_STRUCT: printf("struct"); break;
        case TT_SWITCH: printf("switch"); break;
        case TT_TRUE: printf("true"); break;
        case TT_TYPEDEF: printf("typedef"); break;
        case TT_UNION: printf("union"); break;
        case TT_WHILE: printf("while"); break;
        case TT_DEFER: printf("defer"); break;

        case TT_VOID: printf("void"); break;
        case TT_BOOL: printf("bool"); break;
        case TT_CHAR: printf("char"); break;

        case TT_INT: printf("int"); break;
        case TT_INT8: printf("int8"); break;
        case TT_INT16: printf("int16"); break;

        case TT_INT32: printf("int32"); break;
        case TT_INT64: printf("int64"); break;
        case TT_INT128: printf("int128"); break;

        case TT_UINT: printf("uint"); break;
        case TT_UINT8: printf("uint8"); break;
        case TT_UINT16: printf("uint16"); break;

        case TT_UINT32: printf("uint32"); break;
        case TT_UINT64: printf("uint64"); break;
        case TT_UINT128: printf("uint128"); break;

        case TT_FLOAT: printf("float"); break;
        case TT_FLOAT16: printf("float16"); break;
        case TT_FLOAT32: printf("float32"); break;

        case TT_FLOAT64: printf("float64"); break;
        case TT_FLOAT128: printf("float128"); break;

        case TT_ERROR: printf("<error>"); break;
        case TT_EOF: printf("<eof>"); break;
        case TT_COUNT: printf("<count>"); break;
    }
}

static void debug_token(Parser const* const parser, Token token) {
    printf("<line %lu> Token[", token.line);
    debug_token_type(token.type);

    char* cstr = arena_memcpy(parser->arena, token.start, token.length + 1);
    cstr[token.length] = '\0';

    printf("] = \"%s\"\n", cstr);
}

static ParseResult parseres_ok(ASTNode const node) {
    return (ParseResult){
        .status = PRS_OK,
        .node = node,
    };
}

static ParseResult parseres_err(ASTNode const node) {
    return (ParseResult){
        .status = PRS_ERROR,
        .node = node,
    };
}

static ParseResult parseres_none(void) {
    static ParseResult const NONE = { .status = PRS_NONE };
    return NONE;
}

static ASTNodeResult astres_ok(ASTNode const* const ast) {
    return (ASTNodeResult){ .ok = true, .res.ast = ast };
}

static ASTNodeResult astres_err(Error const err) {
    return (ASTNodeResult){ .ok = false, .res.err = err };
}

static bool parser_is_at_end(Parser const* const parser) {
    return parser->tokens.array[parser->cursor_current].type == TT_EOF;
}


static Token parser_peek_prev(Parser const* const parser) {
    return parser->tokens.array[parser->cursor_current - 1];
}

static Token parser_peek(Parser* parser) {
    return parser->tokens.array[parser->cursor_current];
}

static Token parser_peek_next(Parser* parser) {
    if (parser_is_at_end(parser)) {
        return parser_peek(parser);
    }

    return parser->tokens.array[parser->cursor_current + 1];
}

static void error_at(Parser* const parser, Token const* const token, char const* const message) {
    if (parser->panic_mode) {
        return;
    }
    
    parser->panic_mode = true;

    fprintf(stderr, "[line %lu] Error", token->line);

    if (token->type == TT_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TT_ERROR) {
        // nothing
        fprintf(stderr, " parsing broken token '%.*s'", (int)token->length, token->start);
    } else {
        fprintf(stderr, " at [%.*s]", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);

    parser->had_error = true;
}

static void error(Parser* const parser, char const* const message) {
    Token const token = parser_peek_prev(parser);
    error_at(parser, &token, message);
}

static void error_at_current(Parser* const parser, char const* const message) {
    Token const token = parser_peek(parser);
    error_at(parser, &token, message);
}

static Token parser_advance(Parser* const parser) {
    Token current = parser->tokens.array[parser->cursor_current];
    parser->cursor_current += 1;
    return current;
}

static bool parser_consume(Parser* const parser, TokenType const type, char const* const message) {
    if (parser_peek(parser).type != type) {
        error_at_current(parser, message);
        return false;
    }

    parser_advance(parser);
    return true;
}

static StaticPath* _parser_parse_static_path(Parser* const parser, bool const allow_star) {
    Token const ident = parser_peek(parser);

    if (
        (allow_star && (ident.type != TT_STAR && ident.type != TT_IDENTIFIER))
        || (!allow_star && (ident.type != TT_IDENTIFIER))
    ) {
        return NULL;
    }
    parser_advance(parser);

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    StaticPath* path = arena_alloc(parser->arena, sizeof(StaticPath));
    path->name = name;
    path->root = NULL;

    if (parser_peek(parser).type != TT_COLON_COLON || ident.type == TT_STAR) {
        return path;
    }
    parser_advance(parser);

    StaticPath* root = arena_alloc(parser->arena, sizeof(StaticPath));        
    root = path;

    path = _parser_parse_static_path(parser, allow_star);
    if (path == NULL) {
        return NULL;
    }

    path->root = root;

    return path;
}

static StaticPath* parser_parse_import_path(Parser* const parser) {
    return _parser_parse_static_path(parser, true);
}

static StaticPath* parser_parse_ident_path(Parser* const parser) {
    return _parser_parse_static_path(parser, false);
}

static Maybe_DirectiveType parser_parse_directive_type(Parser* const parser, Token token) {
    for (size_t i = 0; i < DIRECTIVE_MATCHES_LEN; ++i) {
        DirectiveMatch const dt = DIRECTIVE_MATCHES[i];

        size_t const directive_len = strlen(dt.pattern);

        if (token.length == directive_len
            && memcmp(token.start, dt.pattern, directive_len) == 0
        ) {
            return (Maybe_DirectiveType){
                .is_some = true,
                .maybe.val = dt.type,
            };
        }
    }

    return (Maybe_DirectiveType){0};
}

static LL_Directive parser_parse_directives(Parser* const parser) {
    static LL_Directive const EMPTY = {0};

    if (parser_peek(parser).type != TT_COMPILER_DIRECTIVE) {
        return EMPTY;
    }

    LL_Directive directives = {0};

    Token current = parser_peek(parser);
    while (current.type == TT_COMPILER_DIRECTIVE) {
        Maybe_DirectiveType m_type = parser_parse_directive_type(parser, current);
        if (!m_type.is_some) {
            char const* str = arena_strcpy(parser->arena, (String){current.length, current.start}).chars;
            fprintf(stderr, "Directive: [%s]\n", str);
            assert(false); // TODO: better error
        }
        DirectiveType type = m_type.maybe.val;
        
        parser_advance(parser);
        current = parser_peek(parser);

        switch (type) {
            case DT_C_HEADER: {
                assert(parser_consume(parser, TT_LEFT_PAREN, "Expected '(' after @c_header."));

                assert(parser_consume(parser, TT_LITERAL_STRING, "Expected string of c file name. ie. @c_header(\"stdio.h\")"));
                Token include = parser_peek_prev(parser);

                assert(parser_consume(parser, TT_RIGHT_PAREN, "Expected ')' after @c_header(..."));

                Directive directive = {
                    .type = DT_C_HEADER,
                    .dir.c_header = {
                        .include = {
                            .length = include.length - 2,
                            .chars = include.start + 1,
                        },
                    },
                };
                ll_directive_push(parser->arena, &directives, directive);
                break;
            }

            case DT_C_RESTRICT: {
                Directive directive = {
                    .type = DT_C_RESTRICT,
                    .dir.c_restrict = NULL,
                };
                ll_directive_push(parser->arena, &directives, directive);
                break;
            }

            case DT_C_FILE: {
                Directive directive = {
                    .type = DT_C_FILE,
                    .dir.c_file = NULL,
                };
                ll_directive_push(parser->arena, &directives, directive);
                break;
            }

            case DT_IGNORE_UNUSED: {
                Directive directive = {
                    .type = DT_IGNORE_UNUSED,
                    .dir.ignore_unused = NULL,
                };
                ll_directive_push(parser->arena, &directives, directive);
                break;
            }

            case DT_IMPL: {
                Directive directive = {
                    .type = DT_IMPL,
                    .dir.impl = NULL,
                };
                ll_directive_push(parser->arena, &directives, directive);
                break;
            }

            default: fprintf(stderr, "TODO: handle [%d]\n", type); assert(false);
        }
    }

    return directives;
}

static Type* parser_parse_type_wrap(Parser* const parser, Type* type) {
    Token t = parser_peek(parser);

    switch (t.type) {
        case TT_STAR: {
            parser_advance(parser);
            Type* wrapped = arena_alloc(parser->arena, sizeof *wrapped);
            *wrapped = (Type){
                .kind = TK_POINTER,
                .type.ptr.of = type,
            };
            return parser_parse_type_wrap(parser, wrapped);
        }

        case TT_MUT: {
            if (parser_peek_next(parser).type != TT_STAR) {
                break;
            }            
            parser_advance(parser); // mut
            parser_advance(parser); // *

            Type* wrapped = arena_alloc(parser->arena, sizeof *wrapped);
            *wrapped = (Type){
                .kind = TK_MUT_POINTER,
                .type.mut_ptr.of = type,
            };
            return parser_parse_type_wrap(parser, wrapped);
        }

        default: break;
    }
    
    return type;
}

static Type* parser_parse_type(Parser* const parser) {
    LL_Directive const directives = parser_parse_directives(parser);

    Type* type = arena_alloc(parser->arena, sizeof *type);
    type->directives = directives;

    Token t = parser_peek(parser);
    switch (t.type) {
        case TT_IDENTIFIER: {
            if (parser_peek_next(parser).type == TT_COLON_COLON) {
                StaticPath* path = parser_parse_ident_path(parser);
                assert(path);

                type->kind = TK_STATIC_PATH;
                type->type.static_path.path = path;
                return parser_parse_type_wrap(parser, type);
            } else {            
                parser_advance(parser);

                type->kind = TK_TYPE_REF;
                type->type.type_ref.name = (String){
                    .length = t.length,
                    .chars = t.start,
                };
                return parser_parse_type_wrap(parser, type);
            }
        }

        case TT_VOID: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_VOID;
            return parser_parse_type_wrap(parser, type);
        }

        case TT_UINT: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_UINT;
            return parser_parse_type_wrap(parser, type);
        }

        case TT_CHAR: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_CHAR;
            return parser_parse_type_wrap(parser, type);
        }

        default: debug_token(parser, t); fprintf(stderr, "TODO: handle [%d]\n", t.type); assert(false);
    }
    
    return NULL;
}

static ParseResult parser_parse_package(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_PACKAGE, "Expected `pacakge` keyword.")) {
        return parseres_none();
    }

    StaticPath* const path = parser_parse_ident_path(parser);
    if (path == NULL) {
        error_at_current(parser, "Expected package namespace.");
        return parseres_none();
    }

    ASTNode const node = {
        .type = ANT_PACKAGE,
        .node.package.static_path = path,
        .directives = directives,
    };

    if (!parser_consume(parser, TT_SEMICOLON, "Expected semicolon.")) {
        return parseres_err(node);
    }

    return parseres_ok(node);
}

static ParseResult parser_parse_import(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_IMPORT, "Expected `import` keyword.")) {
        return parseres_none();
    }

    /*
        TODO: should we handle multi imports?
          like `import std::{io, String}`

        and then do we allow that to continue?
          like `import std::{io::{printf, println}, ds::{StringBuffer, strbuf_create}}`
    */

    ImportType type = IT_DEFAULT;

    if (parser_peek(parser).type == TT_DOT) {
        parser_advance(parser);
        assert(parser_consume(parser, TT_SLASH, "Expected './'."));
        type = IT_LOCAL;
    } else if (parser_peek(parser).type == TT_TILDE) {
        parser_advance(parser);
        assert(parser_consume(parser, TT_SLASH, "Expected './'."));
        type = IT_ROOT;
    }

    StaticPath* const path = parser_parse_import_path(parser);
    if (path == NULL) {
        error_at_current(parser, "Expected import target.");
        return parseres_none();
    }

    ASTNode const node = {
        .type = ANT_IMPORT,
        .node.import = {
            .type = type,
            .static_path = path,
        },
        .directives = directives,
    };

    if (!parser_consume(parser, TT_SEMICOLON, "Expected semicolon.")) {
        return parseres_err(node);
    }

    return parseres_ok(node);
}

static ParseResult parser_parse_typedef(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_TYPEDEF, "Expected 'typedef' keyword.")) {
        return parseres_none();
    }

    Token ident = parser_peek(parser);
    assert(parser_consume(parser, TT_IDENTIFIER, "Expected name of typedef."));

    assert(parser_consume(parser, TT_EQUAL, "Expected '=' after typedef."));

    Type* type = parser_parse_type(parser);
    assert(type);

    assert(parser_consume(parser, TT_SEMICOLON, "Expected ';' after typedef."));

    return parseres_ok((ASTNode){
        .type = ANT_TYPEDEF_DECL,
        .node.typedef_decl = {
            .name = {
                .length = ident.length,
                .chars = ident.start,
            },
            .type = type,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_struct_decl(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_STRUCT, "Expected 'struct' keyword.")) {
        return parseres_none();
    }

    assert(parser_consume(parser, TT_IDENTIFIER, "Expected struct name."));
    Token name = parser_peek_prev(parser);

    String* m_name = arena_alloc(parser->arena, sizeof *m_name);
    m_name->length = name.length;
    m_name->chars = name.start;

    assert(parser_consume(parser, TT_LEFT_BRACE, "Expected '{'."));

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
        // TODO

        parser_advance(parser);
        t = parser_peek(parser);
    }

    assert(parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'."));

    return parseres_ok((ASTNode){
        .type = ANT_STRUCT_DECL,
        .node.struct_decl = {
            .maybe_name = m_name,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_lit_str(Parser* const parser, LL_Directive const directives) {
    Token const litstr = parser_peek(parser);

    if (!parser_consume(parser, TT_LITERAL_STRING, "Expected string literal.")) {
        return parseres_none();
    }

    if (litstr.length <= 1) {
        error(parser, "Missing string boundaries.");
        return parseres_err((ASTNode){
            .type = ANT_LITERAL,
            .node.literal = {
                .kind = LK_STR,
                .value.lit_str = {
                    .chars = "",
                    .length = 0,
                },
            },
            .directives = directives,
        });
    }

    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal = {
            .kind = LK_STR,
            .value.lit_str = {
                .chars = litstr.start + 1,
                .length = litstr.length - 2,
            },
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_lit_char_s(Parser* const parser, LL_Directive const directives) {
    Token const litchar_s = parser_peek(parser);

    if (!parser_consume(parser, TT_LITERAL_CHAR, "Expected char literal.")) {
        return parseres_none();
    }

    // token includes single-quotes
    if (litchar_s.length <= 2) {
        error(parser, "Empty character not allowed.");
        static char NULL_CHAR = '\0';
        return parseres_err((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_CHAR,
            .node.literal.value.lit_char = {
                .chars = &NULL_CHAR,
                .length = 1,
            },
            .directives = directives,
        });
    }

    // typical character
    if (litchar_s.length == 3) {
        return parseres_ok((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_CHAR,
            .node.literal.value.lit_char = {
                .chars = litchar_s.start + 1,
                .length = 1,
            },
            .directives = directives,
        });
    }

    // escaped character
    if (litchar_s.start[1] == '\\' && litchar_s.length == 4) {
        return parseres_ok((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_CHAR,
            .node.literal.value.lit_char = {
                .chars = litchar_s.start + 1,
                .length = 2,
            },
            .directives = directives,
        });
    }

    // many characters
    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_CHARS,
        .node.literal.value.lit_chars = {
            .chars = litchar_s.start + 1,
            .length = litchar_s.length - 2,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_lit_number(Parser* const parser, LL_Directive const directives) {
    Token const litnum = parser_peek(parser);

    if (!parser_consume(parser, TT_LITERAL_NUMBER, "Expected number literal.")) {
        return parseres_none();
    }

    String const str = arena_strcpy(parser->arena, (String){ .chars = litnum.start, .length = litnum.length });

    bool has_decimal = false;
    for (size_t i = 0; i < str.length; ++i) {
        if (str.chars[i] == '.') {
            has_decimal = true;
            break;
        }
    }

    if (has_decimal) {
        return parseres_ok((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_FLOAT,
            .node.literal.value.lit_float = atof(str.chars),
            .directives = directives,
        });
    }

    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_INT,
        .node.literal.value.lit_int = atoll(str.chars),
        .directives = directives,
    });
}

static ParseResult parser_parse_lit(Parser* const parser, LL_Directive const directives) {
    switch (parser_peek(parser).type) {
        case TT_LITERAL_STRING: return parser_parse_lit_str(parser, directives);
        case TT_LITERAL_CHAR: return parser_parse_lit_char_s(parser, directives);
        case TT_LITERAL_NUMBER: return parser_parse_lit_number(parser, directives);

        default: break;
    }

    return parseres_none();
}

static ParseResult parser_parse_var_ref(Parser* const parser, LL_Directive const directives) {
    StaticPath* const path = parser_parse_ident_path(parser);

    if (path == NULL) {
        return parseres_none();
    }

    return parseres_ok((ASTNode){
        .type = ANT_VAR_REF,
        .node.var_ref.path = path,
        .directives = directives,
    });
}

static ParseResult parser_parse_fn_call(Parser* const parser, LL_Directive const directives) {
    ASTNode fn_call = {
        .type = ANT_FUNCTION_CALL,
        .node.function_call = {0},
        .directives = directives,
    };

    ParseResult const varref_res = parser_parse_var_ref(parser, (LL_Directive){0});
    if (varref_res.status != PRS_OK) {
        return parseres_none();
    }

    if (parser_peek(parser).type != TT_LEFT_PAREN) {
        return parseres_none();
    }
    parser_advance(parser);
    Token current = parser_peek(parser);

    ASTNode* const target = arena_alloc(parser->arena, sizeof(ASTNode));
    *target = varref_res.node;

    fn_call.node.function_call.function = target;

    while (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
        LL_Directive const expr_directives = parser_parse_directives(parser);
        ParseResult const arg_res = parser_parse_expr(parser, expr_directives);
        ll_ast_push(parser->arena, &fn_call.node.function_call.args, arg_res.node);

        if (arg_res.status != PRS_OK) {
            return parseres_err(fn_call);
        }

        current = parser_peek(parser);

        if (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
            if (!parser_consume(parser, TT_COMMA, "Expected comma.")) {
                continue;
            }

            current = parser_peek(parser);
        }
    }

    if (!parser_consume(parser, TT_RIGHT_PAREN, "Expected ')'.")) {
        return parseres_err(fn_call);
    }

    return parseres_ok(fn_call);
}

static ParseResult parser_parse_sizeof(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_SIZEOF, "Expected 'sizeof'.")) {
        return parseres_none();
    }

    // TODO: support sizeof const expr

    assert(parser_consume(parser, TT_LEFT_PAREN, "Exptected '('"));
    Type* type = parser_parse_type(parser);
    assert(parser_consume(parser, TT_RIGHT_PAREN, "Exptected ')'"));

    return parseres_ok((ASTNode){
        .type = ANT_SIZEOF,
        .node.sizeof_ = {
            .kind = SOK_TYPE,
            .sizeof_.type = type,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_expr(Parser* const parser, LL_Directive const directives) {
    switch (parser_peek(parser).type) {
        case TT_SIZEOF: {
            return parser_parse_sizeof(parser, directives);
        }

        default: break;        
    }
    
    ParseResult expr_res;
    size_t const cached_current = parser->cursor_current;

    expr_res = parser_parse_lit(parser, directives);
    if (expr_res.status == PRS_OK) {
        return expr_res;
    }
    parser->cursor_current = cached_current;

    expr_res = parser_parse_fn_call(parser, directives);
    if (expr_res.status == PRS_OK) {
        return expr_res;
    }
    parser->cursor_current = cached_current;

    expr_res = parser_parse_var_ref(parser, directives);
    if (expr_res.status == PRS_OK) {
        return expr_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_var_decl(Parser* const parser, LL_Directive const directives) {
    Token current = parser_peek(parser);

    bool is_static = false;
    if (current.type == TT_STATIC) {
        is_static = true;
        parser_advance(parser);
        current = parser_peek(parser);
    }

    TypeOrLet tol = {0};
    {
        if (current.type == TT_LET) {
            tol.is_let = true;
            parser_advance(parser);
        } else {
            tol.is_let = false;
            tol.maybe_type = parser_parse_type(parser);
        }
        current = parser_peek(parser);

        if (current.type == TT_MUT) {
            tol.is_mut = true;
            parser_advance(parser);
            current = parser_peek(parser);
        }
    }

    VarDeclLHS lhs = {0};

    lhs.type = VDLT_NAME; // TODO: support others
    lhs.count = 1;

    current = parser_peek(parser);
    if (current.type != TT_IDENTIFIER) {
        return parseres_none();
    }
    parser_advance(parser);

    lhs.lhs.name = (String){
        .length = current.length,
        .chars = current.start,
    };
    current = parser_peek(parser);

    ASTNode* rhs = NULL;

    if (current.type == TT_EQUAL) {
        parser_advance(parser);

        LL_Directive const expr_directives = parser_parse_directives(parser);

        ParseResult res = parser_parse_expr(parser, expr_directives);
        assert(res.status == PRS_OK); // TODO: better error

        rhs = arena_alloc(parser->arena, sizeof *rhs);
        *rhs = res.node;
    }

    if (parser_peek(parser).type != TT_SEMICOLON) {
        debug_token(parser, parser_peek_prev(parser));
        debug_token(parser, parser_peek(parser));
        debug_token(parser, parser_peek_next(parser));
        assert(rhs);
        assert(rhs->type == ANT_VAR_REF);
        assert(rhs->type == ANT_FUNCTION_CALL);
    }
    assert(parser_consume(parser, TT_SEMICOLON, "Expected ';'."));

    return parseres_ok((ASTNode){
        .type = ANT_VAR_DECL,
        .node.var_decl = {
            .is_static = is_static,
            .type_or_let = tol,
            .lhs = lhs,
            .initializer = rhs,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_stmt(Parser* const parser) {
    LL_Directive const directives = parser_parse_directives(parser);
    
    Token const current = parser_peek(parser);

    switch (current.type) {        
        // TODO

        default: break;
    }
    
    ParseResult stmt_res;
    size_t const cached_current = parser->cursor_current;

    stmt_res = parser_parse_var_decl(parser, directives);
    if (stmt_res.status == PRS_OK) {
        // semicolon already consumed
        // parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");
        return stmt_res;
    }
    parser->cursor_current = cached_current;

    stmt_res = parser_parse_expr(parser, directives);
    if (stmt_res.status == PRS_OK) {
        parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");
        return stmt_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_fn_decl(Parser* const parser, LL_Directive const directives) {
    Type const* const type = parser_parse_type(parser);
    if (type == NULL) {
        return parseres_none();
    }

    Token const token = parser_peek(parser);
    if (token.type != TT_IDENTIFIER) {
        return parseres_none();
    }
    parser_advance(parser);

    if (parser_peek(parser).type != TT_LEFT_PAREN) {
        return parseres_none();
    }
    
    char* const cname = arena_memcpy(parser->arena, token.start, token.length + 1);
    cname[token.length] = '\0';

    parser_consume(parser, TT_LEFT_PAREN, "Expected '('.");

    LL_FnParam params = {0};

    Token current = parser_peek(parser);
    while (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
        Type* type = parser_parse_type(parser);
        assert(type);
        current = parser_peek(parser);

        bool is_mut = false;
        if (current.type == TT_MUT) {
            is_mut = true;
            parser_advance(parser);
            current = parser_peek(parser);
        }

        current = parser_peek(parser);
        assert(parser_consume(parser, TT_IDENTIFIER, "Expected fn param name."));

        FnParam param = {
            .type = *type,
            .is_mut = is_mut,
            .name = {
                .length = current.length,
                .chars = current.start,
            },
        };

        current = parser_peek(parser);
        if (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
            assert(parser_consume(parser, TT_COMMA, "Expected comma between params."));
        } else if (current.type == TT_COMMA) {
            parser_advance(parser);
        }
        current = parser_peek(parser);

        ll_param_push(parser->arena, &params, param);
    }

    if (!parser_consume(parser, TT_RIGHT_PAREN, "Expected ')'.")) {
        return parseres_none();
    }

    if (parser_peek(parser).type == TT_SEMICOLON) {
        parser_advance(parser);
        return parseres_ok((ASTNode){
            .type = ANT_FUNCTION_HEADER_DECL,
            .node.function_header_decl = {
                .return_type = *type,
                .name = c_str(cname),
                .params = params,
            },
            .directives = directives,
        });
    }

    ASTNode node = {
        .type = ANT_FUNCTION_DECL,
        .node.function_decl = {
            .header = {
                .return_type = *type,
                .name = c_str(cname),
                .params = params,
            },
            .stmts = {0},
        },
        .directives = directives,
    };

    if (!parser_consume(parser, TT_LEFT_BRACE, "Expected '{'.")) {
        return parseres_err(node);
    }

    current = parser_peek(parser);
    while (current.type != TT_RIGHT_BRACE && current.type != TT_EOF) {
        current = parser_peek(parser);

        // ignore semicolons (empty statements)
        while (current.type == TT_SEMICOLON || current.type == TT_ERROR) {
            parser_advance(parser);
            current = parser_peek(parser);
        }

        if (current.type == TT_RIGHT_BRACE || current.type == TT_EOF) {
            break;
        }

        ParseResult const stmt_res = parser_parse_stmt(parser);
        ll_ast_push(parser->arena, &node.node.function_decl.stmts, stmt_res.node);

        if (stmt_res.status != PRS_OK) {
            if (stmt_res.status == PRS_NONE) {
                error_at(parser, &current, "Expected statement.");
            }
            parser_advance(parser);
        }
    }
    
    if (!parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'.")) {
        return parseres_err(node);
    }

    return parseres_ok(node);
}

static ParseResult parser_parse_file_separator(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_MINUS_MINUS_MINUS, "Expected '---' (file separator).")) {
        return parseres_none();
    }

    assert(directives.length == 0);

    return parseres_ok((ASTNode){
        .type = ANT_FILE_SEPARATOR,
        .node.file_separator = NULL,
        .directives = directives,
    });
}

static ParseResult parser_parse_filescope_decl(Parser* const parser) {
    LL_Directive const directives = parser_parse_directives(parser);

    Token const current = parser_peek(parser);

    switch (current.type) {
        case TT_MINUS_MINUS_MINUS: return parser_parse_file_separator(parser, directives);
        
        case TT_PACKAGE: return parser_parse_package(parser, directives);
        case TT_IMPORT: return parser_parse_import(parser, directives);
        case TT_TYPEDEF: return parser_parse_typedef(parser, directives);
        case TT_STRUCT: return parser_parse_struct_decl(parser, directives);

        default: break;
    }

    ParseResult decl_res;
    size_t const cached_current = parser->cursor_current;

    decl_res = parser_parse_fn_decl(parser, directives);
    if (decl_res.status == PRS_OK) {
        return decl_res;
    }
    parser->cursor_current = cached_current;

    decl_res = parser_parse_var_decl(parser, directives);
    if (decl_res.status == PRS_OK) {
        return decl_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_node(Parser* const parser) {
    if (parser_peek(parser).type == TT_EOF) {
        return parseres_none();
    }
    
    return parser_parse_filescope_decl(parser);
}

Parser parser_create(Arena* const arena, ArrayList_Token const tokens) {
    return (Parser){
        .arena = arena,
        .tokens = tokens,
        .cursor_start = 0,
        .cursor_current = 0,

        .had_error = false,
        .panic_mode = false,
    };
}

ASTNodeResult parser_parse(Parser* const parser) {
    LL_ASTNode nodes = {0};

    while (!parser_is_at_end(parser)) {
        parser->cursor_start = parser->cursor_current;
    
        ParseResult const node_res = parser_parse_node(parser);
        if (node_res.status != PRS_OK) {
            error_at_current(parser, "Unexpected error parsing tokens.");
            parser_advance(parser);
        }

        ll_ast_push(parser->arena, &nodes, node_res.node);
    }

    ASTNode* const file_root = arena_alloc(parser->arena, sizeof(ASTNode));
    file_root->type = ANT_FILE_ROOT;
    file_root->node.file_root = (ASTNodeFileRoot){ .nodes = nodes };

    return astres_ok(file_root);
}
