#include <stdio.h>
#include <string.h>

#include "./ast.h"
#include "./type_resolver.h"
#include "./resolved_type.h"
#include "./package.h"

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    PackagePath* dependents;
    PackagePath* dependees;
} AdjacencyList;

static AdjacencyList adjacency_list_create(Arena* arena, size_t capacity) {
    PackagePath* dependents = arena_alloc(arena, sizeof(PackagePath) * capacity);
    PackagePath* dependees = arena_alloc(arena, sizeof(PackagePath) * capacity);

    return (AdjacencyList){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .dependents = dependents,
        .dependees = dependees,
    };
}

static void add_package_dependency(AdjacencyList* list, PackagePath dependent, PackagePath dependee) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->dependents = arena_realloc(list->arena, list->dependents, sizeof(PackagePath) * prev_cap, sizeof(PackagePath) * list->capacity);
        list->dependees = arena_realloc(list->arena, list->dependees, sizeof(PackagePath) * prev_cap, sizeof(PackagePath) * list->capacity);
    }

    list->dependents[list->length] = dependent;
    list->dependees[list->length] = dependee;
    list->length += 1;
}

typedef enum {
    VS_UNVISITED,
    VS_VISITING,
    VS_VISITED,
} VisitState;

static bool _topo_sort_dfs(AdjacencyList* list, VisitState* states, size_t n, size_t* sorted, size_t* sort_index) {
    states[n] = VS_VISITING;

    for (size_t i = 0; i < list->length; ++i) {
        if (!package_path_eq(list->dependees + i, list->dependents + n)) {
            continue;
        }

        if (
            states[i] == VS_VISITING
            || (states[i] == VS_UNVISITED && _topo_sort_dfs(list, states, i, sorted, sort_index))
        ) {
            return false;
        }
    }

    states[n] = VS_VISITED;
    sorted[*sort_index] = n;
    *sort_index += 1;

    return true;
}

static bool topological_sort(AdjacencyList* list, size_t* sorted) {
    VisitState* states = arena_calloc(list->arena, list->length, sizeof *states);
    size_t sort_index = 0;

    for (size_t i = 0; i < list->length; ++i) {
        if (states[i] == VS_UNVISITED && !_topo_sort_dfs(list, states, i, sorted, &sort_index)) {
            // cycle detected, can't sort
            return false;
        }
    }

    return true;
}

static void resolve_types_across_files(TypeResolver* type_resolver) {
    AdjacencyList dependencies = adjacency_list_create(type_resolver->arena, 1);

    for (size_t i = 0; i < type_resolver->packages.lookup_length; ++i) {
        ArrayList_Package bucket = type_resolver->packages.lookup_buckets[i];

        for (size_t j = 0; j < bucket.length; ++j) {
            Package pkg = bucket.array[j];

            assert(pkg.ast);
            assert(pkg.ast->type == ANT_FILE_ROOT);

            PackagePath dependent = {0};
            if (pkg.full_name) {
                dependent = *pkg.full_name;
            }

            LLNode_ASTNode* decl = pkg.ast->node.file_root.nodes.head;
            while (decl) {
                if (decl->data.type == ANT_IMPORT) {
                    ImportPath* path = decl->data.node.import.import_path;
                    PackagePath* dependee = import_path_to_package_path(type_resolver->arena, path);
                    assert(dependee);

                    Package* found = packages_resolve(&type_resolver->packages, dependee);
                    if (!found) {
                        printf("Cannot find package [%s]\n", package_path_to_str(type_resolver->arena, dependee).chars);
                        assert(false);
                    }

                    add_package_dependency(&dependencies, dependent, *dependee);
                }

                decl = decl->next;
            }
        }
    }

    printf("DEPENDENCIES:\n");
    for (size_t i = 0; i < dependencies.length; ++i) {
        PackagePath p1 = dependencies.dependents[i];
        char const* str1;
        if (p1.name.length > 0) {
            str1 = arena_strcpy(type_resolver->arena, package_path_to_str(type_resolver->arena, &p1)).chars;
        } else {
            str1 = "<main>";
        }

        PackagePath p2 = dependencies.dependees[i];
        char const* str2 = arena_strcpy(type_resolver->arena, package_path_to_str(type_resolver->arena, &p2)).chars;

        printf("- [%s] depends on [%s]\n", str1, str2);
    }

    size_t* sorted = arena_calloc(dependencies.arena, dependencies.length, sizeof *sorted);
    assert(sorted);
    assert(topological_sort(&dependencies, sorted));

    printf("SORTED DEPENDENCIES:\n");
    for (size_t si = 0; si < dependencies.length; ++si) {
        size_t i = sorted[si];

        PackagePath p1 = dependencies.dependents[i];
        char const* str1;
        if (p1.name.length > 0) {
            str1 = arena_strcpy(type_resolver->arena, package_path_to_str(type_resolver->arena, &p1)).chars;
        } else {
            str1 = "<main>";
        }

        PackagePath p2 = dependencies.dependees[i];
        char const* str2 = arena_strcpy(type_resolver->arena, package_path_to_str(type_resolver->arena, &p2)).chars;

        printf("- [%s] depends on [%s]\n", str1, str2);
    }

    //

    assert(false);
}

