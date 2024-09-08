#include <stdio.h>
#include <string.h>

#include "../lib/quillc.h"

int main(int const argc, char const* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: quillc [path]\n");
        return EXIT_FAILURE;
    }

    Allocator_s const allocator_s = allocator_create();
    Allocator const allocator = &allocator_s;

    String const source = read_file(allocator, argv[1]);

    printf("---SOURCE---\n%s\n-^--------^-\n", source.chars);

    printf("\n\n");

    Lexer const lexer = lexer_create(allocator, source);
    ScanResult const scan_res = lexer_scan(lexer);

    scanres_assert(scan_res);
    ArrayList_Token const tokens = scan_res.res.tokens;

    // print out tokens
    for (size_t i = 0; i < tokens.length; ++i) {
        Token const token = tokens.array[i];

        char* token_str = allocator->calloc(token.length, sizeof(char));
        strncpy(token_str, token.start, token.length);

        printf("%lu | %d | %s\n", token.line, token.type, token_str);

        allocator->free(token_str);
    }

    // cleanup
    allocator->free(tokens.array);
    allocator->free((void*)source.chars);

    return EXIT_SUCCESS;
}
