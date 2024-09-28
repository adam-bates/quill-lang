#ifndef quill_args_h
#define quill_args_h

#include "../utils/utils.h"

#define ARGC_MAX 255

typedef struct {
    Strings opt_args;
    Strings paths_to_include;
} QuillcArgs;

void parse_args(QuillcArgs* out, int const argc, char* const argv[]);

#endif
