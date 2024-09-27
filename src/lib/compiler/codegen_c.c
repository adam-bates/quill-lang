#include <stdio.h>
#include <string.h>

#include "./ast.h"
#include "./codegen_c.h"

typedef struct {
    bool seen_file_separator;
} State;

static void append_static_path(StringBuffer* strbuf, StaticPath* path) {
    if (path->root != NULL) {
        // if (
        //     strncmp(path->name.chars, "io", 2) == 0
        //     && strncmp(path->root->name.chars, "std", 3) == 0
        // ) {
        //     strbuf_append_chars(strbuf, "printf"); // TODO
        //     return;
        // }

        append_static_path(strbuf, path->root);
        strbuf_append_chars(strbuf, "__");
    }

    strbuf_append_str(strbuf, path->name);
}

static void append_type(StringBuffer* strbuf, Type type) {
    switch (type.kind) {
        case TK_BUILT_IN: {
            switch (type.type.built_in) {
                case TBI_VOID: {
                    strbuf_append_chars(strbuf, "void");
                    return;
                }

                case TBI_UINT: {
                    strbuf_append_chars(strbuf, "size_t");
                    return;
                }

                case TBI_CHAR: {
                    strbuf_append_chars(strbuf, "char");
                    return;
                }

                default: fprintf(stderr, "TODO: handle [%d]\n", type.type.built_in); assert(false);
            }
            return;
        }

        case TK_TYPE_REF: {
            strbuf_append_str(strbuf, type.type.type_ref.name);
            return;
        }

        case TK_STATIC_PATH: {
            append_static_path(strbuf, type.type.static_path.path);
            return;
        }

        case TK_POINTER: {
            assert(type.type.ptr.of);
            append_type(strbuf, *type.type.ptr.of);
            strbuf_append_chars(strbuf, " const*");
            return;
        }

        case TK_MUT_POINTER: {
            assert(type.type.mut_ptr.of);
            append_type(strbuf, *type.type.mut_ptr.of);
            strbuf_append_char(strbuf, '*');
            return;
        }

        default: fprintf(stderr, "TODO: handle [%d]\n", type.kind); assert(false);
    }
}

