#include <assert.h>
#include <stdio.h>

#include "../utils/utils.h"
#include "./compiler.h"
#include "./ast.h"
#include "./token.h"

static ASTNode parser_parse_expr(Parser* const parser, bool const confident);

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

static Token parser_peek(Parser const* const parser) {
    return parser->tokens.array[parser->cursor_current];
}

static Token parser_peek_next(Parser const* const parser) {
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
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);

    parser->had_error = true;
}

static void error(Parser* const parser, char const* const message) {
    Token const token = parser_peek_prev(parser);
    error_at(parser, &token, message);
}

static Token error_at_current(Parser* const parser, char const* const message) {
    Token const token = parser_peek(parser);
    error_at(parser, &token, message);
    return token;
}

static Token parser_advance(Parser* const parser) {
    Token const current = parser->tokens.array[parser->cursor_current];
    parser->cursor_current += 1;
    return current;
}

static Token parser_consume(Parser* const parser, TokenType const type, char const* const message) {
    if (parser_peek(parser).type != type) {
        return error_at_current(parser, message);
    }

    return parser_advance(parser);
}

static StaticPath* parser_parse_static_path(Parser* const parser, bool const confident) {
    Token const ident = parser_peek(parser);

    if (ident.type != TT_STAR && ident.type != TT_IDENTIFIER) {
        if (confident) {
            parser_consume(parser, TT_IDENTIFIER, "Expected an identifier.");
        }

        return NULL;
    } else {
        parser_advance(parser);
    }

    String const name = {
        .chars = ident.start,
        .length = ident.length,
    };

    StaticPath* path = arena_alloc(parser->arena, sizeof(StaticPath));
    path->name = name;
    path->root = NULL;

    if (parser_peek(parser).type != TT_COLON_COLON) {
        return path;
    }
    parser_advance(parser);

    StaticPath* root = arena_alloc(parser->arena, sizeof(StaticPath));        
    root = path;

    path = parser_parse_static_path(parser, true);
    if (path == NULL) {
        return NULL;
    }

    path->root = root;

    return path;
}

static Type* parser_parse_type(Parser* const parser) {
    // TODO: support other types
    if (parser_peek(parser).type != TT_VOID) {
        return NULL;
    }
    parser_advance(parser);

    Type* type = arena_alloc(parser->arena, sizeof(Type));
    type->kind = TK_BUILT_IN;
    type->type.built_in = TBI_VOID;

    return type;
}

static ASTNode parser_parse_import(Parser* const parser) {
    parser_consume(parser, TT_IMPORT, "Expected `import` keyword.");

    ASTNode node = {
        .type = ANT_IMPORT,
        .node.import = {
            .static_path = parser_parse_static_path(parser, true),
        },
    };

    parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");

    return node;
}

static ASTNode parser_parse_lit_str(Parser* const parser, bool const confident) {
    Token litstr = parser_peek(parser);

    if (!confident && litstr.type != TT_LITERAL_STRING) {
        return (ASTNode){0};
    }
    parser_consume(parser, TT_LITERAL_STRING, "Expected string literal.");

    return (ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_STR,
        .node.literal.value.lit_str = {
            .chars = litstr.start,
            .length = litstr.length,
        },
    };
}

static ASTNode parser_parse_lit(Parser* const parser, bool const confident) {
    switch (parser_peek(parser).type) {
        case TT_LITERAL_STRING: return parser_parse_lit_str(parser, true);

        default: break;
    }

    if (confident) {
        error_at_current(parser, "Expected literal value.");
    }

    return (ASTNode){0};
}

static ASTNode parser_parse_var_ref(Parser* const parser, bool const confident) {
    StaticPath* path = parser_parse_static_path(parser, confident);
    if (path != NULL) {
        return (ASTNode){
            .type = ANT_VAR_REF,
            .node.var_ref.path = path,
        };
    }

    if (confident) {
        error_at_current(parser, "Expected a variable reference.");
    }
    
    return (ASTNode){0};
}

static ASTNode parser_parse_fn_call(Parser* const parser, bool const confident) {
    ASTNode fn_call = {
        .type = ANT_FUNCTION_CALL,
        .node.function_call = {0},
    };

    ASTNode* target = arena_alloc(parser->arena, sizeof(ASTNode));
    *target = parser_parse_var_ref(parser, confident);

    if (target->type == ANT_NONE) {
        if (confident) {
            error_at_current(parser, "Expected function call.");
        }

        return (ASTNode){0};
    }
    fn_call.node.function_call.function = target;

    parser_consume(parser, TT_LEFT_PAREN, "Expected '('.");

    LL_ASTNode args = {0};

    Token current = parser_peek(parser);
    while (
        current.type != TT_RIGHT_PAREN
        && current.type != TT_EOF
    ) {
        ASTNode arg = parser_parse_expr(parser, true);
        
        ll_ast_push(parser->arena, &args, arg);

        current = parser_peek(parser);

        if (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
            parser_consume(parser, TT_COMMA, "Expected comma.");
            current = parser_peek(parser);
        }
    }

    fn_call.node.function_call.args = args;

    parser_consume(parser, TT_RIGHT_PAREN, "Expected ')'.");

    return fn_call;
}

