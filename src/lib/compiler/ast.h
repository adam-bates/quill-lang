#ifndef quill_ast_h
#define quill_ast_h

#include "../utils/utils.h"
#include "./token.h"

typedef struct {
    Token const* const start;
    size_t length;
} TokensSlice;

typedef struct {
    size_t length;
    struct LLNode_ASTNode* head;
    struct LLNode_ASTNode* tail;
} LL_ASTNode;

typedef struct {
    size_t length;
    struct LLNode_Directive* head;
    struct LLNode_Directive* tail;
} LL_Directive;

typedef struct {
    size_t length;
    struct LLNode_FnParam* head;
    struct LLNode_FnParam* tail;
} LL_FnParam;

typedef struct StaticPath_s {
    struct StaticPath_s* root;
    String name;
} StaticPath;

typedef enum {
    TK_BUILT_IN,
    TK_TYPE_REF,
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

typedef enum {
    TBI_VOID,
    TBI_UINT,
    // TODO
} TypeBuiltIn;

typedef struct {
    String name;
} TypeTypeRef;

typedef struct {
    struct Type* of;
} TypePointer;

typedef struct Type {
    TypeKind kind;
    union {
        TypeBuiltIn built_in;
        TypeTypeRef type_ref;
        // TypeStaticPath static_path;
        // TypeTuple tuple;
        TypePointer ptr;
        TypePointer mut_ptr;
        // TODO
    } type;
    LL_Directive directives;
} Type;

typedef enum {
    ANT_NONE,

    ANT_FILE_ROOT,
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
    ANT_IMPORT,
    ANT_PACKAGE,
    ANT_TEMPLATE_STRING,
    ANT_CRASH,
    ANT_SWITCH,
    ANT_CAST,
    ANT_STRUCT_DECL,
    ANT_UNION_DECL,
    ANT_ENUM_DECL,
    ANT_TYPEDEF_DECL,
    ANT_GLOBALTAG_DECL,
    ANT_FUNCTION_HEADER_DECL,
    ANT_FUNCTION_DECL,

    ANT_COUNT
} ASTNodeType;

//

typedef struct {
    LL_ASTNode nodes;
} ASTNodeFileRoot;

//

typedef enum {
    UO_NUM_NEGATE,
    UO_BOOL_NEGATE,
    UO_PTR_REF,
    UO_PTR_DEREF,

    UO_COUNT
} UnaryOp;

typedef struct {
    UnaryOp op;
    struct ASTNode* right;    
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
    BinaryOp op;
    struct ASTNode* lhs;
    struct ASTNode* rhs;
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
    LiteralKind kind;
    union {
        bool lit_bool;
        uint64_t lit_int;
        double lit_float;
        String lit_str;
        String lit_char; // String instead of char, to map chars like '\n' to "\\n"
        String lit_chars;
        void* lit_null;
    } value;
} ASTNodeLiteral;

//

typedef struct {
    bool is_let;
    bool is_mut;
    Type* maybe_type;
} TypeOrLet;

typedef enum {
    VDLT_NAME,
    VDLT_TUPLE,

    VDLT_COUNT
} VarDeclLHSType;

typedef struct {
    VarDeclLHSType type;
    union {
        String name;
        String* tuple_names;
    } lhs;
    size_t count;
} VarDeclLHS;

typedef struct {
    bool is_static;
    TypeOrLet type_or_let;

    VarDeclLHS lhs;
    struct ASTNode* initializer;
} ASTNodeVarDecl;

//

typedef struct {
    struct StaticPath_s* path;
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
    struct ASTNode* lhs;
    struct ASTNode* rhs;
} ASTNodeAssignment;

//

typedef struct {
    struct ASTNode* function;

    LL_ASTNode args;
} ASTNodeFunctionCall;

//

typedef struct {
    LL_ASTNode stmts;
} ASTNodeStatementBlock;

//

typedef struct {
    struct ASTNode* cond;
    ASTNodeStatementBlock* block;
} ASTNodeIf;

//

typedef struct {
    struct ASTNode* target;
    struct ASTNode* then;
} ASTNodeElse;

//

typedef struct {
    struct ASTNode* target;
} ASTNodeTry;

//

typedef struct {
    struct ASTNode* target;
    String error;
    struct ASTNode* then;
} ASTNodeCatch;

//

typedef struct {
    struct ASTNode* maybe_expr;
} ASTNodeBreak;

//

typedef struct {
    struct ASTNode* cond;
    ASTNodeStatementBlock* block;
} ASTNodeWhile;

//

typedef struct {
    ASTNodeStatementBlock* block;
    struct ASTNode* cond;
} ASTNodeDoWhile;

//

typedef struct {
    VarDeclLHS var;
    struct ASTNode* iterable;
    ASTNodeStatementBlock* block;
} ASTNodeFor;

//

typedef struct {
    struct ASTNode* maybe_expr;
} ASTNodeReturn;

//

typedef struct {
    TokensSlice lhs;
    struct ASTNode* rhs;
} StructInitField;

typedef struct {
    StructInitField* fields;
    size_t fields_count;
} ASTNodeStructInit;

//

typedef struct {
    TokensSlice lhs;
    struct ASTNode* rhs;
} ArrayInitElem;

typedef struct {
    size_t* maybe_length;
    ArrayInitElem* elems;
    size_t elems_count;
} ASTNodeArrayInit;

//

typedef struct {
    // note: could end in wildcard `*`
    StaticPath* static_path;
} ASTNodeImport;

//

typedef struct {
    StaticPath* static_path;
} ASTNodePackage;

//

typedef struct {
    String* str_parts;
    size_t str_parts_count;

    LL_ASTNode template_expr_parts;
} ASTNodeTemplateString;

//

typedef struct {
    struct ASTNode* maybe_expr;
} ASTNodeCrash;

//

typedef struct {
    LL_ASTNode* matches;

    struct ASTNode* then;
} SwitchCase;

typedef struct {
    struct ASTNode* expr;

    SwitchCase* cases;
    size_t cases_count;

    ASTNodeElse* maybe_else;
} ASTNodeSwitch;

//

typedef struct {
    Type type;
    struct ASTNode* target;
} ASTNodeCast;

//

typedef struct {
    String* maybe_name;
    // TODO
} ASTNodeStructDecl;

//

typedef struct {
    String* maybe_name;
    // TODO
} ASTNodeUnionDecl;

//

typedef struct {
    String name;
    // TODO
} ASTNodeEnumDecl;

//

typedef struct {
    String name;
    Type* type;
} ASTNodeTypedefDecl;

//

typedef struct {
    String* maybe_name;
    // TODO
} ASTNodeGlobaltagDecl;

//

typedef struct {
    Type type;
    bool is_mut;
    String name;
} FnParam;

typedef struct {
    Type return_type;
    String name;
    LL_FnParam params;
} ASTNodeFunctionHeaderDecl;

typedef struct {
    ASTNodeFunctionHeaderDecl header;
    LL_ASTNode stmts;
} ASTNodeFunctionDecl;

//

typedef struct ASTNode {
    ASTNodeType type;
    union {
        ASTNodeFileRoot file_root;
        ASTNodeUnaryOp unary_op;
        ASTNodeBinaryOp binary_op;
        ASTNodeLiteral literal;
        ASTNodeVarDecl var_decl;
        ASTNodeVarRef var_ref;
        ASTNodeFunctionCall function_call;
        ASTNodeStatementBlock statement_block;
        ASTNodeIf if_;
        ASTNodeElse else_;
        ASTNodeTry try_;
        ASTNodeCatch catch_;
        ASTNodeBreak break_;
        ASTNodeWhile while_;
        ASTNodeDoWhile do_while;
        ASTNodeFor for_;
        ASTNodeReturn return_;
        ASTNodeStructInit struct_init;
        ASTNodeArrayInit array_init;
        ASTNodeImport import;
        ASTNodePackage package;
        ASTNodeTemplateString template_string;
        ASTNodeCrash crash;
        ASTNodeSwitch switch_;
        ASTNodeCast cast;
        ASTNodeStructDecl struct_decl;
        ASTNodeUnionDecl union_decl;
        ASTNodeEnumDecl enum_decl;
        ASTNodeTypedefDecl typedef_decl;
        ASTNodeGlobaltagDecl globaltag_decl;
        ASTNodeFunctionHeaderDecl function_header_decl;
        ASTNodeFunctionDecl function_decl;
    } node;
    LL_Directive directives;
} ASTNode;

typedef enum {
    DT_C_HEADER,
    DT_C_RESTRICT,
    DT_C_FILE,
} DirectiveType;

typedef struct {
    String include;
} DirectiveCHeader;

typedef void* DirectiveCRestrict;

typedef void* DirectiveCFile;

typedef struct {
    DirectiveType type;
    union {
        DirectiveCHeader c_header;
        DirectiveCRestrict c_restrict;
        DirectiveCFile c_file;
    } dir;
} Directive;

typedef struct LLNode_ASTNode {
    struct LLNode_ASTNode* next;
    ASTNode data;
} LLNode_ASTNode;

typedef struct LLNode_Directive {
    struct LLNode_Directive* next;
    Directive data;
} LLNode_Directive;

typedef struct LLNode_FnParam {
    struct LLNode_FnParam* next;
    FnParam data;
} LLNode_FnParam;

typedef struct {
    bool const ok;
    union {
        ASTNode const* const ast;
        Error const err;
    } const res;
} ASTNodeResult;

void ll_ast_push(Arena* const arena, LL_ASTNode* const ll, ASTNode const node);
void ll_directive_push(Arena* const arena, LL_Directive* const ll, Directive const directive);
void ll_param_push(Arena* const arena, LL_FnParam* const ll, FnParam const param);

void print_astnode(ASTNode const node);

#endif
