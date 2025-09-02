#ifndef LEX_WORD_H
#define LEX_WORD_H

#include <stdint.h>

#include "utils_string.h"
#include "utils_list.h"

typedef struct lex_word_s lex_word_t;
typedef struct complex_s complex_t;
typedef struct macro_s macro_t;

// 词法单元
enum lex_words {
    LEX_WORD_PLUS = 0, // +
    LEX_WORD_MINUS,    // -
    LEX_WORD_STAR,     // *
    LEX_WORD_DIV,      // / div
    LEX_WORD_MOD,      // %

    LEX_WORD_INC, // ++
    LEX_WORD_DEC, // --

    LEX_WORD_SHL, // <<
    LEX_WORD_SHR, // >>

    LEX_WORD_BIT_AND, // &
    LEX_WORD_BIT_OR,  // |
    LEX_WORD_BIT_NOT, // ~

    LEX_WORD_LOGIC_AND, // &&
    LEX_WORD_LOGIC_OR,  // ||
    LEX_WORD_LOGIC_NOT, // ！
    LEX_WORD_LOGIC_XOR, // xor

    LEX_WORD_ASSIGN,         // = assign
    LEX_WORD_ADD_ASSIGN,     // +=
    LEX_WORD_SUB_ASSIGN,     // -=
    LEX_WORD_MUL_ASSIGN,     // *=
    LEX_WORD_DIV_ASSIGN,     // /=
    LEX_WORD_MOD_ASSIGN,     // %=
    LEX_WORD_SHL_ASSIGN,     // <<=
    LEX_WORD_SHR_ASSIGN,     // >>=
    LEX_WORD_BIT_AND_ASSIGN, // &=
    LEX_WORD_BIT_OR_ASSIGN,  // |=

    LEX_WORD_LT, // < less than
    LEX_WORD_GT, // > greater than

    LEX_WORD_EQ, // == equal
    LEX_WORD_NE, // != not equal
    LEX_WORD_LE, // <= less equal
    LEX_WORD_GE, // >= greater equal

    LEX_WORD_LS, // [ left square brackets
    LEX_WORD_RS, // ] right square brackets

    LEX_WORD_LP, // ( left parentheses
    LEX_WORD_RP, // ) right parentheses

    LEX_WORD_LA, // << left angle brackets
    LEX_WORD_RA, // >> right angle brackets

    LEX_WORD_KEY_SIZEOF,    // sizeof
    LEX_WORD_KEY_CREATE,    // create class object
    LEX_WORD_KEY_CONTAINER, // container_of
    LEX_WORD_KEY_VA_ARG,    // va_arg

    LEX_WORD_ARROW, // ->  arrow
    LEX_WORD_DOT,   // .   dot

    LEX_WORD_RANGE,    // ..  range
    LEX_WORD_VAR_ARGS, // ... variable args

    LEX_WORD_LB, // { left brace
    LEX_WORD_RB, // } right brace

    LEX_WORD_LF,    // '\n', line feed, LF
    LEX_WORD_HASH,  // #  hash
    LEX_WORD_HASH2, // ## hash2

    LEX_WORD_COMMA,     // , comma
    LEX_WORD_SEMICOLON, // ;
    LEX_WORD_COLON,     // : colon
    LEX_WORD_SPACE,     // ' ' space

    LEX_WORD_EOF, // EOF

    LEX_WORD_KEY_IF,   // if
    LEX_WORD_KEY_ELSE, // else

    LEX_WORD_KEY_FOR,   // for
    LEX_WORD_KEY_DO,    // do
    LEX_WORD_KEY_WHILE, // while

    LEX_WORD_KEY_BREAK,    // break
    LEX_WORD_KEY_CONTINUE, // continune

    LEX_WORD_KEY_SWITCH,  // switch
    LEX_WORD_KEY_CASE,    // case
    LEX_WORD_KEY_DEFAULT, // default

    LEX_WORD_KEY_RETURN, // return

    LEX_WORD_KEY_GOTO, // goto

    LEX_WORD_KEY_OPERATOR,  // operator
    LEX_WORD_KEY_UNDERLINE, // _ underline

    LEX_WORD_KEY_INCLUDE, // #include
    LEX_WORD_KEY_DEFINE,  // #define
    LEX_WORD_KEY_ENDIF,   // #endif

    LEX_WORD_KEY_CHAR, // char

    LEX_WORD_KEY_INT,    // int
    LEX_WORD_KEY_FLOAT,  // float
    LEX_WORD_KEY_DOUBLE, // double

