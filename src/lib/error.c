#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "error.h"

Error _err_create(ErrorType type, char const* msg, char const* file, size_t line) {
    return (Error){
        .type = type,
        .msg = msg,
        .file = file,
        .line = line,
    };
}

void err_print(Error err) {
    char* tstr;
    switch (err.type) {
        case ET_UNSPECIFIED: tstr = "Unspecified"; break;
        case ET_UNIMPLEMENTED: tstr = "Not Implemented"; break;
        case ET_OUT_OF_MEMORY: tstr = "Out of Memory"; break;

        case ET_COUNT: assert(err.type != ET_COUNT); return;
    }

    fprintf(stderr, "[%s:%lu] Error! %s: %s\n", err.file, err.line, tstr, err.msg);
}
