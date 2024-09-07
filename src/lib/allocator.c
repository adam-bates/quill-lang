#include <stdlib.h>

#include "allocator.h"

Allocator allocator_create(void) {
    return (Allocator){
        .malloc = malloc,
        .valloc = valloc,
        .calloc = calloc,
        .realloc = realloc,
        .reallocf = reallocf,
        .aligned_alloc = aligned_alloc,
        .free = free,
    };
}
