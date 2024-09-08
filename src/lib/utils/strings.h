#ifndef quillc_string_h
#define quillc_string_h

#include "./base.h"

typedef struct {
    size_t      length;
    char const* chars;
} String;

String c_str(char const* const chars);

#endif
