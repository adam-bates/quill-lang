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

static Type* parser_parse_type(Parser* const parser);
static ParseResult parser_parse_stmt(Parser* const parser);
static ASTNodeStatementBlock parser_parse_stmt_block(Parser* const parser);
static ParseResult parser_parse_simple_expr(Parser* const parser, LL_Directive const directives);
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

static const size_t DIRECTIVE_MATCHES_LEN = 6;
static const DirectiveMatch DIRECTIVE_MATCHES[DIRECTIVE_MATCHES_LEN] = {
    { "@c_header", DT_C_HEADER },
    { "@c_restrict", DT_C_RESTRICT },
    { "@c_FILE", DT_C_FILE },
    { "@ignore_unused", DT_IGNORE_UNUSED },
    { "@impl", DT_IMPL },
    { "@string_literal", DT_STRING_LITERAL },
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
        case TT_DOT_DOT: printf(".."); break;
        case TT_DOT_DOT_EQUAL: printf("..="); break;
        case TT_TILDE: printf("~"); break;
        case TT_SEMICOLON: printf(";"); break;
        case TT_QUESTION: printf("?"); break;
        case TT_AT: printf("@"); break;
        case TT_PERCENT: printf("%%"); break;

        case TT_BANG: printf("!"); break;
        case TT_BANG_EQUAL: printf("!="); break;
        case TT_EQUAL: printf("="); break;
        case TT_EQUAL_EQUAL: printf("=="); break;
        case TT_PLUS: printf("+"); break;
        case TT_PLUS_EQUAL: printf("+="); break;
        case TT_PLUS_PLUS: printf("++"); break;
        case TT_MINUS: printf("-"); break;
        case TT_MINUS_EQUAL: printf("-="); break;
        case TT_MINUS_GREATER: printf("->"); break;
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

        case TT_CRASH: printf("CRASH"); break;
        case TT_ELSE: printf("else"); break;
        case TT_ENUM: printf("enum"); break;
        case TT_FALSE: printf("false"); break;
        case TT_FOR: printf("for"); break;
        case TT_FOREACH: printf("foreach"); break;
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

static StaticPath* parser_parse_static_path(Parser* const parser) {
    Token const ident = parser_peek(parser);

    if (ident.type != TT_IDENTIFIER) {
        return NULL;
    }
    parser_advance(parser);

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    StaticPath* path = arena_alloc(parser->arena, sizeof *path);
    path->name = name;
    path->child = NULL;

    if (parser_peek(parser).type != TT_COLON_COLON) {
        return path;
    }
    parser_advance(parser);

    path->child = parser_parse_static_path(parser);
    return path;
}

static PackagePath* parser_parse_package_path(Parser* const parser) {
    Token const ident = parser_peek(parser);

    if (ident.type != TT_IDENTIFIER) {
        return NULL;
    }
    parser_advance(parser);

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    PackagePath* path = arena_alloc(parser->arena, sizeof *path);
    path->name = name;
    path->child = NULL;

    if (parser_peek(parser).type != TT_SLASH) {
        return path;
    }
    parser_advance(parser);

    path->child = parser_parse_package_path(parser);
    return path;
}

static ImportStaticPath* _parser_parse_import_static_path(Parser* const parser) {
    Token const ident = parser_peek(parser);

    if (ident.type == TT_STAR) {
        parser_advance(parser);

        ImportStaticPath* path = arena_alloc(parser->arena, sizeof *path);
        path->type = ISPT_WILDCARD;
        path->import.wildcard = NULL;

        return path;
    }

    if (ident.type != TT_IDENTIFIER) {
        return NULL;
    }
    parser_advance(parser);

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    ImportStaticPath* path = arena_alloc(parser->arena, sizeof *path);
    path->type = ISPT_IDENT;
    path->import.ident.name = name;
    path->import.ident.child = NULL;

    if (parser_peek(parser).type != TT_COLON_COLON) {
        return path;
    }
    parser_advance(parser);

    path->import.ident.child = _parser_parse_import_static_path(parser);
    return path;
}

static ImportPath* parser_parse_import_path(Parser* const parser) {
    Token const ident = parser_peek(parser);

    if (ident.type != TT_IDENTIFIER) {
        return NULL;
    }
    parser_advance(parser);

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    ImportPath* path = arena_alloc(parser->arena, sizeof *path);

    Token const delim = parser_peek(parser);
    if (delim.type == TT_COLON_COLON) {
        parser_advance(parser);

        if (parser_peek(parser).type != TT_IDENTIFIER && parser_peek(parser).type != TT_STAR) {
            assert(parser_consume(parser, TT_IDENTIFIER, "Exptected ident after '::' in import."));
        }

        path->type = IPT_FILE;
        path->import.file = (ImportFilePath){
            .name = name,
            .child = _parser_parse_import_static_path(parser),
        };

        return path;
    }

    if (delim.type != TT_SLASH) {
        path->type = IPT_FILE;
        path->import.file = (ImportFilePath){
            .name = name,
            .child = NULL,
        };
        return path;
    }
    parser_advance(parser);

    if (!parser_consume(parser, TT_IDENTIFIER, "Expected ident after '/' in import.")) {
        path->type = IPT_FILE;
        path->import.file = (ImportFilePath){
            .name = name,
            .child = NULL,
        };
        return path;
    }
    parser->cursor_current -= 1;

    path->type = IPT_DIR;
    path->import.dir = (ImportDirPath){
        .name = name,
        .child = parser_parse_import_path(parser),
    };

    return path;
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

            case DT_STRING_LITERAL: {
                Directive directive = {
                    .type = DT_STRING_LITERAL,
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
                .id = { parser->next_type_id++ },
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
                .id = { parser->next_type_id++ },
                .kind = TK_MUT_POINTER,
                .type.mut_ptr.of = type,
            };
            return parser_parse_type_wrap(parser, wrapped);
        }

        case TT_LESS: {
            // Only generics on defined types, not built-ins or wraps
            assert(type->kind == TK_STATIC_PATH);
            assert(type->type.static_path.generic_types.length == 0);
            parser_advance(parser);

            t = parser_peek(parser);
            while (t.type != TT_GREATER && t.type != TT_EOF) {
                Type* generic = parser_parse_type(parser);
                assert(generic);

                ll_type_push(parser->arena, &type->type.static_path.generic_types, *generic);

                t = parser_peek(parser);

                if (t.type != TT_GREATER && t.type != TT_EOF) {
                    assert(parser_consume(parser, TT_COMMA, "Expected ',' between generics"));
                    t = parser_peek(parser);
                }
            }
            if (parser_peek(parser).type == TT_COMMA) {
                parser_advance(parser);
            }
            assert(parser_consume(parser, TT_GREATER, "Expected '>' to close generic type"));

            return parser_parse_type_wrap(parser, type);
        }

        case TT_LEFT_BRACKET: {
            parser_advance(parser);
            Token t = parser_peek(parser);
            Token* explicit_size = NULL;
            if (t.type != TT_RIGHT_BRACKET) {
                assert(t.type == TT_LITERAL_NUMBER || t.type == TT_LITERAL_CHAR || t.type == TT_TRUE || t.type == TT_FALSE);
                explicit_size = parser->tokens.array + parser->cursor_current;
                parser_advance(parser);
            }
            assert(parser_consume(parser, TT_RIGHT_BRACKET, "Exptected ']'"));

            Type* wrapped = arena_alloc(parser->arena, sizeof *wrapped);
            *wrapped = (Type){
                .id = { parser->next_type_id++ },
                .kind = TK_ARRAY,
                .type.array = {
                    .explicit_size = explicit_size,
                    .of = type,
                },
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
    type->id.val = parser->next_type_id++;
    type->directives = directives;

    Token t = parser_peek(parser);
    switch (t.type) {
        case TT_IDENTIFIER: {
            StaticPath* path = parser_parse_static_path(parser);
            assert(path);

            type->kind = TK_STATIC_PATH;
            type->type.static_path.path = path;
            type->type.static_path.generic_types = (LL_Type){0};
            type->type.static_path.impl_version = 0;

            if (parser_peek(parser).type == TT_LESS) {
                parser_advance(parser);

                Token t = parser_peek(parser);
                while (t.type != TT_GREATER && t.type != TT_EOF) {
                    Type* generic_arg = parser_parse_type(parser);
                    assert(generic_arg);

                    ll_type_push(parser->arena, &type->type.static_path.generic_types, *generic_arg);
                    t = parser_peek(parser);

                    if (t.type != TT_GREATER && t.type != TT_EOF) {
                        assert(parser_consume(parser, TT_COMMA, "Expected ',' between generic args"));
                    }
                }
                if (parser_peek(parser).type == TT_COMMA) {
                    parser_advance(parser);
                }

                assert(parser_consume(parser, TT_GREATER, "Expected '>' to close generic args"));
            }

            return parser_parse_type_wrap(parser, type);
        }

        case TT_VOID: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_VOID;
            return parser_parse_type_wrap(parser, type);
        }

        case TT_BOOL: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_BOOL;
            return parser_parse_type_wrap(parser, type);
        }

        case TT_INT: {
            parser_advance(parser);

            type->kind = TK_BUILT_IN;
            type->type.built_in = TBI_INT;
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

        default: return NULL;
        // default: debug_token(parser, t); fprintf(stderr, "TODO: handle [%d]\n", t.type); assert(false);
    }
    
    return NULL;
}

static ParseResult parser_parse_package(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_PACKAGE, "Expected `pacakge` keyword.")) {
        return parseres_none();
    }

    PackagePath* const path = parser_parse_package_path(parser);
    if (path == NULL) {
        error_at_current(parser, "Expected package namespace.");
        return parseres_none();
    }

    ASTNode const node = {
        .id = { parser->next_node_id++ },
        .type = ANT_PACKAGE,
        .node.package.package_path = path,
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

    ImportPath* const path = parser_parse_import_path(parser);
    if (path == NULL) {
        error_at_current(parser, "Expected import target.");
        return parseres_none();
    }

    ASTNode const node = {
        .id = { parser->next_node_id++ },
        .type = ANT_IMPORT,
        .node.import = {
            .type = type,
            .import_path = path,
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
        .id = { parser->next_node_id++ },
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

    ArrayList_String generic_params = arraylist_string_create(parser->arena);

    if (parser_peek(parser).type == TT_LESS) {
        parser_advance(parser);

        Token t = parser_peek(parser);
        while (t.type != TT_GREATER && t.type != TT_EOF) {
            assert(parser_consume(parser, TT_IDENTIFIER, "Expected identifier for generic param"));

            arraylist_string_push(&generic_params, (String){
                .length = t.length,
                .chars = t.start,
            });
            t = parser_peek(parser);

            if (t.type != TT_GREATER && t.type != TT_EOF) {
                assert(parser_consume(parser, TT_COMMA, "Expected ',' between generic params"));
            }
        }
        if (parser_peek(parser).type == TT_COMMA) {
            parser_advance(parser);
        }

        assert(parser_consume(parser, TT_GREATER, "Expected '>' to close generic params"));
    }

    assert(parser_consume(parser, TT_LEFT_BRACE, "Expected '{'."));

    LL_StructField fields = {0};

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
        Type* type = parser_parse_type(parser);
        assert(type);

        t = parser_peek(parser);
        String name = (String){
            .length = t.length,
            .chars = t.start,
        };

        ll_field_push(parser->arena, &fields, (StructField){
            .type = type,
            .name = name,
        });

        parser_advance(parser);
        t = parser_peek(parser);

        if (t.type != TT_RIGHT_BRACE && t.type != TT_EOF || t.type == TT_COMMA) {
            assert(parser_consume(parser, TT_COMMA, "Expected comma between struct fields"));
            t = parser_peek(parser);
        }
    }

    assert(parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'."));

    ArrayList_LL_Type generic_impls = {0};
    if (generic_params.length > 0) {
        generic_impls = arraylist_typells_create(parser->arena);
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_STRUCT_DECL,
        .node.struct_decl = {
            .maybe_name = m_name,
            .fields = fields,
            .generic_params = generic_params,
            .generic_impls = generic_impls,
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
            .id = { parser->next_node_id++ },
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
        .id = { parser->next_node_id++ },
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

static ParseResult parser_parse_lit_str_template(Parser* const parser, LL_Directive const directives) {
    Token t = parser_peek(parser);

    switch (t.type) {
        case TT_LITERAL_STRING_TEMPLATE_FULL: {
            ArrayList_String str_parts = arraylist_string_create(parser->arena);

            arraylist_string_push(&str_parts, (String){
                .length = t.length,
                .chars = t.start,
            });
            parser_advance(parser);

            return parseres_ok((ASTNode){
                .id = { parser->next_node_id++ },
                .type = ANT_TEMPLATE_STRING,
                .node.template_string = {
                    .template_expr_parts = {0},
                    .str_parts = str_parts,
                },
                .directives = directives,
            });
        }

        case TT_LITERAL_STRING_TEMPLATE_START: {
            LL_ASTNode expr_parts = {0};
            ArrayList_String str_parts = arraylist_string_create(parser->arena);

            arraylist_string_push(&str_parts, (String){
                .length = t.length,
                .chars = t.start,
            });
            parser_advance(parser);

            size_t iter;
            for (iter = 0; iter < 256; ++iter) {
                ParseResult expr_res = parser_parse_expr(parser, (LL_Directive){0});
                assert(expr_res.status == PRS_OK);

                ll_ast_push(parser->arena, &expr_parts, expr_res.node);

                t = parser_peek(parser);
                assert(t.type == TT_LITERAL_STRING_TEMPLATE_CONT);

                arraylist_string_push(&str_parts, (String){
                    .length = t.length,
                    .chars = t.start,
                });

                parser_advance(parser);

                if (*t.start == '}' && t.start[t.length - 1] == '`') {
                    break;
                }
            }
            assert(iter < 256);
            assert(expr_parts.length + 1 == str_parts.length);

            // printf("[\n");
            // for (size_t i = 0; i < str_parts.length; ++i) {
            //     printf("    \"%s\",\n", arena_strcpy(parser->arena, str_parts.array[i]).chars);
            // }
            // printf("]\n");

            return parseres_ok((ASTNode){
                .id = { parser->next_node_id++ },
                .type = ANT_TEMPLATE_STRING,
                .node.template_string = {
                    .template_expr_parts = expr_parts,
                    .str_parts = str_parts,
                },
                .directives = directives,
            });
        }

        default: return parseres_none();
    }
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
            .id = { parser->next_node_id++ },
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
            .id = { parser->next_node_id++ },
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
            .id = { parser->next_node_id++ },
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
        .id = { parser->next_node_id++ },
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
            .id = { parser->next_node_id++ },
            .type = ANT_LITERAL,
            .node.literal.kind = LK_FLOAT,
            .node.literal.value.lit_float = atof(str.chars),
            .directives = directives,
        });
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_LITERAL,
        .node.literal.kind = LK_INT,
        .node.literal.value.lit_int = atoll(str.chars),
        .directives = directives,
    });
}

static ParseResult parser_parse_bool(Parser* const parser, LL_Directive const directives) {
    Token t = parser_peek(parser);
    if (t.type != TT_TRUE && t.type != TT_FALSE) {
        return parseres_none();
    }
    parser_advance(parser);
    bool is_true = t.type == TT_TRUE;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_LITERAL,
        .node.literal = {
            .kind = LK_BOOL,
            .value.lit_bool = is_true,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_lit(Parser* const parser, LL_Directive const directives) {
    switch (parser_peek(parser).type) {
        case TT_TRUE:
        case TT_FALSE:
            return parser_parse_bool(parser, directives);

        case TT_LITERAL_STRING: return parser_parse_lit_str(parser, directives);
        case TT_LITERAL_CHAR: return parser_parse_lit_char_s(parser, directives);
        case TT_LITERAL_NUMBER: return parser_parse_lit_number(parser, directives);

        case TT_LITERAL_STRING_TEMPLATE_START:
        case TT_LITERAL_STRING_TEMPLATE_FULL:
            return parser_parse_lit_str_template(parser, directives);

        default: break;
    }

    return parseres_none();
}

static ParseResult parser_parse_var_ref(Parser* const parser, LL_Directive const directives) {
    StaticPath* const path = parser_parse_static_path(parser);

    if (path == NULL) {
        return parseres_none();
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_VAR_REF,
        .node.var_ref.path = path,
        .directives = directives,
    });
}

static ParseResult parser_parse_index(Parser* const parser, ASTNode expr) {
    if (parser_peek(parser).type != TT_LEFT_BRACKET) {
        return parseres_none();
    }
    parser_advance(parser);

    ParseResult value_res = parser_parse_simple_expr(parser, (LL_Directive){0});
    if (value_res.status != PRS_OK) {
        return parseres_none();
    }

    assert(parser_consume(parser, TT_RIGHT_BRACKET, "Exptected `]`"));

    ASTNode* root = arena_alloc(parser->arena, sizeof *root);
    *root = expr;

    ASTNode* value = arena_alloc(parser->arena, sizeof *value);
    *value = value_res.node;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_INDEX,
        .node.index = {
            .root = root,
            .value = value,
        },
        .directives = {0},
    });
}

static ParseResult parser_parse_range(Parser* const parser, ASTNode expr) {
    if (parser_peek(parser).type != TT_DOT_DOT && parser_peek(parser).type != TT_DOT_DOT_EQUAL) {
        return parseres_none();
    }
    bool inclusive = parser_peek(parser).type == TT_DOT_DOT_EQUAL;
    parser_advance(parser);

    ParseResult rhs_res = parser_parse_simple_expr(parser, (LL_Directive){0});
    assert(rhs_res.status == PRS_OK);

    ASTNode* lhs = arena_alloc(parser->arena, sizeof *lhs);
    *lhs = expr;

    ASTNode* rhs = arena_alloc(parser->arena, sizeof *rhs);
    *rhs = rhs_res.node;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_RANGE,
        .node.range = {
            .lhs = lhs,
            .rhs = rhs,
            .inclusive = inclusive,
        },
        .directives = {0},
    });
}

