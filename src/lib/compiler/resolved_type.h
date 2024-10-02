#ifndef quill_resolved_type_h
#define quill_resolved_type_h

#include "./ast.h"
#include "../utils/utils.h"

typedef enum {
    RTK_NAMESPACE,

    RTK_COUNT
} ResolvedTypeKind;

typedef struct {
    String key;
    struct ResolvedType* value;
} StringResolvedType;

typedef struct {
    StringResolvedType* kvs;
} ResolvedTypeNamespace;

typedef struct ResolvedType {
    ResolvedTypeKind kind;
    ASTNode const* src;
    union {
        // TODO
        ASTNode const* package_ast;
    } type;
} ResolvedType;

#endif
