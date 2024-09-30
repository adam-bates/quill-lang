#include <stdio.h>
#include <string.h>

#include "../lib/quill.h"

int main(int const argc, char* const argv[]) {
    QuillcArgs args = {0};
    parse_args(&args, argc, argv);
    assert(args.paths_to_include.length > 0);

    MaybeAllocator const m_allocator = allocator_create();
    if (!m_allocator.ok) {
        fprintf(stderr, "Couldn't initialize allocator");
        return EXIT_FAILURE;
    }
    Allocator const allocator = m_allocator.maybe.allocator;

    Arena arena = {0};

    ArrayList_Token* token_lists = malloc(sizeof(*token_lists) * args.paths_to_include.length);
    size_t token_lists_len = 0;

    Strings sources = {
        .length = 0,
        .strings = malloc(sizeof(String) * args.paths_to_include.length),
    };

    Packages packages = packages_create(&arena);

    // TODO: actually handle multiple files.
    ASTNode const* ast;

    size_t next_node_id = 0;

    for (size_t i = 0; i < args.paths_to_include.length; ++i) {
        String const source_path = args.paths_to_include.strings[i];

        String const source = file_read(allocator, source_path);

        Lexer lexer = lexer_create(allocator, source);
        ScanResult const scan_res = lexer_scan(&lexer);

        scanres_assert(scan_res);
        ArrayList_Token const tokens = scan_res.res.tokens;

        Parser parser = parser_create(&arena, tokens);

        parser.next_id = next_node_id;
        ASTNodeResult const ast_res = parser_parse(&parser);
        next_node_id = parser.next_id;

        astres_assert(ast_res);
        ast = ast_res.res.ast;

        printf("AST:");
        if (parser.had_error) {
            printf(" (partial due to errors)");
        }
        printf("\n");

        print_astnode(*ast);
        printf("\n");

        StaticPath* package_name = NULL;
        if (parser.package) {
            package_name = parser.package->node.package.static_path;
        }

        Package* pkg = packages_resolve(&packages, package_name);
        assert(!pkg->ast);
        pkg->ast = ast;

        token_lists[token_lists_len++] = tokens;
        sources.strings[sources.length++] = source;
    }
    assert(ast);

    verify_syntax(ast);

    Arena codegen_arena = {0};

    CodegenC codegen = codegen_c_create(&codegen_arena, ast);
    String const c_code = generate_c_code(&codegen);

    printf("C Code:\n");
    printf("%s\n", arena_strcpy(&codegen_arena, c_code).chars);

    // cleanup
    arena_free(&arena);
    {
        for (size_t i = 0; i < token_lists_len; ++i) {
            arraylist_token_destroy(token_lists[i]);
        }
        free(token_lists);
    }
    {
        for (size_t i = 0; i < sources.length; ++i) {
            quill_free(allocator, (void*)sources.strings[i].chars);
        }
        free(sources.strings);
    }

    return EXIT_SUCCESS;
}