static ParseResult parser_parse_get_field(Parser* const parser, ASTNode expr) {
    Token t = parser_peek(parser);
    if (t.type != TT_DOT && t.type != TT_MINUS_GREATER) {
        return parseres_none();
    }
    bool is_ptr_deref = t.type == TT_MINUS_GREATER;
    parser_advance(parser);

    Token name = parser_peek(parser);
    assert(parser_consume(parser, TT_IDENTIFIER, "Expected field name."));

    ASTNode* const root = arena_alloc(parser->arena, sizeof *root);
    *root = expr;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_GET_FIELD,
        .node.get_field = {
            .root = root,
            .is_ptr_deref = is_ptr_deref,
            .name = {
                .length = name.length,
                .chars = name.start,
            },
        },
        .directives = {0},
    });
}

static ParseResult parser_parse_fn_call(Parser* const parser, ASTNode expr) {
    if (parser_peek(parser).type != TT_LEFT_PAREN) {
        return parseres_none();
    }
    parser_advance(parser);
    Token current = parser_peek(parser);

    ASTNode* const target = arena_alloc(parser->arena, sizeof *target);
    *target = expr;

    ASTNode fn_call = {
        .id = { parser->next_node_id++ },
        .type = ANT_FUNCTION_CALL,
        .node.function_call = {
            .function = target,
            .args = {0},
        },
        .directives = {0},
    };

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
        .id = { parser->next_node_id++ },
        .type = ANT_SIZEOF,
        .node.sizeof_ = {
            .kind = SOK_TYPE,
            .sizeof_.type = type,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_tuple(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_LEFT_PAREN, "Expected '('.")) {
        return parseres_none();
    }

    LL_ASTNode exprs = {0};

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_PAREN && t.type != TT_EOF) {
        ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
        assert(res.status == PRS_OK);

        ll_ast_push(parser->arena, &exprs, res.node);

        t = parser_peek(parser);
        if (t.type != TT_RIGHT_PAREN && t.type != TT_EOF) {
            assert(parser_consume(parser, TT_COMMA, "Expected ','"));
            t = parser_peek(parser);
        }
    }

    assert(parser_consume(parser, TT_RIGHT_PAREN, "Exptected ')'."));

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_TUPLE,
        .node.tuple.exprs = exprs,
        .directives = directives,
    });
}

