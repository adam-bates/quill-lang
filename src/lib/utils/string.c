#include <stdbool.h>
#include <string.h>

#include "string.h"

String c_str(char const* const chars) {
    size_t const length = strlen(chars);

    return (String){
        .length = length,
        .chars = chars,
    };
}

bool str_eq(String str1, String str2) {
    if (str1.length != str2.length) {
        return false;
    }

    return strncmp(str1.chars, str2.chars, str1.length) == 0;
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
