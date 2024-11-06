#include <stdio.h>

#include "./resolved_type.h"

bool resolved_struct_decl_eq(ResolvedStructDecl* a, ResolvedStructDecl* b) {
    if (!a || !b) {
        return !a && !b;
    }

    if (!str_eq(a->name, b->name)) {
        return false;
    }

    if (a->fields_length != b->fields_length) {
        return false;
    }

    if (a->generic_params.length != b->generic_params.length) {
        return false;
    }

    for (size_t i = 0; i < a->fields_length; ++i) {
        if (!str_eq(a->fields[i].name, b->fields[i].name)) {
            return false;
        }

        if (!resolved_type_eq(a->fields[i].type, b->fields[i].type)) {
            return false;
        }
    }

    for (size_t i = 0; i < a->generic_params.length; ++i) {
        if (!str_eq(a->generic_params.strings[i], b->generic_params.strings[i])) {
            return false;
        }
    }

    return true;
}

bool resolved_type_eq(ResolvedType* a, ResolvedType* b) {
    if (!a || !b) {
        return !a && !b;
    }

    switch (a->kind) {
        case RTK_NAMESPACE: assert(false); // TODO
        case RTK_FUNCTION_DECL: assert(false); // TODO
        case RTK_FUNCTION_REF: assert(false); // TODO
        case RTK_ARRAY: assert(false); // TODO
        case RTK_TERMINAL: assert(false); // TODO

        case RTK_VOID:
        case RTK_BOOL:
        case RTK_CHAR:
        case RTK_INT:
        case RTK_INT8:
        case RTK_INT16:
        case RTK_INT32:
        case RTK_INT64:
        case RTK_UINT:
        case RTK_UINT8:
        case RTK_UINT16:
        case RTK_UINT32:
        case RTK_UINT64:
        case RTK_FLOAT:
        case RTK_FLOAT32:
        case RTK_FLOAT64:
            return b->kind == a->kind;

        case RTK_POINTER: switch (b->kind) {
            case RTK_POINTER: return resolved_type_eq(a->type.ptr.of, b->type.ptr.of);
            case RTK_MUT_POINTER: return resolved_type_eq(a->type.ptr.of, b->type.mut_ptr.of);
            default: return false;
        }

        case RTK_MUT_POINTER: switch (b->kind) {
            case RTK_POINTER: return resolved_type_eq(a->type.mut_ptr.of, b->type.ptr.of);
            case RTK_MUT_POINTER: return resolved_type_eq(a->type.mut_ptr.of, b->type.mut_ptr.of);
            default: return false;
        }

        case RTK_STRUCT_REF: switch (b->kind) {
            case RTK_STRUCT_REF: {
                if (a->type.struct_ref.generic_args.length != b->type.struct_ref.generic_args.length) {
                    return false;
                }
                for (size_t i = 0; i < a->type.struct_ref.generic_args.length; ++i) {
                    if (!resolved_type_eq(a->type.struct_ref.generic_args.resolved_types + i, b->type.struct_ref.generic_args.resolved_types + i)) {
                        return false;
                    }
                }
                return resolved_struct_decl_eq(&a->type.struct_ref.decl, &b->type.struct_ref.decl);
            }
            case RTK_STRUCT_DECL: return resolved_struct_decl_eq(&a->type.struct_ref.decl, &b->type.struct_decl);
            default: return false;
        }

        case RTK_STRUCT_DECL: switch (b->kind) {
            case RTK_STRUCT_REF: return resolved_struct_decl_eq(&a->type.struct_decl, &b->type.struct_ref.decl);
            case RTK_STRUCT_DECL: return resolved_struct_decl_eq(&a->type.struct_decl, &b->type.struct_decl);
            default: return false;
        }

        case RTK_GENERIC: {
            if (b->kind != a->kind) {
                return false;
            }
            if (a->src->id.val != b->src->id.val) {
                return false;
            }
            if (a->type.generic.idx != b->type.generic.idx) {
                return false;
            }
            return str_eq(a->type.generic.name, b->type.generic.name);
        }

        case RTK_COUNT: printf("TODO: resolved_type_eq(%d)\n", a->kind); assert(false); // TODO
    }

    return true;
}