static ParseResult parser_parse_struct_init(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_DOT, "Expected '.'")) {
        return parseres_none();
    }
    assert(parser_consume(parser, TT_LEFT_BRACE, "Expected '{'."));

    LL_StructFieldInit fields = {0};

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
        assert(parser_consume(parser, TT_DOT, "Expected '.'"));

        t = parser_peek(parser);
        assert(parser_consume(parser, TT_IDENTIFIER, "Expected name"));
        String name = {
            .length = t.length,
            .chars = t.start,
        };

        assert(parser_consume(parser, TT_EQUAL, "Expected '='"));

        ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
        assert(res.status == PRS_OK);

        ASTNode* value = arena_alloc(parser->arena, sizeof *value);
        *value = res.node;

        ll_field_init_push(parser->arena, &fields, (StructFieldInit){
            .name = name,
            .value = value,
        });

        t = parser_peek(parser);
        if (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
            assert(parser_consume(parser, TT_COMMA, "Expected ','"));
            t = parser_peek(parser);
        }
    }

    assert(parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'."));

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_STRUCT_INIT,
        .node.struct_init.fields = fields,
        .directives = directives,
    });
}

static ParseResult parser_parse_array_init(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_LEFT_BRACKET, "Expected '['.")) {
        return parseres_none();
    }

    ASTNode* maybe_explicit_length = NULL;
    if (parser_peek(parser).type != TT_RIGHT_BRACKET) {
        ParseResult res = parser_parse_simple_expr(parser, (LL_Directive){0});
        assert(res.status == PRS_OK);

        maybe_explicit_length = arena_alloc(parser->arena, sizeof *maybe_explicit_length);
        *maybe_explicit_length = res.node;
    }
    assert(parser_consume(parser, TT_RIGHT_BRACKET, "Expected ']'."));

    assert(parser_consume(parser, TT_LEFT_BRACE, "Expected '{'."));

    LL_ArrayInitElem elems = {0};

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
        size_t cached_current = parser->cursor_current;

        ParseResult s_res = parser_parse_simple_expr(parser, (LL_Directive){0});
        if (s_res.status == PRS_OK) {
            if (parser_peek(parser).type == TT_EQUAL) {
                ASTNode* maybe_index = arena_alloc(parser->arena, sizeof *maybe_index);
                *maybe_index = s_res.node;

                ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
                assert(res.status == PRS_OK);

                ASTNode* value = arena_alloc(parser->arena, sizeof *value);
                *value = res.node;
                ll_array_init_elem_push(parser->arena, &elems, (ArrayInitElem){
                    .maybe_index = NULL,
                    .value = value,
                });
            } else if (parser_peek(parser).type == TT_COMMA || parser_peek(parser).type == TT_RIGHT_BRACE) {
                ASTNode* value = arena_alloc(parser->arena, sizeof *value);
                *value = s_res.node;
                ll_array_init_elem_push(parser->arena, &elems, (ArrayInitElem){
                    .maybe_index = NULL,
                    .value = value,
                });
            } else {
                assert(parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'"));
            }
        } else {
            parser->cursor_current = cached_current;

            ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
            assert(res.status == PRS_OK);

            ASTNode* value = arena_alloc(parser->arena, sizeof *value);
            *value = res.node;
            ll_array_init_elem_push(parser->arena, &elems, (ArrayInitElem){
                .maybe_index = NULL,
                .value = value,
            });
        }

        t = parser_peek(parser);
        if (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
            assert(parser_consume(parser, TT_COMMA, "Expected ','"));
            t = parser_peek(parser);
        }
    }

    assert(parser_consume(parser, TT_RIGHT_BRACE, "Exptected '}'."));

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_ARRAY_INIT,
        .node.array_init = {
            .maybe_explicit_length = maybe_explicit_length,
            .elems = elems,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_unary(Parser* const parser, LL_Directive const directives) {
    UnaryOp op;
    switch (parser_peek(parser).type) {
        case TT_BANG:      op = UO_BOOL_NEGATE; break;
        case TT_MINUS:     op = UO_NUM_NEGATE; break;
        case TT_AMPERSAND: op = UO_PTR_REF; break;
        case TT_STAR:      op = UO_PTR_DEREF; break;

        case TT_PLUS_PLUS: op = UO_PLUS_PLUS; break;
        case TT_MINUS_MINUS: op = UO_MINUS_MINUS; break;

        default: return parseres_none();
    }
    parser_advance(parser);

    ParseResult rhs_res = parser_parse_expr(parser, directives);

    ASTNode* rhs = arena_alloc(parser->arena, sizeof *rhs);
    *rhs = rhs_res.node;

    ASTNode unary = {
        .id = { parser->next_node_id++ },
        .type = ANT_UNARY_OP,
        .node.unary_op = {
            .op = op,
            .right = rhs,
        },
    };

    if (rhs_res.status != PRS_OK) {
        return parseres_err(unary);
    }

    return parseres_ok(unary);
}

static ParseResult parser_parse_postfix(Parser* const parser, ASTNode expr) {
    PostfixOp op;
    switch (parser_peek(parser).type) {
        case TT_PLUS_PLUS: op = PFO_PLUS_PLUS; break;
        case TT_MINUS_MINUS: op = PFO_MINUS_MINUS; break;

        default: return parseres_none();
    }
    parser_advance(parser);

    ASTNode* left = arena_alloc(parser->arena, sizeof *left);
    *left = expr;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_POSTFIX_OP,
        .node.postfix_op = {
            .left = left,
            .op = op,
        },
        .directives = {0},
    });
}