static ASTNode parser_parse_expr(Parser* const parser, bool const confident) {
    ASTNode expr;
    size_t cached_current = parser->cursor_current;

    expr = parser_parse_lit(parser, false);
    if (expr.type != ANT_NONE) {
        return expr;
    } else if (!confident) {
        parser->cursor_current = cached_current;
    }

    expr = parser_parse_fn_call(parser, false);
    if (expr.type != ANT_NONE) {
        return expr;
    } else if (!confident) {
        parser->cursor_current = cached_current;
    }

    if (confident) {
        error_at_current(parser, "Expected expression.");
    }
    
    return (ASTNode){0};
}

static ASTNode parser_parse_stmt(Parser* const parser, bool const confident) {
    ASTNode stmt;
    size_t cached_current = parser->cursor_current;

    stmt = parser_parse_expr(parser, false);
    if (stmt.type != ANT_NONE) {
        parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");
        return stmt;
    } else if (!confident) {
        parser->cursor_current = cached_current;
    }

    if (confident) {
        error_at_current(parser, "Expected statement.");
    }

    return (ASTNode){0};
}

static ASTNode parser_parse_fn_decl(Parser* const parser) {
    Type const* const type = parser_parse_type(parser);
    if (type == NULL) {
        return (ASTNode){0};
    }

    Token const token = parser_peek(parser);
    if (token.type != TT_IDENTIFIER) {
        return (ASTNode){0};
    }
    parser_advance(parser);
    
    char* const cname = arena_memcpy(parser->arena, token.start, token.length + 1);
    cname[token.length] = '\0';

    parser_consume(parser, TT_LEFT_PAREN, "Expected '('.");
    // TODO: parse params
    parser_consume(parser, TT_RIGHT_PAREN, "Expected ')'.");

    if (parser_peek(parser).type == TT_SEMICOLON) {
        parser_advance(parser);
        return (ASTNode){
            .type = ANT_FUNCTION_HEADER_DECL,
            .node.function_header_decl = {
                .return_type = *type,
                .name = c_str(cname),
            },
        };
    }

    ASTNode node = {
        .type = ANT_FUNCTION_DECL,
        .node.function_decl = {
            .header = {
                .return_type = *type,
                .name = c_str(cname),
            },
            .stmts = {0},
        },
    };

    parser_consume(parser, TT_LEFT_BRACE, "Expected '{'.");

    Token current = parser_peek(parser);
    while (
        current.type != TT_RIGHT_BRACE
        && current.type != TT_EOF
        && current.type != TT_ERROR
    ) {
        ASTNode const stmt = parser_parse_stmt(parser, true);

        ll_ast_push(parser->arena, &node.node.function_decl.stmts, stmt);

        current = parser_peek(parser);
    }
    
    parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'.");

    return node;
}

static ASTNode parser_parse_filescope_decl(Parser* const parser) {
    Token current = parser_peek(parser);

    switch (current.type) {
        case TT_IMPORT: return parser_parse_import(parser);

        default: break;
    }

    ASTNode node;
    size_t cached_current = parser->cursor_current;

    node = parser_parse_fn_decl(parser);
    if (node.type != ANT_NONE) {
        return node;
    }
    parser->cursor_current = cached_current;

    error_at_current(parser, "Expected file-scope declaration.");
    return (ASTNode){0};
}

static ASTNode parser_parse_node(Parser* const parser) {
    if (parser_peek(parser).type == TT_EOF) {
        error_at_current(parser, "Unexpected EOF.");
        return (ASTNode){0};
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
    
        ASTNode const node = parser_parse_node(parser);
        if (node.type == ANT_NONE) {
            break;
        }

        ll_ast_push(parser->arena, &nodes, node);
    }

    ASTNode* file_root = arena_alloc(parser->arena, sizeof(ASTNode));
    file_root->type = ANT_FILE_ROOT;
    file_root->node.file_root = (ASTNodeFileRoot){ .nodes = nodes };

    return astres_ok(file_root);
}
