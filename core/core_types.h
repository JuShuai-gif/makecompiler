#ifndef CORE_TYPES_H
#define CORE_TYPES_H

// 前端语义分析相关
typedef struct type_s type_t; // 类型信息
typedef struct variable_s variable_t;// 变量信息，变量名、类型、作用域、存储位置等
typedef struct member_s member_t;// 结构体/类的成员，例如 struct { int x; float y; } 里面的 x 和 y
typedef struct index_s index_t;// 用于数组下标或类似结构的索引信息，例如a[i][j]的i和j
typedef struct label_s label_t;// 表示控制流里的标签，常用于中间代码/汇编生成

// 抽象语法树(AST)/中间表示() IR
typedef struct node_s node_t;// 一般是 AST 的通用节点类型
typedef struct node_s expr_t;// 是node_t的别名，表示表达式节点。表达式(加减乘除、函数调用)通常也是AST节点的一种
typedef struct operator_s operator_t;// 表示运算符(+ - * / && ||等)，通常和 expr_t 结合使用
typedef struct block_s block_t;// 表示一个语句块，例如 { ... }，里面可能有多个语句节点（node_t）
typedef struct function_s function_t;// 表示函数定义，包含函数名、参数列表、返回类型、函数体
typedef struct scopr_s scope_t;// 表示作用域，保存符号表(变量、函数名)、父作用域指针等


//=================  中间代码/三地址码(3AC)  ======================//
typedef struct str_3ac_code_s str_3ac_code_t;// 表示三地址码(TAC)形式的一条或一段代码
typedef struct inst_ops_s inst_ops_t;// 表示指令操作数。可能是寄存器、立即数、内存地址等
typedef struct regs_ops_s regs_ops_t;// 表示寄存器操作数(专门用于寄存器分配阶段)
typedef struct register_s register_t;// 抽象寄存器信息，可能包含编号、是否被占用、分配给那个变量等
typedef struct OpCode_s OpCode_t;// 表示操作码(Opcode)，比如 ADD，SUB，MUL,DIV,MOV

//=================  后端扩展/硬件相关  ======================//
typedef struct epin_s Epin;// 硬件引脚的抽象
typedef struct ecomponent_s Ecomponent;// 电子元件，比如芯片、寄存器组、逻辑门的抽象
typedef struct efunction_s Efunction;// 硬件功能或电路功能模块
typedef struct eboard_s Eboard;// 电路板(board)抽象，一个更大的硬件单元

#define EDA_MAX_BITS 256

// 核心类型
enum core_types{
	// 算术运算
    OP_ADD	= 0,    // +
	OP_SUB,         // -
	OP_MUL,         // *
	OP_DIV,         // / div
	OP_MOD,         // % mod

	// 自增自减
	OP_INC,         // 前缀 ++
	OP_DEC,         // 前缀 --
	OP_INC_POST,    // 后缀 ++
	OP_DEC_POST,    // 后缀 --

	// 一元原酸
	OP_NEG,         // -
	OP_POSITIVE,    // +

	// 位移运算
	OP_SHL,         // <<
	OP_SHR,         // >>

	// 按位运算 
	OP_BIT_AND,     // &
	OP_BIT_OR,      // |
	OP_BIT_NOT,     // ~
	OP_BIT_XOR,     // ^

	// 逻辑运算
	OP_LOGIC_AND,   // &&
	OP_LOGIC_OR,    // ||
	OP_LOGIC_NOT,   // !

	// 赋值运算符
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

	// 比较运算符
	OP_EQ,			// == equal
	OP_NE,			// != not equal
	OP_LT,			// < less than
	OP_GT,			// > greater than
	OP_LE,			// <= less equal 
	OP_GE,			// >= greater equal

	// 其它表达式运算
	OP_EXPR,		// () expr 表达式
	OP_CALL,		// () function call 函数调用
	OP_TYPE_CAST,	// (char*) type cast 类型转换

	OP_CREATE,	    // create 对象/变量创建
	OP_SIZEOF,	    // sizeof

	OP_CONTAINER,   // container_of

	OP_ADDRESS_OF,	// & get variable address 取地址
	OP_DEREFERENCE,	// * pointer dereference 解引用
	OP_ARRAY_INDEX,	// [] array index 数组下标

	OP_POINTER,     // -> struct member 结构体指针成员访问
	OP_DOT,			// . dot 结构体成员访问

	OP_VLA_ALLOC,   // variable length array, VLA 动态数组分配
	OP_VLA_FREE,	// VLA 动态数组释放

	OP_VAR_ARGS,    // ... variable args

	OP_VA_START,
	OP_VA_ARG,
	OP_VA_END,

	// 49
	// 语句/控制流
	OP_BLOCK,		// 语句块
	OP_IF,			// if
	OP_FOR,			// for 
	OP_WHILE,		// while 
	OP_DO,          // do statement

	OP_SWITCH,      // switch-case statement
	OP_CASE,		// case
	OP_DEFAULT,		// default

	OP_RETURN,		// return 
	OP_BREAK,		// break 
	OP_CONTINUE,	// continue 
	OP_ASYNC,       // async 
	LABEL,          // label
	OP_GOTO,        // goto 

	N_OPS, 			// 源语言运算符总数 

	// 58
	//// 三地址码(3AC)扩展指令
	OP_3AC_TEQ,		// test if = 0
	OP_3AC_CMP,		// cmp > 0, < 0, = 0, etc

