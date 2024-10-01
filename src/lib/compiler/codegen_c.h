#ifndef quill_codegen_c_h
#define quill_codegen_c_h

#include "./package.h"
#include "../utils/utils.h"

typedef enum {
    BT_OTHER,
    BT_IMPORT,
    BT_FUNCTION_DECLS,
    BT_STATIC_VARS,
    BT_VARS,
} BlockType;

typedef struct {
    Arena* const arena;
    Packages const packages;

    bool seen_file_separator;
    BlockType prev_block;
} CodegenC;

CodegenC codegen_c_create(Arena* const arena, Packages const packages);

String generate_c_code(CodegenC* const codegen);

#endif
