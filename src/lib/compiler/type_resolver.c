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
            || (states[i] == VS_UNVISITED && _topo_sort_dfs(list, states, i, sorted, sort_index))
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

TypeResolver type_resolver_create(Arena* arena, Packages packages) {
    return (TypeResolver){
        .arena = arena,
        .packages = packages,
    };
}

static ImportPath* expand_import_path(TypeResolver* type_resolver, ASTNodeImport* import) {
    if (!import) {
        return NULL;
    }

    switch (import->type) {
        case IT_DEFAULT: return import->import_path;

        case IT_LOCAL: {
            ImportPath* local = package_path_to_import_path(type_resolver->arena, type_resolver->current_package);
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
            root->import.dir.name = type_resolver->current_package->name;
            root->import.dir.child = import->import_path;
            return root;
        }
    }
}

static void resolve_file(TypeResolver* type_resolver, Scope* scope, ASTNodeFileRoot file);

void resolve_types(TypeResolver* type_resolver) {
    AdjacencyList dependencies = adjacency_list_create(type_resolver->arena, 1);
    ArrayList_PackagePath to_resolve = arraylist_packagepath_create(type_resolver->arena, type_resolver->packages.count);

    // Understand which files depend on which other files
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
            bool has_imports = false;
            while (decl) {
                if (decl->data.type == ANT_IMPORT) {
                    has_imports = true;

                    type_resolver->current_package = &dependent;
                    ImportPath* path = expand_import_path(type_resolver, &decl->data.node.import);
                    type_resolver->current_package = NULL;

                    PackagePath* dependency = import_path_to_package_path(type_resolver->arena, path);
                    assert(dependency);

                    Package* found = packages_resolve(&type_resolver->packages, dependency);
                    if (!found) {
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

        Package* pkg = packages_resolve(&type_resolver->packages, path);
        assert(pkg);
        assert(pkg->ast->type == ANT_FILE_ROOT);

        type_resolver->current_package = path;
        Scope scope = scope_create(type_resolver->arena, NULL);
        resolve_file(type_resolver, &scope, pkg->ast->node.file_root);
        type_resolver->current_package = NULL;
    }
}

static ResolvedType* calc_resolved_type(TypeResolver* type_resolver, Scope* scope, Type* type) {
    assert(type_resolver);
    assert(scope);
    assert(type);

    switch (type->kind) {
        case TK_BUILT_IN: {
            switch (type->type.built_in) {
                case TBI_VOID: {
                    ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                    resolved_type->kind = RTK_VOID;
                    resolved_type->type.void_ = NULL;
                    return resolved_type;
                }

                default: assert(false);
            }
        }

        case TK_MUT_POINTER: {
            ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
            resolved_type->kind = RTK_MUT_POINTER;
            resolved_type->type.mut_ptr.of = calc_resolved_type(type_resolver, scope, type->type.mut_ptr.of);
            return resolved_type;
        }

        default: assert(false);
    }

    return NULL;
}

static ResolvedType* calc_static_path_type(TypeResolver* type_resolver, Scope* scope, StaticPath* static_path) {
    ResolvedType* rt = scope_get(scope, static_path->name);
    if (!rt) {
        return NULL;
    }

    while (static_path->child) {
        static_path = static_path->child;
        switch (rt->kind) {
            case RTK_NAMESPACE: {
                assert(rt->type.namespace_->type == ANT_FILE_ROOT);

                ASTNode* node = find_decl_by_name(rt->type.namespace_->node.file_root, static_path->name);
                assert(node);

                rt = type_resolver->packages.types[node->id.val].type;
                assert(rt);

                break;
            }

            default: assert(false);
        }
    }

    return rt;
}

static Changed resolve_type_type(TypeResolver* type_resolver, Scope* scope, size_t node_id, Type* type) {
    if (type_resolver->packages.types[node_id].status == TIS_CONFIDENT) {
        return false;
    }

    ResolvedType* resolved_type = calc_resolved_type(type_resolver, scope, type);
    if (!resolved_type) {
        return false;
    }

    type_resolver->packages.types[node_id] = (TypeInfo){
        .status = TIS_CONFIDENT,
        .type = resolved_type,
    };

    return true;
}

static Changed resolve_type_node(TypeResolver* type_resolver, Scope* scope, ASTNode const* node) {
    Changed changed = false;
    switch (node->type) {
        case ANT_FILE_ROOT: assert(false);
        case ANT_PACKAGE: break;

        case ANT_IMPORT: {
            if (type_resolver->packages.types[node->id.val].status == TIS_CONFIDENT) {
                break;
            }
            
            ImportPath* import_path = expand_import_path(type_resolver, (ASTNodeImport*)&node->node.import);
            PackagePath* to_import_path = import_path_to_package_path(type_resolver->arena, import_path);
            Package* package = packages_resolve(&type_resolver->packages, to_import_path);
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
                // TODO: support importing decls (ie. `import std::String`)
                assert(false);
            } else {
                // Importing a package namespace, no specific decl
                ResolvedType* resolved_type = arena_alloc(type_resolver->arena, sizeof *resolved_type);
                *resolved_type = (ResolvedType){
                    .src = node,
                    .kind = RTK_NAMESPACE,
                    .type.namespace_ = package->ast,
                };

                type_resolver->packages.types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                scope_set(scope, ip_curr->import.file.name, resolved_type);
            }

            break;
        };

        case ANT_FILE_SEPARATOR: {
            assert(false);
            // type_resolver->seen_separator = true;
            break;
        }

        case ANT_UNARY_OP: {
            assert(false);
            changed |= resolve_type_node(type_resolver, scope, node->node.unary_op.right);
            TypeInfo outer_ti = type_resolver->packages.types[node->id.val];
            TypeInfo inner_ti = type_resolver->packages.types[node->node.unary_op.right->id.val];
            if (inner_ti.status > outer_ti.status) {
                type_resolver->packages.types[node->id.val] = inner_ti;
            }
            break;
        }

        case ANT_BINARY_OP: {
            assert(false); // TODO
            changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.lhs);
            changed |= resolve_type_node(type_resolver, scope, node->node.binary_op.rhs);
            break;
        }

        case ANT_VAR_DECL: {
            if (type_resolver->packages.types[node->id.val].status == TIS_CONFIDENT) {
                break;
            }

            bool is_let = node->node.var_decl.type_or_let.is_let;
            bool has_init = !!node->node.var_decl.initializer;

            if (is_let) {
                if (has_init) {
                    changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
                    type_resolver->packages.types[node->id.val] = type_resolver->packages.types[node->node.var_decl.initializer->id.val];
                } else {
                    type_resolver->packages.types[node->id.val] = (TypeInfo){
                        .status = TIS_UNKNOWN,
                        .type = NULL,
                    };
                }
            } else {
                changed |= resolve_type_type(type_resolver, scope, node->id.val, node->node.var_decl.type_or_let.maybe_type);

                if (has_init) {
                    changed |= resolve_type_node(type_resolver, scope, node->node.var_decl.initializer);
                    // assert(type.resolved_type == node.resolved_type); // TODO

                    type_resolver->packages.types[node->id.val] = type_resolver->packages.types[node->node.var_decl.initializer->id.val];
                }
            }

            if (type_resolver->packages.types[node->id.val].type) {
                scope_set(scope, node->node.var_decl.lhs.lhs.name, type_resolver->packages.types[node->id.val].type);
            }

            break;
        }

        case ANT_GET_FIELD: {
            assert(false); // TODO
            changed |= resolve_type_node(type_resolver, scope, node->node.get_field.root);

            // TODO: get struct field type
            type_resolver->packages.types[node->id.val] = (TypeInfo){
                .status = TIS_UNKNOWN,
                .type = NULL,
            };

            break;
        }

        case ANT_ASSIGNMENT: {
            assert(false); // TODO
            changed |= resolve_type_node(type_resolver, scope, node->node.assignment.lhs);
            changed |= resolve_type_node(type_resolver, scope, node->node.assignment.rhs);
            break;
        }

        case ANT_FUNCTION_CALL: {
            if (type_resolver->packages.types[node->id.val].status == TIS_CONFIDENT) {
                break;
            }

            changed |= resolve_type_node(type_resolver, scope, node->node.function_call.function);

            if (
                type_resolver->packages.types[node->id.val].status != TIS_CONFIDENT
                && type_resolver->packages.types[node->node.function_call.function->id.val].status == TIS_CONFIDENT
            ) {
                ResolvedType* type = type_resolver->packages.types[node->node.function_call.function->id.val].type->type.function.return_type;
                type_resolver->packages.types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type,
                };
                changed = true;
            }

            if (type_resolver->packages.types[node->id.val].status == TIS_CONFIDENT) {
                assert(type_resolver->packages.types[node->node.function_call.function->id.val].type->kind == RTK_FUNCTION);

                bool confident_args = true;
                LLNode_ASTNode* curr = node->node.function_call.args.head;
                while (curr) {
                    changed |= resolve_type_node(type_resolver, scope, &curr->data);

                    if (type_resolver->packages.types[curr->data.id.val].status != TIS_CONFIDENT) {
                        confident_args = false;
                    }
                    
                    curr = curr->next;
                }
                if (!confident_args) {
                    break;
                }

                ResolvedType* type = type_resolver->packages.types[node->node.function_call.function->id.val].type->type.function.return_type;
                type_resolver->packages.types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = type,
                };
                changed = true;
            }

            break;
        }

        case ANT_STATEMENT_BLOCK: {
            assert(false); // TODO
            Scope block_scope = scope_create(type_resolver->arena, scope);
            LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_IF: {
            assert(false); // TODO
            changed |= resolve_type_node(type_resolver, scope, node->node.if_.cond);

            Scope block_scope = scope_create(type_resolver->arena, scope);
            LLNode_ASTNode* curr = node->node.if_.block->stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_ELSE: {
            assert(false); // TODO
            changed |= resolve_type_node(type_resolver, scope, node->node.else_.target);
            changed |= resolve_type_node(type_resolver, scope, node->node.else_.then);
            break;
        }

        case ANT_TRY: assert(false); // TODO
        case ANT_CATCH: assert(false); // TODO
        case ANT_BREAK: assert(false); // TODO
        case ANT_WHILE: assert(false); // TODO
        case ANT_DO_WHILE: assert(false); // TODO
        case ANT_FOR: assert(false); // TODO

        case ANT_RETURN: {
            assert(false); // TODO
            if (node->node.return_.maybe_expr) {
                changed |= resolve_type_node(type_resolver, scope, node->node.return_.maybe_expr);
            }
            break;
        }

        case ANT_STRUCT_INIT: assert(false); // TODO
        case ANT_ARRAY_INIT: assert(false); // TODO

        case ANT_TEMPLATE_STRING: assert(false); // TODO
        case ANT_CRASH: assert(false); // TODO

        case ANT_SIZEOF: {
            assert(false); // TODO
            // TODO ??
            break;
        }

        case ANT_SWITCH: assert(false); // TODO
        case ANT_CAST: assert(false); // TODO
        case ANT_STRUCT_DECL: assert(false); // TODO
        case ANT_UNION_DECL: assert(false); // TODO
        case ANT_ENUM_DECL: assert(false); // TODO

        case ANT_TYPEDEF_DECL: {
            assert(false);
            changed |= resolve_type_type(type_resolver, scope, node->id.val, node->node.typedef_decl.type);

            if (type_resolver->packages.types[node->id.val].type) {
                scope_set(scope, node->node.typedef_decl.name, type_resolver->packages.types[node->id.val].type);
            }
            break;
        }

        case ANT_GLOBALTAG_DECL: assert(false); // TODO

        case ANT_FUNCTION_HEADER_DECL: {
            assert(false); // TODO
            // changed |= resolve_type_type(type_resolver, scope, node->node.function_header_decl.return_type.id, (Type*)&node->node.function_header_decl.return_type);

            // bool is_resolved = type_resolver->packages.types[node->id].status == TIS_CONFIDENT;

            // if (is_resolved) {
            //     type_resolver->packages.types[node->id]
            // }
            break;
        }

        case ANT_FUNCTION_DECL: {
            if (type_resolver->packages.types[node->id.val].status != TIS_CONFIDENT) {
                ResolvedType* param_types = arena_calloc(type_resolver->arena, node->node.function_decl.header.params.length, sizeof *param_types);
                LLNode_FnParam* param = node->node.function_decl.header.params.head;
                size_t i = 0;
                while (param) {
                    ResolvedType* param_type = calc_resolved_type(type_resolver, scope, &param->data.type);
                    if (!param_type) {
                        break;
                    }
                    param_types[i] = *param_type;

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
                    .src = node,
                    .kind = RTK_FUNCTION,
                    .type.function = {
                        .param_types_length = node->node.function_decl.header.params.length,
                        .param_types = param_types,
                        .return_type = return_type,
                    },
                };

                type_resolver->packages.types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;
            }

            Scope block_scope = scope_create(type_resolver->arena, scope);
            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr) {
                if (type_resolver->packages.types[curr->data.id.val].status != TIS_CONFIDENT) {
                    changed |= resolve_type_node(type_resolver, &block_scope, &curr->data);
                }
                curr = curr->next;
            }

            break;
        }

        case ANT_LITERAL: {
            assert(false); // TODO
            break;
        }

        case ANT_VAR_REF: {
            if (type_resolver->packages.types[node->id.val].status == TIS_CONFIDENT) {
                break;
            }
            
            ResolvedType* resolved_type = calc_static_path_type(type_resolver, scope, node->node.var_ref.path);
            if (resolved_type) {
                type_resolver->packages.types[node->id.val] = (TypeInfo){
                    .status = TIS_CONFIDENT,
                    .type = resolved_type,
                };
                changed = true;
            } 

            break;
        }

        case ANT_NONE: assert(false);
        case ANT_COUNT: assert(false);

        default: assert(false);
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
}
