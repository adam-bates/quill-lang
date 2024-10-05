#ifndef quill_resolved_type_h
#define quill_resolved_type_h

#include "./ast.h"
#include "../utils/utils.h"

typedef enum {
    RTK_NAMESPACE,
    RTK_VOID,
    RTK_POINTER,
    RTK_MUT_POINTER,
    RTK_FUNCTION,

    RTK_COUNT
} ResolvedTypeKind;

typedef struct {
    String key;
    struct ResolvedType* value;
} StringResolvedType;

typedef struct {
    StringResolvedType* kvs;
} ResolvedTypeNamespace;

typedef struct {
    struct ResolvedType* of;
} ResolvedTypePointer;

typedef struct {
    size_t param_types_length;
    struct ResolvedType* param_types;
    struct ResolvedType* return_type;
} ResolvedFunction;

typedef struct ResolvedType {
    ResolvedTypeKind kind;
    ASTNode const* src;
    union {
        ASTNode const* namespace_;
        void* void_;
        ResolvedTypePointer ptr;
        ResolvedTypePointer mut_ptr;
        ResolvedFunction function;
    } type;
} ResolvedType;

#endif
