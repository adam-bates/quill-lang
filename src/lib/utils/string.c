#include <string.h>

#include "string.h"

String c_str(char const* const chars) {
    size_t const length = strlen(chars);

    return (String){
        .length = length,
        .chars = chars,
    };
}

