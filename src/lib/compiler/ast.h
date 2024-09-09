#ifndef quillc_ast_h
#define quillc_ast_h

#include "../utils/utils.h"
#include "./token.h"

typedef struct {
    Token const* const start;
    size_t length;
} TokensSlice;

typedef enum {
    ANT_UNARY_OP,
    ANT_BINARY_OP,
    ANT_LITERAL,
    ANT_VAR_DECL,
    ANT_VAL_REF,
    ANT_ASSIGNMENT,
    ANT_FUNCTION_CALL,
    ANT_STATEMENT_BLOCK,
    // TODO: more
} ASTNodeType;

typedef enum {
    UO_NEGATE,
    UO_REF,
    UO_DEREF,

    UO_COUNT
} UnaryOp;

typedef struct {
    UnaryOp const op;
    struct ASTNode_s const* const right;    
} ASTNodeUnaryOp;

typedef enum {
    BO_ADD,
    BO_SUBTRACT,
    BO_MULTIPLY,
    BO_DIVIDE,

    BO_BOOL_OR,
    BO_BOOL_AND,

    BO_BIT_OR,
    BO_BIT_AND,
    BO_BIT_XOR,

    BO_COUNT
} BinaryOp;

typedef struct {
    BinaryOp const op;
    struct ASTNode_s const* const left;
    struct ASTNode_s const* const right;
} ASTNodeBinaryOp;

typedef enum {
    LK_BOOL,
    LK_INT,
    LK_FLOAT,
    LK_STR,
    LK_CHAR,
    LK_CHARS,
    LK_NULL,
} LiteralKind;

typedef struct {
    LiteralKind const kind;
    union {
        bool const lit_bool;
        uint64_t const lit_int;
        double const lit_float;
        String const lit_str;
        char const lit_char;
        String const lit_chars;
        void* const lit_null;
    } const value;
} ASTNodeLiteral;

typedef struct {
    // note: can also be `let` for var decls
    TokensSlice const type_tokens;

    String const var_name;
    struct ASTNode_s const* const initializer;
} ASTNodeVarDecl;

typedef struct {
    struct ASTNode_s const* const function;

    struct ASTNode_s const* const* const args;
    size_t args_count;
} ASTNodeFunctionCall;

typedef struct ASTNode_s {
    ASTNodeType const type;
    union {
        ASTNodeUnaryOp const unary_op;
        ASTNodeBinaryOp const binary_op;
        ASTNodeLiteral const literal;
        ASTNodeVarDecl const var_decl;
        ASTNodeFunctionCall const function_call;
    } const node;
} ASTNode;

#endif
