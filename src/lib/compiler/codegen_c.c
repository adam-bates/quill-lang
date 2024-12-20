#include <stdio.h>
#include <string.h>

#include "./ast.h"
#include "./codegen_c.h"
#include "./package.h"
#include "./resolved_type.h"

static void print_generic_map(Arena* arena, GenericImplMap* map) {
    printf("-> %p", map);
    if (!map) {
        printf("\n");
        return;
    }
    printf(" {\n");

    for (size_t i = 0; i < map->length; ++i) {
        printf("    %s: %s (RTK_%d),\n",
            arena_strcpy(arena, map->generic_names[i]).chars,
            arena_strcpy(arena, map->mapped_types[i]).chars,
            map->mapped_rtypes[i].kind
        );
    }

    print_generic_map(arena, map->parent);

    printf("}\n");
}

typedef enum {
    FT_MAIN,
    FT_C,
    FT_HEADER,
} FileType;

typedef enum {
    TS_MACROS,
    TS_TYPES,
    TS_VARS,
    TS_FN_HEADERS,
    TS_FNS,

    TS_COUNT
} TransformStage;

static DirectiveCHeader* get_c_header(Package* package) {
    if (!package || !package->ast) {
        return NULL;
    }
    assert(package->ast->type == ANT_FILE_ROOT);

    LLNode_ASTNode* n_curr = package->ast->node.file_root.nodes.head;
    while (n_curr) {
        if (n_curr->data.type == ANT_PACKAGE) {
            LLNode_Directive* d_curr = n_curr->data.directives.head;
            while (d_curr) {
                if (d_curr->data.type == DT_C_HEADER) {
                    return &d_curr->data.dir.c_header;
                }
                d_curr = d_curr->next;
            }
        }
        n_curr = n_curr->next;
    }

    return NULL;
}

static size_t unique_id(void) {
    static size_t next_id = 0;
    return next_id++;
}

String unique_var_name(Arena* arena) {
    StringBuffer sb = strbuf_create(arena);
    strbuf_append_chars(&sb, "_ql_");
    strbuf_append_uint(&sb, unique_id());
    return strbuf_to_str(sb);
}

String user_var_name(Arena* arena, String name, Package* pkg) {
    if (!pkg || get_c_header(pkg)) {
        return name;
    }
    
    StringBuffer sb = strbuf_create(arena);
    PackagePath* path = pkg->full_name;
    if (!path) {
        strbuf_append_chars(&sb, "main_");
    } else {
        while (path) {
            strbuf_append_str(&sb, path->name);
            strbuf_append_char(&sb, '_');
            path = path->child;
        }
    }
    strbuf_append_str(&sb, name);
    return strbuf_to_str(sb);
}

static String* get_mapped_generic(GenericImplMap* map, String generic) {
    if (!map) {
        return NULL;
    }

    for (size_t i = 0; i < map->length; ++i) {
        if (str_eq(map->generic_names[i], generic)) {
            return map->mapped_types + i;
        }
    }

    if (map->parent == map) {
        return NULL;
    }

    return get_mapped_generic(map->parent, generic);
}

static int64_t get_mapped_generic_idx(GenericImplMap* map, String generic) {
    if (!map) {
        return -1;
    }

    for (size_t i = 0; i < map->length; ++i) {
        if (str_eq(map->generic_names[i], generic)) {
            return i;
        }
    }

    if (map->parent == map) {
        return -1;
    }

    return get_mapped_generic_idx(map->parent, generic);
}

static void ll_node_push(Arena* const arena, LL_IR_C_Node* const ll, IR_C_Node const node) {
    assert(arena);
    assert(ll);

    LLNode_IR_C_Node* const llnode = arena_alloc(arena, sizeof *llnode);
    assert(llnode);

    llnode->data = node;
    llnode->next = NULL;
    
    if (ll->head == NULL) {
        ll->head = llnode;
        ll->tail = ll->head;
    } else {
        assert(ll->tail);

        ll->tail->next = llnode;
        ll->tail = llnode;
    }

    ll->length += 1;
}

static LL_IR_C_Node* ll_clone(Arena* arena, LL_IR_C_Node* const ll) {
    if (!ll) {
        return NULL;
    }

    LL_IR_C_Node* clone = arena_alloc(arena, sizeof *clone);
    *clone = (LL_IR_C_Node){
        .head = ll->head,
        .tail = ll->tail,
        .length = ll->length,
        .to_defer = ll_clone(arena, ll->to_defer),
    };

    return clone;
}

static void _append_file_path(StringBuffer* sb, PackagePath* package_path) {
    PackagePath* curr = package_path;
    while (curr) {
        strbuf_append_str(sb, curr->name);
        if (curr->child) {
            strbuf_append_char(sb, '_');
        }
        curr = curr->child;
    }
}


static String gen_c_file_path(Arena* arena, PackagePath* package_path) {
    if (!package_path) {
        return c_str("main.c");
    }

    StringBuffer sb = strbuf_create(arena);
    _append_file_path(&sb, package_path);
    strbuf_append_chars(&sb, ".c");
    return strbuf_to_str(sb);
}

static String gen_header_file_path(Arena* arena, PackagePath* package_path) {
    if (!package_path) {
        return c_str("main.h");
    }

    StringBuffer sb = strbuf_create(arena);
    _append_file_path(&sb, package_path);
    strbuf_append_chars(&sb, ".h");
    return strbuf_to_str(sb);
}

static void _append_type(CodegenC* codegen, StringBuffer* sb, Type type, Package* from_pkg) {
    switch (type.kind) {
        case TK_BUILT_IN: {
            switch (type.type.built_in) {
                case TBI_VOID: {
                    strbuf_append_chars(sb, "void");
                    break;
                }

                case TBI_BOOL: {
                    strbuf_append_chars(sb, "bool");
                    break;
                }

                case TBI_CHAR: {
                    strbuf_append_chars(sb, "char");
                    break;
                }

                case TBI_INT: {
                    strbuf_append_chars(sb, "int64_t");
                    break;
                }

                case TBI_INT8: {
                    strbuf_append_chars(sb, "int8_t");
                    break;
                }

                case TBI_INT16: {
                    strbuf_append_chars(sb, "int16_t");
                    break;
                }

                case TBI_INT32: {
                    strbuf_append_chars(sb, "int32_t");
                    break;
                }

                case TBI_INT64: {
                    strbuf_append_chars(sb, "int64_t");
                    break;
                }

                case TBI_UINT: {
                    strbuf_append_chars(sb, "uint64_t");
                    break;
                }

                case TBI_UINT8: {
                    strbuf_append_chars(sb, "uint8_t");
                    break;
                }

                case TBI_UINT16: {
                    strbuf_append_chars(sb, "uint16_t");
                    break;
                }

                case TBI_UINT32: {
                    strbuf_append_chars(sb, "uint32_t");
                    break;
                }

                case TBI_UINT64: {
                    strbuf_append_chars(sb, "uint64_t");
                    break;
                }

                default: printf("TODO: built-in [%d]\n", type.type.built_in); assert(false);
            }
            break;
        }

        case TK_STATIC_PATH: {
            if (
                type.type.static_path.generic_args.length == 0
                && type.type.static_path.impl_version == 0
                && !type.type.static_path.path->child
            ) {
                String* mapped = get_mapped_generic(codegen->generic_map, type.type.static_path.path->name);
                if (mapped) {
                    printf("TK_%d\n", type.kind);
                    printf("%s = %s\n\n",
                        arena_strcpy(codegen->arena, type.type.static_path.path->name).chars,
                        arena_strcpy(codegen->arena, *mapped).chars
                    );
                    // assert(false);
                    strbuf_append_str(sb, *mapped);
                    break;
                } else {
                    printf("searched map, none found\n");
                }
            }
            printf("%lu\n", type.type.static_path.generic_args.length);
            printf("%lu\n", type.type.static_path.impl_version);
            printf("%s\n", type.type.static_path.path->child ? "has child" : "no child");
            printf("%s\n\n", arena_strcpy(codegen->arena, type.type.static_path.path->name).chars);

            StaticPath* curr = type.type.static_path.path;
            while (curr->child) {
                curr = curr->child;
            }

            String name = user_var_name(codegen->arena, curr->name, from_pkg);
            strbuf_append_str(sb, name);

            if (type.type.static_path.generic_args.length > 0) {
                strbuf_append_chars(sb, "_");
                strbuf_append_uint(sb, type.type.static_path.impl_version);
            }

            break;
        }

        case TK_TUPLE: {
            assert(false);
            break;
        }

        case TK_POINTER: {
            _append_type(codegen, sb, *type.type.mut_ptr.of, from_pkg);
            strbuf_append_chars(sb, "*");
            break;
        }

        case TK_MUT_POINTER: {
            _append_type(codegen, sb, *type.type.mut_ptr.of, from_pkg);
            strbuf_append_char(sb, '*');
            break;
        }

        case TK_ARRAY: {
            _append_type(codegen, sb, *type.type.array.of, from_pkg);
            // strbuf_append_chars(sb, "[]");
            break;
        }

        case TK_SLICE: {
            assert(false);
            break;
        }

        default: printf("TODO: type [%d]\n", type.kind); assert(false);
    }
}

