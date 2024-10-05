#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./ast.h"
#include "../utils/utils.h"

ASTNode* find_decl_by_id(ASTNodeFileRoot root, NodeId id) {
    LLNode_ASTNode* curr = root.nodes.head;
    while (curr) {
        if (curr->data.id.val == id.val) {
            return &curr->data;
        }
        curr = curr->next;
    }

    return NULL;
}

ASTNode* find_decl_by_name(ASTNodeFileRoot root, String name) {
    LLNode_ASTNode* curr = root.nodes.head;
    while (curr) {
        ASTNode node = curr->data;
        switch (node.type) {
            case ANT_VAR_DECL: {
                if (node.node.var_decl.lhs.type == VDLT_NAME && str_eq(node.node.var_decl.lhs.lhs.name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_STRUCT_DECL: {
                if (node.node.struct_decl.maybe_name && str_eq(*node.node.struct_decl.maybe_name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_UNION_DECL: {
                if (node.node.union_decl.maybe_name && str_eq(*node.node.union_decl.maybe_name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_ENUM_DECL: {
                if (str_eq(node.node.enum_decl.name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_TYPEDEF_DECL: {
                if (str_eq(node.node.typedef_decl.name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_GLOBALTAG_DECL: {
                if (node.node.globaltag_decl.maybe_name && str_eq(*node.node.globaltag_decl.maybe_name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_FUNCTION_HEADER_DECL: {
                if (str_eq(node.node.function_header_decl.name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_FUNCTION_DECL: {
                if (str_eq(node.node.function_decl.header.name, name)) {
                    return &curr->data;
                }
                break;
            }

            case ANT_NONE: break;
            case ANT_FILE_SEPARATOR: break;
            case ANT_UNARY_OP: break;
            case ANT_BINARY_OP: break;
            case ANT_LITERAL: break;
            case ANT_VAR_REF: break;
            case ANT_GET_FIELD: break;
            case ANT_ASSIGNMENT: break;
            case ANT_FUNCTION_CALL: break;
            case ANT_STATEMENT_BLOCK: break;
            case ANT_IF: break;
            case ANT_ELSE: break;
            case ANT_TRY: break;
            case ANT_CATCH: break;
            case ANT_BREAK: break;
            case ANT_WHILE: break;
            case ANT_DO_WHILE: break;
            case ANT_FOR: break;
            case ANT_RETURN: break;
            case ANT_STRUCT_INIT: break;
            case ANT_ARRAY_INIT: break;
            case ANT_IMPORT: break;
            case ANT_PACKAGE: break;
            case ANT_TEMPLATE_STRING: break;
            case ANT_CRASH: break;
            case ANT_SIZEOF: break;
            case ANT_SWITCH: break;
            case ANT_CAST: break;

            case ANT_FILE_ROOT:
            case ANT_COUNT: assert(false);
        }
        curr = curr->next;
    }

    return NULL;
}

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

static void append_static_path(StringBuffer* sb, StaticPath* path) {
    strbuf_append_str(sb, path->name);

    if (path->child) {
        strbuf_append_chars(sb, "::");
        append_static_path(sb, path->child);
    }
}

static void append_package_path(StringBuffer* sb, PackagePath* path) {
    strbuf_append_str(sb, path->name);

    if (path->child) {
        strbuf_append_char(sb, '/');
        append_package_path(sb, path->child);
    }
}

static void _append_import_static_path(StringBuffer* sb, ImportStaticPath* path) {
    switch (path->type) {
        case ISPT_WILDCARD: {
            strbuf_append_char(sb, '*');
            break;
        }
        case ISPT_IDENT: {
            strbuf_append_str(sb, path->import.ident.name);
            if (path->import.ident.child) {
                strbuf_append_chars(sb, "::");
                _append_import_static_path(sb, path->import.ident.child);
            }
            break;
        }
    }
}

static void append_import_path(StringBuffer* sb, ImportPath* path) {
    switch (path->type) {
        case IPT_DIR: {
            strbuf_append_str(sb, path->import.dir.name);
            if (path->import.dir.child) {
                strbuf_append_char(sb, '/');
                append_import_path(sb, path->import.dir.child);
            }
            break;
        }
        case IPT_FILE: {
            strbuf_append_str(sb, path->import.file.name);
            if (path->import.file.child) {
                strbuf_append_char(sb, '/');
                _append_import_static_path(sb, path->import.file.child);
            }
        }
    }
}

StringBuffer static_path_to_strbuf(Arena* arena, StaticPath* path) {
    StringBuffer sb = strbuf_create_with_capacity(arena, path->name.length);
    append_static_path(&sb, path);
    return sb;
}

String static_path_to_str(Arena* arena, StaticPath* path) {
    StringBuffer sb = static_path_to_strbuf(arena, path);
    return strbuf_to_strcpy(sb);
}

StringBuffer package_path_to_strbuf(Arena* arena, PackagePath* path) {
    StringBuffer sb = strbuf_create_with_capacity(arena, path->name.length);
    append_package_path(&sb, path);
    return sb;
}

String package_path_to_str(Arena* arena, PackagePath* path) {
    StringBuffer sb = package_path_to_strbuf(arena, path);
    return strbuf_to_strcpy(sb);
}

StringBuffer import_path_to_strbuf(Arena* arena, ImportPath* path) {
    StringBuffer sb = strbuf_create(arena);
    append_import_path(&sb, path);
    return sb;
}

String import_path_to_str(Arena* arena, ImportPath* path) {
    StringBuffer sb = import_path_to_strbuf(arena, path);
    return strbuf_to_strcpy(sb);
}

PackagePath* import_path_to_package_path(Arena* arena, ImportPath* import_path) {
    if (!import_path) { return NULL; }
    
    PackagePath* package_path = arena_alloc(arena, sizeof *package_path);
    
    switch (import_path->type) {
        case IPT_DIR: {
            *package_path = (PackagePath){
                .name = import_path->import.dir.name,
                .child = import_path_to_package_path(arena, import_path->import.dir.child),
            };
            break;
        }

        case IPT_FILE: {
            *package_path = (PackagePath){
                .name = import_path->import.file.name,
                .child = NULL,
            };
            break;
        }
    }

    return package_path;
}

ImportPath* package_to_import_path(Arena* arena, PackagePath* package_path) {
    if (!package_path) { return NULL; }

    ImportPath* import_path = arena_alloc(arena, sizeof *import_path);

    if (package_path->child) {
        *import_path = (ImportPath){
            .type = IPT_DIR,
            .import.dir.name = package_path->name,
            .import.dir.child = package_to_import_path(arena, package_path->child),
        };
    } else {
        *import_path = (ImportPath){
            .type = IPT_FILE,
            .import.file.name = package_path->name,
            .import.file.child = NULL,
        };
    }

    return import_path;
}

bool package_path_eq(PackagePath* p1, PackagePath* p2) {
    if (!p1 || !p2) {
        return !p1 && !p2;
    }

    if ((!p1->child) != (!p2->child)) {
        return false;
    }

    if (p1->name.length != p2->name.length) {
        return false;
    }

    if (strncmp(p1->name.chars, p2->name.chars, p1->name.length) != 0) {
        return false;
    }

    if (p1->child) {
        return package_path_eq(p1->child, p2->child);
    } else {
        return true;
    }
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

    print_string(path->name);

    if (path->child != NULL) {
        printf("::");
        print_static_path(path->child);
    }
}

static void print_package_path(PackagePath* path) {
    print_string(path->name);

    if (path->child != NULL) {
        printf("__");
        print_package_path(path->child);
    }
}

static void _print_import_static_path(ImportStaticPath* path) {
    switch (path->type) {
        case ISPT_WILDCARD: {
            printf("*");
            break;
        }
        case ISPT_IDENT: {
            print_string(path->import.ident.name);
            if (path->import.ident.child) {
                printf("__");
                _print_import_static_path(path->import.ident.child);
            }
            break;
        }
    }
}

static void print_import_path(ImportPath* path) {
    switch (path->type) {
        case IPT_DIR: {
            print_string(path->import.dir.name);
            if (path->import.dir.child) {
                printf("__");
                print_import_path(path->import.dir.child);
            }
            break;
        }
        case IPT_FILE: {
            print_string(path->import.file.name);
            if (path->import.file.child) {
                printf("_$_");
                _print_import_static_path(path->import.file.child);
            }
            break;
        }
    }
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

            case DT_IGNORE_UNUSED: printf("@ignore_unused "); break;

            case DT_IMPL: printf("@impl "); break;
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
                    printf("size_t");
                    return;
                }

                case TBI_CHAR: {
                    printf("char");
                    return;
                }
            }
            return;
        }

        case TK_TYPE_REF: {
            print_string(type->type.type_ref.name);
            return;
        }

        case TK_STATIC_PATH: {
            print_static_path(type->type.static_path.path);
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

        case ANT_FILE_ROOT: {
            LL_ASTNode nodes = node.node.file_root.nodes;
            LLNode_ASTNode* fnode = nodes.head;

            typedef enum {
                BT_OTHER,
                BT_IMPORT,
                BT_FUNCTION_DECLS,
                BT_STATIC_VARS,
                BT_VARS,
            } BlockType;

            BlockType curr_block = 0;
            BlockType prev_block = 0;
            bool last_was_ok = true;
            while (fnode != NULL) {
                if (fnode->data.type == ANT_NONE) {
                    if (last_was_ok) {
                        printf("<Unknown AST Node>\n");
                    }
                    last_was_ok = false;
                    fnode = fnode->next;
                    continue;
                }
                last_was_ok = true;

                //

                print_astnode(fnode->data);
                fnode = fnode->next;

                if (fnode == NULL) {
                    continue;
                }

                switch (fnode->data.type) {
                    case ANT_IMPORT: curr_block = BT_IMPORT; break;
                    case ANT_FUNCTION_HEADER_DECL: curr_block = BT_FUNCTION_DECLS; break;
                    case ANT_VAR_DECL: {
                        if (fnode->data.node.var_decl.is_static) {
                            curr_block = BT_STATIC_VARS; break;
                        } else {
                            curr_block = BT_VARS; break;
                        }
                    }

                    default: curr_block = BT_OTHER; break;
                }
                if (curr_block != prev_block) {
                    printf("\n");
                    prev_block = curr_block;
                }
            }

            break;
        }

        case ANT_FILE_SEPARATOR: {
            printf("---\n\n");
            break;
        }

        case ANT_IMPORT: {
            printf("import ");

            switch (node.node.import.type) {
                case IT_DEFAULT: break;
                case IT_LOCAL: printf("./"); break;
                case IT_ROOT: printf("~/"); break;
            }

            print_import_path(node.node.import.import_path);
            printf(";\n");
            break;
        }

        case ANT_PACKAGE: {
            printf("package ");
            print_package_path(node.node.package.package_path);
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
                    
                    param = param->next;

                    if (param != NULL) { printf(", "); }
                }
            }
            printf(") {\n");
            {
                indent += 1;
                LLNode_ASTNode* stmt = node.node.function_decl.stmts.head;
                if (stmt == NULL) {
                    printf("    //\n");
                }
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
            printf("}\n\n");
            
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

        case ANT_SIZEOF: {
            printf("sizeof(");

            switch (node.node.sizeof_.kind) {
                case SOK_TYPE: print_type(node.node.sizeof_.sizeof_.type); break;
                case SOK_EXPR: print_astnode(*node.node.sizeof_.sizeof_.expr); break;
            }

            printf(")");
            break;
        }

        case ANT_GET_FIELD: {
            assert(node.node.get_field.root);
            print_astnode(*node.node.get_field.root);
            printf(".");
            print_string(node.node.get_field.name);
            break;
        }

        case ANT_RETURN: {
            printf("return");

            if (node.node.return_.maybe_expr != NULL) {
                printf(" ");
                print_astnode(*node.node.return_.maybe_expr);
            }
            break;
        }

        case ANT_ASSIGNMENT: {
            assert(node.node.assignment.lhs);
            assert(node.node.assignment.rhs);

            print_astnode(*node.node.assignment.lhs);

            switch (node.node.assignment.op) {
                case AO_ASSIGN:          printf(" = "); break;

                case AO_PLUS_ASSIGN:     printf(" += "); break;
                case AO_MINUS_ASSIGN:    printf(" -= "); break;
                case AO_MULTIPLY_ASSIGN: printf(" *= "); break;
                case AO_DIVIDE_ASSIGN:   printf(" /= "); break;

                case AO_BIT_AND_ASSIGN:  printf(" &= "); break;
                case AO_BIT_OR_ASSIGN:   printf(" |= "); break;
                case AO_BIT_XOR_ASSIGN:  printf(" ^= "); break;

                default: printf(" <op:?> ");
            }

            print_astnode(*node.node.assignment.rhs);
            break;
        }

        case ANT_UNARY_OP: {
            assert(node.node.unary_op.right);

            switch (node.node.unary_op.op) {
                case UO_BOOL_NEGATE: printf("!"); break;
                case UO_NUM_NEGATE:  printf("-"); break;
                case UO_PTR_REF:     printf("&"); break;
                case UO_PTR_DEREF:   printf("*"); break;

                default: printf("<unary_op:?>");
            }

            print_astnode(*node.node.unary_op.right);
            break;             
        }

        default: printf("/* TODO: print_node(%d) */", node.type);
    }

    arena_reset(arena);
}

