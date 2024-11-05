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
    size_t* dependent_idxs;
    size_t* dependency_idxs;
} AdjacencyList;

static AdjacencyList adjacency_list_create(Arena* arena, size_t capacity) {
    size_t* dependents = arena_calloc(arena, capacity, sizeof *dependents);
    size_t* dependencies = arena_calloc(arena, capacity, sizeof *dependencies);

    return (AdjacencyList){
        .arena = arena,
        .capacity = capacity,
        .length = 0,
        .dependent_idxs = dependents,
        .dependency_idxs = dependencies,
    };
}

static void add_package_dependency(AdjacencyList* list, size_t dependent_idx, size_t dependency_idx) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->dependent_idxs = arena_realloc(list->arena, list->dependent_idxs, sizeof(*list->dependent_idxs) * prev_cap, sizeof(*list->dependent_idxs) * list->capacity);
        list->dependency_idxs = arena_realloc(list->arena, list->dependency_idxs, sizeof(*list->dependency_idxs) * prev_cap, sizeof(*list->dependency_idxs) * list->capacity);
    }

    list->dependent_idxs[list->length] = dependent_idx;
    list->dependency_idxs[list->length] = dependency_idx;
    list->length += 1;
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
    size_t scope_id;

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
    static size_t next = 0;

    return (Scope){
        .scope_id = next++,

        .arena = arena,
        .parent = parent,

        .lookup_length = HASHTABLE_BUCKETS,
        .lookup_buckets = arena_calloc(arena, HASHTABLE_BUCKETS, sizeof(Bucket)),
    };
}

static void scope_print_ids(Scope* scope) {
    assert(scope);

    printf("[");

    Scope* curr = scope;
    while (curr) {
        printf("%lu", curr->scope_id);
        curr = curr->parent;
        if (curr) {
            printf(" <- ");
        }
    }

    printf("]\n");
}

