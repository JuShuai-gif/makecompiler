#ifndef CORE_TYPES_H
#define CORE_TYPES_H

typedef struct type_s type_t;
typedef struct variable_s variable_t;

typedef struct member_s member_t;
typedef struct index_s index_t;

typedef struct label_s label_t;

typedef struct node_s node_t;
typedef struct node_s expr_t;
typedef struct operator_s operator_t;

typedef struct block_s block_t;
typedef struct function_s function_t;

typedef struct scopr_s scope_t;

typedef struct 3ac_code_s 3ac_code_t;
typedef struct inst_ops_s inst_ops_t;
typedef struct regs_ops_s regs_ops_t;
typedef struct register_s register_t;
typedef struct OpCode_s OpCode_t;

typedef struct epin_s Epin;
typedef struct ecomponent_s Ecomponent;
typedef struct efunction_s Efunction;
typedef struct eboard_s Eboard;

#define EDA_MAX_BITS 256

enum core_types{
    OP_ADD	= 0,    // +
	OP_SUB,         // -
	OP_MUL,         // *
	OP_DIV,         // / div
	OP_MOD,         // % mod

	OP_INC,         // ++
	OP_DEC,         // --

	// 7
	OP_INC_POST,    // ++
	OP_DEC_POST,    // --

	OP_NEG,         // -
	OP_POSITIVE,    // +

	OP_SHL,         // <<
	OP_SHR,         // >>

	// 13
	OP_BIT_AND,     // &
	OP_BIT_OR,      // |
	OP_BIT_NOT,     // ~
	OP_BIT_XOR,     // ^

	OP_LOGIC_AND,   // &&
	OP_LOGIC_OR,    // ||
	OP_LOGIC_NOT,   // !

	// 20
	OP_ASSIGN,      //  = assign
	OP_ADD_ASSIGN,  // +=
	OP_SUB_ASSIGN,  // -=
	OP_MUL_ASSIGN,  // *=
	OP_DIV_ASSIGN,  // /=
	OP_MOD_ASSIGN,  // %=
	OP_SHL_ASSIGN,  // <<=
	OP_SHR_ASSIGN,  // >>=
	OP_AND_ASSIGN,  // &=
	OP_OR_ASSIGN,   // |=

	// 30
	OP_EQ,			// == equal
	OP_NE,			// != not equal
	OP_LT,			// < less than
	OP_GT,			// > greater than
	OP_LE,			// <= less equal 
	OP_GE,			// >= greater equal

	// 36
	OP_EXPR,		// () expr
	OP_CALL,		// () function call
	OP_TYPE_CAST,	// (char*) type cast

	OP_CREATE,	    // create
	OP_SIZEOF,	    // sizeof

	OP_CONTAINER,   // container_of

	OP_ADDRESS_OF,	// & get variable address
	OP_DEREFERENCE,	// * pointer dereference
	OP_ARRAY_INDEX,	// [] array index

	OP_POINTER,     // -> struct member
	OP_DOT,			// . dot

	OP_VLA_ALLOC,   // variable length array, VLA
	OP_VLA_FREE,

	OP_VAR_ARGS,    // ... variable args

	OP_VA_START,
	OP_VA_ARG,
	OP_VA_END,

	// 49
	OP_BLOCK,		// statement block, first in fisr run
	OP_IF,			// if statement
	OP_FOR,			// for statement
	OP_WHILE,		// while statement
	OP_DO,          // do statement

	OP_SWITCH,      // switch-case statement
	OP_CASE,
	OP_DEFAULT,

	OP_RETURN,		// return statement
	OP_BREAK,		// break statement
	OP_CONTINUE,	// continue statement
	OP_ASYNC,       // async statement

	LABEL,          // label
	OP_GOTO,        // goto statement

	N_OPS, // total operators

	// 58
	OP_3AC_TEQ,		// test if = 0
	OP_3AC_CMP,		// cmp > 0, < 0, = 0, etc

	OP_3AC_LEA,

	OP_3AC_SETZ,
	OP_3AC_SETNZ,
	OP_3AC_SETGT,
	OP_3AC_SETLT,
	OP_3AC_SETGE,
	OP_3AC_SETLE,

	// only for float, double
	OP_3AC_SETA,
	OP_3AC_SETAE,
	OP_3AC_SETB,
	OP_3AC_SETBE,

	// these ops will update the value in memory
	OP_3AC_INC,
	OP_3AC_DEC,

	OP_3AC_ASSIGN_DEREFERENCE,     // left value, *p = expr