static void _append_type_resolved(CodegenC* codegen, StringBuffer* sb, ResolvedType* type) {
    assert(codegen);
    assert(sb);
    assert(type);
    switch (type->kind) {
        case RTK_NAMESPACE: assert(false);

        case RTK_VOID: strbuf_append_chars(sb, "void"); break;
        case RTK_BOOL: strbuf_append_chars(sb, "bool"); break;
        case RTK_CHAR: strbuf_append_chars(sb, "char"); break;

        case RTK_INT: strbuf_append_chars(sb, "int64_t"); break;
        case RTK_INT8: strbuf_append_chars(sb, "int8_t"); break;
        case RTK_INT16: strbuf_append_chars(sb, "int16_t"); break;
        case RTK_INT32: strbuf_append_chars(sb, "int32_t"); break;
        case RTK_INT64: strbuf_append_chars(sb, "int64_t"); break;

        case RTK_UINT: strbuf_append_chars(sb, "uint64_t"); break;
        case RTK_UINT8: strbuf_append_chars(sb, "uint8_t"); break;
        case RTK_UINT16: strbuf_append_chars(sb, "uint16_t"); break;
        case RTK_UINT32: strbuf_append_chars(sb, "uint32_t"); break;
        case RTK_UINT64: strbuf_append_chars(sb, "uint64_t"); break;

        case RTK_FLOAT: strbuf_append_chars(sb, "double"); break;
        case RTK_FLOAT32: strbuf_append_chars(sb, "float"); break;
        case RTK_FLOAT64: strbuf_append_chars(sb, "double"); break;

        case RTK_ARRAY: {
            _append_type_resolved(codegen, sb, type->type.array.of);
            break;
        }

        case RTK_POINTER: {
            _append_type_resolved(codegen, sb, type->type.ptr.of);
            strbuf_append_chars(sb, "*");
            break;
        }

        case RTK_MUT_POINTER: {
            _append_type_resolved(codegen, sb, type->type.ptr.of);
            strbuf_append_char(sb, '*');
            break;
        }

        case RTK_FUNCTION_DECL: {
            assert(false); // TODO ?
            break;
        }

        case RTK_FUNCTION_REF: {
            assert(false); // TODO ?
            break;
        }

        case RTK_STRUCT_DECL: {
            strbuf_append_chars(sb, "struct ");

            String name = user_var_name(codegen->arena, type->type.struct_decl.name, type->from_pkg);
            strbuf_append_str(sb, name);
            assert(type->type.struct_decl.generic_params.length == 0); // otherwise should be RTK_STRUCT_REF
            break;
        }

        case RTK_STRUCT_REF: {
            strbuf_append_chars(sb, "struct ");

            String name = user_var_name(codegen->arena, type->type.struct_ref.decl.name, type->from_pkg);
            strbuf_append_str(sb, name);
            if (type->type.struct_ref.generic_args.length > 0) {
                LL_GenericImpl generic_impls = codegen->packages->generic_impls_nodes_concrete[type->type.struct_ref.decl_node_id.val];
                assert(generic_impls.length > 0);

                size_t version = 0;
                bool found = true;
                LLNode_GenericImpl* curr = generic_impls.head;
                printf("Finding version for struct ref %s<...>\n", arena_strcpy(codegen->arena, type->type.struct_ref.decl.name).chars);
                while (curr) {
                    printf("Checking version %lu...\n", version);

                    if (curr->data.length == type->type.struct_ref.generic_args.length) {
                        printf("len %lu == len %lu\n", curr->data.length, type->type.struct_ref.generic_args.length);

                        found = true;
                        for (size_t i = 0; i < curr->data.length; ++i) {
                            ResolvedType* rt_arg = type->type.struct_ref.generic_args.resolved_types + i;
                            assert(rt_arg);
                            assert(curr->data.resolved_types + i);
                            assert(curr->data.resolved_types[i]);

                            if (rt_arg->kind == RTK_GENERIC) {
                                StringBuffer rt_arg_sb = strbuf_create(codegen->arena);
                                _append_type_resolved(codegen, &rt_arg_sb, rt_arg);

                                StringBuffer curr_sb = strbuf_create(codegen->arena);
                                _append_type_resolved(codegen, &curr_sb, curr->data.resolved_types[i]);
                                if (!str_eq(strbuf_to_str(rt_arg_sb), strbuf_to_str(curr_sb))) {
                                    printf("\"%s\" != \"%s\"\n",
                                        arena_strcpy(codegen->arena, strbuf_to_str(rt_arg_sb)).chars,
                                        arena_strcpy(codegen->arena, strbuf_to_str(curr_sb)).chars
                                    );
                                    found = false;
                                    break;
                                }
                            } else if (!resolved_type_eq(curr->data.resolved_types[i], type->type.struct_ref.generic_args.resolved_types + i)) {
                                printf("RTK_%d != RTK_%d\n", curr->data.resolved_types[i]->kind, (type->type.struct_ref.generic_args.resolved_types + i)->kind);
                                found = false;
                                break;
                            }
                        }

                        if (found) {
                            break;
                        }
                    }

                    version += 1;
                    curr = curr->next;
                }
                assert(found);
                printf("Found version %lu!\n", version);

                strbuf_append_chars(sb, "_");
                strbuf_append_uint(sb, version);
            }
            break;
        }

        case RTK_GENERIC: {
            int64_t mapped_idx = get_mapped_generic_idx(codegen->generic_map, type->type.generic.name);
            if (mapped_idx < 0) {
                println_astnode(*type->src);
                printf("Couldn't find %s\n", arena_strcpy(codegen->arena, type->type.generic.name).chars);
                print_generic_map(codegen->arena, codegen->generic_map);
            }
            assert(mapped_idx >= 0);

            while (codegen->generic_map->mapped_rtypes[mapped_idx].kind == RTK_GENERIC) {
                int64_t next_idx = get_mapped_generic_idx(codegen->generic_map, codegen->generic_map->mapped_types[mapped_idx]);
                while (next_idx >= 0) {
                    mapped_idx = next_idx;
                    next_idx = get_mapped_generic_idx(codegen->generic_map, codegen->generic_map->mapped_types[mapped_idx]);
                }
            }

            String mapped = codegen->generic_map->mapped_types[mapped_idx];

            printf("RTK_%d\n", type->kind);
            printf("<%lu:%s> = %s\n\n",
                type->type.generic.idx,
                arena_strcpy(codegen->arena, type->type.generic.name).chars,
                arena_strcpy(codegen->arena, mapped).chars
            );
            strbuf_append_str(sb, mapped);
            break;
        }

        case RTK_TERMINAL: {
            assert(false); // ??
            break;
        }

        default: printf("TODO: resolved type [%d]\n", type->kind); assert(false);
    }
}

static String gen_type_resolved(CodegenC* codegen, ResolvedType* type) {
    StringBuffer sb = strbuf_create(codegen->arena);
    _append_type_resolved(codegen, &sb, type);
    return strbuf_to_str(sb);
}

static String gen_type(CodegenC* codegen, Type type, Package* from_pkg) {
    StringBuffer sb = strbuf_create(codegen->arena);
    _append_type(codegen, &sb, type, from_pkg);
    return strbuf_to_str(sb);
}

static void fill_nodes(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype, TransformStage stage, bool root_call);

static void _fn_header_decl(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype) {
    ResolvedType* type = codegen->packages->types[node->id.val].type;
    assert(type);
    assert(type->from_pkg);

    Strings params = {
        .length = 0,
        .strings = arena_calloc(codegen->arena, node->node.function_header_decl.params.length, sizeof(String)),
    };
    {
        StringBuffer sb = strbuf_create(codegen->arena);

        LLNode_FnParam* curr = node->node.function_header_decl.params.head;
        while (curr) {
            strbuf_append_str(&sb, gen_type(codegen, curr->data.type, type->from_pkg));
            strbuf_append_char(&sb, ' ');
            strbuf_append_str(&sb, curr->data.name);

            params.strings[params.length++] = strbuf_to_strcpy(sb);
            strbuf_reset(&sb);

            curr = curr->next;
        }
    }

    String name = node->node.function_header_decl.name;

    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
        .type = ICNT_FUNCTION_HEADER_DECL,
        .node.function_decl = {
            .return_type = gen_type(codegen, node->node.function_header_decl.return_type, type->from_pkg),
            .name = name,
            .params = params,
        },
    });
}

