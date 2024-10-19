#include <stdio.h>
#include <string.h>

#include "./ast.h"
#include "./codegen_c.h"
#include "./package.h"
#include "./resolved_type.h"

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

static String* get_mapped_generic(GenericImplMap* map, String generic) {
    if (!map) {
        return NULL;
    }

    for (size_t i = 0; i < map->length; ++i) {
        if (str_eq(map->generic_names[i], generic)) {
            return map->mapped_types + i;
        }
    }

    return get_mapped_generic(map->parent, generic);
}

static void ll_node_push(Arena* const arena, LL_IR_C_Node* const ll, IR_C_Node const node) {
    LLNode_IR_C_Node* const llnode = arena_alloc(arena, sizeof *llnode);
    llnode->data = node;
    llnode->next = NULL;
    
    if (ll->head == NULL) {
        ll->head = llnode;
        ll->tail = ll->head;
    } else {
        ll->tail->next = llnode;
        ll->tail = llnode;
    }

    ll->length += 1;
}

static DirectiveCHeader* get_c_header(Package* package) {
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

static void _append_type(CodegenC* codegen, StringBuffer* sb, Type type) {
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

                case TBI_INT: {
                    strbuf_append_chars(sb, "int64_t");
                    break;
                }

                case TBI_UINT: {
                    strbuf_append_chars(sb, "size_t");
                    break;
                }

                case TBI_CHAR: {
                    strbuf_append_chars(sb, "char");
                    break;
                }

                default: printf("TODO: built-in [%d]\n", type.type.built_in); assert(false);
            }
            break;
        }

        case TK_STATIC_PATH: {
            if (
                type.type.static_path.generic_types.length == 0
                && type.type.static_path.impl_version == 0
                && !type.type.static_path.path->child
            ) {
                String* mapped = get_mapped_generic(codegen->generic_map, type.type.static_path.path->name);
                if (mapped) {
                    assert(false);
                    strbuf_append_str(sb, *mapped);
                    break;
                } else {
                    // printf("searched map, none found\n");
                }
            }
            // printf("%lu\n", type.type.static_path.generic_types.length);
            // printf("%lu\n", type.type.static_path.impl_version);
            // printf("%s\n", type.type.static_path.path->child ? "has child" : "no child");
            // printf("%s\n", arena_strcpy(codegen->arena, type.type.static_path.path->name).chars);

            StaticPath* curr = type.type.static_path.path;
            while (curr->child) {
                curr = curr->child;
            }

            strbuf_append_str(sb, curr->name);

            if (type.type.static_path.generic_types.length > 0) {
                strbuf_append_chars(sb, "__");
                strbuf_append_uint(sb, type.type.static_path.impl_version);
            }

            break;
        }

        case TK_TUPLE: {
            assert(false);
            break;
        }

        case TK_POINTER: {
            _append_type(codegen, sb, *type.type.mut_ptr.of);
            strbuf_append_chars(sb, " const*"); // does this always work?
            break;
        }

        case TK_MUT_POINTER: {
            _append_type(codegen, sb, *type.type.mut_ptr.of);
            strbuf_append_char(sb, '*');
            break;
        }

        case TK_ARRAY: {
            assert(false);
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
    switch (type->kind) {
        case RTK_NAMESPACE: assert(false);

        case RTK_VOID: strbuf_append_chars(sb, "void"); break;
        case RTK_BOOL: strbuf_append_chars(sb, "bool"); break;
        case RTK_INT: strbuf_append_chars(sb, "int64_t"); break;
        case RTK_UINT: strbuf_append_chars(sb, "size_t"); break;
        case RTK_CHAR: strbuf_append_chars(sb, "char"); break;

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

        case RTK_FUNCTION: {
            assert(false); // TODO ?
            break;
        }

        case RTK_STRUCT_REF: {
            strbuf_append_str(sb, type->type.struct_ref.decl.name);
            if (type->type.struct_ref.generic_args.length > 0) {
                strbuf_append_chars(sb, "__");
                strbuf_append_uint(sb, type->type.struct_ref.impl_version);
            }
            break;
        }

        case RTK_GENERIC: {
            String* mapped = get_mapped_generic(codegen->generic_map, type->type.generic.name);
            assert(mapped);
            String* next = get_mapped_generic(codegen->generic_map, *mapped);
            while (next) {
                mapped = next;
                next = get_mapped_generic(codegen->generic_map, *mapped);
            }
            strbuf_append_str(sb, *mapped);
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

static String gen_type(CodegenC* codegen, Type type) {
    StringBuffer sb = strbuf_create(codegen->arena);
    _append_type(codegen, &sb, type);
    return strbuf_to_str(sb);
}

static void fill_nodes(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype, TransformStage stage, bool root_call);

static void _fn_header_decl(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype) {
    ResolvedType* type = codegen->packages->types[node->id.val].type;

    Strings params = {
        .length = 0,
        .strings = arena_calloc(codegen->arena, node->node.function_header_decl.params.length, sizeof(String)),
    };
    {
        StringBuffer sb = strbuf_create(codegen->arena);

        LLNode_FnParam* curr = node->node.function_header_decl.params.head;
        while (curr) {
            strbuf_append_str(&sb, gen_type(codegen, curr->data.type));
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
            .return_type = gen_type(codegen, node->node.function_header_decl.return_type),
            .name = name,
            .params = params,
        },
    });
}

static void fill_nodes(CodegenC* codegen, LL_IR_C_Node* c_nodes, ASTNode* node, FileType ftype, TransformStage stage, bool root_call) {
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
                    String value = node->node.literal.value.lit_str;

                    ResolvedType* str_type = codegen->packages->string_literal_type;
                    assert(str_type);
                    assert(str_type->src->node.struct_decl.maybe_name);

                    StringBuffer sb = strbuf_create(codegen->arena);
                    strbuf_append_chars(&sb, "((");
                    {
                        String str_struct_name = *str_type->src->node.struct_decl.maybe_name;
                        strbuf_append_str(&sb, str_struct_name);
                    }
                    strbuf_append_chars(&sb, "){");
                    strbuf_append_uint(&sb, value.length);
                    strbuf_append_chars(&sb, ", \"");
                    strbuf_append_str(&sb, value);
                    strbuf_append_chars(&sb, "\"})");

                    ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                        .type = ICNT_RAW,
                        .node.raw.str = strbuf_to_str(sb),
                    });

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

                default: assert(false);
            }

            break;
        }

        case ANT_TEMPLATE_STRING: {
            // gen: StringBuffer sb = strbuf_default();
            {
                IR_C_Node* sb_init_target = arena_alloc(codegen->arena, sizeof *sb_init_target);
                *sb_init_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("strbuf_default"),
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
                        .type = c_str("StringBuffer"), // TODO: codegen->string_template_type
                        .name = c_str("sb"),
                        .init = sb_init,
                    },
                });
            }

            // gen: strbuf_append_chars(&sb, "..."); // first string part
            {
                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                *append_chars_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("strbuf_append_chars"),
                };

                LL_IR_C_Node append_chars_args = {0};
                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("&sb"),
                });

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_char(&sb, '"');
                strbuf_append_str(&sb, (String){
                    .length = node->node.template_string.str_parts.array[0].length - 2,
                    .chars = node->node.template_string.str_parts.array[0].chars + 1,
                });
                strbuf_append_char(&sb, '"');
                String str_lit = strbuf_to_str(sb);
                
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

            size_t i = 1;
            LLNode_ASTNode* curr = node->node.template_string.template_expr_parts.head;
            while (curr) {
                switch (codegen->packages->types[curr->data.id.val].type->kind) {
                    case RTK_INT: {
                        IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                        *append_chars_target = (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("strbuf_append_int"),
                        };

                        LL_IR_C_Node append_chars_args = {0};
                        ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("&sb"),
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

                    case RTK_CHAR: {
                        IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                        *append_chars_target = (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("strbuf_append_char"),
                        };

                        LL_IR_C_Node append_chars_args = {0};
                        ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("&sb"),
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
                            .node.raw.str = c_str("strbuf_append_str"),
                        };

                        LL_IR_C_Node append_chars_args = {0};
                        ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("&sb"),
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

                    default: printf("TODO: string template RTK_%d\n", codegen->packages->types[curr->data.id.val].type->kind); assert(false);
                }

                IR_C_Node* append_chars_target = arena_alloc(codegen->arena, sizeof *append_chars_target);
                *append_chars_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("strbuf_append_chars"),
                };

                LL_IR_C_Node append_chars_args = {0};
                ll_node_push(codegen->arena, &append_chars_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("&sb"),
                });

                StringBuffer sb = strbuf_create(codegen->arena);
                strbuf_append_char(&sb, '"');
                strbuf_append_str(&sb, (String){
                    .length = node->node.template_string.str_parts.array[i].length - 2,
                    .chars = node->node.template_string.str_parts.array[i].chars + 1,
                });
                strbuf_append_char(&sb, '"');
                String str_lit = strbuf_to_str(sb);
                
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
 
                i += 1;
                curr = curr->next;
            }

            // gen: strbuf_to_str(sb);
            {
                IR_C_Node* to_str_target = arena_alloc(codegen->arena, sizeof *to_str_target);
                *to_str_target = (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("strbuf_as_str"),
                };

                LL_IR_C_Node to_str_args = {0};
                ll_node_push(codegen->arena, &to_str_args, (IR_C_Node){
                    .type = ICNT_RAW,
                    .node.raw.str = c_str("sb"),
                });

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_FUNCTION_CALL,
                    .node.function_call = {
                        .target = to_str_target,
                        .args = to_str_args,
                    },
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
            while (child->child) {
                child = child->child;
            }

            ResolvedType* type = codegen->packages->types[node->id.val].type;

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = child->name,
            });
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
                case UO_PTR_REF: {
                    op = c_str("&");
                    // break;
                    if (node->node.unary_op.right->type == ANT_LITERAL) {
                        TypeBuiltIn built_in;
                        ResolvedTypeKind rtk;
                        switch (node->node.unary_op.right->node.literal.kind) {
                            case LK_BOOL: built_in = TBI_BOOL; rtk = RTK_BOOL; break;
                            case LK_CHAR: built_in = TBI_CHAR; rtk = RTK_CHAR; break;
                            case LK_INT: built_in = TBI_INT; rtk = RTK_INT; break;
                            default: assert(false);
                        }
                        LL_IR_C_Node vardecl_ll = {0};
                        Type typ = {
                            .id = { 0 },
                            .kind = TK_BUILT_IN,
                            .type.built_in = built_in,
                            .directives = {0},
                        };
                        ASTNode vardecl = {
                            .id = { codegen->packages->types_length - 1 },
                            .type = ANT_VAR_DECL,
                            .node.var_decl = {
                                .is_static = false,
                                .type_or_let = {
                                    .is_let = false,
                                    .is_mut = false,
                                    .maybe_type = &typ,
                                },
                                .lhs = {
                                    .count = 1,
                                    .type = VDLT_NAME,
                                    .lhs.name = c_str("__tmp"),
                                },
                                .initializer = node->node.unary_op.right,
                            },
                            .directives = {0},
                        };
                        ResolvedType rt = {
                            .kind = rtk,
                        };
                        codegen->packages->types[vardecl.id.val].type = &rt;
                        fill_nodes(codegen, &vardecl_ll, &vardecl, ftype, stage, false);
                        assert(vardecl_ll.length == 1);

                        ll_node_push(codegen->arena, codegen->stmt_block, vardecl_ll.head->data);
                        ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                            .type = ICNT_RAW,
                            .node.raw.str = c_str("&__tmp"),
                        });
                        already_done = true;
                    }
                    break;
                }

                default: printf("TODO: unary op [%d]\n", node->node.unary_op.op); assert(false);
            }

            if (!already_done) {
                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_UNARY,
                    .node.unary = {
                        .op = op,
                        .expr = expr,
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
                String type = gen_type(codegen, *node->node.sizeof_.sizeof_.type);

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
                .node.return_.expr = expr,
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
                case AO_ASSIGN: {
                    op = c_str("=");
                    break;
                }

                case AO_PLUS_ASSIGN: {
                    op = c_str("+=");
                    break;
                }

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
                case BO_ADD: op = c_str("+"); break;
                case BO_SUBTRACT: op = c_str("-"); break;
                case BO_MULTIPLY: op = c_str("*"); break;
                case BO_DIVIDE: op = c_str("/"); break;

                case BO_GREATER_OR_EQ: op = c_str(">="); break;
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
            assert(rt);

            String type;
            // if (node->node.var_decl.type_or_let.is_let) {
                type = gen_type_resolved(codegen, rt);
            // } else {
            //     type = gen_type(codegen, *node->node.var_decl.type_or_let.maybe_type);
            // }

            assert(node->node.var_decl.lhs.type == VDLT_NAME);
            String name = node->node.var_decl.lhs.lhs.name;

            if (rt->kind == RTK_ARRAY) {
                StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 2);
                strbuf_append_str(&sb, name);
                strbuf_append_chars(&sb, "[]");
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

            if (stage == TS_FN_HEADERS) {
                ResolvedType* type = codegen->packages->types[node->id.val].type;
                Strings params = {
                    .length = 0,
                    .strings = arena_calloc(codegen->arena, node->node.function_decl.header.params.length, sizeof(String)),
                };
                {
                    StringBuffer sb = strbuf_create(codegen->arena);

                    LLNode_FnParam* curr = node->node.function_decl.header.params.head;
                    while (curr) {
                        strbuf_append_str(&sb, gen_type(codegen, curr->data.type));
                        strbuf_append_char(&sb, ' ');
                        strbuf_append_str(&sb, curr->data.name);

                        params.strings[params.length++] = strbuf_to_strcpy(sb);
                        strbuf_reset(&sb);

                        curr = curr->next;
                    }
                }

                String return_type = gen_type(codegen, node->node.function_decl.header.return_type);

                String name;
                if (node->node.function_decl.header.is_main) {
                    name = c_str("_main");
                } else {
                    name = node->node.function_decl.header.name;
                }

                ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                    .type = ICNT_FUNCTION_HEADER_DECL,
                    .node.function_decl = {
                        .return_type = return_type,
                        .name = name,
                        .params = params,
                    },
                });
                break;
            }

            if (ftype == FT_HEADER) {
                break;
            }

            ResolvedType* type = codegen->packages->types[node->id.val].type;
            Strings params = {
                .length = 0,
                .strings = arena_calloc(codegen->arena, node->node.function_decl.header.params.length, sizeof(String)),
            };
            {
                StringBuffer sb = strbuf_create(codegen->arena);

                LLNode_FnParam* curr = node->node.function_decl.header.params.head;
                while (curr) {
                    strbuf_append_str(&sb, gen_type(codegen, curr->data.type));
                    strbuf_append_char(&sb, ' ');
                    strbuf_append_str(&sb, curr->data.name);

                    params.strings[params.length++] = strbuf_to_strcpy(sb);
                    strbuf_reset(&sb);

                    curr = curr->next;
                }
            }

            LL_IR_C_Node statements = {0};
            codegen->stmt_block = &statements;
            {
                LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
                while (curr) {
                    fill_nodes(codegen, &statements, &curr->data, ftype, stage, false);
                    curr = curr->next;
                }
            }
            codegen->stmt_block = NULL;

            String return_type = gen_type(codegen, node->node.function_decl.header.return_type);

            String name;
            if (node->node.function_decl.header.is_main) {
                name = c_str("_main");
            } else {
                name = node->node.function_decl.header.name;
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
            break;
        }

        case ANT_STRUCT_DECL: {
            if (root_call && (ftype == FT_C || stage != TS_TYPES)) {
                break;
            }
            assert(node->node.struct_decl.maybe_name);
            // printf("CODEGEN: %s [%lu]\n", arena_strcpy(codegen->arena, *node->node.struct_decl.maybe_name).chars, node->node.struct_decl.generic_impls.length);

            ResolvedType* type = codegen->packages->types[node->id.val].type;

            size_t versions = node->node.struct_decl.generic_impls.length;
            if (versions == 0) {
                versions = 1;
            }

            if (node->node.struct_decl.generic_params.length == 0) {
                assert(versions == 1);
            }

            GenericImplMap* root_map = codegen->generic_map;

            for (size_t version = 0; version < versions; ++version) {
                LL_Type generic_impl_types = node->node.struct_decl.generic_impls.array[version];

                GenericImplMap map = {
                    .parent = root_map,
                    .length = node->node.struct_decl.generic_params.length,
                    .generic_names = arena_calloc(codegen->arena, node->node.struct_decl.generic_params.length, sizeof(String)),
                    .mapped_types = arena_calloc(codegen->arena, node->node.struct_decl.generic_params.length, sizeof(String)),
                };
                {
                    LLNode_Type* curr = generic_impl_types.head;
                    for (size_t i = 0; curr && i < node->node.struct_decl.generic_params.length; ++i) {
                        map.generic_names[i] = node->node.struct_decl.generic_params.array[i];
                        map.mapped_types[i] = gen_type(codegen, curr->data);
                        curr = curr->next;
                    }
                }
                codegen->generic_map = &map;

                Strings fields = {
                    .length = 0,
                    .strings = arena_calloc(codegen->arena, node->node.struct_decl.fields.length, sizeof(String)),
                };
                {
                    StringBuffer sb = strbuf_create(codegen->arena);

                    size_t i = 0;
                    LLNode_StructField* curr = node->node.struct_decl.fields.head;
                    while (curr) {
                        strbuf_append_str(&sb, gen_type_resolved(codegen, type->type.struct_decl.fields[i].type));
                        strbuf_append_char(&sb, ' ');
                        strbuf_append_str(&sb, curr->data.name);

                        fields.strings[fields.length++] = strbuf_to_strcpy(sb);
                        strbuf_reset(&sb);

                        i += 1;
                        curr = curr->next;
                    }
                }

                String name = *node->node.struct_decl.maybe_name;

                if (node->node.struct_decl.generic_params.length > 0) {
                    StringBuffer sb = strbuf_create_with_capacity(codegen->arena, name.length + 3);
                    strbuf_append_str(&sb, name);
                    strbuf_append_chars(&sb, "__");
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
            }
            break;
        }

        case ANT_CRASH: {
            ll_node_push(codegen->arena, codegen->stmt_block, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = c_str("assert(false)"),
            });
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
                        .post = c_str(", "),
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

            LL_IR_C_Node then = {0};
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

        case ANT_IF: {
            LL_IR_C_Node cond_ll = {0};
            fill_nodes(codegen, &cond_ll, node->node.if_.cond, ftype, stage, false);
            assert(cond_ll.length == 1);

            LL_IR_C_Node then = {0};
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
                LL_IR_C_Node else_ll = {0};
                fill_nodes(codegen, &else_ll, node->node.if_.else_, ftype, stage, false);
                assert(else_ll.length == 1);

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
            LL_IR_C_Node stmts = {0};
            LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
            while (curr) {
                fill_nodes(codegen, &stmts, &curr->data, ftype, stage, false);
                curr = curr->next;
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_MANY,
                .node.many.nodes = stmts,
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

    for (TransformStage stage = 0; stage < TS_COUNT; ++stage) {
        assert(package->ast->type == ANT_FILE_ROOT);
        LLNode_ASTNode* curr = package->ast->node.file_root.nodes.head;
        while (curr) {
            fill_nodes(codegen, &nodes, &curr->data, ftype, stage, true);
            curr = curr->next;
        }
    }

    if (ftype == FT_MAIN) {
        LL_IR_C_Node main_statements = {0};
        {
            ll_node_push(codegen->arena, &main_statements, (IR_C_Node){
                .type = ICNT_RAW,
                .node.raw.str = c_str("_main()"),
            });
        }
        {
            IR_C_Node* expr_ret = arena_alloc(codegen->arena, sizeof *expr_ret);
            expr_ret->type = ICNT_RAW;
            expr_ret->node.raw.str = c_str("0");
            ll_node_push(codegen->arena, &main_statements, (IR_C_Node){
                .type = ICNT_RETURN,
                .node.return_.expr = expr_ret,
            });
        }

        ll_node_push(codegen->arena, &nodes, (IR_C_Node){
            .type = ICNT_FUNCTION_DECL,
            .node.function_decl = {
                .return_type = c_str("int"),
                .name = c_str("main"),
                .params = {0},
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

static void append_ir_node(StringBuffer* sb, IR_C_Node* node) {
    switch (node->type) {
        case ICNT_RAW: {
            strbuf_append_str(sb, node->node.raw.str);
            break;
        }

        case ICNT_RAW_WRAP: {
            strbuf_append_str(sb, node->node.raw_wrap.pre);
            append_ir_node(sb, node->node.raw_wrap.wrapped);
            strbuf_append_str(sb, node->node.raw_wrap.post);
            break;
        }

        case ICNT_MANY: {
            LLNode_IR_C_Node* curr = node->node.many.nodes.head;
            while (curr) {
                append_ir_node(sb, &curr->data);
                curr = curr->next;
                strbuf_append_chars(sb, ";\n");
            }
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
                    append_ir_node(sb, node->node.array_init.indicies + i);
                    strbuf_append_chars(sb, "] = ");
                }
                append_ir_node(sb, node->node.array_init.elems + i);

                if (i + 1 < node->node.array_init.elems_length) {
                    strbuf_append_chars(sb, ", ");
                }
            }
            strbuf_append_char(sb, '}');
            break;
        }

        case ICNT_UNARY: {
            strbuf_append_str(sb, node->node.unary.op);
            append_ir_node(sb, node->node.unary.expr);
            break;
        }

        case ICNT_GET_FIELD: {
            append_ir_node(sb, node->node.get_field.root);
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
            append_ir_node(sb, node->node.sizeof_expr.expr);
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
            append_ir_node(sb, node->node.function_call.target);
            strbuf_append_char(sb, '(');
            {
                LLNode_IR_C_Node* curr = node->node.function_call.args.head;
                while (curr) {
                    append_ir_node(sb, &curr->data);
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
            strbuf_append_chars(sb, "return");
            if (node->node.return_.expr) {
                strbuf_append_char(sb, ' ');
                append_ir_node(sb, node->node.return_.expr);
            }
            break;
        }

        case ICNT_BINARY_OP: {
            append_ir_node(sb, node->node.binary_op.lhs);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.binary_op.op);
            strbuf_append_char(sb, ' ');
            append_ir_node(sb, node->node.binary_op.rhs);
            break;
        }

        case ICNT_VAR_DECL: {
            strbuf_append_str(sb, node->node.var_decl.type);
            strbuf_append_char(sb, ' ');
            strbuf_append_str(sb, node->node.var_decl.name);

            if (node->node.var_decl.init) {
                strbuf_append_chars(sb, " = ");
                append_ir_node(sb, node->node.var_decl.init);
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
                LLNode_IR_C_Node* curr = node->node.function_decl.statements.head;
                while (curr) {
                    strbuf_append_chars(sb, "    ");
                    append_ir_node(sb, &curr->data);
                    if (sb->chars[sb->length - 1] != ';') {
                        strbuf_append_char(sb, ';');
                    }
                    strbuf_append_char(sb, '\n');
                    curr = curr->next;
                }
            }
            strbuf_append_char(sb, '}');
            break;
        }

        case ICNT_STRUCT_DECL: {
            strbuf_append_chars(sb, "typedef struct {\n");
            for (size_t i = 0; i < node->node.struct_decl.fields.length; ++i) {
                strbuf_append_chars(sb, "    ");
                strbuf_append_str(sb, node->node.struct_decl.fields.strings[i]);
                strbuf_append_chars(sb, ";\n");
            }
            strbuf_append_chars(sb, "} ");
            strbuf_append_str(sb, node->node.struct_decl.name);
            strbuf_append_char(sb, ';');
            break;
        }

        case ICNT_INDEX: {
            append_ir_node(sb, node->node.index.root);
            strbuf_append_char(sb, '[');
            append_ir_node(sb, node->node.index.value);
            strbuf_append_char(sb, ']');
            break;
        }

        case ICNT_STRUCT_INIT: {
            strbuf_append_char(sb, '(');
            strbuf_append_str(sb, node->node.struct_init.type);
            strbuf_append_chars(sb, "){ ");
            LLNode_IR_C_Node* curr = node->node.struct_init.fields.head;
            while (curr) {
                append_ir_node(sb, &curr->data);
                curr = curr->next;
            }
            strbuf_append_chars(sb, " }");
            break;
        }

        case ICNT_IF: {
            strbuf_append_chars(sb, "if (");
            append_ir_node(sb, node->node.if_.cond);
            strbuf_append_chars(sb, ") {\n");
            LLNode_IR_C_Node* curr = node->node.if_.then.head;
            while (curr) {
                strbuf_append_chars(sb, "    ");
                strbuf_append_chars(sb, "    ");
                append_ir_node(sb, &curr->data);
                strbuf_append_chars(sb, ";\n");
                curr = curr->next;
            }
            strbuf_append_chars(sb, "    }");
            if (node->node.if_.else_) {
                strbuf_append_chars(sb, "else ");
                append_ir_node(sb, node->node.if_.else_);
            }
            break;
        }

        case ICNT_WHILE: {
            strbuf_append_chars(sb, "while (");
            append_ir_node(sb, node->node.while_.cond);
            strbuf_append_chars(sb, ") {\n");
            LLNode_IR_C_Node* curr = node->node.while_.then.head;
            while (curr) {
                strbuf_append_chars(sb, "    ");
                strbuf_append_chars(sb, "    ");
                append_ir_node(sb, &curr->data);
                strbuf_append_chars(sb, ";\n");
                curr = curr->next;
            }
            strbuf_append_chars(sb, "    }");
            break;
        }

        default: printf("%s\n---\nTODO: gen [%d]\n", strbuf_to_str(*sb).chars, node->type); assert(false);
    }
}

static void append_ir_file(StringBuffer* sb, IR_C_File* file) {
    LLNode_IR_C_Node* curr = file->nodes.head;
    while (curr) {
        append_ir_node(sb, &curr->data);
        if (sb->chars[sb->length - 1] != ';') {
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
