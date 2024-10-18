#ifndef quill_ast_h
#define quill_ast_h

#include "../utils/utils.h"
#include "./token.h"

typedef struct { size_t val; } NodeId;
typedef struct { size_t val; } TypeId;

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

typedef struct {
    size_t length;
    struct LLNode_StructField* head;
    struct LLNode_StructField* tail;
} LL_StructField;

typedef struct {
    size_t length;
    struct LLNode_StructFieldInit* head;
    struct LLNode_StructFieldInit* tail;
} LL_StructFieldInit;

typedef struct {
    size_t length;
    struct LLNode_ArrayInitElem* head;
    struct LLNode_ArrayInitElem* tail;
} LL_ArrayInitElem;

typedef struct {
    size_t length;
    struct LLNode_Type* head;
    struct LLNode_Type* tail;
} LL_Type;

typedef struct {
    Arena* arena;
    size_t capacity;
    size_t length;
    LL_Type* array;
} ArrayList_LL_Type;

typedef struct StaticPath_s {
    String name;
    struct StaticPath_s* child;
} StaticPath;

typedef enum {
    TK_BUILT_IN,
    TK_STATIC_PATH,
    TK_TUPLE,
    TK_POINTER,
    TK_MUT_POINTER,
    TK_ARRAY,
    TK_SLICE,

    TK_COUNT
} TypeKind;

typedef enum {
    TBI_VOID,
    TBI_BOOL,
    TBI_INT,
    TBI_UINT,
    TBI_CHAR,
    // TODO
} TypeBuiltIn;

typedef struct {
    StaticPath* path;
    LL_Type generic_types;
    size_t impl_version;
} TypeStaticPath;

typedef struct {
    struct Type* of;
} TypePointer;

typedef struct {
    struct Type* of;
} TypeArray;

typedef struct Type {
    TypeId id;
    TypeKind kind;
    union {
        TypeBuiltIn built_in;
        TypeStaticPath static_path;
        // TypeTuple tuple;
        TypePointer ptr;
        TypePointer mut_ptr;
        TypeArray array;
    } type;
    LL_Directive directives;
} Type;

typedef enum {
    ANT_NONE,

    ANT_FILE_ROOT,
    ANT_FILE_SEPARATOR,
    ANT_UNARY_OP,
    ANT_BINARY_OP,
    ANT_LITERAL,
    ANT_TUPLE,
    ANT_VAR_DECL,
    ANT_VAR_REF,
    ANT_GET_FIELD,
    ANT_INDEX,
    ANT_RANGE,
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
    ANT_FOREACH,
    ANT_RETURN,
    ANT_DEFER,
    ANT_STRUCT_INIT,
    ANT_ARRAY_INIT,
    ANT_IMPORT,
    ANT_PACKAGE,
    ANT_TEMPLATE_STRING,
    ANT_CRASH,
    ANT_SIZEOF,
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

typedef void* ASTNodeFileSeparator;

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
    BO_BIT_OR,
    BO_BIT_AND,
    BO_BIT_XOR,

    BO_ADD,
    BO_SUBTRACT,
    BO_MULTIPLY,
    BO_DIVIDE,
    BO_MODULO,

    BO_BOOL_OR,
    BO_BOOL_AND,

    BO_EQ,
    BO_NOT_EQ,

    BO_LESS,
    BO_LESS_OR_EQ,
    BO_GREATER,
    BO_GREATER_OR_EQ,

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
    LL_ASTNode exprs;
} ASTNodeTuple;

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

typedef struct {
    struct ASTNode* root;
    bool is_ptr_deref;
    String name;
} ASTNodeGetField;

//

typedef struct {
    struct ASTNode* root;
    struct ASTNode* value;
} ASTNodeIndex;

//

typedef struct {
    struct ASTNode* lhs;
    struct ASTNode* rhs;
    bool inclusive;
} ASTNodeRange;

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
    AssignmentOp op;
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
    struct ASTNode* init;
    struct ASTNode* cond;
    struct ASTNode* step;
    ASTNodeStatementBlock* block;
} ASTNodeFor;