static void fill_nodes(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype, TransformStage stage, bool root_call) {
    assert(codegen);
    assert(c_nodes);
    assert(node);

    assert(codegen->current_package);

    switch (node->type) {
        case ANT_FILE_ROOT: assert(false);

        case ANT_PACKAGE: break;
        case ANT_FILE_SEPARATOR: break;

        case ANT_IMPORT: {
            if (ftype == FT_C || stage != TS_MACROS) {
                break; // c-files only import their header
            }
            ResolvedType* type = codegen->packages->types[node->id.val].type;

            Package* pkg;
            if (type->kind == RTK_NAMESPACE) {
                pkg = type->type.namespace_;
            } else {
                pkg = type->from_pkg;
            }
            assert(pkg->ast->type == ANT_FILE_ROOT);

            DirectiveCHeader* c_header = get_c_header(pkg);
            if (c_header) {
                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_MACRO_INCLUDE,
                    .node.include = {
                        .is_local = false,
                        .file = c_header->include,
                    },
                });
                break;
            } else {
                String include_path = gen_header_file_path(codegen->arena, pkg->full_name);

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_MACRO_INCLUDE,
                    .node.include = {
                        .is_local = true,
                        .file = include_path,
                    },
                });
                break;
            }
        }

        case ANT_LITERAL: {
            switch (node->node.literal.kind) {
                case LK_STR: {
                    bool is_c_str = false;

                    if (node->directives.length > 0) {
                        LLNode_Directive* curr = node->directives.head;
                        while (curr) {
                            if (curr->data.type == DT_C_STR) {
                                StringBuffer sb = strbuf_create(codegen->arena);
                                strbuf_append_char(&sb, '"');
                                strbuf_append_str(&sb, node->node.literal.value.lit_str);
                                strbuf_append_char(&sb, '"');

                                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = strbuf_to_str(sb),
                                });

                                is_c_str = true;
                                break;
                            }
                            curr = curr->next;
                        }
                    }

                    if (!is_c_str) {
                        String value = node->node.literal.value.lit_str;

                        ResolvedType* str_type = codegen->packages->string_literal_type;
                        assert(str_type);
                        assert(str_type->src);
                        assert(str_type->src->type == ANT_STRUCT_DECL);
                        assert(str_type->src->node.struct_decl.maybe_name);

                        StringBuffer sb = strbuf_create(codegen->arena);
                        strbuf_append_chars(&sb, "((std_String){");
                        // {
                        //     String str_struct_name = user_var_name(codegen->arena, *str_type->src->node.struct_decl.maybe_name, str_type->from_pkg);
                        //     strbuf_append_str(&sb, str_struct_name);
                        // }
                        // strbuf_append_chars(&sb, "){");
                        strbuf_append_uint(&sb, value.length);
                        strbuf_append_chars(&sb, ", \"");
                        strbuf_append_str(&sb, value);
                        strbuf_append_chars(&sb, "\"})");

                        ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = strbuf_to_str(sb),
                        });
                    }

                    break;
                }

                case LK_CHAR: {
                    StringBuffer sb = strbuf_create(codegen->arena);
                    strbuf_append_char(&sb, '\'');
                    strbuf_append_str(&sb, node->node.literal.value.lit_char);
                    strbuf_append_char(&sb, '\'');
                    // strbuf_append_chars(&sb, "(((char_){'");
                    // strbuf_append_str(&sb, node->node.literal.value.lit_char);
                    // strbuf_append_chars(&sb, "'})._)");
                    String value = strbuf_to_str(sb);

                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = value,
                    });
                    break;
                }

                case LK_INT: {
                    StringBuffer sb = strbuf_create(codegen->arena);
                    strbuf_append_int(&sb, node->node.literal.value.lit_int);
                    String value = strbuf_to_str(sb);

                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = value,
                    });
                    break;
                }

                case LK_BOOL: {
                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = c_str(node->node.literal.value.lit_bool ? "true" : "false"),
                    });
                    break;
                }

                case LK_NULL: {
                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = c_str("NULL"),
                    });
                    break;
                }

                default: assert(false);
            }

            break;
        }

        case ANT_TEMPLATE_STRING: {
            codegen->needs_string_template = true;

            String var_name = unique_var_name(codegen->arena);
            String var_ptr;
            {
                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_char(&sb, '&');
                strbuf_append_str(&sb, var_name);
                var_ptr = strbuf_to_str(sb);
            }

            // gen: StringBuffer sb = strbuf_default();
            {
                IR_C_Node* sb_init_target = arena_alloc(codegen->arena, sizeof *sb_init_target);
                *sb_init_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("std_ds_strbuf_default"),
                };

                IR_C_Node* sb_init = arena_alloc(codegen->arena, sizeof *sb_init);
                *sb_init = (IR_C_Node){
                    .type = ICNT_FUNCTION_CALL,
                    .node.function_call = {
                        .target = sb_init_target,
                        .args = {0},
                    },
                };

                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                    .type = ICNT_VAR_DECL,
                    .node.var_decl = {
                        .type = c_str("std_ds_StringBuffer"),
                        .name = var_name,
                        .init = sb_init,
                    },
                });
            }

            // gen: strbuf_append_chars(&sb, "..."); // first string part
            {
                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                *append_chars_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("std_ds_strbuf_append_chars"),
                };

                LL_IR_C_Node append_chars_args = {0};
                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = var_ptr,
                });

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_char(&sb, '"');
                strbuf_append_str(&sb, (String){
                    .length = node->node.template_string.str_parts.array[0].length - 2,
                    .chars = node->node.template_string.str_parts.array[0].chars + 1,
                });
                strbuf_append_char(&sb, '"');
                String str_lit = strbuf_to_str(sb);
                
                if (str_lit.length > 2) {
                    ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = str_lit,
                    });

                    ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                        .type = ICNT_FUNCTION_CALL,
                        .node.function_call = {
                            .target = append_chars_target,
                            .args = append_chars_args,
                        },
                    });
                }
            }

            size_t i = 1;
            LLNode_ASTNode* curr = node->node.template_string.template_expr_parts.head;
            while (curr) {
                assert(codegen->packages->types[curr->data.id.val].type);
                if (curr->data.type == ANT_VAR_REF && !curr->data.node.var_ref.path->child && resolved_type_eq(codegen->packages->types[curr->data.id.val].type, codegen->packages->string_literal_type)) {
                    IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                    *append_chars_target = (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = c_str("std_ds_strbuf_append_str"),
                    };

                    LL_IR_C_Node append_chars_args = {0};
                    ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = var_ptr,
                    });
                    {
                        ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw = user_var_name(codegen->arena, curr->data.node.var_ref.path->name, codegen->current_package),
                        });
                    }

                    ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                        .type = ICNT_FUNCTION_CALL,
                        .node.function_call = {
                            .target = append_chars_target,
                            .args = append_chars_args,
                        },
                    });

                } else {
                    ResolvedType* rt = codegen->packages->types[curr->data.id.val].type;
                    while (true) {
                        assert(rt);

                        switch (rt->kind) {
                            case RTK_INT:
                            case RTK_INT8:
                            case RTK_INT16:
                            case RTK_INT32:
                            case RTK_INT64:
                            {
                                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                                *append_chars_target = (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = c_str("std_ds_strbuf_append_int"),
                                };

                                LL_IR_C_Node append_chars_args = {0};
                                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = var_ptr,
                                });
                                {
                                    LL_IR_C_Node expr_ll = {0};
                                    fill_nodes(codegen, &expr_ll, &curr->data, ftype, stage, false);
                                    assert(expr_ll.length == 1);

                                    ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                        .type = ICNT_RAW_WRAP,
                                        .node.raw_wrap = {
                                            .pre = c_str("(int64_t)"),
                                            .wrapped = &expr_ll.head->data,
                                            .post = c_str(""),
                                        },
                                    });
                                }

                                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                                    .type = ICNT_FUNCTION_CALL,
                                    .node.function_call = {
                                        .target = append_chars_target,
                                        .args = append_chars_args,
                                    },
                                });

                                break;
                            }

                            case RTK_UINT:
                            case RTK_UINT8:
                            case RTK_UINT16:
                            case RTK_UINT32:
                            case RTK_UINT64:
                            {
                                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                                *append_chars_target = (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = c_str("std_ds_strbuf_append_uint"),
                                };

                                LL_IR_C_Node append_chars_args = {0};
                                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = var_ptr,
                                });
                                {
                                    LL_IR_C_Node expr_ll = {0};
                                    fill_nodes(codegen, &expr_ll, &curr->data, ftype, stage, false);
                                    assert(expr_ll.length == 1);

                                    ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                        .type = ICNT_RAW_WRAP,
                                        .node.raw_wrap = {
                                            .pre = c_str("(uint64_t)"),
                                            .wrapped = &expr_ll.head->data,
                                            .post = c_str(""),
                                        },
                                    });
                                }

                                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                                    .type = ICNT_FUNCTION_CALL,
                                    .node.function_call = {
                                        .target = append_chars_target,
                                        .args = append_chars_args,
                                    },
                                });

                                break;
                            }

                            case RTK_CHAR: {
                                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                                *append_chars_target = (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = c_str("std_ds_strbuf_append_char"),
                                };

                                LL_IR_C_Node append_chars_args = {0};
                                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = var_ptr,
                                });
                                {
                                    LL_IR_C_Node expr_ll = {0};
                                    fill_nodes(codegen, &expr_ll, &curr->data, ftype, stage, false);
                                    assert(expr_ll.length == 1);

                                    ll_node_push(codegen->arena, &append_chars_args, expr_ll.head->data);
                                }

                                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                                    .type = ICNT_FUNCTION_CALL,
                                    .node.function_call = {
                                        .target = append_chars_target,
                                        .args = append_chars_args,
                                    },
                                });

                                break;
                            }

                            case RTK_BOOL: {
                                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                                *append_chars_target = (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = c_str("std_ds_strbuf_append_bool"),
                                };

                                LL_IR_C_Node append_chars_args = {0};
                                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = var_ptr,
                                });
                                {
                                    LL_IR_C_Node expr_ll = {0};
                                    fill_nodes(codegen, &expr_ll, &curr->data, ftype, stage, false);
                                    assert(expr_ll.length == 1);

                                    ll_node_push(codegen->arena, &append_chars_args, expr_ll.head->data);
                                }

                                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                                    .type = ICNT_FUNCTION_CALL,
                                    .node.function_call = {
                                        .target = append_chars_target,
                                        .args = append_chars_args,
                                    },
                                });

                                break;
                            }

                            case RTK_STRUCT_REF: {
                                assert(resolved_type_eq(codegen->packages->types[curr->data.id.val].type, codegen->packages->string_literal_type));

                                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                                *append_chars_target = (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = c_str("std_ds_strbuf_append_str"),
                                };

                                LL_IR_C_Node append_chars_args = {0};
                                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                                    .type = ICNT_RAW,
                                    .node.raw.str = var_ptr,
                                });
                                {
                                    LL_IR_C_Node expr_ll = {0};
                                    fill_nodes(codegen, &expr_ll, &curr->data, ftype, stage, false);
                                    assert(expr_ll.length == 1);

                                    ll_node_push(codegen->arena, &append_chars_args, expr_ll.head->data);
                                }

                                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                                    .type = ICNT_FUNCTION_CALL,
                                    .node.function_call = {
                                        .target = append_chars_target,
                                        .args = append_chars_args,
                                    },
                                });

                                break;
                            }

                            case RTK_GENERIC: {
                                int64_t mapped_idx = get_mapped_generic_idx(codegen->generic_map, rt->type.generic.name);
                                if (mapped_idx < 0) {
                                    println_astnode(*rt->src);
                                    printf("Couldn't find %s\n", arena_strcpy(codegen->arena, rt->type.generic.name).chars);
                                    print_generic_map(codegen->arena, codegen->generic_map);
                                }
                                assert(mapped_idx >= 0);

                                while (codegen->generic_map->mapped_rtypes[mapped_idx].kind == RTK_GENERIC) {
                                    int64_t next_idx = get_mapped_generic_idx(codegen->generic_map, codegen->generic_map->mapped_types[mapped_idx]);
                                    while (next_idx >= 0) {
                                        mapped_idx = next_idx;
                                        next_idx = get_mapped_generic_idx(codegen->generic_map, codegen->generic_map->mapped_types[mapped_idx]);
                                    }
                                }

                                rt = codegen->generic_map->mapped_rtypes + mapped_idx;

                                continue;
                            }

                            default: printf("TODO: string template RTK_%d\n", codegen->packages->types[curr->data.id.val].type->kind); assert(false);
                        }

                        break; // while (true)
                    }
                }

                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                *append_chars_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("std_ds_strbuf_append_chars"),
                };

                LL_IR_C_Node append_chars_args = {0};
                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = var_ptr,
                });

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_char(&sb, '"');
                strbuf_append_str(&sb, (String){
                    .length = node->node.template_string.str_parts.array[i].length - 2,
                    .chars = node->node.template_string.str_parts.array[i].chars + 1,
                });
                strbuf_append_char(&sb, '"');
                String str_lit = strbuf_to_str(sb);
                
                if (str_lit.length > 2) {
                    ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = str_lit,
                    });

                    ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                        .type = ICNT_FUNCTION_CALL,
                        .node.function_call = {
                            .target = append_chars_target,
                            .args = append_chars_args,
                        },
                    });
                }
 
                i += 1;
                curr = curr->next;
            }

            // gen: strbuf_to_str(sb);
            {
                IR_C_Node* to_str_target = arena_alloc(codegen->arena, sizeof *to_str_target);
                *to_str_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("std_ds_strbuf_as_str"),
                };

                LL_IR_C_Node to_str_args = {0};
                ll_node_push(codegen->arena, &to_str_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = var_name,
                });

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_FUNCTION_CALL,
                    .node.function_call = {
                        .target = to_str_target,
                        .args = to_str_args,
                    },
                });
            }

            // gen: strbuf_free(sb);
            {
                assert(codegen->stmt_block->to_defer);

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_chars(&sb, "std_ds_strbuf_free(");
                strbuf_append_str(&sb, var_name);
                strbuf_append_char(&sb, ')');

                ll_node_push(codegen->arena, codegen->stmt_block->to_defer, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw = strbuf_to_str(sb),
                });
            }

            break;
        }

        case ANT_ARRAY_INIT: {
            size_t elems_length = node->node.array_init.elems.length;
            IR_C_Node* indicies = arena_calloc(codegen->arena, elems_length, sizeof *indicies);
            IR_C_Node* elems = arena_calloc(codegen->arena, elems_length, sizeof *elems);

            size_t i = 0;
            LLNode_ArrayInitElem* curr = node->node.array_init.elems.head;
            while (curr) {
                if (curr->data.maybe_index) {
                    LL_IR_C_Node expr_ll = {0};
                    fill_nodes(codegen, &expr_ll, curr->data.maybe_index, ftype, stage, false);
                    assert(expr_ll.length == 1);

                    indicies[i] = expr_ll.head->data;
                } else {
                    indicies[i] = (IR_C_Node){
                        .type = ICNT_COUNT,
                    };
                }

                LL_IR_C_Node expr_ll = {0};
                fill_nodes(codegen, &expr_ll, curr->data.value, ftype, stage, false);
                assert(expr_ll.length == 1);

                elems[i] = expr_ll.head->data;

                i += 1;
                curr = curr->next;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_ARRAY_INIT,
                .node.array_init = {
                    .elems_length = elems_length,
                    .indicies = indicies,
                    .elems = elems,
                },
            });
            break;
        }

        case ANT_VAR_REF: {
            assert(node->node.var_ref.path);

            StaticPath* child = node->node.var_ref.path;
            if (!child->child) {
                String name = user_var_name(codegen->arena, child->name, codegen->current_package);

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = name,
                });
            } else {
                while (child->child) {
                    child = child->child;
                }

                ResolvedType* type = codegen->packages->types[node->id.val].type;

                String name = user_var_name(codegen->arena, child->name, type ? type->from_pkg : NULL);

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = name,
                });
            }
            break;                 
        }

        case ANT_UNARY_OP: {
            LL_IR_C_Node expr_ll = {0};
            fill_nodes(codegen, &expr_ll, node->node.unary_op.right, ftype, stage, false);
            assert(expr_ll.length == 1);

            IR_C_Node* expr = &expr_ll.head->data;

            bool already_done = false;

            String op;
            switch (node->node.unary_op.op) {
                case UO_NUM_NEGATE: op = c_str("-"); break;
                case UO_BOOL_NEGATE: op = c_str("!"); break;

                case UO_PLUS_PLUS: op = c_str("++"); break;
                case UO_MINUS_MINUS: op = c_str("--"); break;

                case UO_PTR_DEREF: op = c_str("*"); break;

                case UO_PTR_REF: {
                    op = c_str("&");
                    if (node->node.unary_op.right->type == ANT_LITERAL) {
                        String var_name = unique_var_name(codegen->arena);
                        String var_ptr;
                        {
                            StringBuffer sb = strbuf_create(codegen->arena);
                            strbuf_append_char(&sb, '&');
                            strbuf_append_str(&sb, var_name);
                            var_ptr = strbuf_to_str(sb);
                        }

                        TypeBuiltIn built_in;
                        switch (node->node.unary_op.right->node.literal.kind) {
                            case LK_BOOL: built_in = TBI_BOOL; break;
                            case LK_CHAR: built_in = TBI_CHAR; break;
                            case LK_INT: built_in = TBI_INT; break;
                            default: assert(false);
                        }
                        Type typ = {
                            .id = { 0 },
                            .kind = TK_BUILT_IN,
                            .type.built_in = built_in,
                            .directives = {0},
                        };

                        LL_IR_C_Node lit_ll = {0};
                        fill_nodes(codegen, &lit_ll, node->node.unary_op.right, ftype, stage, false);
                        assert(lit_ll.length == 1);

                        StringBuffer pre = strbuf_create(codegen->arena);
                        strbuf_append_str(&pre, gen_type(codegen, typ, codegen->current_package));
                        strbuf_append_char(&pre, ' ');
                        strbuf_append_str(&pre, var_name);
                        strbuf_append_chars(&pre, " = ");

                        ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                            .type = ICNT_RAW_WRAP,
                            .node.raw_wrap = {
                                .pre = strbuf_to_str(pre),
                                .wrapped = &lit_ll.head->data,
                                .post = c_str(""),
                            },
                        });
                        ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = var_ptr,
                        });
                        already_done = true;
                    } else if (node->node.unary_op.right->type == ANT_ARRAY_INIT) {
                        String var_name = unique_var_name(codegen->arena);

                        LL_IR_C_Node lit_ll = {0};
                        fill_nodes(codegen, &lit_ll, node->node.unary_op.right, ftype, stage, false);
                        assert(lit_ll.length == 1);

                        ResolvedType* rt = codegen->packages->types[node->node.unary_op.right->id.val].type;
                        assert(rt);

                        StringBuffer pre = strbuf_create(codegen->arena);
                        strbuf_append_str(&pre, gen_type_resolved(codegen, rt));
                        strbuf_append_char(&pre, ' ');
                        strbuf_append_str(&pre, var_name);
                        strbuf_append_chars(&pre, "[] = ");

                        ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                            .type = ICNT_RAW_WRAP,
                            .node.raw_wrap = {
                                .pre = strbuf_to_str(pre),
                                .wrapped = &lit_ll.head->data,
                                .post = c_str(""),
                            },
                        });
                        ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = var_name,
                        });
                        already_done = true;
                    }
                    break;
                }

                default: printf("TODO: unary op [%d]\n", node->node.unary_op.op); assert(false);
            }

            if (!already_done) {
                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_RAW_WRAP,
                    .node.raw_wrap = {
                        .pre = op,
                        .wrapped = expr,
                        .post = c_str(""),
                    },
                });
            }
            break;
        }

        case ANT_POSTFIX_OP: {
            LL_IR_C_Node expr_ll = {0};
            fill_nodes(codegen, &expr_ll, node->node.postfix_op.left, ftype, stage, false);
            assert(expr_ll.length == 1);

            IR_C_Node* expr = &expr_ll.head->data;

            bool already_done = false;

            String op;
            switch (node->node.postfix_op.op) {
                case PFO_PLUS_PLUS: op = c_str("++"); break;
                case PFO_MINUS_MINUS: op = c_str("--"); break;
                
                default: printf("TODO: postfix op [%d]\n", node->node.postfix_op.op); assert(false);
            }

            if (!already_done) {
                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_RAW_WRAP,
                    .node.raw_wrap = {
                        .pre = c_str(""),
                        .wrapped = expr,
                        .post = op,
                    },
                });
            }

            break;
        }

        case ANT_GET_FIELD: {
            LL_IR_C_Node root_ll = {0};
            fill_nodes(codegen, &root_ll, node->node.get_field.root, ftype, stage, false);
            assert(root_ll.length == 1);

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_GET_FIELD,
                .node.get_field = {
                    .is_ptr = node->node.get_field.is_ptr_deref,
                    .root = &root_ll.head->data,
                    .name = node->node.get_field.name,
                },
            });
            break;
        }

        case ANT_SIZEOF: {
            if (node->node.sizeof_.kind == SOK_EXPR) {
                LL_IR_C_Node expr_ll = {0};
                fill_nodes(codegen, &expr_ll, node->node.sizeof_.sizeof_.expr, ftype, stage, false);
                assert(expr_ll.length == 1);

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_SIZEOF_EXPR,
                    .node.sizeof_expr.expr = &expr_ll.head->data,
                });
            } else {
                TypeInfo* ti = packages_type_by_type(codegen->packages, node->node.sizeof_.sizeof_.type->id);
                Package* pkg = NULL;
                if (ti && ti->type) {
                    pkg = ti->type->from_pkg;
                }

                String type = gen_type(codegen, *node->node.sizeof_.sizeof_.type, pkg);

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_SIZEOF_TYPE,
                    .node.sizeof_type.type = type,
                });
            }

            break;
        }

        case ANT_FUNCTION_CALL: {
            LL_IR_C_Node target_ll = {0};
            fill_nodes(codegen, &target_ll, node->node.function_call.function, ftype, stage, false);
            assert(target_ll.length == 1);

            IR_C_Node* target = arena_alloc(codegen->arena, sizeof *target);
            *target = target_ll.head->data;

            if (target->type == ICNT_RAW && node->node.function_call.generic_args.length > 0) {
                size_t version = node->node.function_call.impl_version;
                {
                    ResolvedType* rt = codegen->packages->types[node->node.function_call.function->id.val].type;
                    if (rt && rt->kind == RTK_FUNCTION_DECL && rt->src) {
                        LL_GenericImpl impls = codegen->packages->generic_impls_nodes_concrete[rt->src->id.val];
                        // assert(impls.length > 0);

                        if (impls.length == 1) {
                            version = 0;
                        } else if (impls.length > 0) {
                            size_t impl_idx = 0;
                            LLNode_GenericImpl* curr = impls.head;
                            while (curr) {
                                if (curr->data.length == node->node.function_call.generic_args.length) {
                                    size_t idx = 0;
                                    LLNode_Type* arg_curr = node->node.function_call.generic_args.head;
                                    bool match = true;
                                    while (arg_curr && idx < curr->data.length) {
                                        TypeInfo* ti = packages_type_by_type(codegen->packages, arg_curr->data.id);
                                        assert(ti);
                                        assert(ti->type);

                                        if (!resolved_type_eq(ti->type, curr->data.resolved_types[idx])) {
                                            match = false;
                                            break;
                                        }

                                        idx += 1;
                                        arg_curr = arg_curr->next;
                                    }
                                    if (arg_curr || idx < curr->data.length) {
                                        match = false;
                                    }

                                    if (match) {
                                        version = impl_idx;
                                        break;
                                    }
                                }

                                impl_idx += 1;
                                curr = curr->next;
                            }
                        }
                    }
                }

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_str(&sb, target->node.raw.str);
                strbuf_append_chars(&sb, "_");
                strbuf_append_uint(&sb, version);
                target->node.raw.str = strbuf_to_str(sb);
            }
            
            LL_IR_C_Node args = {0};
            {
                LLNode_ASTNode* curr = node->node.function_call.args.head;
                while (curr) {
                    fill_nodes(codegen, &args, &curr->data, ftype, stage, false);
                    curr = curr->next;
                }
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_FUNCTION_CALL,
                .node.function_call = {
                    .target = target,
                    .args = args,
                },
            });
            break;
        }

        case ANT_RETURN: {
            IR_C_Node* expr = NULL;
            if (node->node.return_.maybe_expr) {
                LL_IR_C_Node expr_ll = {0};
                fill_nodes(codegen, &expr_ll, node->node.return_.maybe_expr, ftype, stage, false);
                assert(expr_ll.length == 1);

                expr = &expr_ll.head->data;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RETURN,
                .node.return_ = {
                    .defer_block = ll_clone(codegen->arena, codegen->stmt_block->to_defer),
                    .expr = expr,
                },
            });
            codegen->stmt_block->to_defer = arena_alloc(codegen->arena, sizeof *codegen->stmt_block->to_defer);
            break;
        }

        case ANT_DEFER: {
            LL_IR_C_Node ll = {0};
            fill_nodes(codegen, &ll, node->node.defer.stmt, ftype, stage, false);

            if (ll.length > 0) {
                ll.tail->next = codegen->stmt_block->to_defer->head;
                if (!ll.tail->next) {
                    codegen->stmt_block->to_defer->tail = ll.tail;
                }
                codegen->stmt_block->to_defer->head = ll.head;
                codegen->stmt_block->to_defer->length += ll.length;
            }

            break;
        }

        case ANT_BREAK: {
            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = c_str("break"),
            });
            break;
        }

        case ANT_CONTINUE: {
            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = c_str("continue"),
            });
            break;
        }

        case ANT_ASSIGNMENT: {
            LL_IR_C_Node lhs_expr_ll = {0};
            fill_nodes(codegen, &lhs_expr_ll, node->node.assignment.lhs, ftype, stage, false);
            assert(lhs_expr_ll.length == 1);
            IR_C_Node* lhs_expr = &lhs_expr_ll.head->data;

            LL_IR_C_Node rhs_expr_ll = {0};
            fill_nodes(codegen, &rhs_expr_ll, node->node.assignment.rhs, ftype, stage, false);
            assert(rhs_expr_ll.length == 1);
            IR_C_Node* rhs_expr = &rhs_expr_ll.head->data;

            String op;
            switch (node->node.assignment.op) {
                case AO_ASSIGN: op = c_str("="); break;

                case AO_PLUS_ASSIGN: op = c_str("+="); break;
                case AO_MINUS_ASSIGN: op = c_str("-="); break;
                case AO_MULTIPLY_ASSIGN: op = c_str("*="); break;
                case AO_DIVIDE_ASSIGN: op = c_str("/="); break;

                case AO_BIT_AND_ASSIGN: op = c_str("&="); break;
                case AO_BIT_OR_ASSIGN: op = c_str("|="); break;
                case AO_BIT_XOR_ASSIGN: op = c_str("^="); break;

                default: printf("TODO: assignment op [%d]\n", node->node.assignment.op); assert(false);
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_BINARY_OP,
                .node.binary_op = {
                    .lhs = lhs_expr,
                    .op = op,
                    .rhs = rhs_expr,
                },
            });

            break;
        }

        case ANT_BINARY_OP: {
            LL_IR_C_Node lhs_expr_ll = {0};
            fill_nodes(codegen, &lhs_expr_ll, node->node.binary_op.lhs, ftype, stage, false);
            assert(lhs_expr_ll.length == 1);
            IR_C_Node* lhs_expr = &lhs_expr_ll.head->data;

            LL_IR_C_Node rhs_expr_ll = {0};
            fill_nodes(codegen, &rhs_expr_ll, node->node.binary_op.rhs, ftype, stage, false);
            assert(rhs_expr_ll.length == 1);
            IR_C_Node* rhs_expr = &rhs_expr_ll.head->data;

            String op;
            switch (node->node.binary_op.op) {
                case BO_BIT_OR: op = c_str("|"); break;
                case BO_BIT_AND: op = c_str("&"); break;
                case BO_BIT_XOR: op = c_str("^"); break;

                case BO_ADD: op = c_str("+"); break;
                case BO_SUBTRACT: op = c_str("-"); break;
                case BO_MULTIPLY: op = c_str("*"); break;
                case BO_DIVIDE: op = c_str("/"); break;
                case BO_MODULO: op = c_str("%"); break;

                case BO_BOOL_OR: op = c_str("||"); break;
                case BO_BOOL_AND: op = c_str("&&"); break;

                case BO_EQ: op = c_str("=="); break;
                case BO_NOT_EQ: op = c_str("!="); break;

                case BO_GREATER: op = c_str(">"); break;
                case BO_GREATER_OR_EQ: op = c_str(">="); break;
                case BO_LESS: op = c_str("<"); break;
                case BO_LESS_OR_EQ: op = c_str("<="); break;

                default: printf("TODO: binary op [%d]\n", node->node.assignment.op); assert(false);
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_BINARY_OP,
                .node.binary_op = {
                    .lhs = lhs_expr,
                    .op = op,
                    .rhs = rhs_expr,
                },
            });

            break;
        }

        case ANT_INDEX: {
            LL_IR_C_Node root_expr_ll = {0};
            fill_nodes(codegen, &root_expr_ll, node->node.index.root, ftype, stage, false);
            assert(root_expr_ll.length == 1);
            IR_C_Node* root_expr = &root_expr_ll.head->data;

            LL_IR_C_Node value_expr_ll = {0};
            fill_nodes(codegen, &value_expr_ll, node->node.index.value, ftype, stage, false);
            assert(value_expr_ll.length == 1);
            IR_C_Node* value_expr = &value_expr_ll.head->data;

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_INDEX,
                .node.index = {
                    .root = root_expr,
                    .value = value_expr,
                },
            });

            break;
        }

        case ANT_VAR_DECL: {
            if (root_call && (stage != TS_VARS || ftype == FT_C)) {
                break;
            }
            ResolvedType* rt = codegen->packages->types[node->id.val].type;
            if (!rt) {
                printf("%s\n", arena_strcpy(codegen->arena, node->node.var_decl.lhs.lhs.name).chars);
            }
            assert(rt);

            String type = gen_type_resolved(codegen, rt);

            assert(node->node.var_decl.lhs.type == VDLT_NAME);
            String name = user_var_name(
                codegen->arena,
                node->node.var_decl.lhs.lhs.name,
                codegen->current_package
            );

            if (node->node.var_decl.type_or_let.maybe_type && node->node.var_decl.type_or_let.maybe_type->kind == TK_ARRAY) {
                StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 2);
                strbuf_append_str(&sb, name);
                strbuf_append_char(&sb, '[');
                Token* explicit_size = node->node.var_decl.type_or_let.maybe_type->type.array.explicit_size;
                if (explicit_size) {
                    strbuf_append_str(&sb, (String){
                        .length = explicit_size->length,
                        .chars = explicit_size->start,
                    });
                }
                strbuf_append_char(&sb, ']');
                name = strbuf_to_str(sb);
            }

            IR_C_Node* init = NULL;
            if (node->node.var_decl.initializer) {
                LL_IR_C_Node init_ll = {0};
                fill_nodes(codegen, &init_ll, node->node.var_decl.initializer, ftype, stage, false);
                assert(init_ll.length == 1);

                init = &init_ll.head->data;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_VAR_DECL,
                .node.var_decl = {
                    .type = type,
                    .name = name,
                    .init = init,
                },
            });
            break;
        }

        case ANT_FUNCTION_HEADER_DECL: {
            if (ftype != FT_HEADER || stage != TS_FN_HEADERS) {
                break;
            }
            // _fn_header_decl(codegen, c_nodes, node, ftype);
            break;
        }

        case ANT_FUNCTION_DECL: {
            if (stage != TS_FN_HEADERS && stage != TS_FNS) {
                break;
            }

            LL_GenericImpl generic_impls = codegen->packages->generic_impls_nodes_concrete[node->id.val];

            if (node->node.function_decl.header.generic_params.length > 0 && generic_impls.length == 0) {
                break;
            }

            if (stage == TS_FN_HEADERS) {
                ResolvedType* type = codegen->packages->types[node->id.val].type;
                assert(type);
                assert(type->from_pkg);

                size_t versions = generic_impls.length;
                if (versions == 0) {
                    versions = 1;
                }

                if (node->node.function_decl.header.generic_params.length == 0) {
                    assert(versions == 1);
                }

                GenericImplMap* root_map = codegen->generic_map;

                LLNode_GenericImpl* generic_impls_curr = generic_impls.head;
                for (size_t version = 0; version < versions; ++version) {
                    if (generic_impls.length > 0) {
                        assert(generic_impls_curr);

                        GenericImpl generic_impl = generic_impls_curr->data;

                        if (generic_impl.length == 0 && versions > 1) {
                            continue;
                        }

                        GenericImplMap map = {
                            .parent = root_map,
                            .length = node->node.function_decl.header.generic_params.length,
                            .generic_names = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(String)),
                            .mapped_types = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(String)),
                            .mapped_rtypes = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(ResolvedType)),
                        };
                        {
                            ResolvedType* curr = generic_impl.resolved_types[0];
                            for (size_t i = 0; curr && i < node->node.function_decl.header.generic_params.length; ++i) {
                                map.generic_names[i] = node->node.function_decl.header.generic_params.array[i];
                                map.mapped_types[i] = gen_type_resolved(codegen, curr);
                                map.mapped_rtypes[i] = *curr;
                                curr = generic_impl.resolved_types[i + 1];
                            }
                        }
                        codegen->generic_map = &map;
                    }

                    Strings params = {
                        .length = 0,
                        .strings = arena_calloc(codegen->arena, node->node.function_decl.header.params.length, sizeof(String)),
                    };
                    {
                        StringBuffer sb = strbuf_create(codegen->arena);

                        LLNode_FnParam* curr = node->node.function_decl.header.params.head;
                        while (curr) {
                            TypeInfo* curr_ti = packages_type_by_type(codegen->packages, curr->data.type.id);
                            assert(curr_ti);
                            assert(curr_ti->type);
                            assert(curr_ti->type->from_pkg);

                            // strbuf_append_str(&sb, gen_type(codegen, curr->data.type, curr_ti->type->from_pkg));
                            strbuf_append_str(&sb, gen_type_resolved(codegen, curr_ti->type));
                            strbuf_append_char(&sb, ' ');
                            strbuf_append_str(&sb, user_var_name(codegen->arena, curr->data.name, codegen->current_package));

                            params.strings[params.length++] = strbuf_to_strcpy(sb);
                            strbuf_reset(&sb);

                            curr = curr->next;
                        }
                    }

                    String return_type;
                    {
                        TypeInfo* ti = packages_type_by_type(codegen->packages, node->node.function_decl.header.return_type.id);
                        assert(ti);
                        assert(ti->type);

                        return_type = gen_type_resolved(codegen, ti->type);

                    }

                    String name;
                    if (node->node.function_decl.header.is_main) {
                        name = c_str("_main");
                    } else {
                        name = user_var_name(
                            codegen->arena,
                            node->node.function_decl.header.name,
                            type->from_pkg
                        );
                    }

                    if (node->node.function_decl.header.generic_params.length > 0) {
                        StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 3);
                        strbuf_append_str(&sb, name);
                        strbuf_append_chars(&sb, "_");
                        strbuf_append_uint(&sb, version);
                        name = strbuf_to_str(sb);
                    }

                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_FUNCTION_HEADER_DECL,
                        .node.function_decl = {
                            .return_type = return_type,
                            .name = name,
                            .params = params,
                        },
                    });

                    codegen->generic_map = root_map;
                    if (generic_impls_curr) {
                        generic_impls_curr = generic_impls_curr->next;
                    }
                }
                break;
            }

            if (ftype == FT_HEADER) {
                break;
            }

            ResolvedType* type = codegen->packages->types[node->id.val].type;
            assert(type);
            assert(type->from_pkg);

            size_t versions = generic_impls.length;
            if (versions == 0) {
                versions = 1;
            }

            if (node->node.function_decl.header.generic_params.length == 0) {
                assert(versions == 1);
            }

            GenericImplMap* root_map = codegen->generic_map;

            LLNode_GenericImpl* generic_impls_curr = generic_impls.head;
            for (size_t version = 0; version < versions; ++version) {
                if (generic_impls.length > 0) {
                    assert(generic_impls_curr);

                    GenericImpl generic_impl = generic_impls_curr->data;

                    if (generic_impl.length == 0 && versions > 1) {
                        continue;
                    }

                    GenericImplMap map = {
                        .parent = root_map,
                        .length = node->node.function_decl.header.generic_params.length,
                        .generic_names = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(String)),
                        .mapped_types = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(String)),
                        .mapped_rtypes = arena_calloc(codegen->arena, node->node.function_decl.header.generic_params.length, sizeof(ResolvedType)),
                    };
                    {
                        ResolvedType* curr = generic_impl.resolved_types[0];
                        for (size_t i = 0; curr && i < node->node.function_decl.header.generic_params.length; ++i) {
                            map.generic_names[i] = node->node.function_decl.header.generic_params.array[i];
                            map.mapped_types[i] = gen_type_resolved(codegen, curr);
                            map.mapped_rtypes[i] = *curr;
                            curr = generic_impl.resolved_types[i + 1];
                        }
                    }
                    codegen->generic_map = &map;
                }

                Strings params = {
                    .length = 0,
                    .strings = arena_calloc(codegen->arena, node->node.function_decl.header.params.length, sizeof(String)),
                };
                {
                    StringBuffer sb = strbuf_create(codegen->arena);

                    LLNode_FnParam* curr = node->node.function_decl.header.params.head;
                    while (curr) {
                        TypeInfo* curr_ti = packages_type_by_type(codegen->packages, curr->data.type.id);
                        assert(curr_ti);
                        assert(curr_ti->type);
                        assert(curr_ti->type->from_pkg);

                        // strbuf_append_str(&sb, gen_type(codegen, curr->data.type, curr_ti->type->from_pkg));
                        strbuf_append_str(&sb, gen_type_resolved(codegen, curr_ti->type));
                        strbuf_append_char(&sb, ' ');
                        strbuf_append_str(&sb, user_var_name(codegen->arena, curr->data.name, codegen->current_package));

                        params.strings[params.length++] = strbuf_to_strcpy(sb);
                        strbuf_reset(&sb);

                        curr = curr->next;
                    }
                }

                LL_IR_C_Node statements = {
                    .to_defer = arena_alloc(codegen->arena, sizeof *statements.to_defer),
                };
                *statements.to_defer = (LL_IR_C_Node){0};

                codegen->stmt_block = &statements;
                {
                    LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
                    while (curr) {
                        fill_nodes(codegen, &statements, &curr->data, ftype, stage, false);
                        curr = curr->next;
                    }
                }
                codegen->stmt_block = NULL;

                String return_type;
                {
                    TypeInfo* ti = packages_type_by_type(codegen->packages, node->node.function_decl.header.return_type.id);
                    assert(ti);
                    assert(ti->type);

                    return_type = gen_type_resolved(codegen, ti->type);
                }

                String name;
                if (node->node.function_decl.header.is_main) {
                    name = c_str("_main");
                } else {
                    name = user_var_name(
                        codegen->arena,
                        node->node.function_decl.header.name,
                        type->from_pkg
                    );
                }

                if (node->node.function_decl.header.generic_params.length > 0) {
                    StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 3);
                    strbuf_append_str(&sb, name);
                    strbuf_append_chars(&sb, "_");
                    strbuf_append_uint(&sb, version);
                    name = strbuf_to_str(sb);
                }

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_FUNCTION_DECL,
                    .node.function_decl = {
                        .return_type = return_type,
                        .name = name,
                        .params = params,
                        .statements = statements,
                    },
                });
                codegen->generic_map = root_map;

                if (generic_impls_curr) {
                    generic_impls_curr = generic_impls_curr->next;
                }
            }
            break;
        }

        case ANT_STRUCT_DECL: {
            if (root_call && (ftype == FT_C || stage != TS_TYPES)) {
                break;
            }
            assert(node->node.struct_decl.maybe_name);

            LL_GenericImpl generic_impls = codegen->packages->generic_impls_nodes_concrete[node->id.val];

            if (node->node.struct_decl.generic_params.length > 0 && generic_impls.length == 0) {
                break;
            }

            ResolvedType* type = codegen->packages->types[node->id.val].type;
            assert(type);
            assert(type->from_pkg);

            size_t versions = generic_impls.length;
            if (versions == 0) {
                versions = 1;
            }

            if (node->node.struct_decl.generic_params.length == 0) {
                assert(versions == 1);
            }

            GenericImplMap* root_map = codegen->generic_map;

            LLNode_GenericImpl* generic_impls_curr = generic_impls.head;
            printf("struct %s has %lu versions.\n",
                arena_strcpy(codegen->arena, *node->node.struct_decl.maybe_name).chars,
                versions
            );
            for (size_t version = 0; version < versions; ++version) {
                printf("Version %lu\n", version);
                if (generic_impls.length > 0) {
                    assert(generic_impls_curr);

                    GenericImpl generic_impl = generic_impls_curr->data;

                    if (generic_impl.length == 0 && versions > 1) {
                        printf("Ignoring version %lu\n", version);
                        codegen->generic_map = root_map;

                        if (generic_impls_curr) {
                            generic_impls_curr = generic_impls_curr->next;
                        }
                        continue;
                    }

                    GenericImplMap map = {
                        .parent = root_map,
                        .length = node->node.struct_decl.generic_params.length,
                        .generic_names = arena_calloc(codegen->arena, node->node.struct_decl.generic_params.length, sizeof(String)),
                        .mapped_types = arena_calloc(codegen->arena, node->node.struct_decl.generic_params.length, sizeof(String)),
                        .mapped_rtypes = arena_calloc(codegen->arena, node->node.struct_decl.generic_params.length, sizeof(ResolvedType)),
                    };
                    {
                        ResolvedType* curr = generic_impl.resolved_types[0];
                        for (size_t i = 0; curr && i < node->node.struct_decl.generic_params.length; ++i) {
                            map.generic_names[i] = node->node.struct_decl.generic_params.array[i];
                            map.mapped_types[i] = gen_type_resolved(codegen, curr);
                            map.mapped_rtypes[i] = *curr;

                            curr = generic_impl.resolved_types[i + 1];
                        }
                    }
                    codegen->generic_map = &map;
                }

                Strings fields = {
                    .length = 0,
                    .strings = arena_calloc(codegen->arena, node->node.struct_decl.fields.length, sizeof(String)),
                };
                {
                    StringBuffer sb = strbuf_create(codegen->arena);

                    size_t i = 0;
                    LLNode_StructField* curr = node->node.struct_decl.fields.head;
                    bool ok = true;
                    while (curr) {
                        if (type->type.struct_decl.fields[i].type->kind == RTK_GENERIC && !get_mapped_generic(codegen->generic_map, type->type.struct_decl.fields[i].type->type.generic.name)) {
                            ok = false;
                            break;
                        }
                        
                        String type_str = gen_type_resolved(codegen, type->type.struct_decl.fields[i].type);

                        printf(".%s = %s\n",
                            arena_strcpy(codegen->arena, type->type.struct_decl.fields[i].name).chars,
                            arena_strcpy(codegen->arena, type_str).chars
                        );

                        if (version > 0 && str_eq(type_str, c_str("struct main_Foo_0"))) {
                            printf("RTK_%d\n", type->type.struct_decl.fields[i].type->kind);
                            assert(false);
                        }

                        strbuf_append_str(&sb, type_str);
                        strbuf_append_char(&sb, ' ');
                        strbuf_append_str(&sb, curr->data.name);

                        fields.strings[fields.length++] = strbuf_to_strcpy(sb);
                        strbuf_reset(&sb);

                        i += 1;
                        curr = curr->next;
                    }
                    if (!ok) {
                        codegen->generic_map = root_map;

                        if (generic_impls_curr) {
                            generic_impls_curr = generic_impls_curr->next;
                        }
                        continue;
                    }
                }

                assert(node->node.struct_decl.maybe_name);
                String name = user_var_name(
                    codegen->arena,
                    *node->node.struct_decl.maybe_name,
                    type->from_pkg
                );

                if (node->node.struct_decl.generic_params.length > 0) {
                    StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 3);
                    strbuf_append_str(&sb, name);
                    strbuf_append_chars(&sb, "_");
                    strbuf_append_uint(&sb, version);
                    name = strbuf_to_str(sb);
                }

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_STRUCT_DECL,
                    .node.struct_decl = {
                        .name = name,
                        .fields = fields,
                    },
                });
                codegen->generic_map = root_map;

                if (generic_impls_curr) {
                    generic_impls_curr = generic_impls_curr->next;
                }
            }

            // if (str_eq(*node->node.struct_decl.maybe_name, c_str("Result"))) {
            //     assert(false);
            // }

            break;
        }

        case ANT_TYPEDEF_DECL: {
            if (root_call && (ftype == FT_C || stage != TS_TYPES)) {
                break;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_TYPEDEF_DECL,
                .node.typedef_decl = {
                    .type = gen_type_resolved(codegen, codegen->packages->types[node->id.val].type),
                    .name = user_var_name(codegen->arena, node->node.typedef_decl.name, codegen->current_package),
                },
            });
            break;
        }

        case ANT_CRASH: {
            if (node->node.crash.maybe_expr) {
                codegen->needs_std = true;
                codegen->needs_std_io = true;

                LL_IR_C_Node expr_ll = {0};
                fill_nodes(codegen, &expr_ll, node->node.crash.maybe_expr, ftype, stage, false);
                assert(expr_ll.length == 1);

                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                    .type = ICNT_RAW_WRAP,
                    .node.raw_wrap = {
                        .pre = c_str("std_io_eprintln("),
                        .wrapped = &expr_ll.head->data,
                        .post = c_str("); std_exit(1);\n"),
                    },
                });
            } else {
                codegen->needs_std = true;

                ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("std_assert(false);\n"),
                });
            }
            break;
        }

        case ANT_STRUCT_INIT: {
            ResolvedType* rt = codegen->packages->types[node->id.val].type;
            assert(rt);
            String type = gen_type_resolved(codegen, rt);

            StringBuffer sb = strbuf_create(codegen->arena);

            LL_IR_C_Node fields = {0};
            LLNode_StructFieldInit* curr = node->node.struct_init.fields.head;
            while (curr) {
                LL_IR_C_Node val_ll = {0};
                fill_nodes(codegen, &val_ll, curr->data.value, ftype, stage, false);
                assert(val_ll.length == 1);

                strbuf_reset(&sb);
                strbuf_append_char(&sb, '.');
                strbuf_append_str(&sb, curr->data.name);
                strbuf_append_chars(&sb, " = ");
                String pre = strbuf_to_strcpy(sb);

                ll_node_push(codegen->arena, &fields, (IR_C_Node){
                    .type = ICNT_RAW_WRAP,
                    .node.raw_wrap = {
                        .pre = pre,
                        .wrapped = &val_ll.head->data,
                        .post = c_str(""),
                    },
                });

                curr = curr->next;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_STRUCT_INIT,
                .node.struct_init = {
                    .type = type,
                    .fields = fields,
                },
            });
            break;
        }

        case ANT_WHILE: {
            LL_IR_C_Node cond_ll = {0};
            fill_nodes(codegen, &cond_ll, node->node.while_.cond, ftype, stage, false);
            assert(cond_ll.length == 1);

            LL_IR_C_Node then = {
                .to_defer = arena_alloc(codegen->arena, sizeof *then.to_defer),
            };
            *then.to_defer = (LL_IR_C_Node){
                .to_defer = codegen->stmt_block->to_defer,
            };

            LLNode_ASTNode* curr = node->node.while_.block->stmts.head;
            LL_IR_C_Node* prev_block = codegen->stmt_block;
            codegen->stmt_block = &then;
            while (curr) {
                fill_nodes(codegen, &then, &curr->data, ftype, stage, false);
                curr = curr->next;
            }
            codegen->stmt_block = prev_block;

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_WHILE,
                .node.while_ = {
                    .cond = &cond_ll.head->data,
                    .then = then,
                },
            });
            break;
        }

        case ANT_FOREACH: {
            ResolvedType* rt = codegen->packages->types[node->id.val].type;
            assert(rt);
            assert(rt->kind == RTK_VOID);

            ResolvedType* iter_rt = codegen->packages->types[node->node.foreach.iterable->id.val].type;
            assert(iter_rt);
            assert(iter_rt->kind == RTK_STRUCT_DECL);
            assert(iter_rt == codegen->packages->range_literal_type);

            LL_IR_C_Node iter_ll = {0};
            fill_nodes(codegen, &iter_ll, node->node.foreach.iterable, ftype, stage, false);
            assert(iter_ll.length == 1);

            String iter_type = gen_type_resolved(codegen, codegen->packages->types[node->node.foreach.iterable->id.val].type);

            String var_range_name = unique_var_name(codegen->arena);

            ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                .type = ICNT_VAR_DECL,
                .node.var_decl = {
                    .type = iter_type,
                    .name = var_range_name,
                    .init = &iter_ll.head->data,
                },
            });

            String var_i_name = user_var_name(
                codegen->arena,
                node->node.foreach.var.lhs.name,
                rt->from_pkg
            );

            IR_C_Node* for_init = arena_alloc(codegen->arena, sizeof *for_init);
            IR_C_Node* for_cond = arena_alloc(codegen->arena, sizeof *for_cond);
            IR_C_Node* for_step = arena_alloc(codegen->arena, sizeof *for_step);

            {
                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_chars(&sb, "int64_t ");
                strbuf_append_str(&sb, var_i_name);
                strbuf_append_chars(&sb, " = ");
                strbuf_append_str(&sb, var_range_name);
                strbuf_append_chars(&sb, ".from");

                *for_init = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = strbuf_to_str(sb),
                };
            }

            {
                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_str(&sb, var_i_name);
                strbuf_append_chars(&sb, " < ");
                strbuf_append_str(&sb, var_range_name);
                strbuf_append_chars(&sb, ".to + ");
                strbuf_append_str(&sb, var_range_name);
                strbuf_append_chars(&sb, ".to_inclusive");

                *for_cond = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = strbuf_to_str(sb),
                };
            }

            {
                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_chars(&sb, "++");
                strbuf_append_str(&sb, var_i_name);

                *for_step = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = strbuf_to_str(sb),
                };
            }

            LL_IR_C_Node then = {
                .to_defer = arena_alloc(codegen->arena, sizeof *then.to_defer),
            };
            *then.to_defer = (LL_IR_C_Node){
                .to_defer = codegen->stmt_block->to_defer,
            };

            LLNode_ASTNode* curr = node->node.foreach.block->stmts.head;
            LL_IR_C_Node* prev_block = codegen->stmt_block;
            codegen->stmt_block = &then;
            while (curr) {
                fill_nodes(codegen, &then, &curr->data, ftype, stage, false);
                curr = curr->next;
            }
            codegen->stmt_block = prev_block;

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_FOR,
                .node.for_ = {
                    .init = for_init,
                    .cond = for_cond,
                    .step = for_step,
                    .then = then,
                },
            });
            break;
        }

        case ANT_RANGE: {
            LL_IR_C_Node fields = {0};

            fill_nodes(codegen, &fields, node->node.range.lhs, ftype, stage, false);
            assert(fields.length == 1);

            fill_nodes(codegen, &fields, node->node.range.rhs, ftype, stage, false);
            assert(fields.length == 2);

            ll_node_push(codegen->arena, &fields, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = node->node.range.inclusive ? c_str("true") : c_str("false"),
            });

            IR_C_Node* struct_init = arena_alloc(codegen->arena, sizeof *struct_init);
            *struct_init = (IR_C_Node){
                .type = ICNT_STRUCT_INIT,
                .node.struct_init = {
                    .type = gen_type_resolved(codegen, codegen->packages->types[node->id.val].type),
                    .fields = fields,
                },
            };

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RAW_WRAP,
                .node.raw_wrap = {
                    .pre = c_str("("),
                    .wrapped = struct_init,
                    .post = c_str(")"),
                },
            });

            break;
        }

        case ANT_IF: {
            LL_IR_C_Node cond_ll = {0};
            fill_nodes(codegen, &cond_ll, node->node.if_.cond, ftype, stage, false);
            assert(cond_ll.length == 1);

            LL_IR_C_Node then = {
                .to_defer = arena_alloc(codegen->arena, sizeof *then.to_defer),
            };
            *then.to_defer = (LL_IR_C_Node){
                .to_defer = codegen->stmt_block->to_defer,
            };

            LLNode_ASTNode* curr = node->node.if_.block->stmts.head;
            LL_IR_C_Node* prev_block = codegen->stmt_block;
            codegen->stmt_block = &then;
            while (curr) {
                fill_nodes(codegen, &then, &curr->data, ftype, stage, false);
                curr = curr->next;
            }
            codegen->stmt_block = prev_block;

            IR_C_Node* else_ = NULL;
            if (node->node.if_.else_) {
                assert(node->node.if_.else_->type == ANT_IF || node->node.if_.else_->type == ANT_STATEMENT_BLOCK);
                LL_IR_C_Node else_ll = {
                    .to_defer = arena_alloc(codegen->arena, sizeof *else_ll.to_defer),
                };
                *else_ll.to_defer = (LL_IR_C_Node){
                    .to_defer = codegen->stmt_block->to_defer,
                };

                prev_block = codegen->stmt_block;
                codegen->stmt_block = &else_ll;

                fill_nodes(codegen, &else_ll, node->node.if_.else_, ftype, stage, false);
                assert(else_ll.length == 1);
                codegen->stmt_block = prev_block;

                else_ = &else_ll.head->data;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_IF,
                .node.if_ = {
                    .cond = &cond_ll.head->data,
                    .then = then,
                    .else_ = else_,
                },
            });
            break;
        }

        case ANT_STATEMENT_BLOCK: {
            LL_IR_C_Node then = {
                .to_defer = arena_alloc(codegen->arena, sizeof *then.to_defer),
            };
            *then.to_defer = (LL_IR_C_Node){
                .to_defer = codegen->stmt_block->to_defer,
            };

            LL_IR_C_Node* prev_block = codegen->stmt_block;
            codegen->stmt_block = &then;

            LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
            while (curr) {
                fill_nodes(codegen, &then, &curr->data, ftype, stage, false);
                curr = curr->next;
            }
            codegen->stmt_block = prev_block;

            if (then.length == 0) {
                then.head = then.to_defer->head;
                then.tail = then.to_defer->tail;
                then.length = then.to_defer->length;

                then.to_defer->head = NULL;
                then.to_defer->tail = NULL;
                then.to_defer->length = 0;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_BLOCK,
                .node.block.nodes = then,
            });

            break;
        }

        case ANT_TUPLE: {
            LL_IR_C_Node exprs = {0};
            LLNode_ASTNode* curr = node->node.tuple.exprs.head;
            while (curr) {
                fill_nodes(codegen, &exprs, &curr->data, ftype, stage, false);
                curr = curr->next;
            }

            if (exprs.length == 1) {
                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_RAW_WRAP,
                    .node.raw_wrap = {
                        .pre = c_str("("),
                        .wrapped = &exprs.head->data,
                        .post = c_str(")"),
                    },
                });
            } else {
                assert(false); // TODO: how to handle tuples in c ?
            }

            break;
        }

        case ANT_CAST: {
            TypeInfo* ti = packages_type_by_type(codegen->packages, node->node.cast.type->id);
            assert(ti);

            ResolvedType* rt = ti->type;
            assert(rt);

            String type = gen_type_resolved(codegen, rt);

            StringBuffer sb = strbuf_create(codegen->arena);
            strbuf_append_chars(&sb, "((");
            strbuf_append_str(&sb, type);
            strbuf_append_char(&sb, ')');

            LL_IR_C_Node exprs = {0};
            fill_nodes(codegen, &exprs, node->node.cast.target, ftype, stage, false);
            assert(exprs.length == 1);

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RAW_WRAP,
                .node.raw_wrap = {
                    .pre = strbuf_to_str(sb),
                    .wrapped = &exprs.head->data,
                    .post = c_str(")"),
                },
            });
            break;
        }

        default: {
            printf("TODO: transform (%d) [", node->type);
                print_astnode(*node);
            printf("]\n");
            assert(false);
        }
    }
}