static ParseResult parser_wrap_simple_expr(Parser* const parser, ParseResult expr_res) {
    if (expr_res.status != PRS_OK) {
        return expr_res;
    }
    ASTNode expr = expr_res.node;

    switch (parser_peek(parser).type) {
        case TT_PLUS_PLUS: return parser_wrap_simple_expr(parser, parser_parse_postfix(parser, expr));

        default: break;
    }

    ParseResult wrapped_res;
    size_t const cached_current = parser->cursor_current;

    wrapped_res = parser_parse_fn_call(parser, expr);
    if (wrapped_res.status == PRS_OK) {
        wrapped_res.node.directives = expr.directives;
        expr.directives = (LL_Directive){0};
        return parser_wrap_simple_expr(parser, wrapped_res);
    }
    parser->cursor_current = cached_current;

    wrapped_res = parser_parse_get_field(parser, expr);
    if (wrapped_res.status == PRS_OK) {
        wrapped_res.node.directives = expr.directives;
        expr.directives = (LL_Directive){0};
        return parser_wrap_simple_expr(parser, wrapped_res);
    }
    parser->cursor_current = cached_current;

    wrapped_res = parser_parse_index(parser, expr);
    if (wrapped_res.status == PRS_OK) {
        wrapped_res.node.directives = expr.directives;
        expr.directives = (LL_Directive){0};
        return parser_wrap_simple_expr(parser, wrapped_res);
    }
    parser->cursor_current = cached_current;

    return expr_res;
}

