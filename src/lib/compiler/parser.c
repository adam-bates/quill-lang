#include <assert.h>
#include <stdio.h>

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

static ParseResult parser_parse_expr(Parser* const parser);

static void debug_token_type(Parser const* const parser, TokenType token_type) {
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
    printf("Token[");
    debug_token_type(parser, token.type);

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

    if (ident.type != TT_STAR && ident.type != TT_IDENTIFIER) {
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

    path = parser_parse_static_path(parser);
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

static ParseResult parser_parse_import(Parser* const parser) {
    if (!parser_consume(parser, TT_IMPORT, "Expected `import` keyword.")) {
        return parseres_none();
    }

    /*
        TODO: should we handle multi imports?
          like `import std::{io, String}`

        and then do we allow that to continue?
          like `import std::{io::{printf, println}, ds::{StringBuffer, strbuf_create}}`
    */

    StaticPath* const path = parser_parse_static_path(parser);
    if (path == NULL) {
        error_at_current(parser, "Expected import target.");
        return parseres_none();
    }

    ASTNode const node = {
        .type = ANT_IMPORT,
        .node.import.static_path = path,
    };

    if (!parser_consume(parser, TT_SEMICOLON, "Expected semicolon.")) {
        return parseres_err(node);
    }

    return parseres_ok(node);
}

static ParseResult parser_parse_lit_str(Parser* const parser) {
    Token const litstr = parser_peek(parser);

    if (!parser_consume(parser, TT_LITERAL_STRING, "Expected string literal.")) {
        return parseres_none();
    }

    if (litstr.length <= 1) {
        error(parser, "Missing string boundaries.");
        return parseres_err((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_STR,
            .node.literal.value.lit_str = {
                .chars = "",
                .length = 0,
            },
        });
    }

    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_STR,
        .node.literal.value.lit_str = {
            .chars = litstr.start + 1,
            .length = litstr.length - 2,
        },
    });
}

static ParseResult parser_parse_lit_char_s(Parser* const parser) {
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
        });
    }

    if (litchar_s.length == 3) {
        return parseres_ok((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_CHAR,
            // .node.literal.value.lit_char = litchar_s.start[1],
            .node.literal.value.lit_char = {
                .chars = litchar_s.start + 1,
                .length = 1,
            },
        });
    }

    if (litchar_s.start[1] == '\\' && litchar_s.length == 4) {
        return parseres_ok((ASTNode){
            .type = ANT_LITERAL,
            .node.literal.kind = LK_CHAR,
            .node.literal.value.lit_char = {
                .chars = litchar_s.start + 1,
                .length = 2,
            },
        });
    }

    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_CHARS,
        .node.literal.value.lit_chars = {
            .chars = litchar_s.start + 1,
            .length = litchar_s.length - 2,
        },
    });
}

static ParseResult parser_parse_lit_number(Parser* const parser) {
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
        });
    }

    return parseres_ok((ASTNode){
        .type = ANT_LITERAL,
        .node.literal.kind = LK_INT,
        .node.literal.value.lit_int = atoll(str.chars),
    });
}

static ParseResult parser_parse_lit(Parser* const parser) {
    switch (parser_peek(parser).type) {
        case TT_LITERAL_STRING: return parser_parse_lit_str(parser);
        case TT_LITERAL_CHAR: return parser_parse_lit_char_s(parser);
        case TT_LITERAL_NUMBER: return parser_parse_lit_number(parser);

        default: break;
    }

    return parseres_none();
}

static ParseResult parser_parse_compiler_directive(Parser* const parser) {
    // TODO
    return parseres_none();
}

static ParseResult parser_parse_var_ref(Parser* const parser) {
    StaticPath* const path = parser_parse_static_path(parser);

    if (path == NULL) {
        return parseres_none();
    }

    return parseres_ok((ASTNode){
        .type = ANT_VAR_REF,
        .node.var_ref.path = path,
    });
}

static ParseResult parser_parse_fn_call(Parser* const parser) {
    ASTNode fn_call = {
        .type = ANT_FUNCTION_CALL,
        .node.function_call = {0},
    };

    ParseResult const varref_res = parser_parse_var_ref(parser);
    if (varref_res.status != PRS_OK) {
        return parseres_none();
    }

    if (parser_peek(parser).type != TT_LEFT_PAREN) {
        return parseres_none();
    }

    ASTNode* const target = arena_alloc(parser->arena, sizeof(ASTNode));
    *target = varref_res.node;

    fn_call.node.function_call.function = target;

    parser_consume(parser, TT_LEFT_PAREN, "Expected '('.");

    Token current = parser_peek(parser);
    while (current.type != TT_RIGHT_PAREN && current.type != TT_EOF) {
        ParseResult arg_res = parser_parse_expr(parser);
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

static ParseResult parser_parse_expr(Parser* const parser) {
    ParseResult expr_res;
    size_t const cached_current = parser->cursor_current;

    expr_res = parser_parse_lit(parser);
    if (expr_res.status == PRS_OK) {
        return expr_res;
    }
    parser->cursor_current = cached_current;

    expr_res = parser_parse_fn_call(parser);
    if (expr_res.status == PRS_OK) {
        return expr_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_stmt(Parser* const parser) {
    ParseResult stmt_res;
    size_t const cached_current = parser->cursor_current;

    stmt_res = parser_parse_expr(parser);
    if (stmt_res.status == PRS_OK) {
        parser_consume(parser, TT_SEMICOLON, "Expected semicolon.");
        return stmt_res;
    }
    parser->cursor_current = cached_current;

    return parseres_none();
}

static ParseResult parser_parse_fn_decl(Parser* const parser) {
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

    // TODO: parse params

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
            },
        });
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

    if (!parser_consume(parser, TT_LEFT_BRACE, "Expected '{'.")) {
        return parseres_err(node);
    }

    Token current = parser_peek(parser);
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
                error_at(parser, &current, "Expected statmement.");
            }
            parser_advance(parser);
        }
    }
    
    if (!parser_consume(parser, TT_RIGHT_BRACE, "Expected '}'.")) {
        return parseres_err(node);
    }

    return parseres_ok(node);
}

static ParseResult parser_parse_filescope_decl(Parser* const parser) {
    Token const current = parser_peek(parser);

    switch (current.type) {
        case TT_IMPORT: return parser_parse_import(parser);

        default: break;
    }

    ParseResult decl_res;
    size_t const cached_current = parser->cursor_current;

    decl_res = parser_parse_fn_decl(parser);
    if (decl_res.status == PRS_OK) {
        return decl_res;
    }
    parser->cursor_current = cached_current;

    // TODO: support file-scoped variables.

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
            break;
        }

        ll_ast_push(parser->arena, &nodes, node_res.node);
    }

    ASTNode* const file_root = arena_alloc(parser->arena, sizeof(ASTNode));
    file_root->type = ANT_FILE_ROOT;
    file_root->node.file_root = (ASTNodeFileRoot){ .nodes = nodes };

    return astres_ok(file_root);
}
