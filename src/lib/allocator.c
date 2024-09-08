#include <stdlib.h>

#include "allocator.h"

Allocator_s allocator_create(void) {
    return (Allocator_s){
        .malloc = malloc,
        .valloc = valloc,
        .calloc = calloc,
        .realloc = realloc,
        .reallocf = reallocf,
        .aligned_alloc = aligned_alloc,
        .free = free,
    };
}
