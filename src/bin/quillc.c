#include <stdio.h>
#include <string.h>

#include "../lib/quillc.h"

int main(int const argc, char const* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: quillc [path]\n");
        return EXIT_FAILURE;
    }

    MaybeAllocator const m_allocator = allocator_create();
    if (!m_allocator.ok) {
        fprintf(stderr, "Couldn't initialize allocator");
        return EXIT_FAILURE;
    }
    Allocator const allocator = m_allocator.maybe.allocator;

    String const source = read_file(allocator, argv[1]);

    printf("---SOURCE---\n%s\n-^--------^-\n", source.chars);

    printf("\n\n");

    Lexer lexer = lexer_create(allocator, source);
    ScanResult const scan_res = lexer_scan(&lexer);

    scanres_assert(scan_res);
    ArrayList_Token const tokens = scan_res.res.tokens;

    // print out tokens
    for (size_t i = 0; i < tokens.length; ++i) {
        Token const token = tokens.array[i];

        char* token_str = quill_calloc(allocator, token.length, sizeof(char));
        strncpy(token_str, token.start, token.length);

        printf("%lu | %d | %s\n", token.line, token.type, token_str);

        quill_free(allocator, token_str);
    }

    // cleanup
    quill_free(allocator, tokens.array);
    quill_free(allocator, (void*)source.chars);

    return EXIT_SUCCESS;
}