static ParseResult parser_parse_simple_expr(Parser* const parser, LL_Directive const directives) {
    switch (parser_peek(parser).type) {
        case TT_SIZEOF: return parser_wrap_simple_expr(parser, parser_parse_sizeof(parser, directives));
        case TT_LEFT_PAREN: return parser_wrap_simple_expr(parser, parser_parse_tuple(parser, directives));

        default: break;
    }

    ParseResult expr_res;
    size_t const cached_current = parser->cursor_current;

    expr_res = parser_parse_unary(parser, directives);
    if (expr_res.status == PRS_OK) {
        return parser_wrap_simple_expr(parser, expr_res);
    }
    parser->cursor_current = cached_current;

    expr_res = parser_parse_lit(parser, directives);
    if (expr_res.status == PRS_OK) {
        return parser_wrap_simple_expr(parser, expr_res);
    }
    parser->cursor_current = cached_current;

    expr_res = parser_parse_var_ref(parser, directives);
    if (expr_res.status == PRS_OK) {
        return parser_wrap_simple_expr(parser, expr_res);
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_binary(Parser* const parser, ASTNode expr) {
    BinaryOp op;
    switch (parser_peek(parser).type) {
        case TT_PLUS: op = BO_ADD; break;
        case TT_MINUS: op = BO_SUBTRACT; break;
        case TT_STAR: op = BO_MULTIPLY; break;
        case TT_SLASH: op = BO_DIVIDE; break;
        case TT_PERCENT: op = BO_MODULO; break;

        case TT_PIPE_PIPE: op = BO_BOOL_OR; break;
        case TT_AMPERSAND_AMPERSAND: op = BO_BOOL_AND; break;

        case TT_EQUAL_EQUAL: op = BO_EQ; break;
        case TT_BANG_EQUAL: op = BO_NOT_EQ; break;

        case TT_LESS: op = BO_LESS; break;
        case TT_LESS_EQUAL: op = BO_LESS; break;
        case TT_GREATER: op = BO_GREATER; break;
        case TT_GREATER_EQUAL: op = BO_GREATER_OR_EQ; break;

        case TT_PIPE: op = BO_BIT_OR; break;
        case TT_AMPERSAND: op = BO_BIT_AND; break;
        case TT_CARET: op = BO_BIT_XOR; break;

        default: return parseres_none();
    }
    parser_advance(parser);

    ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
    assert(res.status == PRS_OK);

    ASTNode* lhs = arena_alloc(parser->arena, sizeof *lhs);
    *lhs = expr;

    ASTNode* rhs = arena_alloc(parser->arena, sizeof *rhs);
    *rhs = res.node;

    LL_Directive directives = {0};

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_BINARY_OP,
        .node.binary_op = {
            .lhs = lhs,
            .op = op,
            .rhs = rhs,
        },
        .directives = directives,
    });
}

