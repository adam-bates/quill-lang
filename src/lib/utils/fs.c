#include <stdio.h>
#include <stdlib.h>

#include "./utils.h"
#include "./string_buffer.h"

String file_read(Arena* arena, String path_s) {
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

    char *buffer = arena_alloc(arena, file_size + 1);
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

void write_file(Arena* arena, String dir, String filepath, String content) {
    StringBuffer sb = strbuf_create(arena);
    strbuf_append_str(&sb, dir);
    strbuf_append_char(&sb, '/');
    strbuf_append_str(&sb, filepath);
    String path_s = strbuf_to_strcpy(sb);

    printf("// \"%s\"\n", arena_strcpy(arena, path_s).chars);
    printf("%s\n", arena_strcpy(arena, content).chars);

    char* path = path_s.chars;

    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_written = fwrite(content.chars, sizeof(char), content.length, file);
    assert(bytes_written == content.length * sizeof(char));
}
