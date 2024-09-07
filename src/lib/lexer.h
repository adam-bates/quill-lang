#ifndef snowy_lexer_h
#define snowy_lexer_h

#include "common.h"
#include "token.h"

typedef struct {
    char const* source;

    char const* current;
    int line;

    Allocator alloc;
} Lexer;

typedef struct {
    bool ok;
    union {
        Tokens tokens;
        Error err;
    } res;
} ScanResult;

#define scanres_assert(scanres) \
    if (!scanres.ok) { err_print(scanres.res.err); assert(scanres.ok); }

Lexer lexer_create(Allocator alloc, char const* source);
ScanResult lexer_scan(Lexer lexer);

#endif
