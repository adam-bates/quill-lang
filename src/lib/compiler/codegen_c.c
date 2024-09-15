#include "./codegen_c.h"

CodegenC codegen_c_create(Arena* const arena, ASTNode const* const ast) {
    return (CodegenC){
        .arena = arena,
        .ast = ast,
    };
}

String generate_c_code(CodegenC* const codegen) {
    StringBuffer strbuf = strbuf_create(codegen->arena);

    // TODO
    
    return c_str("todo");
}

