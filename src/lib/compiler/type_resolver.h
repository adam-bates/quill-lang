#ifndef quill_type_resolver_h
#define quill_type_resolver_h

#include "./package.h"
#include "../utils/utils.h"

typedef struct {
    Arena* arena;
    Packages packages;

    PackagePath* current_package;
} TypeResolver;

TypeResolver type_resolver_create(Arena* arena, Packages packages);

void resolve_types(TypeResolver* type_resolver);

#endif