bool resolved_type_implict_to(ResolvedType* from, ResolvedType* to) {
    if (!from || !to) {
        return false;
    }

    if (resolved_type_eq(from, to)) {
        return true;
    }

    if (from->kind == RTK_GENERIC || to->kind == RTK_GENERIC) {
        switch (from->kind) {
            case RTK_VOID:
            case RTK_BOOL:
            case RTK_CHAR:
            case RTK_INT:
            case RTK_INT8:
            case RTK_INT16:
            case RTK_INT32:
            case RTK_INT64:
            case RTK_UINT:
            case RTK_UINT8:
            case RTK_UINT16:
            case RTK_UINT32:
            case RTK_UINT64:
            case RTK_FLOAT:
            case RTK_FLOAT32:
            case RTK_FLOAT64:
            case RTK_POINTER:
            case RTK_MUT_POINTER:
            case RTK_ARRAY:
            case RTK_FUNCTION_DECL:
            case RTK_FUNCTION_REF:
            case RTK_STRUCT_DECL:
            case RTK_STRUCT_REF:
            case RTK_GENERIC:
                break;

            default: false;
        }

        switch (to->kind) {
            case RTK_VOID:
            case RTK_BOOL:
            case RTK_CHAR:
            case RTK_INT:
            case RTK_INT8:
            case RTK_INT16:
            case RTK_INT32:
            case RTK_INT64:
            case RTK_UINT:
            case RTK_UINT8:
            case RTK_UINT16:
            case RTK_UINT32:
            case RTK_UINT64:
            case RTK_FLOAT:
            case RTK_FLOAT32:
            case RTK_FLOAT64:
            case RTK_POINTER:
            case RTK_MUT_POINTER:
            case RTK_ARRAY:
            case RTK_FUNCTION_DECL:
            case RTK_FUNCTION_REF:
            case RTK_STRUCT_DECL:
            case RTK_STRUCT_REF:
            case RTK_GENERIC:
                return true;

            default: false;
        }
     }

    switch (from->kind) {
        case RTK_BOOL:
        case RTK_CHAR:
        case RTK_INT8:
        case RTK_UINT8:
        {
            switch (to->kind) {
                case RTK_BOOL:
                case RTK_CHAR:
                case RTK_INT:
                case RTK_INT8:
                case RTK_INT16:
                case RTK_INT32:
                case RTK_INT64:
                case RTK_UINT:
                case RTK_UINT8:
                case RTK_UINT16:
                case RTK_UINT32:
                case RTK_UINT64:
                case RTK_FLOAT:
                case RTK_FLOAT32:
                case RTK_FLOAT64:
                    return true;

                default: return false;
            }
        }

        case RTK_INT16:
        case RTK_UINT16:
        {
            switch (to->kind) {
                case RTK_BOOL:
                case RTK_INT:
                case RTK_INT16:
                case RTK_INT32:
                case RTK_INT64:
                case RTK_UINT:
                case RTK_UINT16:
                case RTK_UINT32:
                case RTK_UINT64:
                case RTK_FLOAT:
                case RTK_FLOAT32:
                case RTK_FLOAT64:
                    return true;

                default: return false;
            }
        }

        case RTK_INT32:
        case RTK_UINT32:
        case RTK_FLOAT32:
        {
            switch (to->kind) {
                case RTK_BOOL:
                case RTK_INT:
                case RTK_INT16:
                case RTK_INT32:
                case RTK_INT64:
                case RTK_UINT:
                case RTK_UINT16:
                case RTK_UINT32:
                case RTK_UINT64:
                case RTK_FLOAT:
                case RTK_FLOAT32:
                case RTK_FLOAT64:
                    return true;

                default: return false;
            }
        }

        case RTK_INT:
        case RTK_INT64:
        case RTK_UINT:
        case RTK_UINT64:
        case RTK_FLOAT:
        case RTK_FLOAT64:
        {
            switch (to->kind) {
                case RTK_BOOL:
                case RTK_INT:
                case RTK_INT64:
                case RTK_UINT:
                case RTK_UINT64:
                case RTK_FLOAT:
                case RTK_FLOAT64:
                case RTK_POINTER:
                case RTK_MUT_POINTER:
                    return true;

                default: return false;
            }
        }

        case RTK_POINTER: switch (to->kind) {
            case RTK_POINTER: return resolved_type_implict_to(from->type.ptr.of, to->type.ptr.of);
            case RTK_MUT_POINTER: return resolved_type_implict_to(from->type.ptr.of, to->type.mut_ptr.of);
            default: return false;
        }

        case RTK_MUT_POINTER: switch (to->kind) {
            case RTK_POINTER: return resolved_type_eq(from->type.mut_ptr.of, to->type.ptr.of);
            case RTK_MUT_POINTER: return resolved_type_eq(from->type.mut_ptr.of, to->type.mut_ptr.of);
            default: return false;
        }

        case RTK_STRUCT_REF: switch (to->kind) {
            case RTK_STRUCT_REF: {
                if (from->type.struct_ref.generic_args.length != to->type.struct_ref.generic_args.length) {
                    return false;
                }
                for (size_t i = 0; i < from->type.struct_ref.generic_args.length; ++i) {
                    if (!resolved_type_implict_to(from->type.struct_ref.generic_args.resolved_types + i, to->type.struct_ref.generic_args.resolved_types + i)) {
                        return false;
                    }
                }
                return resolved_struct_decl_eq(&to->type.struct_ref.decl, &to->type.struct_ref.decl);
            }
            case RTK_STRUCT_DECL: return resolved_struct_decl_eq(&from->type.struct_ref.decl, &to->type.struct_decl);
            default: return false;
        }

        case RTK_STRUCT_DECL: switch (to->kind) {
            case RTK_STRUCT_REF: return resolved_struct_decl_eq(&from->type.struct_decl, &to->type.struct_ref.decl);
            case RTK_STRUCT_DECL: return resolved_struct_decl_eq(&from->type.struct_decl, &to->type.struct_decl);
            default: return false;
        }

        default: return false;
    }
}