static LL_IR_C_Node transform_to_nodes(CodegenC* codegen, Package* package, FileType ftype) {
    assert(codegen);
    assert(package);

    codegen->current_package = package;

    LL_IR_C_Node nodes = {0};

    if (ftype == FT_C) {
        ll_node_push(codegen->arena, &nodes, (IR_C_Node){
            .type = ICNT_MACRO_INCLUDE,
            .node.include = {
                .is_local = true,
                .file = gen_header_file_path(codegen->arena, package->full_name),
            },
        });
    } else {
        if (ftype == FT_HEADER) {
            String define = arena_strcpy(codegen->arena, gen_header_file_path(codegen->arena, package->full_name));
            define.chars[define.length - 2] = '_';

            ll_node_push(codegen->arena, &nodes, (IR_C_Node){
                .type = ICNT_MACRO_IFNDEF,
                .node.ifndef.condition = define,
            });
            ll_node_push(codegen->arena, &nodes, (IR_C_Node){
                .type = ICNT_MACRO_DEFINE,
                .node.define.name = define,
            });
        }
        ll_node_push(codegen->arena, &nodes, (IR_C_Node){
            .type = ICNT_MACRO_INCLUDE,
            .node.include = {
                .is_local = false,
                .file = c_str("\"_.h\""),
            },
        });
    }

    codegen->needs_std = false;
    codegen->needs_std_io = false;
    codegen->needs_string_template = false;

    for (TransformStage stage = 0; stage < TS_COUNT; ++stage) {
        assert(package->ast->type == ANT_FILE_ROOT);
        LLNode_ASTNode* curr = package->ast->node.file_root.nodes.head;
        while (curr) {
            fill_nodes(codegen, &nodes, &curr->data, ftype, stage, true);
            curr = curr->next;
        }
    }

    if (codegen->needs_std) {
        LLNode_IR_C_Node* prev_head = nodes.head;
        LLNode_IR_C_Node* new_head = arena_alloc(codegen->arena, sizeof *new_head);
        *new_head = (LLNode_IR_C_Node){
            .data = {
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = true,
                    .file = c_str("std.h"),
                },
            },
            .next = prev_head,
        };
        nodes.head = new_head;
        nodes.length += 1;
    }

    if (codegen->needs_std_io) {
        LLNode_IR_C_Node* prev_head = nodes.head;
        LLNode_IR_C_Node* new_head = arena_alloc(codegen->arena, sizeof *new_head);
        *new_head = (LLNode_IR_C_Node){
            .data = {
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = true,
                    .file = c_str("std_io.h"),
                },
            },
            .next = prev_head,
        };
        nodes.head = new_head;
        nodes.length += 1;
    }

    if (codegen->needs_string_template) {
        assert(codegen->packages->string_template_type);
        assert(codegen->packages->string_template_type->from_pkg);

        // include header if not in main
        if (codegen->packages->string_template_type->from_pkg->full_name) {
            LLNode_IR_C_Node* prev_head = nodes.head;
            LLNode_IR_C_Node* new_head = arena_alloc(codegen->arena, sizeof *new_head);
            *new_head = (LLNode_IR_C_Node){
                .data = {
                    .type = ICNT_MACRO_INCLUDE,
                    .node.include = {
                        .is_local = true,
                        .file = gen_header_file_path(codegen->arena, codegen->packages->string_template_type->from_pkg->full_name),
                    },
                },
                .next = prev_head,
            };
            nodes.head = new_head;
            nodes.length += 1;
        } else {
            // if @string_template_literal in main, can only use string templates in main
            assert(ftype == FT_MAIN);
        }
    }

    if (ftype == FT_MAIN) {
        StringBuffer sb = strbuf_create(codegen->arena);
        strbuf_append_chars(&sb, "std_args = (std_Array_0){0};\n");
        strbuf_append_chars(&sb, "    if (argc > 0) {\n");
        strbuf_append_chars(&sb, "        std_args.length = argc;\n");
        strbuf_append_chars(&sb, "        std_args.data = calloc(std_args.length, sizeof *std_args.data);\n");
        strbuf_append_chars(&sb, "        for (size_t i = 0; i < argc; ++i) { std_args.data[i] = (std_String){ strlen(argv[i]), argv[i] }; }\n");
        strbuf_append_chars(&sb, "    }\n");

        LL_IR_C_Node main_statements = {0};
        ll_node_push(codegen->arena, &main_statements, (IR_C_Node){
            .type = ICNT_RAW,
            .node.raw.str = strbuf_to_str(sb),
        });
        ll_node_push(codegen->arena, &main_statements, (IR_C_Node){
            .type = ICNT_RAW,
            .node.raw.str = c_str("_main()"),
        });
        {
            IR_C_Node* expr_ret = arena_alloc(codegen->arena, sizeof *expr_ret);
            expr_ret->type = ICNT_RAW;
            expr_ret->node.raw.str = c_str("0");
            ll_node_push(codegen->arena, &main_statements, (IR_C_Node){
                .type = ICNT_RETURN,
                .node.return_.expr = expr_ret,
            });
        }

        Strings params = {0};
        params.length = 2;
        params.strings = arena_calloc(codegen->arena, 2, sizeof *params.strings);
        params.strings[0] = c_str("int argc");
        params.strings[1] = c_str("char** argv");

        ll_node_push(codegen->arena, &nodes, (IR_C_Node){
            .type = ICNT_FUNCTION_DECL,
            .node.function_decl = {
                .return_type = c_str("int"),
                .name = c_str("main"),
                .params = params,
                .statements = main_statements,
            },
        });
    } else if (ftype == FT_HEADER) {
        ll_node_push(codegen->arena, &nodes, (IR_C_Node){
            .type = ICNT_MACRO_ENDIF,
            .node.endif._ = NULL,
        });
    }

    return nodes;
}

