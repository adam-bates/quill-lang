#include <stdlib.h>

#include "./utils.h"
#include "allocator.h"

typedef void* (*MallocFn)(size_t const size);
typedef void* (*VallocFn)(size_t const size);
typedef void* (*CallocFn)(size_t const count, size_t const size);
typedef void* (*ReallocFn)(void* const ptr, size_t const size);
typedef void* (*ReallocOrFreeFn)(void* const ptr, size_t const size);
typedef void* (*AlignedAllocFn)(size_t const alignment, size_t const size);
typedef void (*FreeFn)(void* const ptr);

typedef struct {
    MallocFn        malloc;
    VallocFn        valloc;
    CallocFn        calloc;
    ReallocFn       realloc;
    ReallocOrFreeFn reallocf;
    AlignedAllocFn  aligned_alloc;
    FreeFn          free;
} StdlibInternalAllocator;

static StdlibInternalAllocator* internal(Allocator const allocator) {
    return (StdlibInternalAllocator*)allocator._internal;
}

MaybeAllocator allocator_create(void) {
    StdlibInternalAllocator* internal = malloc(sizeof(StdlibInternalAllocator));

    if (internal == NULL) {
        return (MaybeAllocator){ .ok = false, .maybe._ = NULL };
    }

    internal->malloc = malloc;
    internal->valloc = valloc;
    internal->calloc = calloc;
    internal->realloc = realloc;
    internal->reallocf = reallocf;
    internal->aligned_alloc = aligned_alloc;
    internal->free = free;

    Allocator const allocator = { ._internal = internal };

    return (MaybeAllocator){
        .ok = true,
        .maybe.allocator = allocator,
    };
}

void allocator_destroy(Allocator const allocator) {
    free((void*)allocator._internal);
}

void* quill_malloc(Allocator const allocator, size_t const size) {
    return internal(allocator)->malloc(size);
}

void* quill_valloc(Allocator const allocator, size_t const size) {
    return internal(allocator)->valloc(size);
}

void* quill_calloc(Allocator const allocator, size_t const count, size_t const size) {
    return internal(allocator)->calloc(count, size);
}

void* quill_realloc(Allocator const allocator, void* const ptr, size_t const size) {
    return internal(allocator)->realloc(ptr, size);
}

void* quill_reallocf(Allocator const allocator, void* const ptr, size_t const size) {
    return internal(allocator)->reallocf(ptr, size);
}

void* quill_aligned_alloc(Allocator const allocator, size_t const alignment, size_t const size) {
    return internal(allocator)->aligned_alloc(alignment, size);
}

void  quill_free(Allocator const allocator, void* const ptr) {
    internal(allocator)->free(ptr);
}
