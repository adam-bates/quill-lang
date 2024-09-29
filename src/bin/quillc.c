#include <stdio.h>
#include <string.h>

#include "../lib/quill.h"

int main(int const argc, char* const argv[]) {
    QuillcArgs args = {0};
    parse_args(&args, argc, argv);

    assert(args.paths_to_include.length > 0);

    // TODO: start using args, load muliple files

    // if (argc != 2) {
    //     fprintf(stderr, "Usage: quillc [path]\n");
    //     return EXIT_FAILURE;
    // }
    String const source_path = args.paths_to_include.strings[0];

    MaybeAllocator const m_allocator = allocator_create();
    if (!m_allocator.ok) {
        fprintf(stderr, "Couldn't initialize allocator");
        return EXIT_FAILURE;
    }
    Allocator const allocator = m_allocator.maybe.allocator;

    String const source = file_read(allocator, source_path);

    Lexer lexer = lexer_create(allocator, source);
    ScanResult const scan_res = lexer_scan(&lexer);

    scanres_assert(scan_res);
    ArrayList_Token const tokens = scan_res.res.tokens;

    Arena parser_arena = {0};
    Parser parser = parser_create(&parser_arena, tokens);
    ASTNodeResult const ast_res = parser_parse(&parser);

    astres_assert(ast_res);
    ASTNode const* const ast = ast_res.res.ast;

    printf("AST:");
    if (parser.had_error) {
        printf(" (partial due to errors)");
    }
    printf("\n");

    print_astnode(*ast);
    printf("\n");

    verify_syntax(ast);

    Arena codegen_arena = {0};

    CodegenC codegen = codegen_c_create(&codegen_arena, ast);
    String const c_code = generate_c_code(&codegen);

    printf("C Code:\n");
    printf("%s\n", arena_strcpy(&codegen_arena, c_code).chars);

    // cleanup
    arena_free(&parser_arena);
    arraylist_token_destroy(tokens);
    quill_free(allocator, (void*)source.chars);

    return EXIT_SUCCESS;
}