static void append_ir_node(StringBuffer* sb, IR_C_Node* node, size_t indent) {
    switch (node->type) {
        case ICNT_RAW: {
            strbuf_append_str(sb, node->node.raw.str);
            break;
        }

        case ICNT_RAW_WRAP: {
            strbuf_append_str(sb, node->node.raw_wrap.pre);
            append_ir_node(sb, node->node.raw_wrap.wrapped, indent);
            strbuf_append_str(sb, node->node.raw_wrap.post);
            break;
        }

        case ICNT_BLOCK: {
            strbuf_append_chars(sb, "{\n");
            indent += 1;

            LL_IR_C_Node block = node->node.block.nodes;
            LLNode_IR_C_Node* curr = block.head;
            size_t idx = 0;
            bool is_defers = false;
            while (curr) {
                for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                append_ir_node(sb, &curr->data, indent);
                if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                    if (sb->chars[sb->length - 1] != '\n') {
                        strbuf_append_chars(sb, ";\n");
                    } else {
                        strbuf_append_char(sb, ';');
                    }
                }

                curr = curr->next;
                idx += 1;
                if (!curr || idx >= block.length) {
                    if (block.to_defer && !is_defers) {
                        is_defers = true;
                        block = *block.to_defer;
                        curr = block.head;
                        idx = 0;
                    } else {
                        break;
                    }
                }
            }

            indent -= 1;
            for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
            strbuf_append_chars(sb, "}\n");

            break;
        }

        case ICNT_MACRO_IFNDEF: {
            strbuf_append_chars(sb, "#ifndef ");
            strbuf_append_str(sb, node->node.ifndef.condition);
            break;
        }

        case ICNT_MACRO_ENDIF: {
            strbuf_append_chars(sb, "#endif");
            break;
        }

        case ICNT_MACRO_DEFINE: {
            strbuf_append_chars(sb, "#define ");
            strbuf_append_str(sb, node->node.define.name);

            if (node->node.define.maybe_value) {
                strbuf_append_char(sb, ' ');
                strbuf_append_str(sb, *node->node.define.maybe_value);
            }

            break;
        }

        case ICNT_MACRO_INCLUDE: {
            strbuf_append_chars(sb, "#include ");
            if (node->node.include.is_local) {
                strbuf_append_char(sb, '"');
            } else {
                // already in string
                // strbuf_append_char(sb, '<');
            }

            strbuf_append_str(sb, node->node.include.file);

            if (node->node.include.is_local) {
                strbuf_append_char(sb, '"');
            } else {
                // already in string
                // strbuf_append_char(sb, '>');
            }
            break;
        }

        case ICNT_ARRAY_INIT: {
            strbuf_append_char(sb, '{');
            for (size_t i = 0; i < node->node.array_init.elems_length; ++i) {
                if (node->node.array_init.indicies[i].type != ICNT_COUNT) {
                    strbuf_append_char(sb, '[');
                    append_ir_node(sb, node->node.array_init.indicies + i, indent);
                    strbuf_append_chars(sb, "] = ");
                }
                append_ir_node(sb, node->node.array_init.elems + i, indent);

                if (i + 1 < node->node.array_init.elems_length) {
                    strbuf_append_chars(sb, ", ");
                }
            }
            strbuf_append_char(sb, '}');
            break;
        }

        case ICNT_GET_FIELD: {
            append_ir_node(sb, node->node.get_field.root, indent);
            if (node->node.get_field.is_ptr) {
                strbuf_append_chars(sb, "->");
            } else {
                strbuf_append_char(sb, '.');
            }
            strbuf_append_str(sb, node->node.get_field.name);
            break;
        }

        case ICNT_SIZEOF_EXPR: {
            strbuf_append_chars(sb, "sizeof(");
            append_ir_node(sb, node->node.sizeof_expr.expr, indent);
            strbuf_append_char(sb, ')');
            break;
        }

        case ICNT_SIZEOF_TYPE: {
            strbuf_append_chars(sb, "sizeof(");
            strbuf_append_str(sb, node->node.sizeof_type.type);
            strbuf_append_char(sb, ')');
            break;
        }

        case ICNT_FUNCTION_CALL: {
            append_ir_node(sb, node->node.function_call.target, indent);
            strbuf_append_char(sb, '(');
            {
                LLNode_IR_C_Node* curr = node->node.function_call.args.head;
                while (curr) {
                    append_ir_node(sb, &curr->data, indent);
                    curr = curr->next;

                    if (curr) {
                        strbuf_append_chars(sb, ", ");
                    }
                }
            }
            strbuf_append_char(sb, ')');
            break;
        }

        case ICNT_RETURN: {
            if (node->node.return_.defer_block) {
                LL_IR_C_Node block = *node->node.return_.defer_block;
                LLNode_IR_C_Node* curr = node->node.return_.defer_block->head;
                size_t idx = 0;

                while (!curr || idx >= block.length) {
                    if (block.to_defer) {
                        block = *block.to_defer;
                        curr = block.head;
                        idx = 0;
                    } else {
                        break;
                    }
                }

                while (curr) {
                    append_ir_node(sb, &curr->data, indent);
                    if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                        if (sb->chars[sb->length - 1] != '\n') {
                            strbuf_append_chars(sb, ";\n");
                        } else {
                            strbuf_append_char(sb, ';');
                        }
                    }
                    curr = curr->next;
                    idx += 1;
                    bool should_break = false;
                    while (!curr || idx >= block.length) {
                        if (block.to_defer) {
                            block = *block.to_defer;
                            curr = block.head;
                            idx = 0;
                        } else {
                            should_break = true;
                            break;
                        }
                    }
                    for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                    if (should_break) {
                        break;
                    }
                }
            }

            strbuf_append_chars(sb, "return");
            if (node->node.return_.expr) {
                strbuf_append_char(sb, ' ');
                append_ir_node(sb, node->node.return_.expr, indent);
            }
            break;
        }

        case ICNT_BINARY_OP: {
            append_ir_node(sb, node->node.binary_op.lhs, indent);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.binary_op.op);
            strbuf_append_char(sb, ' ');
            append_ir_node(sb, node->node.binary_op.rhs, indent);
            break;
        }

        case ICNT_VAR_DECL: {
            strbuf_append_str(sb, node->node.var_decl.type);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.var_decl.name);

            if (node->node.var_decl.init) {
                strbuf_append_chars(sb, " = ");
                append_ir_node(sb, node->node.var_decl.init, indent);
            }
            
            break;
        }

        case ICNT_FUNCTION_HEADER_DECL: {
            strbuf_append_str(sb, node->node.function_header_decl.return_type);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.function_header_decl.name);
            strbuf_append_char(sb, '(');
            if (node->node.function_header_decl.params.length == 0) {
                strbuf_append_chars(sb, "void");
            } else {
                for (size_t i = 0; i < node->node.function_header_decl.params.length; ++i) {
                    strbuf_append_str(sb, node->node.function_header_decl.params.strings[i]);

                    if (i + 1 < node->node.function_header_decl.params.length) {
                        strbuf_append_chars(sb, ", ");
                    }
                }
            }
            strbuf_append_chars(sb, ");");
            break;
        }

        case ICNT_FUNCTION_DECL: {
            strbuf_append_str(sb, node->node.function_decl.return_type);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.function_decl.name);
            strbuf_append_char(sb, '(');
            if (node->node.function_decl.params.length == 0) {
                strbuf_append_chars(sb, "void");
            } else {
                for (size_t i = 0; i < node->node.function_decl.params.length; ++i) {
                    strbuf_append_str(sb, node->node.function_decl.params.strings[i]);

                    if (i + 1 < node->node.function_decl.params.length) {
                        strbuf_append_chars(sb, ", ");
                    }
                }
            }
            strbuf_append_chars(sb, ") {\n");
            {
                LL_IR_C_Node block = node->node.function_decl.statements;
                LLNode_IR_C_Node* curr = block.head;
                size_t idx = 0;
                indent += 1;
                while (curr) {
                    for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                    append_ir_node(sb, &curr->data, indent);
                    if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                        if (sb->chars[sb->length - 1] != '\n') {
                            strbuf_append_chars(sb, ";\n");
                        } else {
                            strbuf_append_char(sb, ';');
                        }
                    }
                    curr = curr->next;
                    idx += 1;
                    while (!curr || idx >= block.length) {
                        if (block.to_defer) {
                            block = *block.to_defer;
                            curr = block.head;
                            idx = 0;
                        } else {
                            break;
                        }
                    }
                }
                indent -= 1;
            }
            strbuf_append_char(sb, '}');
            break;
        }

        case ICNT_STRUCT_DECL: {
            strbuf_append_chars(sb, "typedef struct ");
            strbuf_append_str(sb, node->node.struct_decl.name);
            strbuf_append_chars(sb, " {\n");
            indent += 1;
            for (size_t i = 0; i < node->node.struct_decl.fields.length; ++i) {
                for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                strbuf_append_str(sb, node->node.struct_decl.fields.strings[i]);
                strbuf_append_chars(sb, ";\n");
            }
            indent -= 1;
            strbuf_append_chars(sb, "} ");
            strbuf_append_str(sb, node->node.struct_decl.name);
            strbuf_append_char(sb, ';');
            break;
        }

        case ICNT_TYPEDEF_DECL: {
            strbuf_append_chars(sb, "typedef ");
            strbuf_append_str(sb, node->node.typedef_decl.type);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.typedef_decl.name);
            strbuf_append_char(sb, ';');
            break;
        }

        case ICNT_INDEX: {
            append_ir_node(sb, node->node.index.root, indent);
            strbuf_append_char(sb, '[');
            append_ir_node(sb, node->node.index.value, indent);
            strbuf_append_char(sb, ']');
            break;
        }

        case ICNT_STRUCT_INIT: {
            strbuf_append_char(sb, '(');
            strbuf_append_str(sb, node->node.struct_init.type);
            strbuf_append_chars(sb, "){ ");
            LLNode_IR_C_Node* curr = node->node.struct_init.fields.head;
            while (curr) {
                append_ir_node(sb, &curr->data, indent);
                curr = curr->next;
                if (curr) {
                    strbuf_append_chars(sb, ", ");
                }
            }
            strbuf_append_chars(sb, " }");
            break;
        }

        case ICNT_IF: {
            strbuf_append_chars(sb, "if (");
            append_ir_node(sb, node->node.if_.cond, indent);
            strbuf_append_chars(sb, ") {\n");
            LL_IR_C_Node block = node->node.if_.then;
            LLNode_IR_C_Node* curr = block.head;
            size_t idx = 0;
            indent += 1;
            bool is_defers = false;
            while (curr) {
                for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                append_ir_node(sb, &curr->data, indent);
                if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                    if (sb->chars[sb->length - 1] != '\n') {
                        strbuf_append_chars(sb, ";\n");
                    } else {
                        strbuf_append_char(sb, ';');
                    }
                }
                curr = curr->next;
                idx += 1;
                if (!curr || idx >= block.length) {
                    if (block.to_defer && !is_defers) {
                        is_defers = true;
                        block = *block.to_defer;
                        curr = block.head;
                        idx = 0;
                    } else {
                        break;
                    }
                }
            }
            indent -= 1;
            for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
            strbuf_append_chars(sb, "}");
            if (node->node.if_.else_) {
                strbuf_append_chars(sb, " else ");
                if (node->node.if_.else_->type == ICNT_IF) {
                    append_ir_node(sb, node->node.if_.else_, indent);
                } else {
                    strbuf_append_chars(sb, "{\n");
                    indent += 1;
                    append_ir_node(sb, node->node.if_.else_, indent);
                    indent -= 1;
                    for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                    strbuf_append_chars(sb, "}");
                }
            }
            strbuf_append_char(sb, '\n');
            break;
        }

        case ICNT_WHILE: {
            strbuf_append_chars(sb, "while (");
            append_ir_node(sb, node->node.while_.cond, indent);
            strbuf_append_chars(sb, ") {\n");
            LL_IR_C_Node block = node->node.while_.then;
            LLNode_IR_C_Node* curr = block.head;
            size_t idx = 0;
            indent += 1;
            bool is_defers = false;
            while (curr) {
                for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                append_ir_node(sb, &curr->data, indent);

                if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                    if (sb->chars[sb->length - 1] != '\n') {
                        strbuf_append_chars(sb, ";\n");
                    } else {
                        strbuf_append_char(sb, ';');
                    }
                }
                curr = curr->next;
                idx += 1;
                if (!curr || idx >= block.length) {
                    if (block.to_defer && !is_defers) {
                        is_defers = true;
                        block = *block.to_defer;
                        curr = block.head;
                        idx = 0;
                    } else {
                        break;
                    }
                }
            }
            indent -= 1;
            for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
            strbuf_append_chars(sb, "}");
            break;
        }

        case ICNT_FOR: {
            strbuf_append_chars(sb, "for (");
            append_ir_node(sb, node->node.for_.init, indent);
            strbuf_append_chars(sb, "; ");
            append_ir_node(sb, node->node.for_.cond, indent);
            strbuf_append_chars(sb, "; ");
            append_ir_node(sb, node->node.for_.step, indent);
            strbuf_append_chars(sb, ") {\n");
            LL_IR_C_Node block = node->node.for_.then;
            LLNode_IR_C_Node* curr = block.head;
            size_t idx = 0;
            indent += 1;
            bool is_defers = false;
            while (curr) {
                for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
                append_ir_node(sb, &curr->data, indent);

                if (sb->chars[sb->length - 1] != ';' && sb->chars[sb->length - 1] != '\n') {
                    if (sb->chars[sb->length - 1] != '\n') {
                        strbuf_append_chars(sb, ";\n");
                    } else {
                        strbuf_append_char(sb, ';');
                    }
                }
                curr = curr->next;
                idx += 1;
                if (!curr || idx >= block.length) {
                    if (block.to_defer && !is_defers) {
                        is_defers = true;
                        block = *block.to_defer;
                        curr = block.head;
                        idx = 0;
                    } else {
                        break;
                    }
                }
            }
            indent -= 1;
            for (size_t idnt = 0; idnt < indent; ++idnt) { strbuf_append_chars(sb, "    "); }
            strbuf_append_chars(sb, "}");
            break;
        }

        default: printf("%s\n---\nTODO: gen [%d]\n", strbuf_to_str(*sb).chars, node->type); assert(false);
    }
}