static void resolve_types_within_files(TypeResolver* type_resolver) {
    // TODO
    assert(false);
}

TypeResolver type_resolver_create(Arena* arena, Packages packages) {
    return (TypeResolver){
        .arena = arena,
        .packages = packages,
    };
}

void resolve_types(TypeResolver* type_resolver) {
    /*
        2-stage algorithm:
            - First stage resolves public types across files. Imports cannot be
              cyclical, meaning we can resolve public declarations by walking the
              tree of imports.
            - Second stage resolves types within each file. It starts by creating a
              Scope at the file level, and putting imported public declarations from
              pass 1 within the file's scope.
    */

    resolve_types_across_files(type_resolver);
    resolve_types_within_files(type_resolver);
}

// #define RESOLVE_ITERS_MAX 1024

// #define HASHTABLE_BUCKETS 256
// #define FNV_OFFSET_BASIS 14695981039346656037ULL
// #define FNV_PRIME 1099511628211ULL

// #define INITIAL_SCOPE_CAPACITY 8
// #define INITIAL_BUCKET_CAPACITY 1

// typedef bool Changed;

// typedef struct {
//     String key;
//     ResolvedType* value;
// } BucketItem;

// typedef struct {
//     Arena* arena;

//     size_t capacity;
//     size_t length;
//     BucketItem* array;
// } Bucket;

// typedef struct Scope {
//     Arena* arena;
//     struct Scope* parent;

//     size_t lookup_length;
//     Bucket* lookup_buckets;
// } Scope;

// static size_t hash_str(String str) {
//     if (!str.length) { return 0; }

//     // FNV-1a
//     size_t hash = FNV_OFFSET_BASIS;
//     for (size_t i = 0; i < str.length; ++i) {
//         char c = str.chars[i];
//         hash ^= c;
//         hash *= FNV_PRIME;
//     }

//     // map to bucket index
//     // note: reserve index 0 for empty string
//     hash %= HASHTABLE_BUCKETS - 1;
//     return 1 + hash;
// }

// static Bucket bucket_create_with_capacity(Arena* arena, size_t capacity) {
//     return (Bucket){
//         .arena = arena,

//         .capacity = capacity,
//         .length = 0,
//         .array = arena_calloc(arena, capacity, sizeof(Bucket)),
//     };
// }

// static Bucket bucket_create(Arena* arena) {
//     return bucket_create_with_capacity(arena, INITIAL_BUCKET_CAPACITY);
// }

// static void bucket_push(Bucket* list, BucketItem item) {
//     if (list->length >= list->capacity) {
//         size_t prev_cap = list->capacity;
//         list->capacity = list->length * 2;
//         list->array = arena_realloc(
//             list->arena,
//             list->array,
//             sizeof(Token) * prev_cap,
//             sizeof(Token) * list->capacity
//         );
//     }

//     list->array[list->length] = item;
//     list->length += 1;
// }


// static Scope scope_create(Arena* arena, Scope* parent) {
//     return (Scope){
//         .arena = arena,
//         .parent = parent,

//         .lookup_length = HASHTABLE_BUCKETS,
//         .lookup_buckets = arena_calloc(arena, HASHTABLE_BUCKETS, sizeof(Bucket)),
//     };
// }

