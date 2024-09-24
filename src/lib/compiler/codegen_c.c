#include <stdio.h>

#include "./ast.h"
#include "./codegen_c.h"

static void append_static_path(StringBuffer* strbuf, StaticPath* path) {
    strbuf_append_chars(strbuf, "printf"); // TODO
}

static void append_type(StringBuffer* strbuf, Type type) {
    switch (type.kind) {
        case TK_BUILT_IN: {
            switch (type.type.built_in) {
                case TBI_VOID: {
                    strbuf_append_chars(strbuf, "void");
                    return;
                }
                default: fprintf(stderr, "TODO: handle [%d]\n", type.type.built_in); assert(false);
            }
            return;
        }
        default: fprintf(stderr, "TODO: handle [%d]\n", type.kind); assert(false);
    }
}

static void append_ast_node(StringBuffer* strbuf, ASTNode const* node) {
    switch (node->type) {
        case ANT_FILE_ROOT: {
            LLNode_ASTNode* curr = node->node.file_root.nodes.head;
            while (curr != NULL) {
                append_ast_node(strbuf, &curr->data);
                strbuf_append_char(strbuf, '\n');
                curr = curr->next;
            }
            return;
        }

        case ANT_IMPORT: {
            strbuf_append_chars(strbuf, "#include <stdio.h>\n"); // TODO
            break;
        }

        case ANT_FUNCTION_DECL: {
            bool is_main = strncmp(node->node.function_decl.header.name.chars, "main", 4) == 0;

            if (is_main) {
                strbuf_append_chars(strbuf, "int");
            } else {
                append_type(strbuf, node->node.function_decl.header.return_type);
            }
            strbuf_append_char(strbuf, ' ');
            strbuf_append_str(strbuf, node->node.function_decl.header.name);
            strbuf_append_char(strbuf, '(');

            // if (node->node.function_decl.header.params.length == 0) {
                strbuf_append_chars(strbuf, "void");
            // } else {
            //     // TODO: params
            // }

            strbuf_append_chars(strbuf, ") {\n");

            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr != NULL) {
                strbuf_append_chars(strbuf, "    ");
                append_ast_node(strbuf, &curr->data);
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
            append_ast_node(strbuf, node->node.function_call.function);
            strbuf_append_char(strbuf, '(');

            LLNode_ASTNode* curr = node->node.function_call.args.head;
            while (curr != NULL) {
                append_ast_node(strbuf, &curr->data);
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
    StringBuffer strbuf = strbuf_create(codegen->arena);

    assert(codegen->ast->directives.length == 0);

    append_ast_node(&strbuf, codegen->ast);
    // strbuf_append_chars(&strbuf, "todo");
    
    return strbuf_to_str(strbuf);
}

CodegenC codegen_c_create(Arena* const arena, ASTNode const* const ast) {
    return (CodegenC){
        .arena = arena,
        .ast = ast,
    };
}
