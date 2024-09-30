#include <string.h>

#include "./package.h"

#define INITIAL_PACKAGES_CAPACITY 1
#define HASHTABLE_BUCKETS 16

#define FNV_PRIME 16777619U
#define FNV_OFFSET_BASIS 2166136261U

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
        list->capacity = list->length * 2;
        list->array = arena_realloc(arena, list->array, list->capacity, sizeof(Token) * list->capacity);
    }

    list->array[list->length] = package;
    list->length += 1;
}

static size_t hash_name(StaticPath* name) {
    if (!name) { return 0; }

    // FNV-1a
    size_t hash = FNV_OFFSET_BASIS;
    StaticPath* curr = name;
    while (curr) {
        for (size_t i = 0; i < curr->name.length; ++i) {
            char c = curr->name.chars[i];
            hash ^= c;
            hash *= FNV_PRIME;
        }
        curr = curr->root;
    }

    // map to bucket index
    // note: reserve index 0 for no package name
    hash %= HASHTABLE_BUCKETS - 1;
    return 1 + hash;
}

static bool name_eq(StaticPath* name1, StaticPath* name2) {
    if (!name1 || !name2) {
        return !name1 && !name2;
    }
    
    if ((!name1->root) != (!name2->root)) {
        return false;
    }

    if (name1->name.length != name2->name.length) {
        return false;
    }

    if (strncmp(name1->name.chars, name2->name.chars, name1->name.length) != 0) {
        return false;
    }

    if (name1->root) {
        return name_eq(name1->root, name2->root);
    } else {
        return true;
    }
}

Package* packages_resolve(Packages* packages, StaticPath* name) {
    size_t idx = hash_name(name);
    ArrayList_Package* bucket = packages->lookup_buckets + idx;
    if (bucket->array == NULL) {
        *bucket = arraylist_package_create(packages->arena);
    }

    for (size_t i = 0; i < bucket->length; ++i) {
        if (name_eq(bucket->array[i].full_name, name)) {
            return bucket->array + i;
        }
    }
    arraylist_package_push(packages->arena, bucket, (Package){
        .full_name = name,
        .ast = NULL,
    });

    return bucket->array + (bucket->length - 1);
}

Packages packages_create(Arena* arena) {
    return (Packages){
        .arena = arena,
        .lookup_buckets = arena_alloc(arena, HASHTABLE_BUCKETS * sizeof(ArrayList_Package)),
        .lookup_length = 0,
    };
}
