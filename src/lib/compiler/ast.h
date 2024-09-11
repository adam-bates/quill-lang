#ifndef quillc_ast_h
#define quillc_ast_h

#include "../utils/utils.h"
#include "./token.h"

typedef struct {
    Token const* const start;
    size_t length;
} TokensSlice;

typedef struct StaticPath_s {
    struct StaticPath_s const* const root;
    String const name;
} StaticPath;

typedef enum {
    TK_BUILT_IN,
    TK_STATIC_PATH,
    TK_TUPLE,
    TK_POINTER,
    TK_MUT_POINTER,
    TK_ARRAY,
    TK_SLICE,
    TK_OPTIONAL,
    TK_RESULT,

    TK_COUNT
} TypeKind;

typedef struct {
    TypeKind const kind;
    // TODO
} Type;

typedef enum {
    ANT_UNARY_OP,
    ANT_BINARY_OP,
    ANT_LITERAL,
    ANT_VAR_DECL,
    ANT_VAR_REF,
    ANT_ASSIGNMENT,
    ANT_FUNCTION_CALL,
    ANT_STATEMENT_BLOCK,
    ANT_IF,
    ANT_ELSE,
    ANT_TRY,
    ANT_CATCH,
    ANT_BREAK,
    ANT_WHILE,
    ANT_DO_WHILE,
    ANT_FOR,
    ANT_RETURN,
    ANT_STRUCT_INIT,
    ANT_ARRAY_INIT,
    ANT_TEMPLATE_STRING,
    ANT_CRASH,
    ANT_SWITCH,
    ANT_CAST,
    ANT_STRUCT_DECL,
    ANT_UNION_DECL,
    ANT_ENUM_DECL,
    ANT_GLOBALTAG_DECL,
    // TODO: more

    ANT_COUNT
} ASTNodeType;

//

typedef enum {
    UO_NUM_NEGATE,
    UO_BOOL_NEGATE,
    UO_PTR_REF,
    UO_PTR_DEREF,

    UO_COUNT
} UnaryOp;

typedef struct {
    UnaryOp const op;
    struct ASTNode_s const* const right;    
} ASTNodeUnaryOp;

//

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
    struct ASTNode_s const* const lhs;
    struct ASTNode_s const* const rhs;
} ASTNodeBinaryOp;

//