static void scope_set(Scope* scope, String key, ResolvedType* value) {
    assert(scope);
    // printf("Set \""); print_string(key); printf("\" = RTK_%d; ", value->kind);
    // scope_print_ids(scope);

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
    assert(scope);
    // printf("Get \""); print_string(key); printf("\"; ");
    // scope_print_ids(scope);

    Bucket* bucket = scope->lookup_buckets + idx;

    if (bucket) {
        for (size_t i = 0; i < bucket->length; ++i) {
            BucketItem* item = bucket->array + i;

            if (str_eq(item->key, key)) {
                // printf("Got RTK_%d\n", item->value->kind);
                return item->value;
            }
        }
    }

    if (scope->parent) {
        return _scope_get(scope->parent, key, idx);
    } else {
        // printf("Not found\n");
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

bool is_generic_impl_eq(GenericImpl* a, GenericImpl* b) {
    if (a->length != b->length) {
        return false;
    }

    for (size_t i = 0; i < a->length; ++i) {
        ResolvedType* rta = a->resolved_types[i];
        ResolvedType* rtb = b->resolved_types[i];

        if (!resolved_type_eq(rta, rtb)) {
            return false;
        }
    }

    return true;
}

LL_GenericImpl map_generic_impls_to_concrete(Packages* packages, LL_GenericImpl* generic_impls) {
    LL_GenericImpl to_generic_impls = {
        .length = 0,
        .head = NULL,
        .tail = NULL,
    };

    LLNode_GenericImpl* curr = generic_impls->head;
    while (curr) {
        GenericImpl* generic_impl = &curr->data;

        bool has_generic = false;

        for (size_t i = 0; i < generic_impl->length; ++i) {
            if (generic_impl->resolved_types[i]->kind == RTK_GENERIC) {
                has_generic = true;
                break;
            }
        }

        if (!has_generic) {
            GenericImpl to_generic_impl = {
                .length = 0,
                .resolved_types = arena_calloc(packages->arena, generic_impl->length, sizeof(uintptr_t)),
            };
            for (size_t i = 0; i < generic_impl->length; ++i) {
                to_generic_impl.resolved_types[to_generic_impl.length++] = generic_impl->resolved_types[i];
            }
            ll_generic_impls_push(packages->arena, &to_generic_impls, to_generic_impl);
        } else {
            // ArrayList<ResolvedType*>
            ArrayList_Ptr* first = arena_alloc(packages->arena, sizeof *first);
            *first = arraylist_ptr_create(packages->arena);

            // ArrayList<ArrayList<ResolvedType*>*>
            ArrayList_Ptr to_generic_impl_many = arraylist_ptr_create(packages->arena);
            arraylist_ptr_push(&to_generic_impl_many, first);

            for (size_t rtidx = 0; rtidx < generic_impl->length; ++rtidx) {
                ResolvedType* rt = generic_impl->resolved_types[rtidx];

                if (rt->kind == RTK_GENERIC) {
                    size_t src_node_id = rt->src->id.val;
                    size_t idx = rt->type.generic.idx;

                    LL_GenericImpl* generic_impls = packages->generic_impls_nodes_raw + src_node_id;

                    LL_GenericImpl nested_all = map_generic_impls_to_concrete(packages, generic_impls);

                    // ArrayList<ResolvedType*>
                    ArrayList_Ptr nested = arraylist_ptr_create(packages->arena);

                    LLNode_GenericImpl* curr = nested_all.head;
                    while (curr) {
                        arraylist_ptr_push(&nested, curr->data.resolved_types[idx]);
                        curr = curr->next;
                    }

                    for (size_t mappedidx = 0; mappedidx < nested.length; ++mappedidx) {
                        ResolvedType* mapped = nested.data[mappedidx];
                        if (mappedidx == 0) {
                            for (size_t i = 0; i < to_generic_impl_many.length; ++i) {
                                ArrayList_Ptr* to_generic_impl = to_generic_impl_many.data[i];
                                arraylist_ptr_push(to_generic_impl, mapped);
                            }
                        } else {
                            size_t len = to_generic_impl_many.length;
                            for (size_t i = 0; i < len; ++i) {
                                ArrayList_Ptr* to_generic_impl_ref = to_generic_impl_many.data[i];

                                ArrayList_Ptr* to_generic_impl = arena_alloc(packages->arena, sizeof *to_generic_impl);
                                *to_generic_impl = arraylist_ptr_create_with_capacity(packages->arena, to_generic_impl_ref->capacity);
                                to_generic_impl->length = to_generic_impl_ref->length;
                                for (size_t j = 0; j < to_generic_impl->length; ++j) {
                                    to_generic_impl->data[j] = to_generic_impl_ref->data[j];
                                }

                                size_t last = to_generic_impl->length - 1;
                                to_generic_impl->data[last] = mapped;
                                arraylist_ptr_push(&to_generic_impl_many, to_generic_impl);
                            }
                        }
                    }
                } else {
                    for (size_t i = 0; i < to_generic_impl_many.length; ++i) {
                        ArrayList_Ptr* to_generic_impl = to_generic_impl_many.data[i];
                        arraylist_ptr_push(to_generic_impl, rt);
                    }
                }
            }
            for (size_t to_generic_implidx = 0; to_generic_implidx < to_generic_impl_many.length; ++to_generic_implidx) {
                ArrayList_Ptr* to_generic_impl = to_generic_impl_many.data[to_generic_implidx];
                GenericImpl a = {
                    .length = to_generic_impl->length,
                    .resolved_types = (ResolvedType**)to_generic_impl->data,
                };

                bool found = false;
                for (size_t i = 0; i < to_generic_implidx; ++i) {
                    ArrayList_Ptr* to_generic_impl_other = to_generic_impl_many.data[i];

                    GenericImpl b = {
                        .length = to_generic_impl_other->length,
                        .resolved_types = (ResolvedType**)to_generic_impl_other->data,
                    };

                    if (is_generic_impl_eq(&a, &b)) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    ll_generic_impls_push(packages->arena, &to_generic_impls, a);
                }
            }
        }

        curr = curr->next;
    }

    LL_GenericImpl out = {
        .length = 0,
        .head = NULL,
        .tail = NULL,
    };

    size_t to_generic_implidx = 0;
    curr = to_generic_impls.head;
    while (curr) {
        GenericImpl* to_generic_impl = &curr->data;

        bool found = false;
        size_t i = 0;
        LLNode_GenericImpl* i_curr = to_generic_impls.head;
        while (i_curr) {
            if (i >= to_generic_implidx) {
                break;
            }
            GenericImpl* to_generic_impl_other = &i_curr->data;

            if (is_generic_impl_eq(to_generic_impl, to_generic_impl_other)) {
                found = true;
                break;
            }

            i += 1;
            i_curr = i_curr->next;
        }

        if (!found && to_generic_impl->length > 0) {
            ll_generic_impls_push(packages->arena, &out, *to_generic_impl);
        }

        to_generic_implidx += 1;
        curr = curr->next;
    }

    return out;
}

void resolve_generic_nodes(Packages* packages) {
    for (size_t i = 0; i < packages->generic_impls_nodes_length; ++i) {
        LL_GenericImpl* generic_impls = packages->generic_impls_nodes_raw + i;
        if (generic_impls->length > 0) {
            LL_GenericImpl concrete_generic_impls = map_generic_impls_to_concrete(packages, generic_impls);

            packages->generic_impls_nodes_concrete[i] = concrete_generic_impls;

            printf("Node [%lu]: ", i);
            print_resolved_type(packages_type_by_node(packages, (NodeId){i})->type);
            printf("\n");

            if (concrete_generic_impls.length > 0) {
                printf("- From:\n");
            }
            {
                size_t j = 0;
                LLNode_GenericImpl* curr = generic_impls->head;
                while (curr) {
                    printf("[%lu] ", j);
                    for (size_t k = 0; k < curr->data.length; ++k) {
                        print_resolved_type(curr->data.resolved_types[k]);
                        printf(", ");
                    }
                    printf("\n");

                    j += 1;
                    curr = curr->next;
                }
            }

            if (concrete_generic_impls.length > 0) {
                printf("- To:\n");
                size_t j = 0;
                LLNode_GenericImpl* curr = concrete_generic_impls.head;
                while (curr) {
                    assert(curr->data.length);
                    printf("[%lu] ", j);
                    for (size_t k = 0; k < curr->data.length; ++k) {
                        print_resolved_type(curr->data.resolved_types[k]);
                        printf(", ");
                    }
                    printf("\n");

                    j += 1;
                    curr = curr->next;
                }
            }

            printf("\n");
        }
    }
}

static void resolve_file(TypeResolver* type_resolver, Scope* scope, ASTNodeFileRoot file);

void resolve_types(TypeResolver* type_resolver) {
    size_t packages_len = 0;
    Package* packages = arena_calloc(type_resolver->arena, type_resolver->packages->count, sizeof *packages);

    // Understand which files depend on which other files
    for (size_t i = 0; i < type_resolver->packages->lookup_length; ++i) {
        ArrayList_Package bucket = type_resolver->packages->lookup_buckets[i];

        for (size_t j = 0; j < bucket.length; ++j) {
            Package pkg = bucket.array[j];

            assert(pkg.ast);
            assert(pkg.ast->type == ANT_FILE_ROOT);

            assert(packages_len < type_resolver->packages->count);
            packages[packages_len++] = pkg;
        }
    }
    assert(packages_len == type_resolver->packages->count);

    AdjacencyList dependencies = adjacency_list_create(type_resolver->arena, 1);

    for (size_t i = 0; i < packages_len; ++i) {
        Package pkg = packages[i];
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
                    printf("FROM FILE: \"%s\"\n", pkg.full_name ? package_path_to_str(type_resolver->arena, pkg.full_name).chars : "<main>");
                    printf("Cannot find import [%s]\n", import_path_to_str(type_resolver->arena, path).chars);
                    printf("Cannot find package [%s]\n", package_path_to_str(type_resolver->arena, dependency).chars);
                    assert(false);
                }

                bool found_idx = false;
                for (size_t j = 0; j < packages_len; ++j) {
                    if (i == j) {
                        continue;
                    }

                    if (!packages[j].full_name) {
                        continue;
                    }

                    if (packages[j].ast->id.val == found->ast->id.val) {
                        found_idx = true;
                        add_package_dependency(&dependencies, i, j);
                        break;
                    }
                }
                assert(found);
            }

            decl = decl->next;
        }
    }

    {
        printf("Unsorted packages:\n");
        for (size_t i = 0; i < packages_len; ++i) {
            Package* pkg = packages + i;

            printf("[%lu] %s\n", i,
                pkg->full_name ? package_path_to_str(type_resolver->arena, pkg->full_name).chars : "<main>"
            );
        }
        printf("\n");

        printf("Dependencies:\n");
        for (size_t i = 0; i < dependencies.length; ++i) {
            assert(dependencies.dependent_idxs + i);
            assert(dependencies.dependency_idxs + i);

            size_t dependent_idx = dependencies.dependent_idxs[i];
            size_t dependency_idx = dependencies.dependency_idxs[i];

            assert(packages + dependent_idx);
            assert(packages + dependency_idx);

            Package dependent = packages[dependent_idx];
            Package dependency = packages[dependency_idx];

            assert(dependency.full_name);

            printf("- [");
            printf("%s", dependent.full_name ? package_path_to_str(type_resolver->arena, dependent.full_name).chars : "<main>");
            printf("] depends on [");
            printf("%s", package_path_to_str(type_resolver->arena, dependency.full_name).chars);
            printf("]\n");
        }
        printf("\n");
    }

    size_t to_sort_len = 0;
    size_t* to_sort = arena_calloc(type_resolver->arena, packages_len, sizeof *to_sort);
    for (size_t i = 0; i < packages_len; ++i) {
        to_sort[i] = i;
        to_sort_len += 1;
    }

    size_t resolve_order_len = 0;
    size_t* resolve_order = arena_calloc(type_resolver->arena, packages_len, sizeof *resolve_order);
    for (size_t i = 0; i < packages_len; ++i) {
        bool found = false;
        for (size_t j = 0; j < dependencies.length; ++j) {
            if (dependencies.dependent_idxs[j] == i) {
                found = true;
                break;
            }
        }

        if (!found) {
            resolve_order[resolve_order_len++] = i;
            for (size_t j = 0; j < to_sort_len; ++j) {
                if (to_sort[j] == i) {
                    to_sort_len -= 1;
                    for (size_t k = j; k < to_sort_len; ++k) {
                        to_sort[k] = to_sort[k + 1];
                    }
                    break;
                }
            }
        }
    }

    bool* has_unresolved_dependencies = arena_alloc(type_resolver->arena, packages_len * sizeof *has_unresolved_dependencies);
    while (resolve_order_len < packages_len) {
        for (size_t i = 0; i < packages_len; ++i) {
            has_unresolved_dependencies[i] = false;
        }

        for (size_t i_ = 0; i_ < to_sort_len; ++i_) {
            size_t i = to_sort[i_];

            for (size_t j = 0; j < dependencies.length; ++j) {
                if (dependencies.dependent_idxs[j] == i) {
                    for (size_t k = 0; k < to_sort_len; ++k) {
                        if (dependencies.dependency_idxs[j] == to_sort[k]) {
                            if (has_unresolved_dependencies[to_sort[k]]) {
                                Package a = packages[i];
                                Package b = packages[to_sort[k]];

                                printf("Error: Import cycle detected!\n");
                                printf("- [");
                                printf("%s", package_path_to_str(type_resolver->arena, a.full_name).chars);
                                printf("] depends on [");
                                printf("%s", package_path_to_str(type_resolver->arena, b.full_name).chars);
                                printf("]\n");
                                printf("- [");
                                printf("%s", package_path_to_str(type_resolver->arena, b.full_name).chars);
                                printf("] depends on [");
                                printf("%s", package_path_to_str(type_resolver->arena, a.full_name).chars);
                                printf("]\n\n");

                                assert(false);
                            }
                            has_unresolved_dependencies[i] = true;
                            break;
                        }
                    }

                    if (has_unresolved_dependencies[i]) {
                        break;
                    }
                }
            }

            if (!has_unresolved_dependencies[i]) {
                resolve_order[resolve_order_len++] = i;

                to_sort_len -= 1;
                for (size_t j = i_; j < to_sort_len; ++j) {
                    to_sort[j] = to_sort[j + 1];
                }
                i_ -= 1;
            }
        }
    }

    {
        printf("Resolve order:\n");
        for (size_t i = 0; i < packages_len; ++i) {
            size_t idx = resolve_order[i];

            Package* pkg = packages + idx;
            printf("- %s\n",
                pkg->full_name ? package_path_to_str(type_resolver->arena, pkg->full_name).chars : "<main>"
            );
        }
        printf("\n");
    }

    for (size_t i = 0; i < packages_len; ++i) {
        size_t idx = resolve_order[i];

        Package* pkg = packages + idx;
        assert(packages_resolve(type_resolver->packages, pkg->full_name));

        printf("Resolving package \"%s\"...\n",
            pkg->full_name ? package_path_to_str(type_resolver->arena, pkg->full_name).chars : "<main>"
        );

        type_resolver->current_package = pkg;
        Scope scope = scope_create(type_resolver->arena, NULL);
        resolve_file(type_resolver, &scope, pkg->ast->node.file_root);
        type_resolver->current_package = NULL;
    }
    printf("\n");

    assert(type_resolver->packages->generic_impls_nodes_concrete);
    resolve_generic_nodes(type_resolver->packages);
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
                if (!node) {
                    printf("Couldn't find \"%s\" in \"%s\"\n", arena_strcpy(type_resolver->arena, static_path->name).chars, package_path_to_str(type_resolver->arena, rt->type.namespace_->full_name).chars);
                }
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
            generic_rt->src = rt->src;
            generic_rt->type.generic.idx = i;
            generic_rt->type.generic.name = rt->type.struct_decl.generic_params.strings[i];
            generic_rt->from_pkg = rt->from_pkg;
            scope_set(&fields_scope, rt->type.struct_decl.generic_params.strings[i], generic_rt);
        }

        ResolvedType* rt_new = arena_alloc(type_resolver->arena, sizeof *rt_new);
        *rt_new = (ResolvedType){
            .from_pkg = rt->from_pkg,
            .src = rt->src,
            .kind = RTK_STRUCT_REF,
            .type.struct_ref = {
                .decl = rt->type.struct_decl,
                .decl_node_id = rt->src->id,
                .generic_args = {
                    .length = 0,
                    .resolved_types = arena_calloc(type_resolver->arena, t_static_path->generic_args.length, sizeof *rt_new->type.struct_ref.generic_args.resolved_types),
                },
                .impl_version = 0,
            },
        };
        rt = rt_new;

        assert(t_static_path->generic_args.length == rt->type.struct_ref.decl.generic_params.length);

        LLNode_Type* curr = t_static_path->generic_args.head;
        while (curr) {
            ResolvedType* generic_arg = calc_resolved_type(type_resolver, &fields_scope, &curr->data);
            assert(generic_arg);

            // if (generic_arg->kind == RTK_GENERIC) {
            //     assert(generic_arg->src->id.val != rt->type.struct_ref.decl_node_id.val);
            // }

            assert(rt->type.struct_ref.generic_args.length < t_static_path->generic_args.length);
            rt->type.struct_ref.generic_args.resolved_types[rt->type.struct_ref.generic_args.length++] = *generic_arg;

            curr = curr->next;
        }
        assert(rt->type.struct_ref.generic_args.length == t_static_path->generic_args.length);
    }

    if (rt->kind == RTK_STRUCT_REF) {
        {
            ResolvedType** resolved_types = arena_calloc(type_resolver->arena, t_static_path->generic_args.length, sizeof(uintptr_t));

            size_t idx = 0;
            LLNode_Type* curr = t_static_path->generic_args.head;
            while (curr) {
                TypeInfo* ti = packages_type_by_type(type_resolver->packages, curr->data.id);
                assert(ti);
                if (!ti->type) {
                    ResolvedType* arg_rt = calc_resolved_type(type_resolver, scope, &curr->data);
                    assert(arg_rt);
                    // assert(arg_rt->src);

                    ti->status = TIS_CONFIDENT;
                    ti->type = arg_rt;
                }

                resolved_types[idx] = ti->type;
                
                idx += 1;
                curr = curr->next;
            }

            packages_register_generic_impl(type_resolver->packages, rt->src, t_static_path->generic_args.length, resolved_types);
        }
        if (rt->src->type == ANT_STRUCT_DECL) {
            ArrayList_LL_Type* generic_impls = &rt->src->node.struct_decl.generic_impls;

            bool already_has_decl = false;
            for (size_t i = 0; i < generic_impls->length; ++i) {
                if (typells_eq(generic_impls->array[i], t_static_path->generic_args)) {
                    already_has_decl = true;
                    t_static_path->impl_version = i;
                    rt->type.struct_ref.impl_version = i;
                    break;
                }
            }

            if (!already_has_decl && t_static_path->generic_args.length > 0) {
                t_static_path->impl_version = generic_impls->length;
                rt->type.struct_ref.impl_version = generic_impls->length;
                arraylist_typells_push(generic_impls, t_static_path->generic_args);
            }
        }
    } else {
        assert(t_static_path->generic_args.length == 0);
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

                case TBI_INT8: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_INT8;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_INT16: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_INT16;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_INT32: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_INT32;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_INT64: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_INT64;
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

                case TBI_UINT8: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_UINT8;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_UINT16: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_UINT16;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_UINT32: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_UINT32;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_UINT64: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_UINT64;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_FLOAT: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_FLOAT;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_FLOAT32: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_FLOAT32;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_FLOAT64: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->from_pkg = type_resolver->current_package;
                    resolved_type->kind = RTK_FLOAT64;
                    resolved_type->type.uint_ = NULL;
                    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = resolved_type,
                    };
                    return resolved_type;
                }

                case TBI_COUNT: assert(false);
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
            resolved_type->type.mut_ptr.of = calc_resolved_type(type_resolver, scope, type->type.mut_ptr.of);
            *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
                .status = TIS_CONFIDENT,
                .type = resolved_type,
            };
            resolved_type->from_pkg = resolved_type->type.mut_ptr.of->from_pkg;
            resolved_type->kind = RTK_MUT_POINTER;
            return resolved_type;
        }

        default: assert(false);
    }
    assert(false);

    return NULL;
}

