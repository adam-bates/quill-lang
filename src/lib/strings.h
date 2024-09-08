#ifndef snowy_string_h
#define snowy_string_h

#include <stdlib.h>

typedef struct {
    size_t            length;
    char const* const chars;
} String;

String c_str(char const* const chars);

#endif
