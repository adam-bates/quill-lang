#include <stdio.h>
#include <string.h>

#include "./package.h"

#define INITIAL_PACKAGES_CAPACITY 1
#define HASHTABLE_BUCKETS 16

#define FNV_OFFSET_BASIS 14695981039346656037ULL
#define FNV_PRIME 1099511628211ULL

static ArrayList_Package arraylist_package_create_with_capacity(Arena* arena, size_t const capacity) {
    Package* array = arena_alloc(arena, capacity * sizeof(Package));

    return (ArrayList_Package){
        .capacity = capacity,
        .length = 0,

        .array = array,
    };
}

static ArrayList_Package arraylist_package_create(Arena* arena) {
    return arraylist_package_create_with_capacity(arena, INITIAL_PACKAGES_CAPACITY);
}

static void arraylist_package_push(Arena* arena, ArrayList_Package* const list, Package const package) {
    if (list->length >= list->capacity) {
        size_t prev_cap = list->capacity;
        list->capacity = list->length * 2;
        list->array = arena_realloc(arena, list->array, sizeof(Package) * prev_cap, sizeof(Package) * list->capacity);
    }

    list->array[list->length] = package;
    list->length += 1;
}

static size_t hash_name(PackagePath* name) {
    if (!name) { return 0; }

    // FNV-1a
    size_t hash = FNV_OFFSET_BASIS;
    PackagePath* curr = name;
    while (curr) {
        for (size_t i = 0; i < curr->name.length; ++i) {
            char c = curr->name.chars[i];
            hash ^= c;
            hash *= FNV_PRIME;
        }
        curr = curr->child;
    }

    // map to bucket index
    // note: reserve index 0 for no package name
    hash %= HASHTABLE_BUCKETS - 1;
    return 1 + hash;
}

Package* packages_resolve_or_create(Packages* packages, PackagePath* name) {
    size_t idx = hash_name(name);
    ArrayList_Package* bucket = packages->lookup_buckets + idx;
    if (bucket->array == NULL) {
        *bucket = arraylist_package_create(packages->arena);
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        if (package_path_eq(bucket->array[i].full_name, name)) {
            return bucket->array + i;
        }
    }
    arraylist_package_push(packages->arena, bucket, (Package){
        .full_name = name,
        .ast = NULL,
    });
    packages->count += 1;

    return bucket->array + (bucket->length - 1);
}

Package* packages_resolve(Packages* packages, PackagePath* name) {
    size_t idx = hash_name(name);
    ArrayList_Package* bucket = packages->lookup_buckets + idx;
    if (bucket->array == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        if (package_path_eq(bucket->array[i].full_name, name)) {
            return bucket->array + i;
        }
    }

    return NULL;
}

Packages packages_create(Arena* arena) {
    return (Packages){
        .arena = arena,

        .lookup_length = HASHTABLE_BUCKETS,
        .lookup_buckets = arena_calloc(arena, HASHTABLE_BUCKETS, sizeof(ArrayList_Package)),

        .types_length = 0,
        .types = NULL,

        .count = 0,

        .string_literal_type = NULL,
    };
}

TypeInfo* packages_type_by_node(Packages* packages, NodeId node_id) {
    return packages->types + node_id.val;
}

TypeInfo* packages_type_by_type(Packages* packages, TypeId type_id) {
    return packages->types + (packages->types_length - 1 - type_id.val);
}
