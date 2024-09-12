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
} Parser;

#define astres_assert(astres) \
    if (!astres.ok) { err_print(astres.res.err); assert(astres.ok); }

Parser parser_create(Arena* const arena, ArrayList_Token const tokens);

ASTNodeResult parser_parse(Parser* const parser);

#endif
