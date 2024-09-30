#include <stdio.h>
#include <string.h>

#include "../lib/quill.h"

int main(int const argc, char* const argv[]) {
    Arena arena = {0};

    QuillcArgs args = {0};
    parse_args(&arena, &args, argc, argv);
    assert(args.paths_to_include.length > 0);

    ArrayList_Token* token_lists = arena_calloc(&arena, args.paths_to_include.length, sizeof(*token_lists));
    size_t token_lists_len = 0;

    Strings sources = {
        .length = 0,
        .strings = arena_calloc(&arena, args.paths_to_include.length, sizeof(String)),
    };

    Packages packages = packages_create(&arena);

    // TODO: actually handle multiple files.
    ASTNode const* ast;

    size_t next_node_id = 0;

    for (size_t i = 0; i < args.paths_to_include.length; ++i) {
        String const source_path = args.paths_to_include.strings[i];

        String const source = file_read(&arena, source_path);

        Lexer lexer = lexer_create(&arena, source);
        ScanResult const scan_res = lexer_scan(&lexer);

        scanres_assert(scan_res);
        ArrayList_Token const tokens = scan_res.res.tokens;

        Parser parser = parser_create(&arena, tokens);

        parser.next_id = next_node_id;
        ASTNodeResult const ast_res = parser_parse(&parser);
        next_node_id = parser.next_id;

        astres_assert(ast_res);
        ast = ast_res.res.ast;

        verify_syntax(ast);

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

    CodegenC codegen = codegen_c_create(&arena, ast);
    String const c_code = generate_c_code(&codegen);

    printf("C Code:\n");
    printf("%s\n", arena_strcpy(&arena, c_code).chars);

    // cleanup
    arena_free(&arena);

    return EXIT_SUCCESS;
}
