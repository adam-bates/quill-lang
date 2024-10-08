#ifndef quill_parser_h
#define quill_parser_h

#include "./ast.h"
#include "./token.h"
#include "../utils/utils.h"

typedef struct {
    Arena* const arena;
    
    ArrayList_Token const tokens;
    size_t cursor_start;
    size_t cursor_current;

    ASTNode* package;
    bool had_error;
    bool panic_mode;

    size_t next_node_id;
    size_t next_type_id;
} Parser;

#define astres_assert(astres) \
    if (!astres.ok) { err_print(astres.res.err); assert(astres.ok); }

void debug_token_type(TokenType token_type);

Parser parser_create(Arena* const arena, ArrayList_Token const tokens);

ASTNodeResult parser_parse(Parser* const parser);

#endif