// static void scope_set(Scope* scope, String key, ResolvedType* value) {
//     size_t idx = hash_str(key);
//     Bucket* bucket = scope->lookup_buckets + idx;

//     if (!bucket) {
//         *bucket = bucket_create(scope->arena);
//     }

//     for (size_t i = 0; i < bucket->length; ++i) {
//         BucketItem* item = bucket->array + i;

//         if (str_eq(item->key, key)) {
//             item->value = value;
//         }
//     }

//     bucket_push(bucket, (BucketItem){ key, value });
// }

// static ResolvedType* _scope_get(Scope* scope, String key, size_t idx) {
//     Bucket* bucket = scope->lookup_buckets + idx;

//     if (bucket) {
//         for (size_t i = 0; i < bucket->length; ++i) {
//             BucketItem* item = bucket->array + i;

//             if (str_eq(item->key, key)) {
//                 return item->value;
//             }
//         }
//     }

//     if (scope->parent) {
//         return _scope_get(scope->parent, key, idx);
//     } else {
//         return NULL;
//     }
// }

// static ResolvedType* scope_get(Scope* scope, String key) {
//     return _scope_get(scope, key, hash_str(key));
// }

// Changed resolve_type_node(TypeResolver* type_resolver, Scope* scope, ASTNode const* node) {
//     Changed changed = false;
//     switch (node->type) {
//         case ANT_FILE_ROOT: {
//             LLNode_ASTNode* curr = node->node.file_root.nodes.head;
//             while (curr) {
//                 changed |= resolve_type_node(type_resolver, scope, &curr->data);
//                 curr = curr->next;
//             }
//             break;
//         }

//         case ANT_PACKAGE: {
//             // assert(false); // TODO
//             break;
//         }

//         case ANT_IMPORT: {
//             assert(node->node.import.type == IT_DEFAULT); // TODO

//             Package* package = packages_resolve(&type_resolver->packages, node->node.import.static_path);
//             assert(package);

//             TypeInfo* ti = type_resolver->packages.types + node->id;
//             assert(ti);

//             ti->type = arena_alloc(type_resolver->arena, sizeof *ti->type);
//             *ti->type = (ResolvedType){
//                 .kind = RTK_NAMESPACE,
//                 .src = node,
//                 .type.package_ast = package->ast,
//             };

//             scope_set(scope, node->node.import.static_path->name, ti->type);

//             break;
//         };

//         case ANT_FILE_SEPARATOR: {
//             // type_resolver->seen_separator = true;
//             break;
//         }

//         case ANT_UNARY_OP: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.unary_op.right);
//             break;
//         }

//         case ANT_BINARY_OP: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.lhs);
//             changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.rhs);
//             break;
//         }

//         case ANT_VAR_DECL: {
//             bool is_let = node->node.var_decl.type_or_let.is_let;
//             bool has_init = !!node->node.var_decl.initializer;

//             if (is_let) {
//                 if (has_init) {
//                     changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
//                     type_resolver->packages.types[node->id] = type_resolver->packages.types[node->node.var_decl.initializer->id];
//                 } else {
//                     type_resolver->packages.types[node->id] = (TypeInfo){
//                         .status = TIS_UNKNOWN,
//                         .type = NULL,
//                     };
//                 }
//             } else {
//                 // changed |= resolve_type_type(type_resolver, scope, node->node.var_decl.type_or_let.maybe_type);

//                 if (has_init) {
//                     changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
//                     // assert(type.resolved_type == node.resolved_type); // TODO

//                     type_resolver->packages.types[node->id] = type_resolver->packages.types[node->node.var_decl.initializer->id];
//                 }
//             }

//             assert(node->node.var_decl.lhs.type == VDLT_NAME); // TODO
//             scope_set(scope, node->node.var_decl.lhs.lhs.name, type_resolver->packages.types[node->id].type);

//             break;
//         }

//         case ANT_GET_FIELD: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.get_field.root);

//             // TODO: get struct field type
//             type_resolver->packages.types[node->id] = (TypeInfo){
//                 .status = TIS_UNKNOWN,
//                 .type = NULL,
//             };

//             break;
//         }