bool resolved_type_cast_to(ResolvedType* from, ResolvedType* to) {
    if (resolved_type_implict_to(from, to)) {
        return true;
    }

    if (
        (from->kind == RTK_POINTER || from->kind == RTK_MUT_POINTER)
        && (to->kind == RTK_POINTER || to->kind == RTK_MUT_POINTER)
    ) {
        return true;
    }

    switch (from->kind) {
        case RTK_BOOL:
        case RTK_CHAR:
        case RTK_INT8:
        case RTK_INT16:
        case RTK_INT32:
        case RTK_INT64:
        case RTK_UINT8:
        case RTK_UINT16:
        case RTK_UINT32:
        case RTK_UINT64:
        case RTK_FLOAT:
        case RTK_FLOAT32:
        case RTK_FLOAT64:
        case RTK_POINTER:
        case RTK_MUT_POINTER:
        {
            switch (to->kind) {
                case RTK_BOOL:
                case RTK_CHAR:
                case RTK_INT:
                case RTK_INT8:
                case RTK_INT16:
                case RTK_INT32:
                case RTK_INT64:
                case RTK_UINT:
                case RTK_UINT8:
                case RTK_UINT16:
                case RTK_UINT32:
                case RTK_UINT64:
                case RTK_FLOAT:
                case RTK_FLOAT32:
                case RTK_FLOAT64:
                case RTK_POINTER:
                case RTK_MUT_POINTER:
                    return true;

                default: return false;
            }
        }

        case RTK_STRUCT_REF: switch (to->kind) {
            case RTK_STRUCT_REF:
            case RTK_STRUCT_DECL:
                return true;

            default: return false;
        }

        case RTK_STRUCT_DECL: switch (to->kind) {
            case RTK_STRUCT_REF:
            case RTK_STRUCT_DECL:
                return true;

            default: return false;
        }

        default: return false;
    }
}