	OP_3AC_LEA,		// 取地址 LEA

	// setcc 条件置位
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
	// 内存操作
	OP_3AC_INC,	// 内存值自增/自减
	OP_3AC_DEC,

	OP_3AC_ASSIGN_DEREFERENCE,     // left value, *p = expr

	OP_3AC_ASSIGN_ARRAY_INDEX,     // left value, a[0] = expr
	OP_3AC_ADDRESS_OF_ARRAY_INDEX,

	// 97
	OP_3AC_ASSIGN_POINTER,         // left value, p->a = expr
	OP_3AC_ADDRESS_OF_POINTER,

	// 条件/无条件跳转
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

	// 栈操作
	OP_3AC_PUSH,    // push a var to stack,  only for 3ac & native
	OP_3AC_POP,     // pop a var from stack, only for 3ac & native

	OP_3AC_PUSH_RETS, // push return value registers, only for 3ac & native
	OP_3AC_POP_RETS,  // pop  return value registers, only for 3ac & native

	// 内存操作
	OP_3AC_MEMSET,  //

	OP_3AC_SAVE,     // save a var to memory,   only for 3ac & native
	OP_3AC_LOAD,     // load a var from memory, only for 3ac & native
	OP_3AC_RELOAD,   // reload a var from memory, only for 3ac & native
	OP_3AC_RESAVE,   // resave a var to memory, only for 3ac & native

	// 其他
	OP_3AC_DUMP,
	OP_3AC_NOP,
	OP_3AC_END,

	N_3AC_OPS,      // 三地址码运算符总数


	//// 变量与类型
	VAR_CHAR,       // char 

	// 有符号整数
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
	// 无符号整数
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

	FUNCTION_PTR, // function pointer 函数指针

	// 浮点数
	VAR_FLOAT,      // float variable
	VAR_DOUBLE,		// double variable

	// 其他
	FUNCTION,		// 函数

	STRUCT,			// 结构体
};

/*  通过整数区间/枚举值判断某个操作或类型属于那一类  */
// 判断是否是赋值操作
static int type_is_assign(int type){
    return type >= OP_ASSIGN && type <= OP_OR_ASSIGN;
}

// 判断是否为带二元运算的赋值操作（+=、-=、*=、/=、%=、&=、|= 等）
static int type_is_binary_assign(int type){
    return type >= OP_ADD_ASSIGN && type <= OP_OR_ASSIGN;
}

// 判断是否为解引用赋值操作，例如 *p = v;
static int type_is_assign_dereference(int type){
    return OP_3AC_ASSIGN_DEREFERENCE == type;
}

// 判断是否为数组元素赋值，例如 a[i] = v;
static int type_is_assign_array_index(int type){
    return OP_3AC_ADDRESS_OF_ARRAY_INDEX == type;
}

// 判断是否为指针赋值，例如 p = q; 或 *(p + offset) = v;
static int type_is_assign_pointer(int type){
    return OP_3AC_ASSIGN_POINTER == type;
}

// 判断是否为有符号整数类型（char, short, int, long, intptr 等）
static int type_is_signed(int type){
    return type >= VAR_CHAR && type <= VAR_INTPTR;
}

// 判断是否为无符号整数类型（unsigned 版本的各种整数，以及函数指针）
static int type_is_unsigned(int type){
    return (type >= VAR_U8 && type <= VAR_UINTPTR) || FUNCTION_PTR == type;
}

// 判断是否为整数类型（包含有符号和无符号整数）
static int type_is_integer(int type){
    return (type >= VAR_CHAR && type <= VAR_UINTPTR);
}

// 判断是否为浮点数类型（float 或 double）
static int type_is_float(int type){
    return VAR_FLOAT == type || VAR_DOUBLE == type;
}

// 判断是否为数值类型（整数 + 浮点数）
static int type_is_number(int type){
    return type >= VAR_CHAR && type <= VAR_DOUBLE;
}

// 判断是否为变量类型（所有基本数值类型 + 结构体）
static int type_is_var(int type){
    return (type >= VAR_CHAR && type <= VAR_DOUBLE) || type >= STRUCT;
}

// 判断是否为运算符（加减乘除、逻辑、比较等），范围在 N_3AC_OPS 之前
static int type_is_operator(int type){
    return type >= OP_ADD && type < N_3AC_OPS;
}

// 判断是否为比较运算符（==、!=、<、<=、>、>=）
static int type_is_cmp_operator(int type){
    return type >= OP_EQ && type <= OP_GE;
}

// 判断是否为逻辑运算符（逻辑与、逻辑或、逻辑非）
static int type_is_logic_operator(int type){
    return type >= OP_LOGIC_AND && type <= OP_LOGIC_NOT;
}

// 判断是否为跳转指令（goto、条件跳转 JZ, JNZ, JB, JBE ...）
static int type_is_jmp(int type){
    return type == OP_GOTO || (type >= OP_3AC_JZ && type <= OP_3AC_JBE);
}

// 判断是否为条件跳转指令（JZ, JNZ, JB, JBE ...）
static int type_is_jcc(int type){
    return type >= OP_3AC_JZ && type <= OP_3AC_JBE;
}

// 判断是否为 setcc 指令（条件置位，setz, setnz, setb, setbe ...）
static int type_is_setcc(int type){
    return type >= OP_3AC_SETZ && type <= OP_3AC_SETBE;
}
#endif