static ParseResult parser_wrap_expr(Parser* const parser, ParseResult expr_res) {
    if (expr_res.status != PRS_OK) {
        return expr_res;
    }
    ASTNode expr = expr_res.node;

    ParseResult wrapped_res;
    size_t const cached_current = parser->cursor_current;

    wrapped_res = parser_parse_binary(parser, expr);
    if (wrapped_res.status == PRS_OK) {
        wrapped_res.node.directives = expr.directives;
        expr.directives = (LL_Directive){0};
        return parser_wrap_expr(parser, wrapped_res);
    }
    parser->cursor_current = cached_current;

    wrapped_res = parser_parse_range(parser, expr);
    if (wrapped_res.status == PRS_OK) {
        wrapped_res.node.directives = expr.directives;
        expr.directives = (LL_Directive){0};
        return parser_wrap_expr(parser, wrapped_res);
    }
    parser->cursor_current = cached_current;

    return expr_res;
}

static ParseResult parser_parse_expr(Parser* const parser, LL_Directive const directives) {
    switch (parser_peek(parser).type) {
        case TT_DOT: return parser_wrap_expr(parser, parser_parse_struct_init(parser, directives));
        case TT_LEFT_BRACKET: return parser_wrap_expr(parser, parser_parse_array_init(parser, directives));

        default: break;
    }

    ParseResult expr_res;
    size_t const cached_current = parser->cursor_current;

    expr_res = parser_parse_simple_expr(parser, directives);
    if (expr_res.status == PRS_OK) {
        return parser_wrap_expr(parser, expr_res);
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
            if (!tol.maybe_type) {
                return parseres_none();
            }
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
        .id = { parser->next_node_id++ },
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

static ParseResult parser_parse_return(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_RETURN, "Expected 'return'")) {
        return parseres_none();
    }
    
    ASTNode* m_expr = NULL;

    if (parser_peek(parser).type != TT_SEMICOLON && parser_peek(parser).type != TT_COLON) {

        ParseResult res = parser_parse_expr(parser, (LL_Directive){0});
        if (res.status != PRS_NONE) {
            m_expr = arena_alloc(parser->arena, sizeof *m_expr);
            *m_expr = res.node;
        }
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_RETURN,
        .node.return_.maybe_expr = m_expr,
        .directives = directives,
    });
}

static ParseResult parser_parse_defer(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_DEFER, "Expected 'defer'")) {
        return parseres_none();
    }

    ParseResult res = parser_parse_stmt(parser);
    assert(res.status == PRS_OK);

    ASTNode* stmt = arena_alloc(parser->arena, sizeof *stmt);
    *stmt = res.node;

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_DEFER,
        .node.defer.stmt = stmt,
        .directives = directives,
    });
}

