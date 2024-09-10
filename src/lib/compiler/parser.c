#include "./compiler.h"
#include "../utils/utils.h"

static ASTNodeResult res_ok(ASTNode const ast) {
    return (ASTNodeResult){ .ok = true, .res.ast = ast };
}

static ASTNodeResult res_err(Error const err) {
    return (ASTNodeResult){ .ok = false, .res.err = err };
}

Parser parser_create(Allocator const allocator, ArrayList_Token const tokens) {
    return (Parser){
        .allocator = allocator,
        .tokens = tokens,
    };
}

ASTNodeResult parser_parse(Parser* const parser) {
    // TODO

    return res_err(err_create(ET_UNIMPLEMENTED, "parser_parse"));
}

