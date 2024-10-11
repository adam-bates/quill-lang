#ifndef quill_resolved_type_h
#define quill_resolved_type_h

#include "./ast.h"
#include "../utils/utils.h"

typedef struct {
    PackagePath* full_name;
    ASTNode const* ast;
    bool is_entry;
} Package;

typedef enum {
    RTK_NAMESPACE,
    RTK_VOID,
    RTK_INT,
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
    String name;
    Strings generic_params;
    size_t fields_length;
    ResolvedStructField* fields;
} ResolvedStruct;

typedef struct ResolvedType {
    ResolvedTypeKind kind;
    Package* from_pkg;
    ASTNode const* src;
    union {
        Package* namespace_;
        void* void_;
        void* int_;
        void* uint_;
        void* char_;
        ResolvedTypePointer ptr;
        ResolvedTypePointer mut_ptr;
        ResolvedFunction function;
        ResolvedStruct struct_;
    } type;
} ResolvedType;

#endif
