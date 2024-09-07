#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/common.h"
#include "../lib/fs.h"
#include "../lib/lexer.h"

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: adam [path]\n");
        return EXIT_FAILURE;
    }

    Allocator const alloc = allocator_create();

    char const* source = read_file(alloc, argv[1]);

    printf("---SOURCE---\n%s\n-^--------^-\n", source);

    printf("\n\n");

    Lexer lexer = lexer_create(alloc, source);
    ScanResult scan_res = lexer_scan(lexer);

    scanres_assert(scan_res);
    Tokens tokens = scan_res.res.tokens;

    // print out tokens
    for (size_t i = 0; i < tokens.length; ++i) {
        Token token = tokens.arr[i];

        char* token_str = alloc.calloc(token.length, sizeof(char));
        strncpy(token_str, token.start, token.length);

        printf("%lu | %d | %s\n", token.line, token.type, token_str);

        free(token_str);
    }

    // cleanup
    alloc.free(tokens.arr);
    alloc.free((void*)source);

    return EXIT_SUCCESS;
}
