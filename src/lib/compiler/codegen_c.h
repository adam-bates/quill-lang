#ifndef quill_codegen_c_h
#define quill_codegen_c_h

#include "./ast.h"
#include "../utils/utils.h"

typedef struct {
    Arena* const arena;
    ASTNode const* const ast;
} CodegenC;

CodegenC codegen_c_create(Arena* const arena, ASTNode const* const ast);

String generate_c_code(CodegenC* const codegen);

#endif
