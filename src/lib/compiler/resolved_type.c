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
        case RTK_BOOL:
        case RTK_CHAR:
        case RTK_UINT:
        case RTK_INT:
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
            case RTK_STRUCT_REF: return resolved_struct_decl_eq(&a->type.struct_ref.decl, &b->type.struct_ref.decl);
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
            return str_eq(a->type.generic.name, b->type.generic.name);
        }

        default: printf("TODO: resolved_type_eq(%d)\n", a->kind); assert(false); // TODO
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

    switch (to->kind) {
        case RTK_INT:
        case RTK_UINT:
        case RTK_BOOL:
        case RTK_CHAR:
        case RTK_POINTER:
        case RTK_MUT_POINTER:
        {
            switch (from->kind) {
                case RTK_INT:
                case RTK_UINT:
                case RTK_BOOL:
                case RTK_CHAR:
                case RTK_POINTER:
                case RTK_MUT_POINTER:
                    return true;

                default: return false;
            }
        }

        default: return false;
    }
}