    LEX_WORD_KEY_INT8,  // int8_t
    LEX_WORD_KEY_INT1,  // int1_t
    LEX_WORD_KEY_INT2,  // int2_t
    LEX_WORD_KEY_INT3,  // int3_t
    LEX_WORD_KEY_INT4,  // int4_t
    LEX_WORD_KEY_INT16, // int16_t
    LEX_WORD_KEY_INT32, // int32_t
    LEX_WORD_KEY_INT64, // int64_t

    LEX_WORD_KEY_UINT8,  // uint8_t
    LEX_WORD_KEY_BIT,    // bit
    LEX_WORD_KEY_BIT2,   // bit2_t
    LEX_WORD_KEY_BIT3,   // bit3_t
    LEX_WORD_KEY_BIT4,   // bit4_t
    LEX_WORD_KEY_UINT16, // uint16_t
    LEX_WORD_KEY_UINT32, // uint32_t
    LEX_WORD_KEY_UINT64, // uint64_t

    LEX_WORD_KEY_INTPTR,  // intptr_t
    LEX_WORD_KEY_UINTPTR, // uintptr_t

    LEX_WORD_KEY_VOID, // void

    LEX_WORD_KEY_VA_START, // va_start
    LEX_WORD_KEY_VA_END,   // va_end

    LEX_WORD_KEY_CLASS, // calss

    LEX_WORD_KEY_CONST,  // const
    LEX_WORD_KEY_STATIC, // static
    LEX_WORD_KEY_EXTERN, // extern
    LEX_WORD_KEY_INLINE, // inline

    LEX_WORD_KEY_ASYNC, // async
    LEX_WORD_KEY_AWAIT, // await

    LEX_WORD_KEY_ENUM,   // enum
    LEX_WORD_KEY_UNION,  // union
    LEX_WORD_KEY_STRUCT, // struct

    LEX_WORD_KEY_VAR,

    LEX_WORD_CONST_CHAR,

    LEX_WORD_CONST_INT,
    LEX_WORD_CONST_U32,
    LEX_WORD_CONST_I64,
    LEX_WORD_CONST_U64,

    LEX_WORD_CONST_FLOAT,
    LEX_WORD_CONST_DOUBLE,
    LEX_WORD_CONST_COMPLEX,

    LEX_WORD_CONST_STRING,

    LEX_WORD_ID, // identity, start of _, a-z, A-Z, may include 0-9

};

// 复数
struct complex_s {
    float real; // 实部
    float imag; // 虚部
};

// 表示一个宏定义
struct macro_s {
    int refs;// 宏定义引用的次数
    lex_word_t *w;// 宏的名字
    vector_t *argv;// 宏的参数列表
    lex_word_t *text_list;// 宏的展开内容(词法单元链表)
};

// lexer生成的Token(词法单元)结构体
struct lex_word_s {
    lex_word_t *next;// 指向下一个token(链表形式)
    int type;// token的类型，比如标识符、关键字、整数常量、浮点常量、字符串、字符串、符号

    // 联合体 union: 存储不同类型的常量值
    union {
        int32_t i;    // value for <= int32_t
        uint32_t u32; // value for <= uint32_t
        int64_t i64;  // value for int64_t
        uint64_t u64; // value for uint64_t
        float f;      // value for float
        double d;     // value for double
        complex_t z;  // value for complex
        string_t *s;  // value for string
    } data;

    string_t *text; // 原始源码里的字符串(比如 123 if x)
    string_t *file; // token 所在的源文件名
    int line;       // 所在行号
    int pos;        // 所在列号
};

// 判断 token 是否是标识符
static inline int lex_is_identity(lex_word_t *w) {
    return LEX_WORD_ID == w->type;
}

// 判断 token 是否是 运算符或符号
static inline int lex_is_operator(lex_word_t *w) {
    return w->type >= LEX_WORD_PLUS && w->type <= LEX_WORD_DOT;
}

// 判断 token 是否是 常量
static inline int lex_is_const(lex_word_t *w) {
    return w->type >= LEX_WORD_CONST_CHAR && w->type <= LEX_WORD_CONST_STRING;
}

// 判断 token 是否是 整数常量
static inline int lex_is_const_integer(lex_word_t *w) {
    return w->type >= LEX_WORD_CONST_CHAR && w->type <= LEX_WORD_CONST_U64;
}

// 判断 token 是否是 基础类型关键字
static inline int lex_is_base_type(lex_word_t *w) {
    return LEX_WORD_KEY_CHAR <= w->type && LEX_WORD_KEY_VOID >= w->type;
}

// 
macro_t *macro_alloc(lex_word_t *w);

void macro_free(macro_t *m);

lex_word_t *lex_word_alloc(string_t *file, int line, int pos, int type);

lex_word_t *lex_word_clone(lex_word_t *w);

void lex_word_free(lex_word_t *w);

#endif
