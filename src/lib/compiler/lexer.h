#ifndef quillc_lexer_h
#define quillc_lexer_h

#include "../utils/utils.h"
#include "./token.h"

typedef struct {
    Allocator const allocator;

    String const source;

    char const* current;
    int line;
} Lexer;

typedef struct {
    bool const ok;
    union {
        ArrayList_Token const tokens;
        Error const err;
    } const res;
} ScanResult;

#define scanres_assert(scanres) \
    if (!scanres.ok) { err_print(scanres.res.err); assert(scanres.ok); }

Lexer lexer_create(Allocator const allocator, String const source);
ScanResult lexer_scan(Lexer const lexer);

#endif
