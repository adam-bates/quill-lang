#ifndef quill_resolved_type_h
#define quill_resolved_type_h

#include "./ast.h"
#include "../utils/utils.h"

typedef struct {
    PackagePath* full_name;
    ASTNode const* ast;
    bool is_entry;
} Package;

typedef struct {
    size_t length;
    struct ResolvedType* resolved_types;
} ResolvedTypes;

typedef enum {
    RTK_NAMESPACE,
    RTK_VOID,
    RTK_BOOL,
    RTK_INT,
    RTK_UINT,
    RTK_CHAR,
    RTK_POINTER,
    RTK_MUT_POINTER,
    RTK_FUNCTION,
    RTK_ARRAY,

    // separate struct decl vs ref because
    // generic params vs generic args
    RTK_STRUCT_DECL,
    RTK_STRUCT_REF,

    RTK_GENERIC,

    RTK_TERMINAL,

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
    bool has_explicit_length;
    size_t explicit_length;
    struct ResolvedType* of;
} ResolvedArray;

typedef struct {
    struct ResolvedType* type;
    String name;
} ResolvedStructField;

typedef struct {
    String name;
    Strings generic_params;
    size_t fields_length;
    ResolvedStructField* fields;
} ResolvedStructDecl;

typedef struct {
    ResolvedStructDecl decl;
    ResolvedTypes generic_args;
    size_t impl_version;
} ResolvedStructRef;

typedef struct {
    String name;
} ResolvedGeneric;

typedef void* ResolvedTerminal;

typedef struct ResolvedType {
    ResolvedTypeKind kind;
    Package* from_pkg;
    ASTNode* src;
    union {
        Package* namespace_;
        void* void_;
        void* int_;
        void* uint_;
        void* char_;
        ResolvedTypePointer ptr;
        ResolvedTypePointer mut_ptr;
        ResolvedFunction function;
        ResolvedArray array;
        ResolvedStructDecl struct_decl;
        ResolvedStructRef struct_ref;
        ResolvedGeneric generic;
        ResolvedTerminal terminal;
    } type;
} ResolvedType;

#endif