typedef enum {
    LK_BOOL,
    LK_INT,
    LK_FLOAT,
    LK_STR,
    LK_CHAR,
    LK_CHARS,
    LK_NULL,

    LK_COUNT
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

//

typedef enum {
    VDLT_NAME,
    VDLT_TUPLE,

    VDLT_COUNT
} VarDeclLHSType;

typedef struct {
    VarDeclLHSType const type;
    union {
        String const name;
        String const* const tuple_names;
    } const lhs;
    size_t const count;
} VarDeclLHS;

//

typedef struct {
    bool const is_let;
    bool const is_mut;
    Type const* const maybe_type;
} TypeOrLet;

typedef struct {
    TypeOrLet const type_or_let;

    VarDeclLHS const lhs;
    struct ASTNode_s const* const initializer;
} ASTNodeVarDecl;

//

typedef struct {
    String const var_name;
} ASTNodeVarRef;

//

typedef enum {
    AO_ASSIGN,

    AO_PLUS_ASSIGN,
    AO_MINUS_ASSIGN,
    AO_MULTIPLY_ASSIGN,
    AO_DIVIDE_ASSIGN,

    AO_BIT_AND_ASSIGN,
    AO_BIT_OR_ASSIGN,
    AO_BIT_XOR_ASSIGN,

    AO_COUNT
} AssignmentOp;

typedef struct {
    AssignmentOp const op;
    struct ASTNode_s const* const lhs;
    struct ASTNode_s const* const rhs;
} ASTNodeAssignment;

//

typedef struct {
    struct ASTNode_s const* const function;

    struct ASTNode_s const* const* const args;
    size_t const args_count;
} ASTNodeFunctionCall;

//

typedef struct {
    struct ASTNode_s const* const* const stmts;
    size_t const stmts_count;
} ASTNodeStatementBlock;

//

typedef struct {
    struct ASTNode_s const* const cond;
    ASTNodeStatementBlock const* const block;
} ASTNodeIf;

//

typedef struct {
    struct ASTNode_s const* const target;
    struct ASTNode_s const* const then;
} ASTNodeElse;

//

typedef struct {
    struct ASTNode_s const* const target;
} ASTNodeTry;

//

typedef struct {
    struct ASTNode_s const* const target;
    String const error;
    struct ASTNode_s const* const then;
} ASTNodeCatch;

//

typedef struct {
    struct ASTNode_s const* const maybe_expr;
} ASTNodeBreak;

//

typedef struct {
    struct ASTNode_s const* const cond;
    ASTNodeStatementBlock const* const block;
} ASTNodeWhile;

//

typedef struct {
    ASTNodeStatementBlock const* const block;
    struct ASTNode_s const* const cond;
} ASTNodeDoWhile;

//

typedef struct {
    VarDeclLHS const var;
    struct ASTNode_s const* const iterable;
    ASTNodeStatementBlock const* const block;
} ASTNodeFor;

//

typedef struct {
    struct ASTNode_s const* const maybe_expr;
} ASTNodeReturn;

//

typedef struct {
    TokensSlice const lhs;
    struct ASTNode_s const* const rhs;
} StructInitField;

typedef struct {
    StructInitField const* const fields;
    size_t const fields_count;
} ASTNodeStructInit;

//

typedef struct {
    TokensSlice const lhs;
    struct ASTNode_s const* const rhs;
} ArrayInitElem;

typedef struct {
    size_t const* const maybe_length;
    ArrayInitElem const* const elems;
    size_t const elems_count;
} ASTNodeArrayInit;

//

typedef struct {
    // note: could end in wildcard `*`
    StaticPath const static_path;
} ASTNodeImport;

//

typedef struct {
    String const* const str_parts;
    size_t const str_parts_count;

    struct ASTNode_s const* const* const template_expr_parts;
    size_t const template_expr_parts_count;
} ASTNodeTemplateString;

//

typedef struct {
    struct ASTNode_s const* const maybe_expr;
} ASTNodeCrash;

//

typedef struct {
    struct ASTNode_s const* const* const matches;
    size_t const matches_count;

    struct ASTNode_s const* const then;
} SwitchCase;

typedef struct {
    struct ASTNode_s const* const expr;

    SwitchCase const* const cases;
    size_t const cases_count;

    ASTNodeElse const* const maybe_else;
} ASTNodeSwitch;

//

typedef struct {
    Type const type;
    struct ASTNode_s const* const target;
} ASTNodeCast;

//

typedef struct {
    String const* const maybe_name;
    // TODO
} ASTNodeStructDecl;

//

typedef struct {
    String const* const maybe_name;
    // TODO
} ASTNodeUnionDecl;

//

typedef struct {
    String const* const maybe_name;
    // TODO
} ASTNodeEnumDecl;

//

typedef struct {
    String const* const maybe_name;
    // TODO
} ASTNodeGlobaltagDecl;

//

typedef struct ASTNode_s {
    ASTNodeType const type;
    union {
        ASTNodeUnaryOp const unary_op;
        ASTNodeBinaryOp const binary_op;
        ASTNodeLiteral const literal;
        ASTNodeVarDecl const var_decl;
        ASTNodeVarRef const var_ref;
        ASTNodeFunctionCall const function_call;
        ASTNodeStatementBlock const statement_block;
        ASTNodeIf const if_;
        ASTNodeElse const else_;
        ASTNodeTry const try_;
        ASTNodeCatch const catch_;
        ASTNodeBreak const break_;
        ASTNodeWhile const while_;
        ASTNodeDoWhile const do_while;
        ASTNodeFor const for_;
        ASTNodeReturn const return_;
        ASTNodeStructInit const struct_init;
        ASTNodeArrayInit const array_init;
        ASTNodeImport const import;
        ASTNodeTemplateString const template_string;
        ASTNodeCrash const crash;
        ASTNodeSwitch const switch_;
        ASTNodeCast const cast;
        ASTNodeStructDecl const struct_decl;
        ASTNodeUnionDecl const union_decl;
        ASTNodeEnumDecl const enum_decl;
        ASTNodeGlobaltagDecl const globaltag_decl;
    } const node;
} ASTNode;

typedef struct {
    bool const ok;
    union {
        ASTNode const ast;
        Error const err;
    } const res;
} ASTNodeResult;

#endif
