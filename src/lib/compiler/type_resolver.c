#include <stdio.h>

#include "./ast.h"
#include "./type_resolver.h"

typedef bool Changed;

TypeResolver type_resolver_create(Arena* arena, Packages packages) {
    return (TypeResolver){
        .arena = arena,
        .packages = packages,
    };
}

Changed resolve_type_node(TypeResolver* type_resolver, ASTNode const* node) {
    // TODO

    // switch (node->type) {
    //     case ANT_FILE_ROOT: {}
    // }

    return false;
}

void resolve_types(TypeResolver* type_resolver) {
    Changed changed = true;
    while (changed) {
        changed = false;

        for (size_t i = 0; i < type_resolver->packages.lookup_length; ++i) {
            ArrayList_Package* bucket = type_resolver->packages.lookup_buckets + i;
            if (!bucket) {
                continue;
            }

            for (size_t j = 0; j < bucket->length; ++j) {
                Package* package = bucket->array + j;
                changed |= resolve_type_node(type_resolver, package->ast);
            }
        }
    }
}
