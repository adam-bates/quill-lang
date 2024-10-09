#ifndef quill_lexer_h
#define quill_lexer_h

#include "./token.h"
#include "../utils/utils.h"

typedef struct {
    Arena* arena;

    char* start;
    char* current;
    size_t line;

    size_t template_string_nest;
    size_t multiline_comment_nest;
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

Lexer lexer_create(Arena* const arena, String const source);

ScanResult lexer_scan(Lexer* const lexer);

#endif
