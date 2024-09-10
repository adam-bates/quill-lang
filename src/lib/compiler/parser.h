#ifndef quill_parser_h
#define quill_parser_h

#include "./ast.h"
#include "./token.h"
#include "../utils/utils.h"

typedef struct {
    Allocator const allocator;
    
    ArrayList_Token const tokens;
} Parser;

#define astres_assert(astres) \
    if (!astres.ok) { err_print(astres.res.err); assert(astres.ok); }

Parser parser_create(Allocator const allocator, ArrayList_Token const tokens);

ASTNodeResult parser_parse(Parser* const parser);

#endif
