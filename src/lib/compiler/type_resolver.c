#include <stdio.h>
#include <string.h>

#include "./ast.h"
#include "./type_resolver.h"
#include "./resolved_type.h"
#include "./package.h"

#define HASHTABLE_BUCKETS 256
#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

#define INITIAL_SCOPE_CAPACITY 8
#define INITIAL_BUCKET_CAPACITY 1

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    PackagePath* dependents;
    PackagePath* dependencies;
} AdjacencyList;

static AdjacencyList adjacency_list_create(Arena* arena, size_t capacity) {
    PackagePath* dependents = arena_alloc(arena, sizeof(PackagePath) * capacity);
    PackagePath* dependencies = arena_alloc(arena, sizeof(PackagePath) * capacity);

    return (AdjacencyList){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .dependents = dependents,
        .dependencies = dependencies,
    };
}

static void add_package_dependency(AdjacencyList* list, PackagePath dependent, PackagePath dependency) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->dependents = arena_realloc(list->arena, list->dependents, sizeof(PackagePath) * prev_cap, sizeof(PackagePath) * list->capacity);
        list->dependencies = arena_realloc(list->arena, list->dependencies, sizeof(PackagePath) * prev_cap, sizeof(PackagePath) * list->capacity);
    }

    list->dependents[list->length] = dependent;
    list->dependencies[list->length] = dependency;
    list->length += 1;
}

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    PackagePath* array;
} ArrayList_PackagePath;

static ArrayList_PackagePath arraylist_packagepath_create(Arena* arena, size_t capacity) {
    PackagePath* array = arena_alloc(arena, sizeof(PackagePath) * capacity);

    return (ArrayList_PackagePath){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .array = array,
    };
}

static void arraylist_packagepath_push(ArrayList_PackagePath* list, PackagePath path) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->array = arena_realloc(list->arena, list->array, sizeof(PackagePath) * prev_cap, sizeof(PackagePath) * list->capacity);
    }

    list->array[list->length] = path;
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
        if (!package_path_eq(list->dependencies + i, list->dependents + n)) {
            continue;
        }

        if (
            states[i] == VS_VISITING
            || (states[i] == VS_UNVISITED && !_topo_sort_dfs(list, states, i, sorted, sort_index))
        ) {
            return false;
        }
    }

    states[n] = VS_VISITED;
    sorted[*sort_index] = n;
    *sort_index -= 1;

    return true;
}

static bool topological_sort(AdjacencyList* list, size_t* sorted) {
    VisitState* states = arena_calloc(list->arena, list->length, sizeof *states);
    size_t sort_index = list->length - 1;

    for (size_t i = 0; i < list->length; ++i) {
        if (states[i] == VS_UNVISITED && !_topo_sort_dfs(list, states, i, sorted, &sort_index)) {
            // cycle detected, can't sort
            return false;
        }
    }

    return true;
}

typedef bool Changed;

typedef struct {
    String key;
    ResolvedType* value;
} BucketItem;

typedef struct {
    Arena* arena;

    size_t capacity;
    size_t length;
    BucketItem* array;
} Bucket;

typedef struct Scope {
    Arena* arena;
    struct Scope* parent;

    size_t lookup_length;
    Bucket* lookup_buckets;
} Scope;

static size_t hash_str(String str) {
    if (!str.length) { return 0; }

    // FNV-1a
    size_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < str.length; ++i) {
        char c = str.chars[i];
        hash ^= c;
        hash *= FNV_PRIME;
    }

    // map to bucket index
    // note: reserve index 0 for empty string
    hash %= HASHTABLE_BUCKETS - 1;
    return 1 + hash;
}

static Bucket bucket_create_with_capacity(Arena* arena, size_t capacity) {
    return (Bucket){
        .arena = arena,

        .capacity = capacity,
        .length = 0,
        .array = arena_calloc(arena, capacity, sizeof(Bucket)),
    };
}

static Bucket bucket_create(Arena* arena) {
    return bucket_create_with_capacity(arena, INITIAL_BUCKET_CAPACITY);
}

static void bucket_push(Bucket* list, BucketItem item) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->array = arena_realloc(
            list->arena,
            list->array,
            sizeof(Token) * prev_cap,
            sizeof(Token) * list->capacity
        );
    }

    list->array[list->length] = item;
    list->length += 1;
}


static Scope scope_create(Arena* arena, Scope* parent) {
    return (Scope){
        .arena = arena,
        .parent = parent,

        .lookup_length = HASHTABLE_BUCKETS,
        .lookup_buckets = arena_calloc(arena, HASHTABLE_BUCKETS, sizeof(Bucket)),
    };
}

static void scope_set(Scope* scope, String key, ResolvedType* value) {
    size_t idx = hash_str(key);
    Bucket* bucket = scope->lookup_buckets + idx;
    assert(bucket);

    if (!bucket->array) {
        *bucket = bucket_create(scope->arena);
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        BucketItem* item = bucket->array + i;

        if (str_eq(item->key, key)) {
            item->value = value;
        }
    }

    bucket_push(bucket, (BucketItem){ key, value });
}

static ResolvedType* _scope_get(Scope* scope, String key, size_t idx) {
    Bucket* bucket = scope->lookup_buckets + idx;

    if (bucket) {
        for (size_t i = 0; i < bucket->length; ++i) {
            BucketItem* item = bucket->array + i;

            if (str_eq(item->key, key)) {
                return item->value;
            }
        }
    }

    if (scope->parent) {
        return _scope_get(scope->parent, key, idx);
    } else {
        return NULL;
    }
}

static ResolvedType* scope_get(Scope* scope, String key) {
    return _scope_get(scope, key, hash_str(key));
}

TypeResolver type_resolver_create(Arena* arena, Packages* packages) {
    return (TypeResolver){
        .arena = arena,
        .packages = packages,

        .current_package = NULL,
        .current_function = NULL,
        .seen_separator = false,
    };
}

static ImportPath* expand_import_path(TypeResolver* type_resolver, ASTNodeImport* import) {
    if (!import) {
        return NULL;
    }

    switch (import->type) {
        case IT_DEFAULT: return import->import_path;

        case IT_LOCAL: {
            ImportPath* local = package_path_to_import_path(type_resolver->arena, type_resolver->current_package->full_name);
            ImportPath* curr = local;
            while (curr->type == IPT_DIR && curr->import.dir.child->type == IPT_DIR) {
                curr = curr->import.dir.child;
                assert(curr);
            }
            curr->import.dir.child = import->import_path;
            return local;
        }

        case IT_ROOT: {
            ImportPath* root = arena_alloc(type_resolver->arena, sizeof *root);
            root->type = IPT_DIR;
            root->import.dir.name = type_resolver->current_package->full_name->name;
            root->import.dir.child = import->import_path;
            return root;
        }
    }
}