void print_resolved_type(ResolvedType* rt) {
    if (!rt) {
        printf("null");
        return;
    }

    switch (rt->kind) {
        case RTK_VOID: printf("void"); break;
        case RTK_BOOL: printf("bool"); break;
        case RTK_CHAR: printf("char"); break;
        case RTK_INT: printf("int"); break;
        case RTK_INT8: printf("int8"); break;
        case RTK_INT16: printf("int16"); break;
        case RTK_INT32: printf("int32"); break;
        case RTK_INT64: printf("int64"); break;
        case RTK_UINT: printf("uint"); break;
        case RTK_UINT8: printf("uint8"); break;
        case RTK_UINT16: printf("uint16"); break;
        case RTK_UINT32: printf("uint32"); break;
        case RTK_UINT64: printf("uint64"); break;
        case RTK_FLOAT: printf("float"); break;
        case RTK_FLOAT32: printf("float32"); break;
        case RTK_FLOAT64: printf("float64"); break;

        case RTK_POINTER: {
            print_resolved_type(rt->type.ptr.of);
            printf("*");
            break;
        }

        case RTK_MUT_POINTER: {
            print_resolved_type(rt->type.mut_ptr.of);
            printf("*");
            break;
        }

        case RTK_ARRAY: {
            print_resolved_type(rt->type.array.of);
            printf("[");
            if (rt->type.array.has_explicit_length) {
                printf("%lu", rt->type.array.explicit_length);
            }
            printf("]");
            break;
        }

        case RTK_FUNCTION_DECL: {
            print_resolved_type(rt->type.function_decl.return_type);
            printf(" ");
            if (rt->type.function_decl.generic_params.length > 0) {
                printf("<");
                for (size_t i = 0; i < rt->type.function_decl.generic_params.length; ++i) {
                    if (i > 0) {
                        printf(", ");
                    }
                    print_string(rt->type.function_decl.generic_params.strings[i]);
                }
                printf(">");
            }
            printf("(");
            for (size_t i = 0; i < rt->type.function_decl.params_length; ++i) {
                if (i > 0) {
                    printf(", ");
                }
                print_resolved_type(rt->type.function_decl.params[i].type);
                printf(" ");
                print_string(rt->type.function_decl.params[i].name);
            }
            printf(") { ... }");
            break;
        }

        case RTK_FUNCTION_REF: {
            printf("<name_unknown>");
            if (rt->type.function_ref.generic_args.length > 0) {
                printf("<");
                for (size_t i = 0; i < rt->type.function_ref.generic_args.length; ++i) {
                    if (i > 0) {
                        printf(", ");
                    }
                    print_resolved_type(rt->type.function_ref.generic_args.resolved_types + i);
                }
                printf(">");
            }
            break;
        }

        case RTK_STRUCT_DECL: {
            print_string(rt->type.struct_decl.name);
            if (rt->type.struct_decl.generic_params.length > 0) {
                printf("<");
                for (size_t i = 0; i < rt->type.struct_decl.generic_params.length; ++i) {
                    if (i > 0) {
                        printf(", ");
                    }
                    print_string(rt->type.struct_decl.generic_params.strings[i]);
                }
                printf(">");
            }
            printf(" {\n");
            for (size_t i = 0; i < rt->type.struct_decl.fields_length; ++i) {
                printf("    ");
                print_resolved_type(rt->type.struct_decl.fields[i].type);
                printf(" ");
                print_string(rt->type.struct_decl.fields[i].name);
                printf(",\n");
            }
            printf("}");
            break;
        }

        case RTK_STRUCT_REF: {
            print_string(rt->type.struct_ref.decl.name);
            if (rt->type.struct_ref.generic_args.length > 0) {
                printf("<");
                for (size_t i = 0; i < rt->type.struct_ref.generic_args.length; ++i) {
                    if (i > 0) {
                        printf(", ");
                    }
                    print_resolved_type(rt->type.struct_ref.generic_args.resolved_types + i);
                }
                printf(">");
            }
            break;
        }

        case RTK_GENERIC: {
            printf("<%lu:", rt->type.generic.idx);
            print_string(rt->type.generic.name);
            printf(">");
            break;
        }

        case RTK_NAMESPACE:
        case RTK_TERMINAL:
        {
            printf("RTK_%d", rt->kind);
            return;
        }

        case RTK_COUNT: assert(false);
    }
}