static void append_ir_file(StringBuffer* sb, IR_C_File* file) {
    LLNode_IR_C_Node* curr = file->nodes.head;
    while (curr) {
        bool is_macro = ICNT_MACRO_IFNDEF <= curr->data.type && curr->data.type <= ICNT_MACRO_ENDIF;
        append_ir_node(sb, &curr->data, 0);
        if (!is_macro && sb->chars[sb->length - 1] != ';') {
            strbuf_append_char(sb, ';');
        }
        strbuf_append_char(sb, '\n');
        curr = curr->next;
    }
}

CodegenC codegen_c_create(Arena* arena, Packages* packages) {
    IR_C_File* files = arena_calloc(arena, packages->count * 2, sizeof *files);

    return (CodegenC){
        .arena = arena,
        .packages = packages,
        .ir = {
            .files_length = 0,
            .files = files,
        },

        .current_package = NULL,
        .generic_map = NULL,
        .stmt_block = NULL,

        .seen_file_separator = false,
        .prev_block = BT_OTHER,
    };
}

GeneratedFiles generate_c_code(CodegenC* codegen) {    
    for (size_t bi = 0; bi < codegen->packages->lookup_length; ++bi) {
        ArrayList_Package* bucket = codegen->packages->lookup_buckets + bi;

        for (size_t pi = 0; pi < bucket->length; ++pi) {
            Package* package = bucket->array + pi;
            printf("Transforming package \"%s\"...\n",
                package->full_name ? package_path_to_str(codegen->arena, package->full_name).chars : "<main>"
            );

            DirectiveCHeader* c_header = get_c_header(package);
            if (c_header) {
                continue;
            }
            FileType c_ftype = package->is_entry ? FT_MAIN : FT_C;

            IR_C_File* file = codegen->ir.files + codegen->ir.files_length;
            codegen->ir.files_length += 1;

            file->name = gen_c_file_path(codegen->arena, package->full_name);
            file->nodes = transform_to_nodes(codegen, package, c_ftype);

            if (c_ftype == FT_C) {
                file = codegen->ir.files + codegen->ir.files_length;
                codegen->ir.files_length += 1;

                file->name = gen_header_file_path(codegen->arena, package->full_name);
                file->nodes = transform_to_nodes(codegen, package, FT_HEADER);
            }
        }
    }

    {
        LL_IR_C_Node common = {0};
        {
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_IFNDEF,
                .node.ifndef.condition = c_str("_h"),
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_DEFINE,
                .node.define.name = c_str("_h"),
            });

            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = false,
                    .file = c_str("<assert.h>"),
                },
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = false,
                    .file = c_str("<stdlib.h>"),
                },
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = false,
                    .file = c_str("<stdbool.h>"),
                },
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = false,
                    .file = c_str("<stdint.h>"),
                },
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_INCLUDE,
                .node.include = {
                    .is_local = false,
                    .file = c_str("<string.h>"),
                },
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = c_str("typedef struct { char _; } char_;\n"),
            });
            ll_node_push(codegen->arena, &common, (IR_C_Node){
                .type = ICNT_MACRO_ENDIF,
                .node.endif._ = NULL,
            });
        }
        codegen->ir.files[codegen->ir.files_length++] = (IR_C_File){
            .name = c_str("_.h"),
            .nodes = common,
        };
    }

    GeneratedFiles files = {
        .length = codegen->ir.files_length,
        .files = arena_calloc(codegen->arena, codegen->ir.files_length, sizeof(GeneratedFile)),
    };
    StringBuffer sb = strbuf_create(codegen->arena);
    for (size_t i = 0; i < codegen->ir.files_length; ++i) {
        GeneratedFile* file = files.files + i;
        file->filepath = codegen->ir.files[i].name;

        append_ir_file(&sb, codegen->ir.files + i);

        file->content = strbuf_to_strcpy(sb);
        strbuf_reset(&sb);
    }

    return files;
}
