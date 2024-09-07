#ifndef snowy_allocator_h
#define snowy_allocator_h

#include <stdlib.h>

typedef void* (*const MallocFn)(size_t size);
typedef void* (*const VallocFn)(size_t size);
typedef void* (*const CallocFn)(size_t count, size_t size);
typedef void* (*const ReallocFn)(void* ptr, size_t size);
typedef void* (*const ReallocOrFreeFn)(void* ptr, size_t size);
typedef void* (*const AlignedAllocFn)(size_t alignment, size_t size);
typedef void (*const FreeFn)(void* ptr);

typedef struct {
    MallocFn        malloc;
    VallocFn        valloc;
    CallocFn        calloc;
    ReallocFn       realloc;
    ReallocOrFreeFn reallocf;
    AlignedAllocFn  aligned_alloc;
    FreeFn          free;
} Allocator;

Allocator allocator_create(void);

#endif
