#include <stdio.h>
#include <stdlib.h>

#include "./utils.h"

String file_read(Allocator const allocator, String path_s) {
    char const* path = path_s.chars;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Could not seek end of file \"%s\".\n", path);
        exit(74);
    };

    size_t file_size = ftell(file);
    if (file_size == 0) {
        fprintf(stderr, "File is empty, or could not tell size of fize \"%s\".\n", path);
        exit(74);
    }
    
    rewind(file);

    char *buffer = (char*)quill_malloc(allocator, file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    
    buffer[bytes_read] = '\0';

    fclose(file);

    return c_str(buffer);
}

