#ifndef quill_codegen_c_h
#define quill_codegen_c_h

#include "./package.h"
#include "../utils/utils.h"

typedef enum {
    BT_OTHER,
    BT_IMPORT,
    BT_FUNCTION_DECLS,
    BT_STATIC_VARS,
    BT_VARS,
} BlockType;

typedef struct {
    size_t length;
    struct LLNode_IR_C_Node* head;
    struct LLNode_IR_C_Node* tail;
} LL_IR_C_Node;

typedef enum {
    ICNT_MACRO_IFNDEF,
    ICNT_MACRO_DEFINE,
    ICNT_MACRO_INCLUDE,
    ICNT_MACRO_ENDIF,

    ICNT_VAR_REF,
    ICNT_ARRAY_INIT,
    ICNT_UNARY,
    ICNT_GET_FIELD,
    ICNT_SIZEOF_EXPR,
    ICNT_SIZEOF_TYPE,
    ICNT_FUNCTION_CALL,
    ICNT_RETURN,
    ICNT_ASSIGNMENT,

    ICNT_VAR_DECL,
    ICNT_FUNCTION_HEADER_DECL,
    ICNT_FUNCTION_DECL,
    ICNT_STRUCT_DECL,

    ICNT_COUNT
} IR_C_NodeType;

typedef struct {
    String condition;
} IR_C_MacroIfndef;

typedef struct {
    String name;
    String* maybe_value;
} IR_C_MacroDefine;

typedef struct {
    bool is_local;
    String file;
} IR_C_MacroInclude;

typedef struct {
    void* _;
} IR_C_MacroEndif;

typedef struct {
    String name;
} IR_C_VarRef;

typedef struct {
    String op;
    struct IR_C_Node* expr;
} IR_C_Unary;

typedef struct {
    struct IR_C_Node* root;
    String name;
} IR_C_GetField;

typedef struct {
    struct IR_C_Node* expr;
} IR_C_SizeofExpr;

typedef struct {
    String type;
} IR_C_SizeofType;

typedef struct {
    struct IR_C_Node* target;
    LL_IR_C_Node args;
} IR_C_FunctionCall;

typedef struct {
    struct IR_C_Node* expr;
} IR_C_Return;

typedef struct {
    struct IR_C_Node* lhs;
    String op;
    struct IR_C_Node* rhs;
} IR_C_Assignment;

typedef struct {
    String type;
    String name;
    struct IR_C_Node* init;
} IR_C_VarDecl;

typedef struct {
    size_t elems_length;
    struct IR_C_Node* indicies;
    struct IR_C_Node* elems;
} IR_C_ArrayInit;

typedef struct {
    String return_type;
    String name;
    Strings params;
} IR_C_FunctionHeaderDecl;

typedef struct {
    String return_type;
    String name;
    Strings params;
    LL_IR_C_Node statements;
} IR_C_FunctionDecl;

typedef struct {
    String name;
    Strings fields;
} IR_C_StructDecl;

typedef struct IR_C_Node {
    IR_C_NodeType type;
    union {
        IR_C_MacroIfndef ifndef;
        IR_C_MacroDefine define;
        IR_C_MacroInclude include;
        IR_C_MacroEndif endif;
        IR_C_VarRef var_ref;
        IR_C_Unary unary;
        IR_C_GetField get_field;
        IR_C_SizeofExpr sizeof_expr;
        IR_C_SizeofType sizeof_type;
        IR_C_FunctionCall function_call;
        IR_C_Return return_;
        IR_C_Assignment assignment;
        IR_C_VarDecl var_decl;
        IR_C_ArrayInit array_init;
        IR_C_FunctionHeaderDecl function_header_decl;
        IR_C_FunctionDecl function_decl;
        IR_C_StructDecl struct_decl;
    } node;
} IR_C_Node;

typedef struct LLNode_IR_C_Node {
    IR_C_Node data;
    struct LLNode_IR_C_Node* next;
} LLNode_IR_C_Node;

typedef struct {
    String name;
    LL_IR_C_Node nodes;
} IR_C_File;

typedef struct {
    size_t files_length;
    IR_C_File* files;
} IR_C;

typedef struct GenericImplMap {
    struct GenericImplMap* parent;
    size_t length;
    String* generic_names;
    String* mapped_types;
} GenericImplMap;

typedef struct {
    Arena* arena;
    Packages* packages;

    IR_C ir;

    GenericImplMap* generic_map;
    LL_IR_C_Node* stmt_block;

    bool seen_file_separator;
    BlockType prev_block;
} CodegenC;

typedef struct {
    String filepath;
    String content;
} GeneratedFile;

typedef struct {
    size_t length;
    GeneratedFile* files;
} GeneratedFiles;

CodegenC codegen_c_create(Arena* arena, Packages* packages);
GeneratedFiles generate_c_code(CodegenC* codegen);

#endif
