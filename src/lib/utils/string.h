#ifndef quill_string_h
#define quill_string_h

#include "./base.h"

typedef struct {
    size_t      length;
    char const* chars;
} String;

typedef struct {
    size_t  length;
    String* strings;
} Strings;

String c_str(char const* const chars);

void strs_remove(Strings* const strs, size_t const idx);

#endif
