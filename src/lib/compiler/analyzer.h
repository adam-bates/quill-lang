#ifndef quill_analyzer_h
#define quill_analyzer_h

#include "./ast.h"

typedef struct {
    bool has_package;
    bool has_separator;

    // TODO
} Analyzer;

void verify_syntax(Analyzer* analyzer, ASTNode const* const ast);

#endif