//

typedef struct {
    VarDeclLHS var;
    struct ASTNode* iterable;
    ASTNodeStatementBlock* block;
} ASTNodeForEach;

//

typedef struct {
    struct ASTNode* maybe_expr;
} ASTNodeReturn;

//

typedef struct {
    struct ASTNode* stmt;
} ASTNodeDefer;

//

typedef struct {
    String name;
    struct ASTNode* value;
} StructFieldInit;

typedef struct {
    LL_StructFieldInit fields;
} ASTNodeStructInit;

//

typedef struct {
    struct ASTNode* maybe_index;
    struct ASTNode* value;
} ArrayInitElem;

typedef struct {
    struct ASTNode* maybe_explicit_length;
    LL_ArrayInitElem elems;
} ASTNodeArrayInit;

//

typedef enum {
    IPT_DIR,
    IPT_FILE,
} ImportPathType;

typedef struct {
    String name;
    struct ImportPath_s* child;
} ImportDirPath;

typedef enum {
    ISPT_WILDCARD,
    ISPT_IDENT,
} ImportStaticPathType;

typedef struct {
    String name;
    struct ImportStaticPath_s* child;
} ImportStaticPathIdent;

typedef struct ImportStaticPath_s {
    ImportStaticPathType type;
    union {
        void* wildcard;
        ImportStaticPathIdent ident;
    } import;
} ImportStaticPath;

typedef struct {
    String name;
    ImportStaticPath* child;
} ImportFilePath;

typedef struct ImportPath_s {
    ImportPathType type;
    union {
        ImportDirPath dir;
        ImportFilePath file;
    } import;
} ImportPath;

typedef enum {
    IT_DEFAULT,
    IT_LOCAL,
    IT_ROOT,
} ImportType;

typedef struct {
    ImportType type;
    // note: could end in wildcard `*`
    ImportPath* import_path;
} ASTNodeImport;

//

typedef struct PackagePath_s {
    String name;
    struct PackagePath_s* child;
} PackagePath;

typedef struct {
    PackagePath* package_path;
} ASTNodePackage;

//

typedef struct {
    ArrayList_String str_parts;
    LL_ASTNode template_expr_parts;
} ASTNodeTemplateString;

//

typedef struct {
    struct ASTNode* maybe_expr;
} ASTNodeCrash;

//

typedef enum {
    SOK_TYPE,
    SOK_EXPR,
} SizeofKind;

typedef struct {
    SizeofKind kind;
    union {
        Type* type;
        struct ASTNode* expr;
    } sizeof_;
} ASTNodeSizeof;

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
    Type* type;
    String name;
} StructField;

typedef struct {
    String* maybe_name;
    LL_StructField fields;
    ArrayList_String generic_params;
    ArrayList_LL_Type generic_impls;
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
    bool is_main;
} ASTNodeFunctionHeaderDecl;

typedef struct {
    ASTNodeFunctionHeaderDecl header;
    LL_ASTNode stmts;
} ASTNodeFunctionDecl;

//

typedef struct ASTNode {
    NodeId id;
    ASTNodeType type;
    union {
        ASTNodeFileRoot file_root;
        ASTNodeFileSeparator file_separator;
        ASTNodeUnaryOp unary_op;
        ASTNodeBinaryOp binary_op;
        ASTNodeLiteral literal;
        ASTNodeTuple tuple;
        ASTNodeVarDecl var_decl;
        ASTNodeVarRef var_ref;
        ASTNodeGetField get_field;
        ASTNodeIndex index;
        ASTNodeRange range;
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
        ASTNodeForEach foreach;
        ASTNodeReturn return_;
        ASTNodeDefer defer;
        ASTNodeStructInit struct_init;
        ASTNodeArrayInit array_init;
        ASTNodeImport import;
        ASTNodePackage package;
        ASTNodeTemplateString template_string;
        ASTNodeCrash crash;
        ASTNodeSizeof sizeof_;
        ASTNodeSwitch switch_;
        ASTNodeCast cast;
        ASTNodeStructDecl struct_decl;
        ASTNodeUnionDecl union_decl;
        ASTNodeEnumDecl enum_decl;
        ASTNodeTypedefDecl typedef_decl;
        ASTNodeGlobaltagDecl globaltag_decl;
        ASTNodeFunctionHeaderDecl function_header_decl;
        ASTNodeFunctionDecl function_decl;
        ASTNodeAssignment assignment;
    } node;
    LL_Directive directives;
} ASTNode;

