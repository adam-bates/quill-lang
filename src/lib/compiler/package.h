#ifndef quill_package_h
#define quill_package_h

#include "./ast.h"
#include "../utils/utils.h"

typedef struct {
    StaticPath* full_name;
    ASTNode const* ast;
} Package;

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
    Type* type;
} TypeInfo;

typedef struct {
    Arena* arena;

    // HashTable<String, Package>
    size_t lookup_length;
    ArrayList_Package* lookup_buckets;

    // AST Node ID indexes into type info
    size_t types_length;
    TypeInfo* types;
} Packages;

Packages packages_create(Arena* arena);
Package* packages_resolve(Packages* packages, StaticPath* name);

#endif
