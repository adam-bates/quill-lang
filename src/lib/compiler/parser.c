#include "../utils/utils.h"
#include "./compiler.h"
#include "./ast.h"

static ASTNodeResult astres_ok(ASTNode const* const ast) {
    return (ASTNodeResult){ .ok = true, .res.ast = ast };
}

static ASTNodeResult astres_err(Error const err) {
    return (ASTNodeResult){ .ok = false, .res.err = err };
}

static bool parser_is_at_end(Parser const* const parser) {
    return parser->tokens.array[parser->cursor_current].type == TT_EOF;
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

static LL_ASTNode* parser_parse_node(Parser* const parser) {
    // TODO
    return NULL;
}

Parser parser_create(Arena* const arena, ArrayList_Token const tokens) {
    return (Parser){
        .arena = arena,
        .tokens = tokens,
        .cursor_start = 0,
        .cursor_current = 0,
    };
}

ASTNodeResult parser_parse(Parser* const parser) {
    LL_ASTNode* nodes_head = NULL;

    while (!parser_is_at_end(parser)) {
        LL_ASTNode* const node = parser_parse_node(parser);
        if (node == NULL) {
            break;
        }

        if (nodes_head != NULL) {
            node->next = nodes_head;
        }
        nodes_head = node;
    }

    ASTNode* file_root = arena_alloc(parser->arena, sizeof(ASTNode));
    file_root->type = ANT_FILE_ROOT;
    file_root->node.file_root = (ASTNodeFileRoot){ .nodes = nodes_head };

    return astres_ok(file_root);
}
