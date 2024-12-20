#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../lib/quill.h"

int main(int const argc, char* const argv[]) {    
    // TODO: optimize memory usage; use smaller-scoped arenas within an area, result stored in this arena.
    Arena arena = {0};

    QuillcArgs args = {0};
    parse_args(&arena, &args, argc, argv);
    assert(args.paths_to_include.length > 0);

    Packages packages = packages_create(&arena);
    size_t next_node_id = 0;
    size_t next_type_id = 0;

    for (size_t i = 0; i < args.paths_to_include.length; ++i) {
        String const source_path = args.paths_to_include.strings[i];

        String const source = file_read(&arena, source_path);

        Lexer lexer = lexer_create(&arena, source);
        ScanResult const scan_res = lexer_scan(&lexer);

        scanres_assert(scan_res);
        ArrayList_Token const tokens = scan_res.res.tokens;

        Parser parser = parser_create(&arena, tokens);
        parser.next_node_id = next_node_id;
        parser.next_type_id = next_type_id;

        ASTNodeResult const ast_res = parser_parse(&parser);
        next_node_id = parser.next_node_id;
        next_type_id = parser.next_type_id;

        astres_assert(ast_res);
        ASTNode const* ast = ast_res.res.ast;

        Analyzer analyzer = {0};
        verify_syntax(&analyzer, ast);

        printf("AST:");
        if (parser.had_error) {
            printf(" (partial due to errors)");
        }
        printf("\n");

        print_astnode(*ast);
        printf("\n");

        PackagePath* package_name = NULL;
        if (parser.package) {
            package_name = parser.package->node.package.package_path;
        }

        Package* pkg = packages_resolve_or_create(&packages, package_name);
        assert(!pkg->ast);
        pkg->ast = ast;

        // look for main
        {
            LLNode_ASTNode* curr = ast->node.file_root.nodes.head;
            while (curr) {
                if (curr->data.type == ANT_FUNCTION_DECL) {
                    if (curr->data.node.function_decl.header.is_main) {
                        pkg->is_entry = true;
                    }
                }

                curr = curr->next;
            }
        }
    }

    packages.generic_impls_nodes_length = next_node_id;
    packages.generic_impls_nodes_raw = arena_calloc(&arena, next_node_id, sizeof *packages.generic_impls_nodes_raw);
    packages.generic_impls_nodes_concrete = arena_calloc(&arena, next_node_id, sizeof *packages.generic_impls_nodes_concrete);

    packages.types_length = next_node_id + next_type_id;
    packages.types = arena_calloc(&arena, packages.types_length, sizeof *packages.types);

    TypeResolver type_resolver = type_resolver_create(&arena, &packages);
    resolve_types(&type_resolver);

    CodegenC codegen = codegen_c_create(&arena, &packages);
    GeneratedFiles const c_code = generate_c_code(&codegen);

    String build_dir = args.opt_args.strings[QO_BUILD_DIR];
    assert(build_dir.length);

    printf("C Code:\n");
    for (size_t i = 0; i < c_code.length; ++i) {
        GeneratedFile* file = c_code.files + i;
        write_file(&arena, build_dir, file->filepath, file->content);
    }

    // cleanup
    arena_free(&arena);

    return EXIT_SUCCESS;
}
