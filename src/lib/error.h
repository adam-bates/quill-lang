#ifndef snowy_error_h
#define snowy_error_h

#include <stdlib.h>

#include "strings.h"

typedef enum {
    ET_UNSPECIFIED,
    ET_UNIMPLEMENTED,
    ET_OUT_OF_MEMORY,

    ET_COUNT,
} ErrorType;

typedef struct {
    ErrorType const type;
    String const    msg;

    char const* const file;
    size_t const      line;
} Error;

#define err_create(type, msg) _err_create(type, msg, __FILE__, __LINE__)

Error _err_create(
    ErrorType const type,
    char const* const msg,
    char const* const file,
    size_t const line);

void err_print(Error const err);

#endif
