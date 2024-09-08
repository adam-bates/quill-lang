#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "error.h"

Error _err_create(
    ErrorType const type,
    char const* const msg,
    char const* const file,
    size_t const line
) {
    return (Error){
        .type = type,
        .msg = c_str(msg),
        .file = file,
        .line = line,
    };
}

void err_print(Error const err) {
    char const* tstr;
    switch (err.type) {
        case ET_UNSPECIFIED: tstr = "Unspecified"; break;
        case ET_UNIMPLEMENTED: tstr = "Not Implemented"; break;
        case ET_OUT_OF_MEMORY: tstr = "Out of Memory"; break;

        case ET_COUNT: assert(err.type != ET_COUNT); return;
    }

    fprintf(stderr, "[%s:%lu] Error! %s: %s\n", err.file, err.line, tstr, err.msg.chars);
}
