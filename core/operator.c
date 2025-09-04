#include "operator.h"
#include "core_types.h"

static operator_t base_operators[]={
    {"(",NULL,OP_EXPR,0,1,OP_ASSOCIATIVITY_LEFT},
    
    {"(",NULL,OP_CALL,1,-1,OP_ASSOCIATIVITY_LEFT},
    {"[","i",OP_ARRAY_INDEX,1,2,OP_ASSOCIATIVITY_LEFT},
    {"->","p",OP_POINTER,1,2,OP_ASSOCIATIVITY_LEFT},
    {".","p",OP_POINTER,1,2,OP_ASSOCIATIVITY_LEFT},

    {"va_start",NULL,OP_VA_START,1,2,OP_ASSOCIATIVITY_LEFT},
    {"va_arg",NULL,OP_VA_ARG,1,2,OP_ASSOCIATIVITY_LEFT},
    {"va_end",NULL,OP_VA_END,1,1,OP_ASSOCIATIVITY_LEFT},
    {"container",NULL,OP_CONTAINER,1,3,OP_ASSOCIATIVITY_LEFT},

    {"create",NULL,OP_CREATE,2,-1,OP_ASSOCIATIVITY_RIGHT},
    {"(",NULL,OP_TYPE_CAST,2,2,OP_ASSOCIATIVITY_RIGHT},
    {"!","not",OP_LOGIC_NOT,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"~","bnot",OP_BIT_NOT,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"-","neg",OP_NEG,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"+","pos",OP_POSITIVE,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"sizeof",NULL,OP_SIZEOF,2,1,OP_ASSOCIATIVITY_RIGHT},

    {"++","inc",OP_INC,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"--","dec",OP_DEC,2,1,OP_ASSOCIATIVITY_RIGHT},

    {"++","inc",OP_INC_POST,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"--","dec",OP_DEC_POST,2,1,OP_ASSOCIATIVITY_RIGHT},

    {"*",NULL,OP_DEREFERENCE,2,1,OP_ASSOCIATIVITY_RIGHT},
    {"&",NULL,OP_ADDRESS_OF,2,1,OP_ASSOCIATIVITY_RIGHT},

    {"*",         "mul",  OP_MUL,           4,  2,  OP_ASSOCIATIVITY_LEFT},
	{"/",         "div",  OP_DIV,           4,  2,  OP_ASSOCIATIVITY_LEFT},
	{"%",         "mod",  OP_MOD,           4,  2,  OP_ASSOCIATIVITY_LEFT},

	{"+",         "add",  OP_ADD,           5,  2,  OP_ASSOCIATIVITY_LEFT},
	{"-",         "sub",  OP_SUB,           5,  2,  OP_ASSOCIATIVITY_LEFT},

	{"<<",         NULL,  OP_SHL,           6,  2,  OP_ASSOCIATIVITY_LEFT},
	{">>",         NULL,  OP_SHR,           6,  2,  OP_ASSOCIATIVITY_LEFT},

	{"&",         "band", OP_BIT_AND,       7,  2,  OP_ASSOCIATIVITY_LEFT},
	{"|",         "bor",  OP_BIT_OR,        7,  2,  OP_ASSOCIATIVITY_LEFT},

	{"==",        "eq",   OP_EQ,            8,  2,  OP_ASSOCIATIVITY_LEFT},
	{"!=",        "ne",   OP_NE,            8,  2,  OP_ASSOCIATIVITY_LEFT},
	{">",         "gt",   OP_GT,            8,  2,  OP_ASSOCIATIVITY_LEFT},
	{"<",         "lt",   OP_LT,            8,  2,  OP_ASSOCIATIVITY_LEFT},
	{">=",        "ge",   OP_GE,            8,  2,  OP_ASSOCIATIVITY_LEFT},
	{"<=",        "le",   OP_LE,            8,  2,  OP_ASSOCIATIVITY_LEFT},

	{"&&",        NULL,   OP_LOGIC_AND,     9,  2,  OP_ASSOCIATIVITY_LEFT},
	{"||",        NULL,   OP_LOGIC_OR,      9,  2,  OP_ASSOCIATIVITY_LEFT},

	{"=",         "a",    OP_ASSIGN,       10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"+=",        "add_", OP_ADD_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"-=",        "sub_", OP_SUB_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"*=",        "mul_", OP_MUL_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"/=",        "div_", OP_DIV_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"%=",        "mod_", OP_MOD_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"<<=",        NULL,  OP_SHL_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{">>=",        NULL,  OP_SHR_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"&=",        "and_", OP_AND_ASSIGN,   10,  2,  OP_ASSOCIATIVITY_RIGHT},
	{"|=",        "or_",  OP_OR_ASSIGN,    10,  2,  OP_ASSOCIATIVITY_RIGHT},

	{"{}",         NULL,  OP_BLOCK,        15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"return",     NULL,  OP_RETURN,       15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"break",      NULL,  OP_BREAK,        15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"continue",   NULL,  OP_CONTINUE,     15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"goto",       NULL,  OP_GOTO,         15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"label",      NULL,  LABEL,           15, -1,  OP_ASSOCIATIVITY_LEFT},

	{"if",         NULL,  OP_IF,           15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"while",      NULL,  OP_WHILE,        15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"do",         NULL,  OP_DO,           15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"for",        NULL,  OP_FOR,          15, -1,  OP_ASSOCIATIVITY_LEFT},

	{"switch",     NULL,  OP_SWITCH,       15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"case",       NULL,  OP_CASE,         15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"default",    NULL,  OP_DEFAULT,      15, -1,  OP_ASSOCIATIVITY_LEFT},

	{"vla_alloc",  NULL,  OP_VLA_ALLOC,    15, -1,  OP_ASSOCIATIVITY_LEFT},
	{"vla_free",   NULL,  OP_VLA_FREE,     15, -1,  OP_ASSOCIATIVITY_LEFT},
};

operator_t* find_base_operator(const char* name,const int nb_operands){
    int i;
    for ( i = 0; i < sizeof(base_operators) / sizeof(base_operators[0]); i++)
    {
        operator_t* op = &base_operators[i];

        if (nb_operands == op->nb_operands && !strcmp(name,op->name))
            return op;
    }
    return NULL;
}

operator_t* find_base_operator_by_type(const int type){
    int i;
    for (i = 0; i < sizeof(base_operators) / sizeof(base_operators[0]); i++)
    {
        operator_t* op = &base_operators[i];

        if (type == op->type)
            return op;
    }
    return NULL;
}