static ParseResult parser_parse_if(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_IF, "Expected 'if'")) {
        return parseres_none();
    }

    ParseResult cond_res = parser_parse_expr(parser, (LL_Directive){0});
    assert(cond_res.status != PRS_NONE);

    ASTNode* cond = arena_alloc(parser->arena, sizeof *cond);
    *cond = cond_res.node;

    ASTNodeStatementBlock* block = arena_alloc(parser->arena, sizeof *block);
    *block = parser_parse_stmt_block(parser);

    ASTNode* else_ = NULL;
    if (parser_peek(parser).type == TT_ELSE) {
        parser_advance(parser);

        else_ = arena_alloc(parser->arena, sizeof *else_);

        if (parser_peek(parser).type == TT_IF) {
            ParseResult res = parser_parse_if(parser, (LL_Directive){0});
            assert(res.status == PRS_OK);
            *else_ = res.node;
        } else {
            ASTNodeStatementBlock else_block = parser_parse_stmt_block(parser);
            *else_ = (ASTNode){
                .id = { parser->next_node_id++ },
                .type = ANT_STATEMENT_BLOCK,
                .node.statement_block = else_block,
                .directives = {0},
            };
        }
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_IF,
        .node.if_ = {
            .cond = cond,
            .block = block,
            .else_ = else_,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_while(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_WHILE, "Expected 'while'")) {
        return parseres_none();
    }

    ParseResult cond_res = parser_parse_expr(parser, (LL_Directive){0});
    assert(cond_res.status != PRS_NONE);

    ASTNode* cond = arena_alloc(parser->arena, sizeof *cond);
    *cond = cond_res.node;

    ASTNodeStatementBlock* block = arena_alloc(parser->arena, sizeof *block);
    *block = parser_parse_stmt_block(parser);

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_WHILE,
        .node.while_ = {
            .cond = cond,
            .block = block,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_foreach(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_FOREACH, "Expected 'foreach'")) {
        return parseres_none();
    }

    bool is_foreach;
    {
        size_t cursor = parser->cursor_current;

        is_foreach = parser->tokens.array[cursor++].type == TT_IDENTIFIER;

        if (is_foreach && parser->tokens.array[cursor].type == TT_COMMA) {
            is_foreach = is_foreach && parser->tokens.array[cursor++].type == TT_COMMA;
            is_foreach = is_foreach && parser->tokens.array[cursor++].type == TT_IDENTIFIER;
            assert(false); // TODO: support `idx, var` in foreach loop
        }

        is_foreach = is_foreach && parser->tokens.array[cursor++].type == TT_IN;
    }

    Token var_t = parser_peek(parser);
    assert(parser_consume(parser, TT_IDENTIFIER, "Expected variable"));

    VarDeclLHS var = {
        .type = VDLT_NAME,
        .count = 1,
        .lhs.name = {
            .length = var_t.length,
            .chars = var_t.start,
        },
    };

    if (parser_peek(parser).type == TT_COMMA) {
        assert(false); // TODO: support `idx, var` in foreach loop
    }

    assert(parser_consume(parser, TT_IN, "Expected 'in'"));

    ParseResult iter_res = parser_parse_simple_expr(parser, (LL_Directive){0});
    assert(iter_res.status == PRS_OK);

    ASTNode* iter = arena_alloc(parser->arena, sizeof *iter);
    *iter = iter_res.node;

    ASTNodeStatementBlock* block = arena_alloc(parser->arena, sizeof *block);
    *block = parser_parse_stmt_block(parser);

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_FOREACH,
        .node.foreach = {
            .var = var,
            .iterable = iter,
            .block = block,
        },
        .directives = directives,
    });
}

static ParseResult parser_parse_crash(Parser* const parser, LL_Directive const directives) {
    if (!parser_consume(parser, TT_CRASH, "Expected 'crash'")) {
        return parseres_none();
    }

    ASTNode* maybe_expr;
    switch (parser_peek(parser).type) {
        case TT_SEMICOLON: {
            maybe_expr = NULL;
            break;
        }

        case TT_LITERAL_STRING: {
            ParseResult msg_res = parser_parse_lit_str(parser, (LL_Directive){0});
            assert(msg_res.status == PRS_OK);

            maybe_expr = arena_alloc(parser->arena, sizeof *maybe_expr);
            *maybe_expr = msg_res.node;

            break;
        }

        case TT_LITERAL_STRING_TEMPLATE_START:
        case TT_LITERAL_STRING_TEMPLATE_FULL:
        {
            ParseResult msg_res = parser_parse_lit_str_template(parser, (LL_Directive){0});
            assert(msg_res.status == PRS_OK);

            maybe_expr = arena_alloc(parser->arena, sizeof *maybe_expr);
            *maybe_expr = msg_res.node;

            break;
        }

        default: assert(false);
    }

    return parseres_ok((ASTNode){
        .id = { parser->next_node_id++ },
        .type = ANT_CRASH,
        .node.crash.maybe_expr = maybe_expr,
        .directives = directives,
    });
}

static ParseResult parser_parse_assignment(Parser* const parser, ASTNode expr) {
    AssignmentOp op;
    switch (parser_peek(parser).type) {
        case TT_EQUAL:           op = AO_ASSIGN; break;
        case TT_PLUS_EQUAL:      op = AO_PLUS_ASSIGN; break;
        case TT_MINUS_EQUAL:     op = AO_MINUS_ASSIGN; break;
        case TT_STAR_EQUAL:      op = AO_MULTIPLY_ASSIGN; break;
        case TT_SLASH_EQUAL:     op = AO_DIVIDE_ASSIGN; break;
        case TT_AMPERSAND_EQUAL: op = AO_BIT_AND_ASSIGN; break;
        case TT_PIPE_EQUAL:      op = AO_BIT_OR_ASSIGN; break;
        case TT_CARET_EQUAL:     op = AO_BIT_XOR_ASSIGN; break;

        default: return parseres_none();
    }
    parser_advance(parser);

    ASTNode* lhs = arena_alloc(parser->arena, sizeof *lhs);
    *lhs = expr;

    LL_Directive const directives = parser_parse_directives(parser);
    ParseResult rhs_res = parser_parse_expr(parser, directives);

    ASTNode* rhs = arena_alloc(parser->arena, sizeof *rhs);
    *rhs = rhs_res.node;

    ASTNode assignment = {
        .id = { parser->next_node_id++ },
        .type = ANT_ASSIGNMENT,
        .node.assignment = {
            .lhs = lhs,
            .op = op,
            .rhs = rhs,
        },
    };

    if (rhs_res.status != PRS_OK) {
        return parseres_err(assignment);
    }

    return parseres_ok(assignment);
}

