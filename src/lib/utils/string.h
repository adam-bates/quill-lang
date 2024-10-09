#ifndef quill_string_h
#define quill_string_h

#include "./base.h"
#include "./arena.h"

typedef struct {
    size_t length;
    char*  chars;
} String;

String c_str(char* const chars);

bool str_eq(String str1, String str2);

typedef struct {
    size_t  length;
    String* strings;
} Strings;

void strs_remove(Strings* const strs, size_t const idx);

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    String* array;
} ArrayList_String;

ArrayList_String arraylist_string_create(Arena* const arena);
ArrayList_String arraylist_string_create_with_capacity(Arena* const arena, size_t const capacity);

void arraylist_string_push(ArrayList_String* const list, String const string);

#endif
