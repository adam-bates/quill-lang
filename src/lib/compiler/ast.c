#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./ast.h"
#include "../utils/utils.h"

void ll_ast_push(Arena* const arena, LL_ASTNode* const ll, ASTNode const node) {
    LLNode_ASTNode* const llnode = arena_alloc(arena, sizeof(LLNode_ASTNode));
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

static Arena ast_print_arena = {0};
static Arena* arena = &ast_print_arena;

static size_t indent = 0;

static void print_tabs(void) {
    for (size_t i = 0; i < indent; ++i) {
        printf("    ");
    }
}

static void print_string(String const str) {
    printf("%s", arena_strcpy(arena, str).chars);
}

static void print_static_path(StaticPath const* path) {
    if (path == NULL) {
        printf("<static_path:NULL>");
        return;
    }

    if (path->root != NULL) {
        print_static_path(path->root);
        printf("::");
    }

    print_string(path->name);
}

void print_astnode(ASTNode const node) {
    switch (node.type) {
        case ANT_NONE: {
            printf("<Incomplete AST Node>");
            break;
        }

        case ANT_IMPORT: {
            printf("import ");
            print_static_path(node.node.import.static_path);
            printf(";\n");
            break;
        }

        case ANT_FUNCTION_DECL: {
            printf("void ");
            print_string(node.node.function_header_decl.name);
            printf("() {\n");
            {
                indent += 1;
                LLNode_ASTNode* stmt = node.node.function_decl.stmts.head;
                bool last_was_ok = true;
                while (stmt != NULL) {
                    if (stmt->data.type == ANT_NONE) {
                        if (last_was_ok) {
                            print_tabs();
                            printf("<Unknown AST Node>\n");
                        }
                        last_was_ok = false;
                        stmt = stmt->next;
                        continue;
                    }
                    last_was_ok = true;

                    print_tabs();
                    print_astnode(stmt->data);
                    printf(";\n");
                    stmt = stmt->next;
                }
                
                indent -= 1;
            }
            printf("}\n");
            
            break;
        }

        case ANT_FUNCTION_CALL: {
            print_astnode(*node.node.function_call.function);
            printf("(");
            {
                LLNode_ASTNode* arg = node.node.function_call.args.head;
                while (arg != NULL) {
                    print_astnode(arg->data);
                    arg = arg->next;

                    if (arg != NULL) {
                        printf(", ");
                    }
                }
            }
            printf(")");
            
            break;
        }

        case ANT_VAR_REF: {
            print_static_path(node.node.var_ref.path);
            break;
        }

        case ANT_LITERAL: {
            switch (node.node.literal.kind) {
                case LK_STR: {
                    printf("\"");
                    print_string(node.node.literal.value.lit_str);
                    printf("\"");
                    break;
                }
                case LK_CHARS: {
                    printf("'");
                    print_string(node.node.literal.value.lit_chars);
                    printf("'");
                    break;
                }
                case LK_CHAR: {
                    printf("'");
                    print_string(node.node.literal.value.lit_char);
                    printf("'");
                    break;
                }
                case LK_INT: {
                    printf("%llu", node.node.literal.value.lit_int);
                    break;
                }
                case LK_FLOAT: {
                    printf("%f", node.node.literal.value.lit_float);
                    break;
                }
                default: print_tabs(); printf("/* TODO: print_astnode(%d) */", node.type);
            }

            break;
        }

        default: printf("/* TODO: print_astnode(%d) */", node.type);
    }

    arena_reset(arena);
}

