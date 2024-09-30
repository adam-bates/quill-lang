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

// HashTable<String, Package>
typedef struct {
    Arena* arena;
    ArrayList_Package* lookup_buckets;
    size_t lookup_length;
} Packages;

Packages packages_create(Arena* arena);
Package* packages_resolve(Packages* packages, StaticPath* name);

#endif
