#include <stdio.h>
#include <string.h>

#include "../lib/quill.h"

int main(int const argc, char const* const argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: quillc [path]\n");
        return EXIT_FAILURE;
    }
    char const* const source_path = argv[1];

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

    // print out tokens
    // for (size_t i = 0; i < tokens.length; ++i) {
    //     Token const token = tokens.array[i];

    //     char* token_str = quill_calloc(allocator, token.length, sizeof(char));
    //     strncpy(token_str, token.start, token.length);

    //     printf("%-*lu | %-*d | [", count_digits((int)lexer.line), token.line, count_digits(TT_COUNT), token.type);
    //     debug_token_type(token.type);
    //     printf("] %s\n", token_str);

    //     quill_free(allocator, token_str);
    // }
    // printf("\n");

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

    // LL_ASTNode nodes = ast->node.file_root.nodes;
    // LLNode_ASTNode* node = nodes.head;

    // bool last_was_ok = true;
    // while (node != NULL) {
    //     if (node->data.type == ANT_NONE) {
    //         if (last_was_ok) {
    //             printf("<Unknown AST Node>\n");
    //         }
    //         last_was_ok = false;
    //         node = node->next;
    //         continue;
    //     }
    //     last_was_ok = true;

    //     print_astnode(node->data);
    //     node = node->next;
    // }
    printf("\n");

    verify_syntax(ast);

    Arena codegen_arena = {0};

    CodegenC codegen = codegen_c_create(&codegen_arena, ast);
    String const c_code = generate_c_code(&codegen);

    // printf("C Code:\n");
    printf("%s\n", arena_strcpy(&codegen_arena, c_code).chars);

    // cleanup
    arena_free(&parser_arena);
    arraylist_token_destroy(tokens);
    quill_free(allocator, (void*)source.chars);

    return EXIT_SUCCESS;
}
