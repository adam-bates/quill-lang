#ifndef quill_package_h
#define quill_package_h

#include "./ast.h"
#include "./resolved_type.h"
#include "../utils/utils.h"

typedef struct {
    size_t capacity;
    size_t length;
    Package* array;
} ArrayList_Package;

typedef enum {
    TIS_UNKNOWN,
    TIS_HUNCH,
    TIS_CONFIDENT,
    TIS_COUNT
} TypeInfoStatus;

typedef struct {
    TypeInfoStatus status;
    ResolvedType* type;
} TypeInfo;

typedef struct {
    size_t length;
    ResolvedType** resolved_types;
} GenericImpl;

typedef struct LLNode_GenericImpl {
    struct LLNode_GenericImpl* next;
    GenericImpl data;
} LLNode_GenericImpl;

typedef struct {
    size_t length;
    LLNode_GenericImpl* head;
    LLNode_GenericImpl* tail;
} LL_GenericImpl;

typedef struct {
    Arena* arena;

    size_t count;

    // HashTable<String, Package>
    size_t lookup_length;
    ArrayList_Package* lookup_buckets;

    // AST Node ID indexes into type info
    size_t types_length;
    TypeInfo* types;

    size_t generic_impls_nodes_length;
    LL_GenericImpl* generic_impls_nodes_raw;
    LL_GenericImpl* generic_impls_nodes_concrete;

    ResolvedType* string_literal_type;
    ResolvedType* string_template_type;
    ResolvedType* range_literal_type;
} Packages;

Packages packages_create(Arena* arena);
Package* packages_resolve(Packages* packages, PackagePath* name);
Package* packages_resolve_or_create(Packages* packages, PackagePath* name);

TypeInfo* packages_type_by_node(Packages* packages, NodeId node_id);
TypeInfo* packages_type_by_type(Packages* packages, TypeId type_id);

void packages_register_generic_impl(Packages* packages, ASTNode* src, size_t resolved_types_length, ResolvedType** resolved_types);

void ll_generic_impls_push(Arena* arena, LL_GenericImpl* generic_impls, GenericImpl generic_impl);

#endif
