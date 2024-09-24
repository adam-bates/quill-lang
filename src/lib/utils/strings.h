#ifndef quill_strings_h
#define quill_strings_h

#include "./base.h"

typedef struct {
    size_t      length;
    char const* chars;
} String;

String c_str(char const* const chars);

#endif
