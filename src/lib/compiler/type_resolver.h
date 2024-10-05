#ifndef quill_type_resolver_h
#define quill_type_resolver_h

#include "./package.h"
#include "./resolved_type.h"
#include "../utils/utils.h"

typedef struct {
    Arena* arena;
    Packages packages;

    Package* current_package;
    ResolvedFunction* current_function;
    bool seen_separator;
} TypeResolver;

TypeResolver type_resolver_create(Arena* arena, Packages packages);

void resolve_types(TypeResolver* type_resolver);

#endif
