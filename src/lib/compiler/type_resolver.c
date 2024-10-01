#include <stdio.h>

#include "./ast.h"
#include "./type_resolver.h"
#include "./resolved_type.h"

#define RESOLVE_ITERS_MAX 1024

typedef bool Changed;

typedef struct Scope {
    struct Scope* parent;

    // TODO
} Scope;

TypeResolver type_resolver_create(Arena* arena, Packages packages) {
    return (TypeResolver){
        .arena = arena,
        .packages = packages,
    };
}

Changed resolve_type_node(TypeResolver* type_resolver, ASTNode const* node) {
    Changed changed = false;
    switch (node->type) {
        case ANT_FILE_ROOT: {
            LLNode_ASTNode* curr = node->node.file_root.nodes.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &curr->data);
                curr = curr->next;
            }
            break;
        }

        case ANT_PACKAGE: {
            // assert(false); // TODO
            break;
        }

        case ANT_IMPORT: {
            assert(node->node.import.type == IT_DEFAULT); // TODO

            Package* package = packages_resolve(&type_resolver->packages, node->node.import.static_path);
            assert(package);

            TypeInfo* ti = type_resolver->packages.types + node->id;
            assert(ti);

            ti->type = arena_alloc(type_resolver->arena, sizeof *ti->type);
            *ti->type = (ResolvedType){
                .kind = RTK_NAMESPACE,
                .type.package_ast = package->ast,
            };

            break;
        };

        case ANT_FILE_SEPARATOR: {
            // type_resolver->seen_separator = true;
            break;
        }

        case ANT_UNARY_OP: {
            changed |= resolve_type_node(type_resolver, node->node.unary_op.right);
            break;
        }

        case ANT_BINARY_OP: {
            changed |= resolve_type_node(type_resolver, node->node.binary_op.lhs);
            changed |= resolve_type_node(type_resolver, node->node.binary_op.rhs);
            break;
        }

        case ANT_VAR_DECL: {
            if (!node->node.var_decl.type_or_let.is_let) {
                // resolve_type_node(type_resolver, node->node.var_decl.type_or_let.maybe_type);
            }

            if (node->node.var_decl.initializer) {
                changed |= resolve_type_node(type_resolver, node->node.var_decl.initializer);
            }

            break;
        }

        case ANT_GET_FIELD: {
            changed |= resolve_type_node(type_resolver, node->node.get_field.root);
            break;
        }

        case ANT_ASSIGNMENT: {
            changed |= resolve_type_node(type_resolver, node->node.assignment.lhs);
            changed |= resolve_type_node(type_resolver, node->node.assignment.rhs);
            break;
        }

        case ANT_FUNCTION_CALL: {
            changed |= resolve_type_node(type_resolver, node->node.function_call.function);

            LLNode_ASTNode* curr = node->node.function_call.args.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_STATEMENT_BLOCK: {
            LLNode_ASTNode* curr = node->node.statement_block.stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_IF: {
            changed |= resolve_type_node(type_resolver, node->node.if_.cond);

            LLNode_ASTNode* curr = node->node.if_.block->stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_ELSE: {
            changed |= resolve_type_node(type_resolver, node->node.else_.target);
            changed |= resolve_type_node(type_resolver, node->node.else_.then);
            break;
        }

        case ANT_TRY: assert(false); // TODO
        case ANT_CATCH: assert(false); // TODO
        case ANT_BREAK: assert(false); // TODO
        case ANT_WHILE: assert(false); // TODO
        case ANT_DO_WHILE: assert(false); // TODO
        case ANT_FOR: assert(false); // TODO

        case ANT_RETURN: {
            if (node->node.return_.maybe_expr) {
                changed |= resolve_type_node(type_resolver, node->node.return_.maybe_expr);
            }
            break;
        }

        case ANT_STRUCT_INIT: assert(false); // TODO
        case ANT_ARRAY_INIT: assert(false); // TODO

        case ANT_TEMPLATE_STRING: assert(false); // TODO
        case ANT_CRASH: assert(false); // TODO

        case ANT_SIZEOF: {
            // TODO ??
            break;
        }

        case ANT_SWITCH: assert(false); // TODO
        case ANT_CAST: assert(false); // TODO
        case ANT_STRUCT_DECL: assert(false); // TODO
        case ANT_UNION_DECL: assert(false); // TODO
        case ANT_ENUM_DECL: assert(false); // TODO

        case ANT_TYPEDEF_DECL: {
            // changed |= resolve_type_node(type_resolver, node->node.typedef_decl.type);
            break;
        }

        case ANT_GLOBALTAG_DECL: assert(false); // TODO

        case ANT_FUNCTION_HEADER_DECL: {
            // changed |= resolve_type_node(type_resolver, &node->node.function_header_decl.return_type);
            break;
        }

        case ANT_FUNCTION_DECL: {
            // changed |= resolve_type_node(type_resolver, &node->node.function_decl.header.return_type);

            LLNode_ASTNode* curr = node->node.function_decl.stmts.head;
            while (curr) {
                changed |= resolve_type_node(type_resolver, &curr->data);
                curr = curr->next;
            }

            break;
        }

        case ANT_LITERAL: break;
        case ANT_VAR_REF: break;

        case ANT_NONE: assert(false);
        case ANT_COUNT: assert(false);

        default: assert(false);
    }

    return changed;
}

void resolve_types(TypeResolver* type_resolver) {
    size_t iter = 0;
    Changed changed = true;
    while (changed) {
        assert(iter++ < RESOLVE_ITERS_MAX);
        changed = false;

        for (size_t i = 0; i < type_resolver->packages.lookup_length; ++i) {
            ArrayList_Package* bucket = type_resolver->packages.lookup_buckets + i;
            if (!bucket) {
                continue;
            }

            for (size_t j = 0; j < bucket->length; ++j) {
                Package* package = bucket->array + j;
                changed |= resolve_type_node(type_resolver, package->ast);
            }
        }
    }
}