//         case ANT_ASSIGNMENT: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.assignment.lhs);
//             changed |= resolve_type_node(type_resolver, scope, node->node.assignment.rhs);
//             break;
//         }

//         case ANT_FUNCTION_CALL: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.function_call.function);

//             LLNode_ASTNode* curr = node->node.function_call.args.head;
//             while (curr) {
//                 changed |= resolve_type_node(type_resolver, scope, &curr->data);
//                 curr = curr->next;
//             }

//             break;
//         }

//         case ANT_STATEMENT_BLOCK: {
//             Scope block_scope = scope_create(type_resolver->arena, scope);
//             LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
//             while (curr) {
//                 changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
//                 curr = curr->next;
//             }

//             break;
//         }

//         case ANT_IF: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.if_.cond);

//             Scope block_scope = scope_create(type_resolver->arena, scope);
//             LLNode_ASTNode* curr = node->node.if_.block->stmts.head;
//             while (curr) {
//                 changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
//                 curr = curr->next;
//             }

//             break;
//         }

//         case ANT_ELSE: {
//             changed |= resolve_type_node(type_resolver, scope, node->node.else_.target);
//             changed |= resolve_type_node(type_resolver, scope, node->node.else_.then);
//             break;
//         }

//         case ANT_TRY: assert(false); // TODO
//         case ANT_CATCH: assert(false); // TODO
//         case ANT_BREAK: assert(false); // TODO
//         case ANT_WHILE: assert(false); // TODO
//         case ANT_DO_WHILE: assert(false); // TODO
//         case ANT_FOR: assert(false); // TODO

//         case ANT_RETURN: {
//             if (node->node.return_.maybe_expr) {
//                 changed |= resolve_type_node(type_resolver, scope, node->node.return_.maybe_expr);
//             }
//             break;
//         }

//         case ANT_STRUCT_INIT: assert(false); // TODO
//         case ANT_ARRAY_INIT: assert(false); // TODO

//         case ANT_TEMPLATE_STRING: assert(false); // TODO
//         case ANT_CRASH: assert(false); // TODO

//         case ANT_SIZEOF: {
//             // TODO ??
//             break;
//         }

//         case ANT_SWITCH: assert(false); // TODO
//         case ANT_CAST: assert(false); // TODO
//         case ANT_STRUCT_DECL: assert(false); // TODO
//         case ANT_UNION_DECL: assert(false); // TODO
//         case ANT_ENUM_DECL: assert(false); // TODO

//         case ANT_TYPEDEF_DECL: {
//             // changed |= resolve_type_node(type_resolver, node->node.typedef_decl.type);
//             break;
//         }

//         case ANT_GLOBALTAG_DECL: assert(false); // TODO

//         case ANT_FUNCTION_HEADER_DECL: {
//             // changed |= resolve_type_node(type_resolver, &node->node.function_header_decl.return_type);
//             break;
//         }

//         case ANT_FUNCTION_DECL: {
//             // changed |= resolve_type_node(type_resolver, &node->node.function_decl.header.return_type);

//             Scope block_scope = scope_create(type_resolver->arena, scope);
//             LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
//             while (curr) {
//                 changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
//                 curr = curr->next;
//             }

//             break;
//         }

//         case ANT_LITERAL: break;
//         case ANT_VAR_REF: break;

//         case ANT_NONE: assert(false);
//         case ANT_COUNT: assert(false);

//         default: assert(false);
//     }

//     return changed;
// }

// void resolve_types(TypeResolver* type_resolver) {
//     size_t iter = 0;
//     Changed changed = true;
//     while (changed) {
//         assert(iter++ < RESOLVE_ITERS_MAX);
//         changed = false;

//         for (size_t i = 0; i < type_resolver->packages.lookup_length; ++i) {
//             ArrayList_Package* bucket = type_resolver->packages.lookup_buckets + i;
//             if (!bucket) {
//                 continue;
//             }

//             for (size_t j = 0; j < bucket->length; ++j) {
//                 Package* package = bucket->array + j;
//                 Scope scope = scope_create(type_resolver->arena, NULL);
//                 changed |= resolve_type_node(type_resolver, &scope, package->ast);
//             }
//         }
//     }
// }
