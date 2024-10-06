#include <assert.h>

#include "./analyzer.h"
#include "./ast.h"

// TODO: better error handling

static void verify_type(Analyzer* analyzer, Type const* type, size_t depth, size_t* iter) {
    assert(type);

    *iter += 1;

    if (type->directives.length > 0) {
        size_t seen_flags = 0;

        LLNode_Directive* curr = type->directives.head;
        while (curr) {
            // Each directive used at most once per AST node
            assert((seen_flags & (1<<curr->data.type)) == 0);
            seen_flags |= (1<<curr->data.type);

            switch (curr->data.type) {
                case DT_C_RESTRICT:
                case DT_C_FILE: {
                    assert(type->kind == TK_BUILT_IN);
                    assert(type->type.built_in == TBI_VOID);
                }

                case DT_C_HEADER: break;

                // Only valid on nodes
                case DT_IGNORE_UNUSED: assert(false);
                case DT_IMPL: assert(false);
            }
            
            curr = curr->next;
        }
    }

    // TODO
}

static void verify_node(Analyzer* analyzer, ASTNode const* const ast, size_t depth, size_t* iter) {
    assert(ast);

    *iter += 1;

    if (ast->directives.length > 0) {
        size_t seen_flags = 0;

        LLNode_Directive* curr = ast->directives.head;
        while (curr) {
            // Each directive used at most once per AST node
            assert((seen_flags & (1<<curr->data.type)) == 0);
            seen_flags |= (1<<curr->data.type);

            switch (curr->data.type) {
                case DT_C_HEADER: {
                    assert(ast->type == ANT_PACKAGE);
                    break;
                }

                case DT_IGNORE_UNUSED: {
                    assert(ast->type == ANT_VAR_DECL);
                    break;
                }

                case DT_IMPL: {
                    assert(ast->type == ANT_FUNCTION_DECL);
                    break;
                }

                // Only valid on types
                case DT_C_RESTRICT: assert(false);
                case DT_C_FILE: assert(false);
            }
            
            curr = curr->next;
        }
    }

    switch (ast->type) {
        case ANT_FILE_ROOT: {
            assert(depth == 0);
            assert(*iter == 1);
            assert(ast->directives.length == 0);

            LLNode_ASTNode const* curr = ast->node.file_root.nodes.head;
            while (curr) {
                verify_node(analyzer, &curr->data, depth + 1, iter);
                curr = curr->next;
            }
            break;
        }

        case ANT_PACKAGE: {
            assert(depth == 1);
            assert(*iter == 2);

            assert(!analyzer->has_package);
            analyzer->has_package = true;
            break;
        }

        case ANT_FILE_SEPARATOR: {
            assert(depth == 1);

            assert(!analyzer->has_separator);
            analyzer->has_separator = true;
            break;
        }

        case ANT_UNARY_OP: {
            verify_node(analyzer, ast->node.unary_op.right, depth + 1, iter);
            break;
        }

        case ANT_BINARY_OP: {
            verify_node(analyzer, ast->node.binary_op.lhs, depth + 1, iter);
            verify_node(analyzer, ast->node.binary_op.rhs, depth + 1, iter);
            break;
        }

        case ANT_VAR_DECL: {
            if (!ast->node.var_decl.type_or_let.is_let) {
                verify_type(analyzer, ast->node.var_decl.type_or_let.maybe_type, depth + 1, iter);
            }

            if (ast->node.var_decl.initializer) {
                verify_node(analyzer, ast->node.var_decl.initializer, depth + 1, iter);
            }

            break;
        }

        case ANT_GET_FIELD: {
            verify_node(analyzer, ast->node.get_field.root, depth + 1, iter);
            break;
        }

        case ANT_ASSIGNMENT: {
            verify_node(analyzer, ast->node.assignment.lhs, depth + 1, iter);
            verify_node(analyzer, ast->node.assignment.rhs, depth + 1, iter);
            break;
        }

        case ANT_FUNCTION_CALL: {
            verify_node(analyzer, ast->node.function_call.function, depth + 1, iter);

            LLNode_ASTNode* curr = ast->node.function_call.args.head;
            while (curr) {
                verify_node(analyzer, &curr->data, depth + 1, iter);
                curr = curr->next;
            }

            break;
        }

        case ANT_STATEMENT_BLOCK: {
            LLNode_ASTNode* curr = ast->node.statement_block.stmts.head;
            while (curr) {
                verify_node(analyzer, &curr->data, depth + 1, iter);
                curr = curr->next;
            }

            break;
        }

        case ANT_IF: {
            assert(ast->node.if_.block);

            verify_node(analyzer, ast->node.if_.cond, depth + 1, iter);

            LLNode_ASTNode* curr = ast->node.if_.block->stmts.head;
            while (curr) {
                verify_node(analyzer, &curr->data, depth + 1, iter);
                curr = curr->next;
            }

            break;
        }

        case ANT_ELSE: {
            verify_node(analyzer, ast->node.else_.target, depth + 1, iter);
            verify_node(analyzer, ast->node.else_.then, depth + 1, iter);
            break;
        }

        case ANT_TRY: assert(false); // TODO
        case ANT_CATCH: assert(false); // TODO
        case ANT_BREAK: assert(false); // TODO
        case ANT_WHILE: assert(false); // TODO
        case ANT_DO_WHILE: assert(false); // TODO
        case ANT_FOR: assert(false); // TODO

        case ANT_RETURN: {
            if (ast->node.return_.maybe_expr) {
                verify_node(analyzer, ast->node.return_.maybe_expr, depth + 1, iter);
            }
            break;
        }

        case ANT_STRUCT_INIT: assert(false); // TODO
        case ANT_ARRAY_INIT: assert(false); // TODO

        case ANT_IMPORT: break;

        case ANT_TEMPLATE_STRING: assert(false); // TODO
        case ANT_CRASH: assert(false); // TODO

        case ANT_SIZEOF: {
            // TODO ??
            break;
        }

        case ANT_SWITCH: assert(false); // TODO
        case ANT_CAST: assert(false); // TODO

        case ANT_STRUCT_DECL: {
            LLNode_StructField* curr = ast->node.struct_decl.fields.head;
            while (curr) {
                verify_type(analyzer, curr->data.type, depth + 1, iter);
                curr = curr->next;
            }
            break;
        }

        case ANT_UNION_DECL: assert(false); // TODO
        case ANT_ENUM_DECL: assert(false); // TODO

        case ANT_TYPEDEF_DECL: {
            verify_type(analyzer, ast->node.typedef_decl.type, depth + 1, iter);
            break;
        }

        case ANT_GLOBALTAG_DECL: assert(false); // TODO

        case ANT_FUNCTION_HEADER_DECL: {
            assert(depth == 1);
            verify_type(analyzer, &ast->node.function_header_decl.return_type, depth + 1, iter);
            break;
        }

        case ANT_FUNCTION_DECL: {
            assert(depth == 1);
            verify_type(analyzer, &ast->node.function_decl.header.return_type, depth + 1, iter);

            LLNode_ASTNode* curr = ast->node.function_decl.stmts.head;
            while (curr) {
                verify_node(analyzer, &curr->data, depth + 1, iter);
                curr = curr->next;
            }

            break;
        }

        case ANT_LITERAL: break;
        case ANT_VAR_REF: break;

        case ANT_NONE: assert(false);
        case ANT_COUNT: assert(false);
    }
}

void verify_syntax(Analyzer* analyzer, ASTNode const* const ast) {
    /*
        TODO:
        - verify main function
        - only allow 1 main function
        - only allow missing package if file has main function
        - prevent duplicate imports
        - prevent importing self
        - ensure c-headers don't have statements or expressions
        - ensure function header decls match function impl decls
    */

    assert(ast->type == ANT_FILE_ROOT);

    size_t iter = 0;
    verify_node(analyzer, ast, 0, &iter);
}