static Changed resolve_type_type(TypeResolver* type_resolver, Scope* scope, ASTNode* node, Type* type) {
    bool already_known = type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT;

    ResolvedType* resolved_type = calc_resolved_type(type_resolver, scope, type);
    if (!resolved_type) {
        return false;
    }
    if (!resolved_type->from_pkg) {
        resolved_type->from_pkg = type_resolver->current_package;
    }
    resolved_type->src = node;

    type_resolver->packages->types[node->id.val] = (TypeInfo){
        .status = TIS_CONFIDENT,
        .type = resolved_type,
    };

    ResolvedType* resolved_type2 = arena_alloc(type_resolver->arena, sizeof *resolved_type2);
    *resolved_type2 = *resolved_type;

    *packages_type_by_type(type_resolver->packages, type->id) = (TypeInfo){
        .status = TIS_CONFIDENT,
        .type = resolved_type2,
    };

    return !already_known;
}

static Changed resolve_type_node(TypeResolver* type_resolver, Scope* scope, ASTNode* node) {
    Changed changed = false;
    bool already_known = type_resolver->packages->types[node->id.val].status == TIS_CONFIDENT;

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
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;
                            case RTK_CHAR: rt->kind = RTK_INT16; break;
                            case RTK_BOOL: rt->kind = RTK_INT8; break;
                            case RTK_UINT: rt->kind = RTK_INT; break;
                            case RTK_UINT8: rt->kind = RTK_INT16; break;
                            case RTK_UINT16: rt->kind = RTK_INT32; break;
                            case RTK_UINT32: rt->kind = RTK_INT64; break;
                            case RTK_UINT64: rt->kind = RTK_INT64; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case UO_BOOL_NEGATE: {
                        switch (rt->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                rt->kind = RTK_BOOL; break;

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
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = RTK_UINT8; break;

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
            if (inner_ti1.type && inner_ti2.type) {
                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                *rt = *inner_ti1.type;

                switch (node->node.binary_op.op) {
                    case BO_BIT_OR:
                    case BO_BIT_AND:
                    case BO_BIT_XOR:
                    {
                        switch (inner_ti2.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            default: assert(false);
                        }
                        if (inner_ti1.type->kind == inner_ti2.type->kind) {
                            break;
                        }
                        switch (inner_ti1.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                                rt->kind = RTK_UINT8;
                                break;

                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_ADD:
                    case BO_SUBTRACT:
                    {
                        switch (inner_ti1.type->kind) {
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                            case RTK_POINTER:
                            case RTK_MUT_POINTER:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT8; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = inner_ti1.type->kind; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_MULTIPLY:
                    case BO_DIVIDE:
                    {
                        switch (inner_ti1.type->kind) {
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT8; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = inner_ti1.type->kind; break;

                            default: assert(false);
                        }
                        break;
                    }

                    case BO_MODULO: {
                        switch (inner_ti1.type->kind) {
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT8; break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
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
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            default: assert(false);
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
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
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                can_cmp_num = true;
                                break;

                            default: break;
                        }
                        switch (inner_ti2.type->kind) {
                            case RTK_BOOL:
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
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
                            case RTK_CHAR:
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            case RTK_FLOAT:
                            case RTK_FLOAT32:
                            case RTK_FLOAT64:
                                break;

                            case RTK_BOOL: rt->kind = RTK_INT8; break;

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

                if (has_init && type_resolver->packages->types[node->id.val].type) {
                    changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
                    // assert(type.resolved_type == node.resolved_type); // TODO

                    if (!type_resolver->packages->types[node->node.var_decl.initializer->id.val].type
                        && node->node.var_decl.initializer->type == ANT_STRUCT_INIT
                    ) {
                        TypeInfo* ti = packages_type_by_type(type_resolver->packages, node->node.var_decl.type_or_let.maybe_type->id);
                        if (ti && ti->type) {
                            type_resolver->packages->types[node->node.var_decl.initializer->id.val] = *ti;
                        } else {
                            type_resolver->packages->types[node->node.var_decl.initializer->id.val] = type_resolver->packages->types[node->id.val];
                        }
                    }
                }
            }

            if (type_resolver->packages->types[node->id.val].type) {
                // type_resolver->packages->types[node->id.val].type->from_pkg = type_resolver->current_package;
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
            ResolvedStructRef* maybe_struct_ref = NULL;
            ResolvedStructDecl struct_decl;
            if (node->node.get_field.is_ptr_deref) {
                assert(root.type->kind == RTK_POINTER || root.type->kind == RTK_MUT_POINTER);
                if (root.type->kind == RTK_POINTER) {
                    assert(root.type->type.ptr.of->kind == RTK_STRUCT_REF || root.type->type.ptr.of->kind == RTK_STRUCT_DECL);
                    if (root.type->type.ptr.of->kind == RTK_STRUCT_REF) {
                        maybe_struct_ref = &root.type->type.ptr.of->type.struct_ref;
                        struct_decl = root.type->type.ptr.of->type.struct_ref.decl;
                    } else {
                        struct_decl = root.type->type.ptr.of->type.struct_decl;
                    }
                } else {
                    assert(root.type->type.mut_ptr.of->kind == RTK_STRUCT_REF || root.type->type.mut_ptr.of->kind == RTK_STRUCT_DECL);
                    if (root.type->type.mut_ptr.of->kind == RTK_STRUCT_REF) {
                        maybe_struct_ref = &root.type->type.mut_ptr.of->type.struct_ref;
                        struct_decl = root.type->type.mut_ptr.of->type.struct_ref.decl;
                    } else {
                        struct_decl = root.type->type.mut_ptr.of->type.struct_decl;
                    }
                }
            } else {
                assert(root.type->kind == RTK_STRUCT_REF || root.type->kind == RTK_STRUCT_DECL);
                if (root.type->kind == RTK_STRUCT_REF) {
                    maybe_struct_ref = &root.type->type.struct_ref;
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

            // TODO: generalize this
            if (found->type->kind == RTK_POINTER && found->type->type.ptr.of->kind == RTK_GENERIC) {
                assert(maybe_struct_ref);
                assert(maybe_struct_ref->generic_args.length == 1);
                assert(maybe_struct_ref->generic_args.resolved_types->kind != RTK_GENERIC);
                println_astnode(*maybe_struct_ref->generic_args.resolved_types->src);

                found->type->type.ptr.of = maybe_struct_ref->generic_args.resolved_types;
                // assert(false);
            }

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

            ResolvedType* fn_rt = type_resolver->packages->types[node->node.function_call.function->id.val].type;

            if (fn_rt) {
                assert(fn_rt->kind == RTK_FUNCTION_DECL || fn_rt->kind == RTK_FUNCTION_REF);
                assert(fn_rt->kind == RTK_FUNCTION_DECL);

                if (fn_rt->type.function_decl.generic_params.length > 0) {
                    assert(node->node.function_call.generic_args.length <= fn_rt->type.function_decl.generic_params.length);

                    if (node->node.function_call.generic_args.length < fn_rt->type.function_decl.generic_params.length) {
                        assert(node->node.function_call.generic_args.length == 0);

                        // TODO: support implicit generic args
                        assert(false);
                    } else {
                        assert(node->node.function_call.generic_args.length == fn_rt->type.function_decl.generic_params.length);

                        ResolvedType** resolved_types = arena_calloc(type_resolver->arena, node->node.function_call.generic_args.length, sizeof(uintptr_t));

                        size_t i = 0;
                        LLNode_Type* curr = node->node.function_call.generic_args.head;
                        while (curr) {
                            ResolvedType* gen_arg_rt = calc_resolved_type(type_resolver, scope, &curr->data);
                            assert(gen_arg_rt);

                            gen_arg_rt->src = node;

                            resolved_types[i] = gen_arg_rt;

                            i += 1;
                            curr = curr->next;
                        }

                        packages_register_generic_impl(type_resolver->packages, fn_rt->src, node->node.function_call.generic_args.length, resolved_types);
                    }
                } else {
                    assert(node->node.function_call.generic_args.length == 0);
                }

                if (node->node.function_call.generic_args.length > 0) {
                    assert(fn_rt->kind == RTK_FUNCTION_DECL);
                    assert(fn_rt->type.function_decl.generic_params.length > 0);
                    assert(fn_rt->src->type == ANT_FUNCTION_DECL);

                    ResolvedType* new_fn_rt = arena_alloc(type_resolver->arena, sizeof *fn_rt);
                    *new_fn_rt = (ResolvedType){
                        .from_pkg = fn_rt->from_pkg,
                        .src = fn_rt->src,
                        .kind = RTK_FUNCTION_REF,
                        .type.function_ref = {
                            .decl = fn_rt->type.function_decl,
                            .generic_args = {
                                .length = 0,
                                .resolved_types = arena_calloc(type_resolver->arena, node->node.function_call.generic_args.length, sizeof(ResolvedType)),
                            },
                            .impl_version = 0,
                        },
                    };
                    fn_rt = new_fn_rt;

                    ArrayList_LL_Type* generic_impls = &fn_rt->src->node.function_decl.header.generic_impls;

                    bool already_has_decl = false;
                    for (size_t i = 0; i < generic_impls->length; ++i) {
                        if (typells_eq(generic_impls->array[i], node->node.function_call.generic_args)) {
                            already_has_decl = true;
                            node->node.function_call.impl_version = i;
                            fn_rt->type.function_ref.impl_version = i;
                            break;
                        }
                    }

                    if (!already_has_decl) {
                        node->node.function_call.impl_version = generic_impls->length;
                        fn_rt->type.function_ref.impl_version = generic_impls->length;
                        arraylist_typells_push(generic_impls, node->node.function_call.generic_args);
                    }
                } else if (fn_rt->kind == RTK_FUNCTION_DECL) {
                    ResolvedType* new_fn_rt = arena_alloc(type_resolver->arena, sizeof *fn_rt);
                    *new_fn_rt = (ResolvedType){
                        .from_pkg = fn_rt->from_pkg,
                        .src = fn_rt->src,
                        .kind = RTK_FUNCTION_REF,
                        .type.function_ref = {
                            .decl = fn_rt->type.function_decl,
                            .generic_args = {0},
                            .impl_version = 0,
                        },
                    };
                    fn_rt = new_fn_rt;
                }

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

                ResolvedType* type = fn_rt->type.function_ref.decl.return_type;
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
                assert(
                    type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_BOOL
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_CHAR
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT64
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT64
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_FLOAT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_FLOAT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_FLOAT64
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_POINTER
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_MUT_POINTER
                );
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

        case ANT_BREAK: {
            // TODO breakable-context (continuable + switch)
            break;
        }

        case ANT_CONTINUE: {
            // TODO continuable-context
            break;
        }

        case ANT_FOREACH: {
            assert(type_resolver->packages->range_literal_type);
            assert(type_resolver->packages->range_literal_type->kind == RTK_STRUCT_DECL);

            changed |= resolve_type_node(type_resolver, scope, node->node.foreach.iterable);

            bool resolved = true;
            if (type_resolver->packages->types[node->node.foreach.iterable->id.val].type) {
                assert(resolved_type_eq(
                    type_resolver->packages->types[node->node.foreach.iterable->id.val].type,
                    type_resolver->packages->range_literal_type
                ));
            } else {
                resolved = false;
            }

            Scope block_scope = scope_create(type_resolver->arena, scope);
            ResolvedType* i_rt = arena_alloc(type_resolver->arena, sizeof *i_rt);
            i_rt->kind = type_resolver->packages->range_literal_type->type.struct_decl.fields[0].type->kind;
            i_rt->type = type_resolver->packages->range_literal_type->type.struct_decl.fields[0].type->type;
            i_rt->src = node;
            i_rt->from_pkg = type_resolver->current_package;
            scope_set(&block_scope, node->node.foreach.var.lhs.name, i_rt);
            LLNode_ASTNode* curr = node->node.foreach.block->stmts.head;
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
                    .from_pkg = type_resolver->current_package,
                    .kind = RTK_VOID,
                };
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .type = rt,
                };
            }

            break;
        }

        case ANT_WHILE: {
            changed |= resolve_type_node(type_resolver, scope, node->node.while_.cond);

            bool resolved = true;
            if (type_resolver->packages->types[node->node.while_.cond->id.val].type) {
                assert(
                    type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_BOOL
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_CHAR
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_UINT8
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_UINT16
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_UINT32
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_UINT64
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_INT8
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_INT16
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_INT32
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_INT64
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_FLOAT
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_FLOAT32
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_FLOAT64
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_POINTER
                    || type_resolver->packages->types[node->node.while_.cond->id.val].type->kind == RTK_MUT_POINTER
                );
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
                    .from_pkg = type_resolver->current_package,
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
                    if (!resolved_type_implict_to(
                            type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type,
                            type_resolver->current_function->return_type
                    )) {
                        printf("ERROR!\n- ");
                        print_resolved_type(type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type);
                        printf("\n can't implicitly become\n- ");
                        print_resolved_type(type_resolver->current_function->return_type);
                        printf("\n");
                    }
                    assert(resolved_type_implict_to(
                            type_resolver->packages->types[node->node.return_.maybe_expr->id.val].type,
                            type_resolver->current_function->return_type
                    ));

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
                    type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_BOOL
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_CHAR
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT8
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT16
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT32
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_UINT64
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT8
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT16
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT32
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_INT64
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_FLOAT
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_FLOAT32
                    || type_resolver->packages->types[node->node.index.value->id.val].type->kind == RTK_FLOAT64
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
                    assert(
                        rt->kind == RTK_UINT
                        || rt->kind == RTK_UINT8
                        || rt->kind == RTK_UINT16
                        || rt->kind == RTK_UINT32
                        || rt->kind == RTK_UINT64
                        || rt->kind == RTK_INT
                        || rt->kind == RTK_INT8
                        || rt->kind == RTK_INT16
                        || rt->kind == RTK_INT32
                        || rt->kind == RTK_INT64
                    );
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
                generic_rt->type.generic.idx = i;
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
                        assert(node->node.struct_decl.fields.length >= 2);
                        assert(fields[0].type->kind == RTK_UINT);
                        assert(fields[1].type->kind == RTK_POINTER);
                        assert(fields[1].type->type.ptr.of->kind == RTK_CHAR);
                        type_resolver->packages->string_literal_type = resolved_type;
                    } else if (curr->data.type == DT_STRING_TEMPLATE) {
                        assert(node->node.struct_decl.fields.length >= 3);
                        assert(fields[0].type->kind == RTK_UINT);
                        assert(fields[1].type->kind == RTK_UINT);
                        assert(fields[2].type->kind == RTK_POINTER);
                        assert(fields[2].type->type.ptr.of->kind == RTK_CHAR);
                        type_resolver->packages->string_template_type = resolved_type;
                    } else if (curr->data.type == DT_RANGE_LITERAL) {
                        assert(node->node.struct_decl.fields.length >= 3);
                        assert(fields[0].type->kind == RTK_INT);
                        assert(fields[1].type->kind == RTK_INT);
                        assert(fields[2].type->kind == RTK_BOOL);
                        type_resolver->packages->range_literal_type = resolved_type;
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
                .kind = RTK_FUNCTION_DECL,
                .type.function_decl = {
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
            // if (type_resolver->packages->types[node->id.val].status != TIS_CONFIDENT) {
                Strings generic_params = {
                    .length = node->node.function_decl.header.generic_params.length,
                    .strings = node->node.function_decl.header.generic_params.array,
                };

                Scope signature_scope = scope_create(type_resolver->arena, scope);
                for (size_t i = 0; i < generic_params.length; ++i) {
                    ResolvedType* generic_rt = arena_alloc(type_resolver->arena, sizeof *generic_rt);
                    generic_rt->from_pkg = type_resolver->current_package;
                    generic_rt->src = node;
                    generic_rt->kind = RTK_GENERIC;
                    generic_rt->type.generic.idx = i;
                    generic_rt->type.generic.name = generic_params.strings[i];

                    scope_set(&signature_scope, generic_params.strings[i], generic_rt);
                }

                ResolvedFunctionParam* params = arena_calloc(type_resolver->arena, node->node.function_decl.header.params.length, sizeof *params);
                LLNode_FnParam* param = node->node.function_decl.header.params.head;
                size_t i = 0;
                while (param) {
                    ResolvedType* param_type = calc_resolved_type(type_resolver, &signature_scope, &param->data.type);
                    if (!param_type) {
                        break;
                    }
                    *packages_type_by_type(type_resolver->packages, param->data.type.id) = (TypeInfo){
                        .status = TIS_CONFIDENT,
                        .type = param_type,
                    };
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

                ResolvedType* return_type = calc_resolved_type(type_resolver, &signature_scope, &node->node.function_decl.header.return_type);
                if (!return_type) {
                    break;
                }
                *packages_type_by_type(type_resolver->packages, node->node.function_decl.header.return_type.id) = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = return_type,
                };

                ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                *resolved_type = (ResolvedType){
                    .from_pkg = type_resolver->current_package,
                    .src = node,
                    .kind = RTK_FUNCTION_DECL,
                    .type.function_decl = {
                        .generic_params = generic_params,
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
            // }

            ResolvedType* fn_type = type_resolver->packages->types[node->id.val].type;
            assert(fn_type);
            assert(fn_type->kind == RTK_FUNCTION_DECL);

            type_resolver->current_function = &fn_type->type.function_decl;

            Scope block_scope = scope_create(type_resolver->arena, &signature_scope);

            // Add fn params to block scope
            for (size_t i = 0; i < fn_type->type.function_decl.params_length; ++i) {
                ResolvedFunctionParam param = fn_type->type.function_decl.params[i];
                scope_set(&block_scope, param.name, param.type);
            }

            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr) {
                // if (type_resolver->packages->types[curr->data.id.val].status != TIS_CONFIDENT) {
                    changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                // }
                curr = curr->next;
            }

            type_resolver->current_function = NULL;

            break;
        }

        case ANT_LITERAL: {
            switch (node->node.literal.kind) {
                case LK_STR: {
                    bool is_c_str = false;

                    if (node->directives.length > 0) {
                        LLNode_Directive* curr = node->directives.head;
                        while (curr) {
                            if (curr->data.type == DT_C_STR) {
                                ResolvedType* rt_char = arena_alloc(type_resolver->arena, sizeof *rt_char);
                                rt_char->src = node;
                                rt_char->from_pkg = type_resolver->current_package;
                                rt_char->kind = RTK_CHAR;
                                rt_char->type.char_ = NULL;

                                ResolvedType* rt = arena_alloc(type_resolver->arena, sizeof *rt);
                                rt->src = node;
                                rt->from_pkg = type_resolver->current_package;
                                rt->kind = RTK_POINTER;
                                rt->type.ptr.of = rt_char;
                                type_resolver->packages->types[node->id.val] = (TypeInfo){
                                    .status = TIS_CONFIDENT,
                                    .type = rt,
                                };

                                is_c_str = true;
                                break;
                            }
                            curr = curr->next;
                        }
                    }

                    if (!is_c_str) {
                        assert(type_resolver->packages->string_literal_type);

                        type_resolver->packages->types[node->id.val] = (TypeInfo){
                            .status = TIS_CONFIDENT,
                            .type = type_resolver->packages->string_literal_type,
                        };
                    }
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

                case LK_NULL: {
                    ResolvedType* type = arena_alloc(type_resolver->arena, sizeof *type);
                    type->src = node;
                    type->from_pkg = type_resolver->current_package;
                    type->kind = RTK_POINTER;
                    type->type.ptr.of = NULL;
                    break;
                }

                default: printf("TODO: ANT_LITERAL::LK_%d\n", node->node.literal.kind); assert(false); // TODO
            }
            break;
        }

        case ANT_VAR_REF: {
            TypeStaticPath t_path = {
                .path = node->node.var_ref.path,
                .generic_args = {0},
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

        case ANT_RANGE: {
            changed |= resolve_type_node(type_resolver, scope, node->node.range.lhs);
            changed |= resolve_type_node(type_resolver, scope, node->node.range.rhs);

            ResolvedType* lhs = type_resolver->packages->types[node->node.range.lhs->id.val].type;
            ResolvedType* rhs = type_resolver->packages->types[node->node.range.rhs->id.val].type;

            if (lhs) {
                assert(
                    type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_CHAR
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT64
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT64
                );
            }

            if (rhs) {
                 assert(
                    type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_CHAR
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_UINT64
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT8
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT16
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT32
                    || type_resolver->packages->types[node->node.if_.cond->id.val].type->kind == RTK_INT64
                );
            }

            if (lhs && rhs) {
                type_resolver->packages->types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type_resolver->packages->range_literal_type,
                };
            }

            break;
        }

        case ANT_DEFER: {
            changed |= resolve_type_node(type_resolver, scope, node->node.defer.stmt);

            type_resolver->packages->types[node->id.val] = type_resolver->packages->types[node->node.defer.stmt->id.val];
            break;
        }

        case ANT_NONE: assert(false);
        case ANT_COUNT: assert(false);

        default: printf("TODO: ANT_%d\n", node->type); assert(false);
    }

    return changed && !already_known;
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
                println_astnode(curr->data);
            }
            assert(type_resolver->packages->types[curr->data.id.val].type);
        }

        curr = curr->next;
    }
}
