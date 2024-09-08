#ifndef quillc_allocator_h
#define quillc_allocator_h

#include <stdlib.h>

typedef void* (*const MallocFn)(size_t size);
typedef void* (*const VallocFn)(size_t size);
typedef void* (*const CallocFn)(size_t count, size_t size);
typedef void* (*const ReallocFn)(void* ptr, size_t size);
typedef void* (*const ReallocOrFreeFn)(void* ptr, size_t size);
typedef void* (*const AlignedAllocFn)(size_t alignment, size_t size);
typedef void (*const FreeFn)(void* ptr);

typedef struct {
    MallocFn const        malloc;
    VallocFn const        valloc;
    CallocFn const        calloc;
    ReallocFn const       realloc;
    ReallocOrFreeFn const reallocf;
    AlignedAllocFn const  aligned_alloc;
    FreeFn const          free;
} Allocator_s;

typedef Allocator_s const* Allocator;

Allocator_s allocator_create(void);

#endif