	OP_3AC_ASSIGN_ARRAY_INDEX,     // left value, a[0] = expr
	OP_3AC_ADDRESS_OF_ARRAY_INDEX,

	// 97
	OP_3AC_ASSIGN_POINTER,         // left value, p->a = expr
	OP_3AC_ADDRESS_OF_POINTER,

	OP_3AC_JZ,		// jz
	OP_3AC_JNZ,		// jnz
	OP_3AC_JGT,		// jgt
	OP_3AC_JLT,		// jlt
	OP_3AC_JGE,		// jge
	OP_3AC_JLE,		// jle

	// only for float, double
	OP_3AC_JA,
	OP_3AC_JAE,
	OP_3AC_JB,
	OP_3AC_JBE,

	OP_3AC_PUSH,    // push a var to stack,  only for 3ac & native
	OP_3AC_POP,     // pop a var from stack, only for 3ac & native

	OP_3AC_PUSH_RETS, // push return value registers, only for 3ac & native
	OP_3AC_POP_RETS,  // pop  return value registers, only for 3ac & native

	OP_3AC_MEMSET,  //

	OP_3AC_SAVE,     // save a var to memory,   only for 3ac & native
	OP_3AC_LOAD,     // load a var from memory, only for 3ac & native
	OP_3AC_RELOAD,   // reload a var from memory, only for 3ac & native
	OP_3AC_RESAVE,   // resave a var to memory, only for 3ac & native

	OP_3AC_DUMP,
	OP_3AC_NOP,
	OP_3AC_END,

	N_3AC_OPS,      // totaol 3ac operators

	VAR_CHAR,       // char variable

	VAR_I8,
	VAR_I1,
	VAR_I2,
	VAR_I3,
	VAR_I4,
	VAR_I16,
	VAR_I32,
	VAR_INT = VAR_I32,
	VAR_I64,
	VAR_INTPTR = VAR_I64,

	// 122
	VAR_U8,
	VAR_VOID,
	VAR_BIT,
	VAR_U2,
	VAR_U3,
	VAR_U4,
	VAR_U16,
	VAR_U32,
	VAR_U64,
	VAR_UINTPTR = VAR_U64,

	FUNCTION_PTR, // function pointer

	VAR_FLOAT,      // float variable
	VAR_DOUBLE,		// double variable


	FUNCTION,		// function

	STRUCT,			// struct type defined by user
};

static int type_is_assign(int type){
    return type >= OP_ASSIGN && type <= OP_OR_ASSIGN;
}

static int type_is_binary_assign(int type){
    return type >= OP_ADD_ASSIGN && type <= OP_OR_ASSIGN;
}

static int type_is_assign_dereference(int type){
    return OP_3AC_ASSIGN_DEREFERENCE == type;
}

static int type_is_assign_array_index(int type){
    return OP_3AC_ADDRESS_OF_ARRAY_INDEX == type;
}

static int type_is_assign_pointer(int type){
    return OP_3AC_ASSIGN_POINTER == type;
}

static int type_is_signed(int type){
    return type >= VAR_CHAR && type <= VAR_INTPTR;
}

static int type_is_unsigned(int type){
    return (type >= VAR_U8 && type <= VAR_UINTPTR) || FUNCTION_PTR == type;
}

static int type_is_integer(int type){
    return (type >= VAR_CHAR && type <= VAR_UINTPTR);
}

static int type_is_float(int type){
    return VAR_FLOAT == type || VAR_DOUBLE == type;
}

static int type_is_number(int type){
    return type >= VAR_CHAR && type <= VAR_DOUBLE;
}

static int type_is_var(int type){
    return (type >= VAR_CHAR && type <= VAR_DOUBLE) || type >= STRUCT;
}

static int type_is_operator(int type){
    return type >= OP_ADD && type < N_3AC_OPS;
}

static int type_is_cmp_operator(int type){
    return type >= OP_EQ && type <= OP_GE;
}

static int type_is_logic_operator(int type){
    return type >= OP_LOGIC_AND && type <= OP_LOGIC_NOT;
}

static int type_is_jmp(int type){
    return type == OP_GOTO || (type >= OP_3AC_JZ && type <= OP_3AC_JBE);
}

static int type_is_jcc(int type){
    return type >= OP_3AC_JZ && type <= OP_3AC_JBE;
}

static int type_is_setcc(int type){
    return type >= OP_3AC_SETZ && type <= OP_3AC_SETBE;
}
#endif