static void append_ast_node(State* state, StringBuffer* strbuf, ASTNode const* node) {
    switch (node->type) {
        case ANT_FILE_ROOT: {
            LLNode_ASTNode* curr = node->node.file_root.nodes.head;
            while (curr != NULL) {
                append_ast_node(state, strbuf, &curr->data);
                strbuf_append_char(strbuf, '\n');
                curr = curr->next;
            }
            return;
        }

        case ANT_FILE_SEPARATOR: {
            state->seen_file_separator = true;
            return;
        }

        case ANT_IMPORT: {
            strbuf_append_chars(strbuf, "#include \"");
            append_static_path(strbuf, node->node.import.static_path);
            strbuf_append_chars(strbuf, "\"\n");
            break;
        }

        case ANT_PACKAGE: {
            strbuf_append_chars(strbuf, "/* package ");
            append_static_path(strbuf, node->node.package.static_path);
            strbuf_append_chars(strbuf, " */\n");
            break;
        }

        case ANT_STRUCT_DECL: {
            String* m_name = node->node.struct_decl.maybe_name;

            if (m_name != NULL) {
                strbuf_append_chars(strbuf, "typedef struct {\n");
            } else {
                strbuf_append_chars(strbuf, "struct {\n");
            }

            strbuf_append_chars(strbuf, "    /* TODO */\n");

            strbuf_append_char(strbuf, '}');

            if (m_name != NULL) {
                strbuf_append_char(strbuf, ' ');
                strbuf_append_str(strbuf, *m_name);
            }

            strbuf_append_chars(strbuf, ";\n");

            break;
        }

        case ANT_FUNCTION_HEADER_DECL: {
            if (state->seen_file_separator) {
                // strbuf_append_chars(strbuf, "static ");
            }

            bool is_main = strncmp(node->node.function_header_decl.name.chars, "main", 4) == 0;

            if (is_main) {
                strbuf_append_chars(strbuf, "int");
            } else {
                append_type(strbuf, node->node.function_header_decl.return_type);
            }

            strbuf_append_char(strbuf, ' ');
            strbuf_append_str(strbuf, node->node.function_header_decl.name);
            strbuf_append_char(strbuf, '(');

            if (node->node.function_header_decl.params.length == 0) {
                strbuf_append_chars(strbuf, "void");
            } else {
                LLNode_FnParam* param = node->node.function_header_decl.params.head;
                while (param != NULL) {
                    append_type(strbuf, param->data.type);
                    strbuf_append_char(strbuf, ' ');

                    if (!param->data.is_mut) {
                        strbuf_append_chars(strbuf, "const ");
                    }

                    strbuf_append_str(strbuf, param->data.name);

                    param = param->next;
                    if (param != NULL) {
                        strbuf_append_chars(strbuf, ", ");
                    }
                }
            }

            strbuf_append_chars(strbuf, ");\n");
            return;
        }

        case ANT_FUNCTION_DECL: {
            if (state->seen_file_separator) {
                // strbuf_append_chars(strbuf, "static ");
            }

            bool is_main = strncmp(node->node.function_decl.header.name.chars, "main", 4) == 0;

            if (is_main) {
                strbuf_append_chars(strbuf, "int");
            } else {
                append_type(strbuf, node->node.function_decl.header.return_type);
            }
            strbuf_append_char(strbuf, ' ');
            strbuf_append_str(strbuf, node->node.function_decl.header.name);
            strbuf_append_char(strbuf, '(');

            if (node->node.function_decl.header.params.length == 0) {
                strbuf_append_chars(strbuf, "void");
            } else {
                LLNode_FnParam* param = node->node.function_header_decl.params.head;
                while (param != NULL) {
                    append_type(strbuf, param->data.type);
                    strbuf_append_char(strbuf, ' ');

                    if (!param->data.is_mut) {
                        strbuf_append_chars(strbuf, "const ");
                    }

                    strbuf_append_str(strbuf, param->data.name);

                    param = param->next;
                    if (param != NULL) {
                        strbuf_append_chars(strbuf, ", ");
                    }
                }
            }

            strbuf_append_chars(strbuf, ") {\n");

            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr != NULL) {
                strbuf_append_chars(strbuf, "    ");
                append_ast_node(state, strbuf, &curr->data);
                strbuf_append_chars(strbuf, ";\n");
                curr = curr->next;
            }

            if (is_main) {
                strbuf_append_chars(strbuf, "    return 0;\n");
            }

            strbuf_append_chars(strbuf, "}\n");
            return;
        }

        case ANT_FUNCTION_CALL: {
            append_ast_node(state, strbuf, node->node.function_call.function);
            strbuf_append_char(strbuf, '(');

            LLNode_ASTNode* curr = node->node.function_call.args.head;
            while (curr != NULL) {
                append_ast_node(state, strbuf, &curr->data);
                curr = curr->next;

                if (curr != NULL) {
                    strbuf_append_chars(strbuf, ", ");
                }
            }

            strbuf_append_char(strbuf, ')');
            return;
        }

        case ANT_VAR_REF: {
            append_static_path(strbuf, node->node.var_ref.path);
            return;
        }

        case ANT_VAR_DECL: {
            if (node->node.var_decl.is_static) {
                strbuf_append_chars(strbuf, "static ");
            }

            TypeOrLet tol = node->node.var_decl.type_or_let;
            assert(!tol.is_let); // TODO: need type info

            append_type(strbuf, *tol.maybe_type);
            strbuf_append_char(strbuf, ' ');

            if (!tol.is_mut) {
                printf("const ");
            }

            VarDeclLHS lhs = node->node.var_decl.lhs;

            // TODO: support other LHS types
            assert(lhs.count == 1);
            assert(lhs.type == VDLT_NAME);

            strbuf_append_str(strbuf, lhs.lhs.name);

            if (node->node.var_decl.initializer != NULL) {
                strbuf_append_chars(strbuf, " = ");
                append_ast_node(state, strbuf, node->node.var_decl.initializer);
            }

            return;
        }

        case ANT_SIZEOF: {
            strbuf_append_chars(strbuf, "sizeof(");

            switch (node->node.sizeof_.kind) {
                case SOK_TYPE: append_type(strbuf, *node->node.sizeof_.sizeof_.type); break;
                case SOK_EXPR: append_ast_node(state, strbuf, node->node.sizeof_.sizeof_.expr); break;
            }

            strbuf_append_char(strbuf, ')');
            return;
        }

        case ANT_LITERAL: {
            switch (node->node.literal.kind) {
                case LK_STR: {
                    strbuf_append_char(strbuf, '"');
                    strbuf_append_str(strbuf, node->node.literal.value.lit_str);
                    strbuf_append_chars(strbuf, "\\n"); // TODO: temp
                    strbuf_append_char(strbuf, '"');
                    return;
                }
                default: fprintf(stderr, "TODO: handle [%d]\n", node->node.literal.kind); assert(false);
            }
            return;
        }

        default: fprintf(stderr, "TODO: handle [%d]\n", node->type); assert(false);
    }
}

String generate_c_code(CodegenC* const codegen) {
    assert(codegen->ast->type == ANT_FILE_ROOT);
    assert(codegen->ast->directives.length == 0);

    StringBuffer strbuf = strbuf_create(codegen->arena);

    // Don't generate code for @c_header files
    {
        ASTNodeFileRoot root = codegen->ast->node.file_root;
        if (root.nodes.length > 0) {
            ASTNode first = root.nodes.head->data;
            if (first.type == ANT_PACKAGE && first.directives.length > 0) {
                assert(first.directives.length == 1);
                assert(first.directives.head->data.type == DT_C_HEADER);

                strbuf_append_chars(&strbuf, "/* This file provides Quill types for the c file: ");
                strbuf_append_str(&strbuf, first.directives.head->data.dir.c_header.include);
                strbuf_append_chars(&strbuf, " */");

                return strbuf_to_str(strbuf);
            }
        }
    }

    State state = {0};

    append_ast_node(&state, &strbuf, codegen->ast);
    // strbuf_append_chars(&strbuf, "todo");
    
    return strbuf_to_str(strbuf);
}

CodegenC codegen_c_create(Arena* const arena, ASTNode const* const ast) {
    return (CodegenC){
        .arena = arena,
        .ast = ast,
    };
}
