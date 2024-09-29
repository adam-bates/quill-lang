#ifndef quill_allocator_h
#define quill_allocator_h

#include "./base.h"

typedef struct {
    void const* _internal;
} Allocator;

typedef struct {
    bool const ok;
    union {
        Allocator const allocator;
        void const* const _;
    } maybe;
} MaybeAllocator;

MaybeAllocator allocator_create(void);
void allocator_destroy(Allocator const allocator);

void* quill_malloc(Allocator const allocator, size_t const size);
void* quill_valloc(Allocator const allocator, size_t const size);
void* quill_calloc(Allocator const allocator, size_t const count, size_t const size);
void* quill_realloc(Allocator const allocator, void* const ptr, size_t const size);
void* quill_reallocf(Allocator const allocator, void* const ptr, size_t const size);
void* quill_aligned_alloc(Allocator const allocator, size_t const alignment, size_t const size);
void  quill_free(Allocator const allocator, void* const ptr);

#endif