static void resolve_file(TypeResolver* type_resolver, Scope* scope, ASTNodeFileRoot file);

void resolve_types(TypeResolver* type_resolver) {
    AdjacencyList dependencies = adjacency_list_create(type_resolver->arena, 1);
    ArrayList_PackagePath to_resolve = arraylist_packagepath_create(type_resolver->arena, type_resolver->packages->count);

    // Understand which files depend on which other files
    for (size_t i = 0; i < type_resolver->packages->lookup_length; ++i) {
        ArrayList_Package bucket = type_resolver->packages->lookup_buckets[i];

        for (size_t j = 0; j < bucket.length; ++j) {
            Package pkg = bucket.array[j];

            assert(pkg.ast);
            assert(pkg.ast->type == ANT_FILE_ROOT);

            PackagePath dependent = {0};
            if (pkg.full_name) {
                dependent = *pkg.full_name;
            }

            LLNode_ASTNode* decl = pkg.ast->node.file_root.nodes.head;
            bool has_imports = false;
            while (decl) {
                if (decl->data.type == ANT_IMPORT) {
                    has_imports = true;

                    type_resolver->current_package = &pkg;
                    ImportPath* path = expand_import_path(type_resolver, &decl->data.node.import);
                    type_resolver->current_package = NULL;

                    PackagePath* dependency = import_path_to_package_path(type_resolver->arena, path);
                    assert(dependency);

                    Package* found = packages_resolve(type_resolver->packages, dependency);
                    if (!found) {
                        printf("FROM FILE: \"%s\"\n", package_path_to_str(type_resolver->arena, &dependent).chars);
                        printf("Cannot find import [%s]\n", import_path_to_str(type_resolver->arena, path).chars);
                        printf("Cannot find package [%s]\n", package_path_to_str(type_resolver->arena, dependency).chars);
                        assert(false);
                    }

                    add_package_dependency(&dependencies, dependent, *dependency);
                }

                decl = decl->next;
            }

            // will resolve files w/ no dependencies first
            if (!has_imports) {
                arraylist_packagepath_push(&to_resolve, dependent);
            }
        }
    }

    // sort topologically so packages are only resolved after all of their dependencies
    {
        printf("DEPENDENCIES:\n");
        for (size_t i = 0; i < dependencies.length; ++i) {
            PackagePath p1 = dependencies.dependents[i];
            char const* str1;
            if (p1.name.length > 0) {
                str1 = package_path_to_str(type_resolver->arena, &p1).chars;
            } else {
                str1 = "<main>";
            }

            PackagePath p2 = dependencies.dependencies[i];
            char const* str2 = package_path_to_str(type_resolver->arena, &p2).chars;

            printf("- [%s] depends on [%s]\n", str1, str2);
        }
        printf("\n");

        size_t* sorted_deps = arena_calloc(dependencies.arena, dependencies.length, sizeof *sorted_deps);
        assert(sorted_deps);
        assert(topological_sort(&dependencies, sorted_deps));

        PackagePath* prev = NULL;
        for (size_t si = 0; si < dependencies.length; ++si) {
            size_t i = sorted_deps[si];

            PackagePath* path = dependencies.dependents + i;
            assert(path);

            if (prev && package_path_eq(prev, path)) {
                continue;
            }
            prev = path;

            arraylist_packagepath_push(&to_resolve, *path);
        }

        // debug print
        printf("DEPENDENCIES:\n");
        for (size_t si = 0; si < dependencies.length; ++si) {
            size_t i = sorted_deps[si];

            PackagePath p1 = dependencies.dependents[i];
            char const* str1;
            if (p1.name.length > 0) {
                str1 = package_path_to_str(type_resolver->arena, &p1).chars;
            } else {
                str1 = "<main>";
            }

            PackagePath p2 = dependencies.dependencies[i];
            char const* str2 = package_path_to_str(type_resolver->arena, &p2).chars;

            printf("- [%s] depends on [%s]\n", str1, str2);
        }
        printf("\n");

        printf("Resolve order:\n");
        for (size_t i = 0; i < to_resolve.length; ++i) {
            PackagePath* path = to_resolve.array + i;
            assert(path);

            char const* name = "<main>";
            if (path->name.length > 0) {
                name = package_path_to_str(type_resolver->arena, path).chars;
            }
            printf("- %s\n", name);
        }
        printf("\n");
    }

    for (size_t i = 0; i < to_resolve.length; ++i) {
        PackagePath* path = to_resolve.array + i;
        if (path->name.length == 0) {
            path = NULL;
        }

        Package* pkg = packages_resolve(type_resolver->packages, path);
        assert(pkg);
        assert(pkg->ast->type == ANT_FILE_ROOT);

        printf("Resolving package \"%s\"...\n",
            path ? package_path_to_str(type_resolver->arena, pkg->full_name).chars : "<main>"
        );

        type_resolver->current_package = pkg;
        Scope scope = scope_create(type_resolver->arena, NULL);
        resolve_file(type_resolver, &scope, pkg->ast->node.file_root);
        type_resolver->current_package = NULL;
    }
    printf("\n");
}

static ResolvedType* calc_resolved_type(TypeResolver* type_resolver, Scope* scope, Type* type);

