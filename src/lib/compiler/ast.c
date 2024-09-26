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

void ll_directive_push(Arena* const arena, LL_Directive* const ll, Directive const directive) {
    LLNode_Directive* const llnode = arena_alloc(arena, sizeof(LLNode_Directive));
    llnode->data = directive;
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

void ll_param_push(Arena* const arena, LL_FnParam* const ll, FnParam const param) {
    LLNode_FnParam* const llnode = arena_alloc(arena, sizeof(LLNode_FnParam));
    llnode->data = param;
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

static void print_directives(LL_Directive directives) {
    LLNode_Directive* curr = directives.head;
    while (curr != NULL) {
        switch (curr->data.type) {
            case DT_C_HEADER: {
                String include_str = arena_strcpy(arena, curr->data.dir.c_header.include);
                printf("@c_header(\"%s\") ", include_str.chars);
                break;
            }

            case DT_C_RESTRICT: printf("@c_restrict "); break;

            case DT_C_FILE: printf("@c_FILE "); break;
        }

        curr = curr->next;
    }
}

static void print_type(Type const* type) {
    if (type == NULL) {
        printf("<type:NULL>");
        return;
    }

    if (type->directives.length > 0) {
        print_directives(type->directives);
    }

    switch (type->kind) {
        case TK_BUILT_IN: {
            switch (type->type.built_in) {
                case TBI_VOID: {
                    printf("void");
                    return;
                }

                case TBI_UINT: {
                    printf("uint32_t");
                    return;
                }
            }
            return;
        }

        case TK_TYPE_REF: {
            print_string(type->type.type_ref.name);
            return;
        }

        case TK_POINTER: {
            print_type(type->type.ptr.of);
            printf("*");
            return;
        }

        case TK_MUT_POINTER: {
            print_type(type->type.mut_ptr.of);
            printf(" mut*");
            return;
        }

        default: printf("<type:%d>", type->kind); return;
    }
}

void print_astnode(ASTNode const node) {
    if (node.directives.length > 0) {
        print_directives(node.directives);
    }

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

        case ANT_PACKAGE: {
            printf("package ");
            print_static_path(node.node.package.static_path);
            printf(";\n");
            break;
        }

        case ANT_TYPEDEF_DECL: {
            printf("typedef ");
            print_string(node.node.typedef_decl.name);
            printf(" = ");
            print_type(node.node.typedef_decl.type);
            printf(";\n");
            break;
        }

        case ANT_STRUCT_DECL: {
            printf("struct ");

            String* m_name = node.node.struct_decl.maybe_name;
            if (m_name != NULL) {
                print_string(*m_name);
                printf(" ");
            }

            printf("{\n");
            indent += 1;

            print_tabs();
            printf("/* TODO */\n");

            indent -= 1;
            printf("}\n");
            break;
        }

        case ANT_VAR_DECL: {
            if (node.node.var_decl.is_static) { printf("static "); }

            if (node.node.var_decl.type_or_let.is_let) {
                printf("let ");
            } else {
                print_type(node.node.var_decl.type_or_let.maybe_type);
                printf(" ");
            }

            if (node.node.var_decl.type_or_let.is_mut) {
                printf("mut ");
            }

            assert(node.node.var_decl.lhs.type == VDLT_NAME);
            assert(node.node.var_decl.lhs.count == 1);
            print_string(node.node.var_decl.lhs.lhs.name);

            if (node.node.var_decl.initializer != NULL) {
                printf(" = ");
                print_astnode(*node.node.var_decl.initializer);
            }

            printf(";\n");
            break;
        }

        case ANT_FUNCTION_HEADER_DECL: {
            print_type(&node.node.function_header_decl.return_type);
            printf(" ");
            print_string(node.node.function_header_decl.name);
            printf("(");
            {
                LLNode_FnParam* param = node.node.function_header_decl.params.head;
                while (param != NULL) {
                    print_type(&param->data.type);
                    printf(" ");

                    if (param->data.is_mut) {
                        printf("mut ");
                    }
                    
                    print_string(param->data.name);

                    param = param->next;

                    if (param != NULL) { printf(", "); }
                }
            }
            printf(");\n");
            
            break;
        }

        case ANT_FUNCTION_DECL: {
            print_type(&node.node.function_header_decl.return_type);
            printf(" ");
            print_string(node.node.function_header_decl.name);
            printf("(");
            {
                LLNode_FnParam* param = node.node.function_header_decl.params.head;
                while (param != NULL) {
                    print_type(&param->data.type);
                    printf(" ");

                    if (param->data.is_mut) {
                        printf("mut ");
                    }
                    
                    print_string(param->data.name);
                    printf(", ");
                    
                    param = param->next;
                }
            }
            printf(") {\n");
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
                default: print_tabs(); printf("/* TODO: print_literal(%d) */", node.type);
            }

            break;
        }

        default: printf("/* TODO: print_node(%d) */", node.type);
    }

    arena_reset(arena);
}

