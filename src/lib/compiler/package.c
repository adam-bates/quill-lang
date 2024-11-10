#include <stdio.h>
#include <string.h>

#include "./package.h"
#include "./ast.h"
#include "resolved_type.h"

#define INITIAL_PACKAGES_CAPACITY 1
#define HASHTABLE_BUCKETS 16

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static ArrayList_Package arraylist_package_create_with_capacity(Arena* arena, size_t const capacity) {
    Package* array = arena_alloc(arena, capacity * sizeof(Package));

    return (ArrayList_Package){
        .capacity = capacity,
        .length = 0,

        .array = array,
    };
}

static ArrayList_Package arraylist_package_create(Arena* arena) {
    return arraylist_package_create_with_capacity(arena, INITIAL_PACKAGES_CAPACITY);
}

static void arraylist_package_push(Arena* arena, ArrayList_Package* const list, Package const package) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->array = arena_realloc(arena, list->array, sizeof(Package) * prev_cap, sizeof(Package) * list->capacity);
    }

    list->array[list->length] = package;
    list->length += 1;
}

static size_t hash_name(PackagePath* name) {
    if (!name) { return 0; }

    // FNV-1a
    size_t hash = FNV_OFFSET_BASIS;
    PackagePath* curr = name;
    while (curr) {
        for (size_t i = 0; i < curr->name.length; ++i) {
            char c = curr->name.chars[i];
            hash ^= c;
            hash *= FNV_PRIME;
        }
        curr = curr->child;
    }

    // map to bucket index
    // note: reserve index 0 for no package name
    hash %= HASHTABLE_BUCKETS - 1;
    return 1 + hash;
}

Package* packages_resolve_or_create(Packages* packages, PackagePath* name) {
    size_t idx = hash_name(name);
    ArrayList_Package* bucket = packages->lookup_buckets + idx;
    if (bucket->array == NULL) {
        *bucket = arraylist_package_create(packages->arena);
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        if (package_path_eq(bucket->array[i].full_name, name)) {
            return bucket->array + i;
        }
    }
    arraylist_package_push(packages->arena, bucket, (Package){
        .full_name = name,
        .ast = NULL,
    });
    packages->count += 1;

    return bucket->array + (bucket->length - 1);
}

Package* packages_resolve(Packages* packages, PackagePath* name) {
    size_t idx = hash_name(name);
    ArrayList_Package* bucket = packages->lookup_buckets + idx;
    if (bucket->array == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        if (package_path_eq(bucket->array[i].full_name, name)) {
            return bucket->array + i;
        }
    }

    return NULL;
}

Packages packages_create(Arena* arena) {
    return (Packages){
        .arena = arena,

        .lookup_length = HASHTABLE_BUCKETS,
        .lookup_buckets = arena_calloc(arena, HASHTABLE_BUCKETS, sizeof(ArrayList_Package)),

        .types_length = 0,
        .types = NULL,

        .generic_impls_nodes_length = 0,
        .generic_impls_nodes_raw = NULL,
        .generic_impls_nodes_concrete = NULL,

        .count = 0,

        .string_literal_type = NULL,
    };
}

TypeInfo* packages_type_by_node(Packages* packages, NodeId node_id) {
    return packages->types + node_id.val;
}

TypeInfo* packages_type_by_type(Packages* packages, TypeId type_id) {
    return packages->types + (packages->types_length - 1 - type_id.val);
}

void ll_generic_impls_push(Arena* arena, LL_GenericImpl* generic_impls, GenericImpl generic_impl) {
    LLNode_GenericImpl* node = arena_alloc(arena, sizeof *generic_impls->tail);
    *node = (LLNode_GenericImpl){
        .data = generic_impl,
        .next = NULL,
    };

    if (!generic_impls->head) {
        generic_impls->head = node;
        generic_impls->tail = node;
    } else {
        generic_impls->tail->next = node;
        generic_impls->tail = node;
    }

    generic_impls->length += 1;
}

void packages_register_generic_impl(Packages* packages, ASTNode* src, size_t resolved_types_length, ResolvedType** resolved_types) {
    if (resolved_types_length == 0) {
        return;
    }

    assert(packages);
    assert(packages->generic_impls_nodes_raw);
    assert(src);
    assert(resolved_types);

    // if (src->type != ANT_STRUCT_DECL && src->type != ANT_FUNCTION_DECL) {
    //     return;
    // }

    LL_GenericImpl* generic_impls = packages->generic_impls_nodes_raw + src->id.val;
    assert(generic_impls);

    LLNode_GenericImpl* curr = generic_impls->head;
    while (curr) {
        assert(curr->data.length == resolved_types_length);
        assert(curr->data.length > 0);

        bool is_same = true;

        for (size_t i = 0; i < curr->data.length; ++i) {
            if (!resolved_type_eq(resolved_types[i], curr->data.resolved_types[i])) {
                is_same = false;
                break;
            }
        }

        if (is_same) {
            return;
        }

        curr = curr->next;
    }

    ll_generic_impls_push(packages->arena, generic_impls, (GenericImpl){
        .length = resolved_types_length,
        .resolved_types = resolved_types,
    });

    // for (size_t idx = 0; idx < resolved_types_length; ++idx) {
    //     ResolvedType* rt = resolved_types[idx];

    //     if (rt->kind == RTK_GENERIC) {
    //         assert(rt->src->type == ANT_STRUCT_DECL || rt->src->type == ANT_FUNCTION_DECL);
    //     }

    //     if (src->type == ANT_STRUCT_DECL) {
    //         assert(src->node.struct_decl.generic_params.length > idx);
    //         String param = src->node.struct_decl.generic_params.array[idx];

    //         println_astnode(*src);
    //         printf("Uses: \n\n");

    //         if (rt->kind == RTK_GENERIC) {
    //             printf("<");
    //             print_string(rt->type.generic.name);
    //             printf("> from ");
    //         }

    //         if (rt->src) {
    //             print_astnode(*rt->src);
    //         } else {
    //             print_resolved_type(rt);
    //         }

    //         Arena arena = {0};
    //         printf("\n\nAs <%s>\n----\n", arena_strcpy(&arena, param).chars);
    //     } else if (src->type == ANT_FUNCTION_DECL) {
    //         assert(src->node.function_decl.header.generic_params.length > idx);
    //         String param = src->node.function_decl.header.generic_params.array[idx];

    //         assert(rt->src->type == ANT_FUNCTION_CALL);
    //         assert(rt->src->node.function_call.generic_args.length == src->node.function_decl.header.generic_params.length);

    //         size_t i = 0;
    //         LLNode_Type* curr = rt->src->node.function_call.generic_args.head;
    //         while (curr && i < idx) {
    //             i += 1;
    //             curr = curr->next;
    //         }
    //         assert(i == idx);

    //         println_astnode(*src);
    //         printf("Uses: \n\n");

    //         print_type(&curr->data);
    //         printf(" from ");
    //         print_astnode(*rt->src);

    //         Arena arena = {0};
    //         printf("\n\nAs <%s>\n----\n", arena_strcpy(&arena, param).chars);
    //     } else {
    //         printf("Unexpected src ANT_%d:\n", src->type);
    //         println_astnode(*src);
    //         assert(false);
    //     }
    // }
}