static ResolvedType* calc_static_path_type(TypeResolver* type_resolver, Scope* scope, TypeStaticPath* t_static_path) {
    assert(t_static_path);
    assert(t_static_path->path);

    StaticPath* static_path = t_static_path->path;

    ResolvedType* rt = scope_get(scope, static_path->name);
    if (!rt) {
        return NULL;
    }

    while (static_path->child) {
        static_path = static_path->child;
        switch (rt->kind) {
            case RTK_NAMESPACE: {
                assert(rt->type.namespace_->ast->type == ANT_FILE_ROOT);

                ASTNode* node = find_decl_by_name(rt->type.namespace_->ast->node.file_root, static_path->name);
                assert(node);

                ResolvedType* tmp = packages_type_by_node(type_resolver->packages, node->id)->type;
                if (!tmp) {
                    printf("failed lookup for: \"%s\"\n", arena_strcpy(type_resolver->arena, static_path->name).chars);
                    assert(rt->type.namespace_->ast->node.file_root.nodes.head);
                    assert(rt->type.namespace_->ast->node.file_root.nodes.head->data.type == ANT_PACKAGE);
                    printf("Package: \"%s\"\n", package_path_to_str(type_resolver->arena, rt->type.namespace_->ast->node.file_root.nodes.head->data.node.package.package_path).chars);
                }
                assert(tmp);
                rt = tmp;

                break;
            }

            default: assert(false);
        }
    }
    assert(rt);

    if (rt->kind == RTK_STRUCT_DECL) {
        ArrayList_LL_Type* generic_impls = &rt->src->node.struct_decl.generic_impls;
        if (!generic_impls || !generic_impls->array) {
            *generic_impls = arraylist_typells_create(type_resolver->arena);
        }

        Scope fields_scope = scope_create(type_resolver->arena, scope);
        for (size_t i = 0; i < rt->type.struct_decl.generic_params.length; ++i) {
            ResolvedType* generic_rt = arena_alloc(type_resolver->arena, sizeof *generic_rt);
            generic_rt->kind = RTK_GENERIC;
            generic_rt->type.generic.name = rt->type.struct_decl.generic_params.strings[i];
            generic_rt->from_pkg = type_resolver->current_package;
            scope_set(&fields_scope, rt->type.struct_decl.generic_params.strings[i], generic_rt);
        }

        ResolvedType rt_new = {
            .from_pkg = rt->from_pkg,
            .src = rt->src,
            .kind = RTK_STRUCT_REF,
            .type.struct_ref = {
                .decl = rt->type.struct_decl,
                .generic_args = {
                    .length = 0,
                    .resolved_types = arena_calloc(type_resolver->arena, t_static_path->generic_types.length, sizeof(ResolvedType)),
                },
                .impl_version = 0,
            },
        };
        rt = arena_alloc(type_resolver->arena, sizeof *rt);
        *rt = rt_new;

        LLNode_Type* curr = t_static_path->generic_types.head;
        while (curr) {
            ResolvedType* generic_arg = calc_resolved_type(type_resolver, &fields_scope, &curr->data);
            assert(generic_arg);

            assert(rt->type.struct_ref.generic_args.length < t_static_path->generic_types.length);
            rt->type.struct_ref.generic_args.resolved_types[rt->type.struct_ref.generic_args.length++] = *generic_arg;

            curr = curr->next;
        }
    }

    if (rt->kind == RTK_STRUCT_REF && rt->src->type == ANT_STRUCT_DECL) {
        ArrayList_LL_Type* generic_impls = &rt->src->node.struct_decl.generic_impls;

        bool already_has_decl = false;
        for (size_t i = 0; i < generic_impls->length; ++i) {
            if (typells_eq(generic_impls->array[i], t_static_path->generic_types)) {
                already_has_decl = true;
                t_static_path->impl_version = i;
                rt->type.struct_ref.impl_version = i;
                break;
            }
        }

        if (!already_has_decl) {
            t_static_path->impl_version = generic_impls->length;
            rt->type.struct_ref.impl_version = generic_impls->length;
            arraylist_typells_push(generic_impls, t_static_path->generic_types);
        }
    } else {
        assert(t_static_path->generic_types.length == 0);
    }

    return rt;
}

static ResolvedType* calc_resolved_type(TypeResolver* type_resolver, Scope* scope, Type* type) {
    assert(type_resolver);
    assert(scope);
    assert(type);

    // printf("TK-%d\n", type->kind);
    switch (type->kind) {
        case TK_STATIC_PATH: {
            return calc_static_path_type(type_resolver, scope, &type->type.static_path);
        }

        case TK_BUILT_IN: {
            // printf("TBI-%d\n", type->type.built_in);
            switch (type->type.built_in) {
                case TBI_VOID: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_VOID;
                    resolved_type->type.void_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_BOOL: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_BOOL;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_INT: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_INT;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_UINT: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_UINT;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_CHAR: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_CHAR;
                    resolved_type->type.char_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                default: assert(false);
            }

            break;
        }

        case TK_ARRAY: {
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->from_pkg = type_resolver->current_package;
            resolved_type->kind = RTK_ARRAY;
            resolved_type->type.array.of = calc_resolved_type(type_resolver, scope, type->type.array.of);
            *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            return resolved_type;
        }

        case TK_POINTER: {
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->from_pkg = type_resolver->current_package;
            resolved_type->kind = RTK_POINTER;
            resolved_type->type.ptr.of = calc_resolved_type(type_resolver, scope, type->type.ptr.of);
            *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            return resolved_type;
        }

        case TK_MUT_POINTER: {
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->from_pkg = type_resolver->current_package;
            resolved_type->kind = RTK_MUT_POINTER;
            resolved_type->type.mut_ptr.of = calc_resolved_type(type_resolver, scope, type->type.mut_ptr.of);
            *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            return resolved_type;
        }

        default: assert(false);
    }
    assert(false);

    return NULL;
}

static Changed resolve_type_type(TypeResolver* type_resolver, Scope* scope, ASTNode* node, Type* type) {
    if (type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT) {
        return false;
    }

    ResolvedType* resolved_type = calc_resolved_type(type_resolver, scope, type);
    if (!resolved_type) {
        return false;
    }
    resolved_type->from_pkg = type_resolver->current_package;
    resolved_type->src = node;

    type_resolver->packages->types[node->id.val] = (TypeInfo){
        .status = TIS_CONFIDENT,
        .type = resolved_type,
    };

    return true;
}

