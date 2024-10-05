#ifndef quill_resolved_type_h
#define quill_resolved_type_h

#include "./ast.h"
#include "../utils/utils.h"

typedef enum {
    RTK_NAMESPACE,
    RTK_VOID,
    RTK_UINT,
    RTK_CHAR,
    RTK_POINTER,
    RTK_MUT_POINTER,
    RTK_FUNCTION,
    RTK_STRUCT,

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
    struct ResolvedType* type;
    String name;
} ResolvedFunctionParam;

typedef struct {
    size_t params_length;
    ResolvedFunctionParam* params;
    struct ResolvedType* return_type;
} ResolvedFunction;

typedef struct {
    struct ResolvedType* type;
    String name;
} ResolvedStructField;

typedef struct {
    size_t fields_length;
    ResolvedStructField* fields;
} ResolvedStruct;

typedef struct ResolvedType {
    ResolvedTypeKind kind;
    ASTNode const* src;
    union {
        ASTNode const* namespace_;
        void* void_;
        void* uint_;
        void* char_;
        ResolvedTypePointer ptr;
        ResolvedTypePointer mut_ptr;
        ResolvedFunction function;
        ResolvedStruct struct_;
    } type;
} ResolvedType;

#endif