static ParseResult parser_wrap_stmt_expr(Parser* const parser, ParseResult expr_res) {
    if (expr_res.status != PRS_OK) {
        return expr_res;
    }
    ASTNode expr = expr_res.node;

    ParseResult stmt_res;
    size_t const cached_current = parser->cursor_current;

    stmt_res = parser_parse_assignment(parser, expr);
    if (stmt_res.status == PRS_OK) {
        stmt_res.node.directives = expr.directives;
        return stmt_res;
    }
    parser->cursor_current = cached_current;

    return expr_res;
}

static ParseResult parser_parse_stmt(Parser* const parser) {
    LL_Directive const directives = parser_parse_directives(parser);
    
    Token const current = parser_peek(parser);

    ParseResult stmt_res;

    switch (current.type) {
        case TT_RETURN: {
            stmt_res = parser_parse_return(parser, directives);
            assert(parser_consume(parser, TT_SEMICOLON, "Expected semicolon."));
            return stmt_res;
        }

        case TT_DEFER: {
            return parser_parse_defer(parser, directives);
        }

        case TT_IF: {
            return parser_parse_if(parser, directives);
        }

        case TT_WHILE: {
            return parser_parse_while(parser, directives);
        }

        case TT_FOR: assert(false);

        case TT_FOREACH: {
            return parser_parse_foreach(parser, directives);
        }

        case TT_CRASH: {
            stmt_res = parser_parse_crash(parser, directives);
            assert(parser_consume(parser, TT_SEMICOLON, "Expected semicolon."));
            return stmt_res;
        }

        default: break;
    }
    size_t const cached_current = parser->cursor_current;

    stmt_res = parser_parse_expr(parser, directives);
    if (stmt_res.status == PRS_OK) {
        if (parser_peek(parser).type == TT_SEMICOLON) {
            parser_advance(parser);
            return stmt_res;
        } else {
            stmt_res = parser_wrap_stmt_expr(parser, stmt_res);
            if (stmt_res.status == PRS_OK && parser_peek(parser).type == TT_SEMICOLON) {
                return stmt_res;
            }
        }
    }
    parser->cursor_current = cached_current;

    stmt_res = parser_parse_var_decl(parser, directives);
    if (stmt_res.status == PRS_OK) {
        // semicolon already consumed
        // parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");
        return stmt_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ASTNodeStatementBlock parser_parse_stmt_block(Parser* const parser) {
    assert(parser_consume(parser, TT_LEFT_BRACE, "Expected '{'"));

    LL_ASTNode stmts = {0};

    Token t = parser_peek(parser);
    while (t.type != TT_RIGHT_BRACE && t.type != TT_EOF) {
        // ignore semicolons (empty statements)
        while (t.type == TT_SEMICOLON || t.type == TT_ERROR) {
            parser_advance(parser);
            t = parser_peek(parser);
        }

        if (t.type == TT_RIGHT_BRACE || t.type == TT_EOF) {
            break;
        }

        ParseResult res = parser_parse_stmt(parser);
        if (res.status != PRS_OK) {
            debug_token(parser, t);
            debug_token(parser, parser_peek(parser));
        }
        assert(res.status == PRS_OK);

        ll_ast_push(parser->arena, &stmts, res.node);
        t = parser_peek(parser);
    }
    assert(parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'"));

    return (ASTNodeStatementBlock){ .stmts = stmts };
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

    bool is_main = strncmp(cname, "main", 4) == 0;

    if (parser_peek(parser).type == TT_SEMICOLON) {
        parser_advance(parser);
        return parseres_ok((ASTNode){
            .id = { parser->next_node_id++ },
            .type = ANT_FUNCTION_HEADER_DECL,
            .node.function_header_decl = {
                .return_type = *type,
                .name = c_str(cname),
                .params = params,
                .is_main = is_main,
            },
            .directives = directives,
        });
    }

    ASTNode node = {
        .id = { parser->next_node_id++ },
        .type = ANT_FUNCTION_DECL,
        .node.function_decl = {
            .header = {
                .return_type = *type,
                .name = c_str(cname),
                .params = params,
                .is_main = is_main,
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
        .id = { parser->next_node_id++ },
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

        .package = NULL,
        .had_error = false,
        .panic_mode = false,

        .next_node_id = 0,
        .next_type_id = 0,
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

        if (node_res.node.type == ANT_PACKAGE) {
            assert(!parser->package);
            parser->package = &nodes.tail->data;
        }
    }

    ASTNode* const file_root = arena_alloc(parser->arena, sizeof(ASTNode));
    file_root->id.val = parser->next_node_id++;
    file_root->type = ANT_FILE_ROOT;
    file_root->node.file_root = (ASTNodeFileRoot){ .nodes = nodes };

    return astres_ok(file_root);
}
