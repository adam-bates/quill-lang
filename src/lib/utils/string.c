#include <string.h>

#include "string.h"

String c_str(char const* const chars) {
    size_t const length = strlen(chars);

    return (String){
        .length = length,
        .chars = chars,
    };
}

void strs_remove(Strings* const strs, size_t const idx) {
    if (idx >= strs->length) {
        return;
    }

    for (size_t i = idx + 1; i < strs->length; ++i) {
        strs->strings[i - 1] = strs->strings[i];
    }
    strs->strings[idx] = (String){0};

    strs->length -= 1;
}
