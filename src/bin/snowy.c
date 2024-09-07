#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/common.h"
#include "../lib/fs.h"
#include "../lib/scanner.h"

int main(int argc, char const* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: adam [path]\n");
        return EXIT_FAILURE;
    }

    char const* source = read_file(argv[1]);

    printf("---SOURCE---\n%s\n-^--------^-\n", source);

    printf("\n\n");

    Scanner s = scanner_create(source);

    ScanResult scan_res = scanner_scan(s);
    if (!scan_res.ok) {
        fprintf(stderr, "Error(%d): %s\n", scan_res.res.err.type, scan_res.res.err.msg);
        return EXIT_FAILURE;
    }
    Tokens tokens = scan_res.res.val;

    // print out tokens
    for (size_t i = 0; i < tokens.length; ++i) {
        Token token = tokens.arr[i];

        char* token_str = calloc(token.length, sizeof(char));
        strncpy(token_str, token.start, token.length);

        printf("%lu | %d | %s\n", token.line, token.type, token_str);

        free(token_str);
    }

    // cleanup
    free(tokens.arr);
    free((void*)source);

    return EXIT_SUCCESS;
}