static Changed resolve_type_node(TypeResolver* type_resolver, Scope* scope, ASTNode* node) {
    Changed changed = false;
    if (node->type != ANT_FUNCTION_DECL && type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT) {
        return changed;
    }
    switch (node->type) {
        case ANT_FILE_ROOT: assert(false);
        case ANT_PACKAGE: {
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            *resolved_type = (ResolvedType){
                .src = node,
                .from_pkg = type_resolver->current_package,
                .kind = RTK_NAMESPACE,
                .type.namespace_ = type_resolver->current_package,
            };
            type_resolver->packages->types[node->id.val] = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };

            break;
        };

        case ANT_IMPORT: {
            ImportPath* import_path = expand_import_path(type_resolver, (ASTNodeImport*)&node->node.import);
            PackagePath* to_import_path = import_path_to_package_path(type_resolver->arena, import_path);
            Package* package = packages_resolve(type_resolver->packages, to_import_path);
            if (!package) {
                printf("ERROR! Could not resolve: \"%s\"\n", package_path_to_str(type_resolver->arena, to_import_path).chars);
            }
            assert(package);

            ImportPath* ip_curr = node->node.import.import_path;
            while (ip_curr->type != IPT_FILE) {
                ip_curr = ip_curr->import.dir.child;
            }
            ImportStaticPath* static_path = ip_curr->import.file.child;

            if (static_path) {
                assert(static_path->type == ISPT_IDENT);
                assert(static_path->import.ident.child == NULL);

                ASTNode* found = find_decl_by_name(package->ast->node.file_root, static_path->import.ident.name);
                TypeInfo* ti = packages_type_by_node(type_resolver->packages, found->id);
                assert(ti);
                assert(ti->status == TIS_CONFIDENT);
                assert(ti->type);

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = ti->type,
                };
                scope_set(scope, static_path->import.ident.name, ti->type);
            } else {
                // Importing a package namespace, no specific decl
                ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                *resolved_type = (ResolvedType){
                    .from_pkg = type_resolver->current_package,
                    .src = node,
                    .kind = RTK_NAMESPACE,
                    .type.namespace_ = package,
                };

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                scope_set(scope, ip_curr->import.file.name, resolved_type);
            }

            break;
        };

        case ANT_FILE_SEPARATOR: {
            type_resolver->seen_separator = true;
            break;
        }

        case ANT_UNARY_OP: {
            changed |= resolve_type_node(type_resolver, scope, node->node.unary_op.right);
            TypeInfo inner_ti = type_resolver->packages->types[node->node.unary_op.right->id.val];
            if (inner_ti.type) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = *inner_ti.type;

                switch (node->node.unary_op.op) {
                    case UO_NUM_NEGATE: {
                        switch (rt->kind) {
                            case RTK_INT: break;
                            case RTK_UINT: rt->kind = RTK_INT; break;
                            case RTK_CHAR: rt->kind = RTK_INT; break;
                            case RTK_BOOL: rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case UO_BOOL_NEGATE: {
                        switch (rt->kind) {
                            case RTK_BOOL: break;
                            case RTK_INT: rt->kind = RTK_BOOL; break;
                            case RTK_UINT: rt->kind = RTK_BOOL; break;
                            case RTK_CHAR: rt->kind = RTK_BOOL; break;

                            case RTK_POINTER:
                            case RTK_MUT_POINTER: {
                                rt->kind = RTK_BOOL;
                                rt->type.bool_ = NULL;
                                break;
                            }

                            default: assert(false);
                        }
                        break;
                    }

                    case UO_PTR_REF: {
                        rt->kind = RTK_MUT_POINTER;
                        rt->type.mut_ptr.of = inner_ti.type;
                        break;
                    }

                    case UO_PTR_DEREF: {
                        switch (rt->kind) {
                            case RTK_POINTER: {
                                *rt = *inner_ti.type->type.ptr.of;
                                break;
                            }
                            case RTK_MUT_POINTER: {
                                *rt = *inner_ti.type->type.mut_ptr.of;
                                break;
                            }

                            default: assert(false);
                        }
                        assert(rt->kind != RTK_VOID && rt->kind != RTK_TERMINAL);
                        break;
                    }

                    case UO_PLUS_PLUS:
                    case UO_MINUS_MINUS: {
                        switch (rt->kind) {
                            case RTK_INT:
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT; break;

                            case RTK_POINTER:
                            case RTK_MUT_POINTER:
                                break;

                            default: assert(false);
                        }
                        break;
                    }

                    default: assert(false);
                }
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = inner_ti.status,
                    .type = rt,
                };
                changed = true;
            }
            break;
        }

        case ANT_BINARY_OP: {
            changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.lhs);
            changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.rhs);

            TypeInfo inner_ti1 = type_resolver->packages->types[node->node.binary_op.lhs->id.val];
            TypeInfo inner_ti2 = type_resolver->packages->types[node->node.binary_op.rhs->id.val];

                printf("\n\n------\n");
                print_astnode(*node);
                printf("\n%s, %s\n", inner_ti1.type ? "true" : "false", inner_ti2.type ? "true" : "false");
                printf("------\n\n\n");

            if (inner_ti1.type && inner_ti2.type) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = *inner_ti1.type;

                switch (node->node.binary_op.op) {
                    case BO_BIT_OR:
                    case BO_BIT_AND:
                    case BO_BIT_XOR:
                    {
                        switch (inner_ti2.type->kind) {
                            case RTK_INT:
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            default: assert(false);
                        }
                        switch (inner_ti1.type->kind) {
                            case RTK_INT:
                            case RTK_UINT:
                                break;

                            case RTK_BOOL:
                            case RTK_CHAR:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_ADD:
                    case BO_SUBTRACT:
                    {
                        switch (inner_ti1.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                            case RTK_POINTER:
                            case RTK_MUT_POINTER:
                                break;

                            case RTK_INT:
                            case RTK_BOOL:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_INT:
                            case RTK_BOOL:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_MULTIPLY:
                    case BO_DIVIDE:
                    {
                        switch (inner_ti1.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_INT:
                            case RTK_BOOL:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_INT:
                            case RTK_BOOL:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_MODULO: {
                        switch (inner_ti1.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_INT:
                            case RTK_BOOL:
                                rt->kind = RTK_INT; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_BOOL:
                                break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_BOOL_OR:
                    case BO_BOOL_AND:
                    {
                        rt->kind = RTK_BOOL;
                        switch (inner_ti1.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_UINT:
                                break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_BOOL:
                                break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_NOT_EQ:
                    case BO_EQ:
                    case BO_LESS:
                    case BO_LESS_OR_EQ:
                    case BO_GREATER:
                    case BO_GREATER_OR_EQ:
                    {
                        rt->kind = RTK_BOOL;
                        bool can_cmp_num = false;
                        switch (inner_ti1.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_UINT:
                                can_cmp_num = true;
                                break;

                            default: break;
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_UINT:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_BOOL:
                                assert(can_cmp_num);
                                break;

                            default: {
                                assert(resolved_type_eq(inner_ti1.type, inner_ti2.type));
                                break;
                            }
                        }
                        break;
                    }

                    default: assert(false);
                }
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = inner_ti1.status,
                    .type = rt,
                };
                changed = true;
            }
            break;
        }

        case ANT_POSTFIX_OP: {
            changed |= resolve_type_node(type_resolver, scope, node->node.postfix_op.left);
            TypeInfo inner_ti = type_resolver->packages->types[node->node.postfix_op.left->id.val];
            if (inner_ti.type) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = *inner_ti.type;

                switch (node->node.postfix_op.op) {
                    case PFO_PLUS_PLUS:
                    case PFO_MINUS_MINUS:
                    {
                        switch (rt->kind) {
                            case RTK_INT:
                            case RTK_UINT:
                            case RTK_CHAR:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT; break;

                            case RTK_POINTER:
                            case RTK_MUT_POINTER:
                                break;

                            default: assert(false);
                        }
                        break;
                    }

                    default: assert(false);
                }
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = inner_ti.status,
                    .type = rt,
                };
                changed = true;
            }
            break;
        }

        case ANT_TUPLE: {
            LLNode_ASTNode* curr = node->node.tuple.exprs.head;
            bool resolved = true;
            while (curr) {
                changed |= resolve_type_node(type_resolver, scope, &curr->data);

                if (!type_resolver->packages->types[curr->data.id.val].type) {
                    resolved = false;
                }
                
                curr = curr->next;
            }

            if (resolved) {
                if (node->node.tuple.exprs.length == 1) {
                    type_resolver->packages->types[node->id.val] = type_resolver->packages->types[node->node.tuple.exprs.head->data.id.val];
                    changed = true;
                } else {
                    assert(false);
                }
            }
            break;
        }

        case ANT_VAR_DECL: {
            bool is_let = node->node.var_decl.type_or_let.is_let;
            bool has_init = !!node->node.var_decl.initializer;

            if (is_let) {
                if (has_init) {
                    changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
                    type_resolver->packages->types[node->id.val] = type_resolver->packages->types[node->node.var_decl.initializer->id.val];
                } else {
                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_UNKNOWN,
                        .type = NULL,
                    };
                    assert(false); // TODO
                }
            } else {
                changed |= resolve_type_type(type_resolver, scope, node, node->node.var_decl.type_or_let.maybe_type);

                if (has_init) {
                    if (type_resolver->packages->types[node->id.val].type) {
                        changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
                        // assert(type.resolved_type == node.resolved_type); // TODO

                        if (!type_resolver->packages->types[node->node.var_decl.initializer->id.val].type
                            && node->node.var_decl.initializer->type == ANT_STRUCT_INIT
                        ) {
                            type_resolver->packages->types[node->node.var_decl.initializer->id.val] = type_resolver->packages->types[node->id.val];
                        }
                    }
                }
            }

            if (type_resolver->packages->types[node->id.val].type) {
                scope_set(scope, node->node.var_decl.lhs.lhs.name, type_resolver->packages->types[node->id.val].type);
            }

            break;
        }

        case ANT_GET_FIELD: {
            changed |= resolve_type_node(type_resolver, scope, node->node.get_field.root);

            TypeInfo root = type_resolver->packages->types[node->node.get_field.root->id.val];
            if (!root.type) {
                break;
            }
            ResolvedStructDecl struct_decl;
            if (node->node.get_field.is_ptr_deref) {
                assert(root.type->kind == RTK_POINTER || root.type->kind == RTK_MUT_POINTER);
                if (root.type->kind == RTK_POINTER) {
                    assert(root.type->type.ptr.of->kind == RTK_STRUCT_REF || root.type->type.ptr.of->kind == RTK_STRUCT_DECL);
                    if (root.type->type.ptr.of->kind == RTK_STRUCT_REF) {
                        struct_decl = root.type->type.ptr.of->type.struct_ref.decl;
                    } else {
                        struct_decl = root.type->type.ptr.of->type.struct_decl;
                    }
                } else {
                    assert(root.type->type.mut_ptr.of->kind == RTK_STRUCT_REF || root.type->type.mut_ptr.of->kind == RTK_STRUCT_DECL);
                    if (root.type->type.mut_ptr.of->kind == RTK_STRUCT_REF) {
                        struct_decl = root.type->type.mut_ptr.of->type.struct_ref.decl;
                    } else {
                        struct_decl = root.type->type.mut_ptr.of->type.struct_decl;
                    }
                }
            } else {
                assert(root.type->kind == RTK_STRUCT_REF || root.type->kind == RTK_STRUCT_DECL);
                if (root.type->kind == RTK_STRUCT_REF) {
                    struct_decl = root.type->type.struct_ref.decl;
                } else {
                    struct_decl = root.type->type.struct_decl;
                }
            }

            ResolvedStructField* found = NULL;
            for (size_t i = 0; i < struct_decl.fields_length; ++i) {
                if (str_eq(struct_decl.fields[i].name, node->node.get_field.name)) {
                    found = struct_decl.fields + i;
                    break;
                }
            }
            if (!found) {
                printf("Couldn't find %s\n", arena_strcpy(type_resolver->arena, node->node.get_field.name).chars);
            }
            assert(found);

            type_resolver->packages->types[node->id.val] = (TypeInfo){
                .status = root.status,
                .type = found->type,
            };
            changed = true;

            break;
        }

        case ANT_ASSIGNMENT: {
            changed |= resolve_type_node(type_resolver, scope, node->node.assignment.lhs);
            changed |= resolve_type_node(type_resolver, scope, node->node.assignment.rhs);

            TypeInfo* lhs = packages_type_by_node(type_resolver->packages, node->node.assignment.lhs->id);
            TypeInfo* rhs = packages_type_by_node(type_resolver->packages, node->node.assignment.rhs->id);

            if (lhs->type && rhs->type) {
                // TODO: assert can assign lhs to rhs
                // TODO: assert assignment op makes sense for types

                if (lhs->status < rhs->status) {
                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = lhs->status,
                        .type = lhs->type,
                    };
                } else {
                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = rhs->status,
                        .type = rhs->type,
                    };
                }
            }
            break;
        }

        case ANT_FUNCTION_CALL: {
            changed |= resolve_type_node(type_resolver, scope, node->node.function_call.function);

            if (
                type_resolver->packages->types[node->id.val].status != TIS_CONFIDENT
                && type_resolver->packages->types[node->node.function_call.function->id.val].status == TIS_CONFIDENT
            ) {
                ResolvedType* type = type_resolver->packages->types[node->node.function_call.function->id.val].type->type.function.return_type;
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type,
                };
                changed = true;
            }

            if (type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT) {
                assert(type_resolver->packages->types[node->node.function_call.function->id.val].type->kind == RTK_FUNCTION);

                bool confident_args = true;
                LLNode_ASTNode* curr = node->node.function_call.args.head;
                while (curr) {
                    changed |= resolve_type_node(type_resolver, scope, &curr->data);

                    if (type_resolver->packages->types[curr->data.id.val].status != TIS_CONFIDENT) {
                        confident_args = false;
                    }
                    
                    curr = curr->next;
                }
                if (!confident_args) {
                    break;
                }

                ResolvedType* type = type_resolver->packages->types[node->node.function_call.function->id.val].type->type.function.return_type;
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type,
                };
                changed = true;
            }

            break;
        }

        case ANT_STATEMENT_BLOCK: {
            Scope block_scope = scope_create(type_resolver->arena, scope);
            LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
            bool resolved = true;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                if (!type_resolver->packages->types[curr->data.id.val].type) {
                    resolved = false;
                }
                curr = curr->next;
            }

            if (resolved) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = (ResolvedType){
                    .src = node,
                    .from_pkg = type_resolver->current_package,
                    .kind = RTK_VOID,
                    .type.void_ = NULL,
                };

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = rt,
                };
            }

            break;
        }

        case ANT_IF: {
            changed |= resolve_type_node(type_resolver, scope, node->node.if_.cond);

            bool resolved = true;
            if (type_resolver->packages->types[node->node.if_.cond->id.val].type) {
                assert(type_resolver->packages->types[node->node.if_.cond->id.val].type->kind = RTK_BOOL);
            } else {
                resolved = false;
            }

            {
                Scope block_scope = scope_create(type_resolver->arena, scope);
                LLNode_ASTNode* curr = node->node.if_.block->stmts.head;
                while (curr) {
                    changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                    if (!type_resolver->packages->types[curr->data.id.val].type) {
                        resolved = false;
                    }
                    curr = curr->next;
                }
            }

            if (node->node.if_.else_) {
                changed |= resolve_type_node(type_resolver, scope, node->node.if_.else_);
                if (!type_resolver->packages->types[node->node.if_.else_->id.val].type) {
                    resolved = false;
                }
            }

            if (resolved) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = (ResolvedType){
                    .kind = RTK_VOID,
                };
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .type = rt,
                };
            }

            break;
        }

        case ANT_TRY: assert(false); // TODO
        case ANT_CATCH: assert(false); // TODO
        case ANT_BREAK: assert(false); // TODO

        case ANT_WHILE: {
            changed |= resolve_type_node(type_resolver, scope, node->node.while_.cond);

            bool resolved = true;
            if (type_resolver->packages->types[node->node.while_.cond->id.val].type) {
                assert(type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_BOOL);
            } else {
                resolved = false;
            }

            Scope block_scope = scope_create(type_resolver->arena, scope);
            LLNode_ASTNode* curr = node->node.while_.block->stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                if (!type_resolver->packages->types[curr->data.id.val].type) {
                    resolved = false;
                }
                curr = curr->next;
            }

            if (resolved) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = (ResolvedType){
                    .kind = RTK_VOID,
                };
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .type = rt,
                };
            }

            break;
        }

        case ANT_DO_WHILE: assert(false); // TODO
        case ANT_FOR: assert(false); // TODO

        case ANT_RETURN: {
            assert(type_resolver->current_function);
            assert(type_resolver->current_function->return_type);

            if (node->node.return_.maybe_expr) {
                assert(type_resolver->current_function->return_type->kind != RTK_VOID);

                changed |= resolve_type_node(type_resolver, scope, node->node.return_.maybe_expr);

                if (type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type) {
                    assert(type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type->kind == type_resolver->current_function->return_type->kind);

                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type,
                    };
                    changed = true;
                } else if (node->node.return_.maybe_expr->type == ANT_STRUCT_INIT) {
                    assert(
                        type_resolver->current_function->return_type->kind != RTK_STRUCT_DECL
                        || type_resolver->current_function->return_type->kind != RTK_STRUCT_REF
                    );
                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type_resolver->current_function->return_type,
                    };
                    changed = true;

                    type_resolver->packages->types[node->node.return_.maybe_expr->id.val] = type_resolver->packages->types[node->id.val];
                }
            } else {
                assert(type_resolver->current_function->return_type->kind == RTK_VOID);

                ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                resolved_type->from_pkg = type_resolver->current_package;
                resolved_type->src = node;
                resolved_type->kind = RTK_VOID;
                resolved_type->type.void_ = NULL;

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;
            }
            break;
        }

        case ANT_INDEX: {
            changed |= resolve_type_node(type_resolver, scope, node->node.index.root);
            changed |= resolve_type_node(type_resolver, scope, node->node.index.value);

            if (type_resolver->packages->types[node->node.index.root->id.val].type
                && type_resolver->packages->types[node->node.index.value->id.val].type
            ) {
                ResolvedType* inner;
                switch (type_resolver->packages->types[node->node.index.root->id.val].type->kind) {
                    case RTK_ARRAY: inner = type_resolver->packages->types[node->node.index.root->id.val].type->type.array.of; break;
                    case RTK_POINTER: inner = type_resolver->packages->types[node->node.index.root->id.val].type->type.ptr.of; break;
                    case RTK_MUT_POINTER: inner = type_resolver->packages->types[node->node.index.root->id.val].type->type.mut_ptr.of; break;
                    default: assert(false);
                }
                assert(
                    type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_BOOL
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_CHAR
                );

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = type_resolver->packages->types[node->node.index.root->id.val].status,
                    .type = inner,
                };
            }

            break;
        }

        case ANT_STRUCT_INIT: {
            LLNode_StructFieldInit* curr = node->node.struct_init.fields.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, scope, curr->data.value);
                curr = curr->next;
            }
            break;
        }

        case ANT_ARRAY_INIT: {
            bool resolved = true;
            if (node->node.array_init.maybe_explicit_length) {
                changed |= resolve_type_node(type_resolver, scope, node->node.array_init.maybe_explicit_length);

                ResolvedType* rt = type_resolver->packages->types[node->node.array_init.maybe_explicit_length->id.val].type;
                if (rt) {
                    assert(rt->kind == RTK_INT || rt->kind == RTK_UINT);
                } else {
                    resolved = false;
                }
            }

            LLNode_ArrayInitElem* curr = node->node.array_init.elems.head;

            ResolvedType* of = NULL;

            while (curr) {
                if (curr->data.maybe_index) {
                    changed |= resolve_type_node(type_resolver, scope, curr->data.maybe_index);
                    if (!type_resolver->packages->types[curr->data.maybe_index->id.val].type) {
                        resolved = false;
                    }
                }
                changed |= resolve_type_node(type_resolver, scope, curr->data.value);
                ResolvedType* rt = type_resolver->packages->types[curr->data.value->id.val].type;
                if (resolved && rt) {
                    if (of) {
                        // TODO: assert type matches
                        assert(rt->kind == of->kind);
                    } else {
                        of = rt;
                    }
                } else {
                    resolved = false;
                }
                curr = curr->next;
            }

            if (resolved) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = (ResolvedType){
                    .from_pkg = type_resolver->current_package,
                    .src = node,
                    .kind = RTK_ARRAY,
                    .type.array = {
                        .has_explicit_length = false, // TODO
                        .explicit_length = 0, // TODO
                        .of = of,
                    },
                };

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = rt,
                };
                changed = true;
            }

            break;
        }

        case ANT_TEMPLATE_STRING: {
            bool resolved = true;

            LLNode_ASTNode* curr = node->node.template_string.template_expr_parts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, scope, &curr->data);

                if (!type_resolver->packages->types[curr->data.id.val].type) {
                    resolved = false;
                }
                
                curr = curr->next;
            }

            if (resolved) {
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type_resolver->packages->string_literal_type,
                };
            }
            break;
        }

        case ANT_CRASH: {
           if (node->node.crash.maybe_expr) {
                changed |= resolve_type_node(type_resolver, scope, node->node.crash.maybe_expr);

                if (type_resolver->packages->types[node->node.crash.maybe_expr->id.val].type) {
                    ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                    *rt = (ResolvedType){
                        .from_pkg = type_resolver->current_package,
                        .src = node,
                        .kind = RTK_TERMINAL,
                        .type.terminal = NULL,
                    };
                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = rt,
                    };
                }
            } else {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = (ResolvedType){
                    .from_pkg = type_resolver->current_package,
                    .src = node,
                    .kind = RTK_TERMINAL,
                    .type.terminal = NULL,
                };
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = rt,
                };
            }
            break;
        }

        case ANT_SIZEOF: {
            if (node->node.sizeof_.kind == SOK_EXPR) {
                if (!packages_type_by_node(type_resolver->packages, node->node.sizeof_.sizeof_.expr->id)) {
                    changed |= resolve_type_node(type_resolver, scope, node->node.sizeof_.sizeof_.expr);
                }
            } else if (!packages_type_by_type(type_resolver->packages, node->node.sizeof_.sizeof_.type->id)) {
                ResolvedType* resolved_type = calc_resolved_type(type_resolver, scope, node->node.sizeof_.sizeof_.type);
                assert(resolved_type);
                resolved_type->from_pkg = type_resolver->current_package;
                resolved_type->src = node;
                *packages_type_by_type(type_resolver->packages, node->node.sizeof_.sizeof_.type->id) = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;
            }

            if (type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT) {
                break;
            }
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->from_pkg = type_resolver->current_package;
            resolved_type->src = node;
            resolved_type->kind = RTK_UINT;
            resolved_type->type.uint_ = NULL;

            type_resolver->packages->types[node->id.val] = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            changed = true;

            break;
        }

        case ANT_SWITCH: assert(false); // TODO
        case ANT_CAST: assert(false); // TODO

        case ANT_STRUCT_DECL: {
            assert(node->node.struct_decl.maybe_name); // TODO

            ResolvedStructField* fields = arena_calloc(type_resolver->arena, node->node.struct_decl.fields.length, sizeof *fields);

            Strings generic_params = {
                .length = node->node.struct_decl.generic_params.length,
                .strings = node->node.struct_decl.generic_params.array,
            };

            Scope fields_scope = scope_create(type_resolver->arena, scope);
            for (size_t i = 0; i < generic_params.length; ++i) {
                ResolvedType* generic_rt = arena_alloc(type_resolver->arena, sizeof *generic_rt);
                generic_rt->from_pkg = type_resolver->current_package;
                generic_rt->src = node;
                generic_rt->kind = RTK_GENERIC;
                generic_rt->type.generic.name = generic_params.strings[i];

                scope_set(&fields_scope, generic_params.strings[i], generic_rt);
            }

            size_t i = 0;
            LLNode_StructField* curr = node->node.struct_decl.fields.head;
            while (curr) {
                ResolvedType* resolved_type = calc_resolved_type(type_resolver, &fields_scope, curr->data.type);
                if (!resolved_type) {
                    break;
                }

                fields[i] = (ResolvedStructField){
                    .name = curr->data.name,
                    .type = resolved_type,
                };

                i += 1;
                curr = curr->next;
            }
            if (i < node->node.struct_decl.fields.length) {
                break;
            }

            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->from_pkg = type_resolver->current_package;
            resolved_type->src = node;
            resolved_type->kind = RTK_STRUCT_DECL;
            resolved_type->type.struct_decl = (ResolvedStructDecl){
                .name = *node->node.struct_decl.maybe_name,
                .generic_params = generic_params,
                .fields_length = node->node.struct_decl.fields.length,
                .fields = fields,
            };

            type_resolver->packages->types[node->id.val] = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            scope_set(scope, *node->node.struct_decl.maybe_name, resolved_type);
            changed = true;

            if (node->directives.length > 0) {
                LLNode_Directive* curr = node->directives.head;
                while (curr) {
                    if (curr->data.type == DT_STRING_LITERAL) {
                        // assert(i == 2);

                        assert(fields[0].type->kind == RTK_UINT);
                        // assert(str_eq(fields[0].name, c_str("length")));

                        assert(fields[1].type->kind == RTK_POINTER);
                        assert(fields[1].type->type.ptr.of->kind == RTK_CHAR);
                        // assert(str_eq(fields[1].name, c_str("bytes")));

                        type_resolver->packages->string_literal_type = resolved_type;
                    }
                    curr = curr->next;
                }
            }

            break;
        }

        case ANT_UNION_DECL: assert(false); // TODO
        case ANT_ENUM_DECL: assert(false); // TODO

        case ANT_TYPEDEF_DECL: {
            changed |= resolve_type_type(type_resolver, scope, node, node->node.typedef_decl.type);

            if (type_resolver->packages->types[node->id.val].type) {
                scope_set(scope, node->node.typedef_decl.name, type_resolver->packages->types[node->id.val].type);
            }
            break;
        }

        case ANT_GLOBALTAG_DECL: assert(false); // TODO

        case ANT_FUNCTION_HEADER_DECL: {
            ResolvedFunctionParam* params = arena_calloc(type_resolver->arena, node->node.function_header_decl.params.length, sizeof *params);
            LLNode_FnParam* param = node->node.function_header_decl.params.head;
            size_t i = 0;
            while (param) {
                ResolvedType* param_type = calc_resolved_type(type_resolver, scope, &param->data.type);
                if (!param_type) {
                    break;
                }
                params[i] = (ResolvedFunctionParam){
                    .type = param_type,
                    .name = param->data.name,
                };

                param = param->next;
                i += 1;
            }
            if (i < node->node.function_header_decl.params.length) {
                break;
            }

            ResolvedType* return_type = calc_resolved_type(type_resolver, scope, (Type*)&node->node.function_header_decl.return_type);
            if (!return_type) {
                break;
            }

            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            *resolved_type = (ResolvedType){
                .from_pkg = type_resolver->current_package,
                .src = node,
                .kind = RTK_FUNCTION,
                .type.function = {
                    .params_length = node->node.function_decl.header.params.length,
                    .params = params,
                    .return_type = return_type,
                },
            };

            type_resolver->packages->types[node->id.val] = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            changed = true;

            // TODO: handle cross-checking forward decl.
            scope_set(scope, node->node.function_header_decl.name, resolved_type);

            break;
        }

        case ANT_FUNCTION_DECL: {
            if (type_resolver->packages->types[node->id.val].status != TIS_CONFIDENT) {
                ResolvedFunctionParam* params = arena_calloc(type_resolver->arena, node->node.function_decl.header.params.length, sizeof *params);
                LLNode_FnParam* param = node->node.function_decl.header.params.head;
                size_t i = 0;
                while (param) {
                    ResolvedType* param_type = calc_resolved_type(type_resolver, scope, &param->data.type);
                    if (!param_type) {
                        break;
                    }
                    params[i] = (ResolvedFunctionParam){
                        .type = param_type,
                        .name = param->data.name,
                    };

                    param = param->next;
                    i += 1;
                }
                if (i < node->node.function_decl.header.params.length) {
                    break;
                }

                ResolvedType* return_type = calc_resolved_type(type_resolver, scope, (Type*)&node->node.function_decl.header.return_type);
                if (!return_type) {
                    break;
                }

                ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                *resolved_type = (ResolvedType){
                    .from_pkg = type_resolver->current_package,
                    .src = node,
                    .kind = RTK_FUNCTION,
                    .type.function = {
                        .params_length = node->node.function_decl.header.params.length,
                        .params = params,
                        .return_type = return_type,
                    },
                };

                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;

                // TODO: handle cross-checking forward decl.
                scope_set(scope, node->node.function_header_decl.name, resolved_type);
            }

            ResolvedType* fn_type = type_resolver->packages->types[node->id.val].type;
            assert(fn_type);
            assert(fn_type->kind = RTK_FUNCTION);

            type_resolver->current_function = &fn_type->type.function;

            Scope block_scope = scope_create(type_resolver->arena, scope);

            // Add fn params to block scope
            for (size_t i = 0; i < fn_type->type.function.params_length; ++i) {
                ResolvedFunctionParam param = fn_type->type.function.params[i];
                scope_set(&block_scope, param.name, param.type);
            }

            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr) {
                if (type_resolver->packages->types[curr->data.id.val].status != TIS_CONFIDENT) {
                    changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                }
                curr = curr->next;
            }

            type_resolver->current_function = NULL;

            break;
        }

        case ANT_LITERAL: {
            switch (node->node.literal.kind) {
                case LK_STR: {
                    assert(type_resolver->packages->string_literal_type);

                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type_resolver->packages->string_literal_type,
                    };
                    break;
                }

                case LK_CHAR: {
                    ResolvedType* type = arena_alloc(type_resolver->arena, sizeof *type);
                    type->src = node;
                    type->from_pkg = type_resolver->current_package;
                    type->kind = RTK_CHAR;
                    type->type.char_ = NULL;

                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type,
                    };
                    break;
                }

                case LK_INT: {
                    ResolvedType* type = arena_alloc(type_resolver->arena, sizeof *type);
                    type->src = node;
                    type->from_pkg = type_resolver->current_package;
                    type->kind = RTK_INT;
                    type->type.int_ = NULL;

                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type,
                    };
                    break;
                }

                case LK_BOOL: {
                    ResolvedType* type = arena_alloc(type_resolver->arena, sizeof *type);
                    type->src = node;
                    type->from_pkg = type_resolver->current_package;
                    type->kind = RTK_BOOL;
                    type->type.bool_ = NULL;

                    type_resolver->packages->types[node->id.val] = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = type,
                    };
                    break;
                }

                default: printf("TODO: ANT_LITERAL::LK_%d\n", node->node.literal.kind); assert(false); // TODO
            }
            break;
        }

        case ANT_VAR_REF: {
            TypeStaticPath t_path = {
                .path = node->node.var_ref.path,
                .generic_types = {0},
                .impl_version = 0,
            };
            ResolvedType* resolved_type = calc_static_path_type(type_resolver, scope, &t_path);
            if (resolved_type) {
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;
            } 

            break;
        }

        case ANT_NONE: assert(false);
        case ANT_COUNT: assert(false);

        default: printf("TODO: ANT_%d\n", node->type); assert(false);
    }

    printf("%s\n", changed ? "true" : "false");
    if (changed) {
        print_astnode(*node);
        printf("\n\n");
    }

    return changed;
}

static void resolve_file(TypeResolver* type_resolver, Scope* scope, ASTNodeFileRoot file) {
    Changed changed;
    do {
        changed = false;

        LLNode_ASTNode* curr = file.nodes.head;
        while (curr) {
            changed |= resolve_type_node(type_resolver, scope, &curr->data);
            curr = curr->next;
        }
    } while (changed);

    // Ensure all types resolved
    bool requires_type;
    LLNode_ASTNode* curr = file.nodes.head;
    while (curr) {
        switch (curr->data.type) {
            case ANT_FILE_ROOT:
            case ANT_FILE_SEPARATOR:
            case ANT_PACKAGE:
                requires_type = false;
                break;

            default:
                requires_type = true;
                break;
        }

        if (requires_type) {
            if (!type_resolver->packages->types[curr->data.id.val].type) {
                print_astnode(curr->data);
            }
            assert(type_resolver->packages->types[curr->data.id.val].type);
        }

        curr = curr->next;
    }
}
