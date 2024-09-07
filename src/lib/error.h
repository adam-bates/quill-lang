#ifndef snowy_error_h
#define snowy_error_h

#include <stdlib.h>

typedef enum {
    ET_UNSPECIFIED,
    ET_UNIMPLEMENTED,
    ET_OUT_OF_MEMORY,

    ET_COUNT,
} ErrorType;

typedef struct {
    ErrorType   type;
    char const* msg;

    char const* file;
    size_t line;
} Error;

#define err_create(type, msg) _err_create(type, msg, __FILE__, __LINE__)

Error _err_create(ErrorType type, char const* msg, char const* file, size_t line);
void err_print(Error err);

#endif