typedef enum {
    DT_C_HEADER,
    DT_C_RESTRICT,
    DT_C_FILE,
    DT_IGNORE_UNUSED,
    DT_IMPL,
    DT_STRING_LITERAL,
} DirectiveType;

typedef struct {
    String include;
} DirectiveCHeader;

typedef void* DirectiveCRestrict;
typedef void* DirectiveCFile;
typedef void* DirectiveIgnoreUnused;
typedef void* DirectiveImpl;
typedef void* DirectiveStringLiteral;

typedef struct {
    DirectiveType type;
    union {
        DirectiveCHeader c_header;
        DirectiveCRestrict c_restrict;
        DirectiveCFile c_file;
        DirectiveIgnoreUnused ignore_unused;
        DirectiveImpl impl;
        DirectiveStringLiteral string_literal;
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

typedef struct LLNode_StructField {
    struct LLNode_StructField* next;
    StructField data;
} LLNode_StructField;

typedef struct LLNode_StructFieldInit {
    struct LLNode_StructFieldInit* next;
    StructFieldInit data;
} LLNode_StructFieldInit;

typedef struct LLNode_ArrayInitElem {
    struct LLNode_ArrayInitElem* next;
    ArrayInitElem data;
} LLNode_ArrayInitElem;

typedef struct LLNode_Type {
    struct LLNode_Type* next;
    Type data;
} LLNode_Type;

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
void ll_field_push(Arena* const arena, LL_StructField* const ll, StructField const field);
void ll_field_init_push(Arena* const arena, LL_StructFieldInit* const ll, StructFieldInit const field_init);
void ll_array_init_elem_push(Arena* const arena, LL_ArrayInitElem* const ll, ArrayInitElem const array_init_elem);
void ll_type_push(Arena* const arena, LL_Type* const ll, Type const type);

ArrayList_LL_Type arraylist_typells_create(Arena* const arena);
ArrayList_LL_Type arraylist_typells_create_with_capacity(Arena* const arena, size_t capacity);

void arraylist_typells_push(ArrayList_LL_Type* list, LL_Type type_ll);

bool static_path_eq(StaticPath a, StaticPath b);
bool type_static_path_eq(TypeStaticPath a, TypeStaticPath b);
bool type_eq(Type a, Type b);
bool directive_eq(Directive a, Directive b);

bool directivells_eq(LL_Directive a, LL_Directive b);
bool typells_eq(LL_Type a, LL_Type b);

ASTNode* find_decl_by_id(ASTNodeFileRoot, NodeId id);
ASTNode* find_decl_by_name(ASTNodeFileRoot root, String name);

String static_path_to_str(Arena* arena, StaticPath* path);
StringBuffer static_path_to_strbuf(Arena* arena, StaticPath* path);

String package_path_to_str(Arena* arena, PackagePath* path);
StringBuffer package_path_to_strbuf(Arena* arena, PackagePath* path);

String import_path_to_str(Arena* arena, ImportPath* path);
StringBuffer import_path_to_strbuf(Arena* arena, ImportPath* path);

PackagePath* import_path_to_package_path(Arena* arena, ImportPath* import_path);
ImportPath* package_path_to_import_path(Arena* arena, PackagePath* package_path);

bool package_path_eq(PackagePath* p1, PackagePath* p2);

void print_astnode(ASTNode const node);

#endif
