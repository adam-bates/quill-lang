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

static String gen_name(CodegenC* codegen, PackagePath* pkg, String name) {
    StringBuffer sb = strbuf_create(codegen->arena);
    while (pkg) {
        strbuf_append_str(&sb, pkg->name);
        strbuf_append_char(&sb, '_');
        pkg = pkg->child;
    }
    strbuf_append_char(&sb, '_');
    strbuf_append_str(&sb, name);
    return strbuf_to_str(sb);
}

static void _append_return_type(CodegenC* codegen, StringBuffer* sb, Type type) {
    switch (type.kind) {
        case TK_BUILT_IN: {
            switch (type.type.built_in) {
                case TBI_VOID: {
                    strbuf_append_chars(sb, "void");
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

        case TK_TYPE_REF: {
            Package* package = packages_type_by_type(codegen->packages, type.id)->type->from_pkg;
            String name = gen_name(codegen, package->full_name, type.type.type_ref.name);
            strbuf_append_str(sb, name);
            break;
        }

        case TK_STATIC_PATH: {
            StaticPath* curr = type.type.static_path.path;
            while (curr->child) {
                curr = curr->child;
            }

            strbuf_append_str(sb, curr->name);
            break;
        }

        case TK_TUPLE: {
            assert(false);
            break;
        }

        case TK_POINTER: {
            _append_return_type(codegen, sb, *type.type.mut_ptr.of);
            strbuf_append_chars(sb, " const*"); // does this always work?
            break;
        }

        case TK_MUT_POINTER: {
            _append_return_type(codegen, sb, *type.type.mut_ptr.of);
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
        case RTK_UINT: strbuf_append_chars(sb, "size_t"); break;
        case RTK_CHAR: strbuf_append_chars(sb, "char"); break;

        case RTK_POINTER: {
            _append_type_resolved(codegen, sb, type->type.ptr.of);
            strbuf_append_chars(sb, " const*");
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

        case RTK_STRUCT: {
            strbuf_append_str(sb, type->type.struct_.name);
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
    _append_return_type(codegen, &sb, type);
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
            assert(type->kind == RTK_NAMESPACE);
            assert(type->type.namespace_->ast->type == ANT_FILE_ROOT);

            DirectiveCHeader* c_header = get_c_header(type->type.namespace_);
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
                String include_path = gen_header_file_path(codegen->arena, type->type.namespace_->full_name);

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
                        .type = ICNT_VAR_REF,
                        .node.var_ref.name = strbuf_to_str(sb),
                    });

                    break;
                }

                default: assert(false);
            }

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
                .type = ICNT_VAR_REF,
                .node.var_ref.name = child->name,
            });
            break;
        }

        case ANT_GET_FIELD: {
            LL_IR_C_Node root_ll = {0};
            fill_nodes(codegen, &root_ll, node->node.get_field.root, ftype, stage, false);
            assert(root_ll.length == 1);

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_GET_FIELD,
                .node.get_field = {
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

        case ANT_VAR_DECL: {
            if (root_call && stage != TS_VARS) {
                break;
            }
            ResolvedType* rt = codegen->packages->types[node->id.val].type;
            assert(rt);

            String type;
            if (node->node.var_decl.type_or_let.is_let) {
                type = gen_type_resolved(codegen, rt);
            } else {
                type = gen_type(codegen, *node->node.var_decl.type_or_let.maybe_type);
            }

            assert(node->node.var_decl.lhs.type == VDLT_NAME);
            String name = node->node.var_decl.lhs.lhs.name;

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
            {
                LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
                while (curr) {
                    fill_nodes(codegen, &statements, &curr->data, ftype, stage, false);
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

            ResolvedType* type = codegen->packages->types[node->id.val].type;

            Strings fields = {
                .length = 0,
                .strings = arena_calloc(codegen->arena, node->node.struct_decl.fields.length, sizeof(String)),
            };
            {
                StringBuffer sb = strbuf_create(codegen->arena);

                LLNode_StructField* curr = node->node.struct_decl.fields.head;
                while (curr) {
                    strbuf_append_str(&sb, gen_type(codegen, *curr->data.type));
                    strbuf_append_char(&sb, ' ');
                    strbuf_append_str(&sb, curr->data.name);

                    fields.strings[fields.length++] = strbuf_to_strcpy(sb);
                    strbuf_reset(&sb);

                    curr = curr->next;
                }
            }

            ll_node_push(codegen->arena, c_nodes, (IR_C_Node){
                .type = ICNT_STRUCT_DECL,
                .node.struct_decl = {
                    .name = *node->node.struct_decl.maybe_name,
                    .fields = fields,
                },
            });
            break;
        }

        default: {
            printf("TODO: transform [");
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
    } else if (ftype == FT_HEADER) {
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
            .file = c_str("<stdlib.h>"),
        },
    });

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
                .type = ICNT_VAR_REF,
                .node.var_ref.name = c_str("_main()"),
            });
        }
        {
            IR_C_Node* expr_ret = arena_alloc(codegen->arena, sizeof *expr_ret);
            expr_ret->type = ICNT_VAR_REF;
            expr_ret->node.var_ref.name = c_str("0");
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

        case ICNT_VAR_REF: {
            strbuf_append_str(sb, node->node.var_ref.name);
            break;
        }

        case ICNT_GET_FIELD: {
            append_ir_node(sb, node->node.get_field.root);
            strbuf_append_char(sb, '.');
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
            strbuf_append_char(sb, ';');
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

            strbuf_append_char(sb, ';');
            
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
                    strbuf_append_chars(sb, "\n");
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

        default: printf("%s\n---\nTODO: gen [%d]\n", strbuf_to_str(*sb).chars, node->type); assert(false);
    }
}

static void append_ir_file(StringBuffer* sb, IR_C_File* file) {
    LLNode_IR_C_Node* curr = file->nodes.head;
    while (curr) {
        append_ir_node(sb, &curr->data);
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
