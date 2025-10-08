#ifndef DFA_UTIL_H
#define DFA_UTIL_H

#include"lex_word.h"
#include"dfa.h"

// 判断是否为 '#' 预处理符号
static inline int dfa_is_hash(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_HASH == w->type;
}

// 判断是否为换行符 LF
static inline int dfa_is_LF(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LF == w->type;
}

// 判断是否为左括号 '('
static inline int dfa_is_lp(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LP == w->type;
}

// 判断是否为右括号 ')'
static inline int dfa_is_rp(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RP == w->type;
}

// 判断是否为左方括号 '['
static inline int dfa_is_ls(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LS == w->type;
}
// 判断是否为右方括号 ']'
static inline int dfa_is_rs(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RS == w->type;
}
// 判断是否为左花括号 '{'
static inline int dfa_is_lb(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LB == w->type;
}
// 判断是否为右花括号 '}'
static inline int dfa_is_rb(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RB == w->type;
}
// 判断是否为范围操作符 '-'（例如数组下标或其他用途）
static inline int dfa_is_range(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RANGE == w->type;
}
// 判断是否为点号 '.'
static inline int dfa_is_dot(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_DOT == w->type;
}
// 判断是否为逗号 ','
static inline int dfa_is_comma(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_COMMA == w->type;
}
// 判断是否为分号 ';'
static inline int dfa_is_semicolon(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_SEMICOLON == w->type;
}
// 判断是否为 enum 关键字
static inline int dfa_is_enum(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ENUM == w->type;
}
// 判断是否为 union 关键字
static inline int dfa_is_union(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_UNION == w->type;
}
// 判断是否为冒号 ':'
static inline int dfa_is_colon(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_COLON == w->type;
}
// 判断是否为星号 '*'（指针符号）
static inline int dfa_is_star(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_STAR == w->type;
}

// 判断是否为赋值符 '='
static inline int dfa_is_assign(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_ASSIGN == w->type;
}

// 判断是否为标识符（变量名、类型名等）
static inline int dfa_is_identity(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_identity(w);
}

// 判断是否为整数常量
static inline int dfa_is_const_integer(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_const_integer(w);
}

// 判断是否为基础类型（int、char、float 等）
static inline int dfa_is_base_type(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_base_type(w);
}

// 判断是否为 sizeof 关键字
static inline int dfa_is_sizeof(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_SIZEOF == w->type;
}

// 判断是否为 include 关键字
static inline int dfa_is_include(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_INCLUDE == w->type;
}

// 判断是否为 define 关键字
static inline int dfa_is_define(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DEFINE == w->type;
}

// 判断是否为可变参数标记 '...'
static inline int dfa_is_vargs(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_VAR_ARGS == w->type;
}

// 判断是否为 const 关键字
static inline int dfa_is_const(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONST == w->type;
}

// 判断是否为 static 关键字
static inline int dfa_is_static(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_STATIC == w->type;
}

// 判断是否为 extern 关键字
static inline int dfa_is_extern(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_EXTERN == w->type;
}

// 判断是否为 inline 关键字
static inline int dfa_is_inline(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_INLINE == w->type;
}

// 判断是否为 va_start 关键字（可变参数宏）
static inline int dfa_is_va_start(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_START == w->type;
}

// 判断是否为 va_arg 关键字
static inline int dfa_is_va_arg(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_ARG == w->type;
}

// 判断是否为 va_end 关键字
static inline int dfa_is_va_end(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_END == w->type;
}

// 判断是否为 container 关键字
static inline int dfa_is_container(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONTAINER == w->type;
}

// 判断是否为字符串常量
static inline int dfa_is_const_string(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_CONST_STRING == w->type;
}

// 判断是否为 if 关键字
static inline int dfa_is_if(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_IF == w->type;
}

// 判断是否为 else 关键字
static inline int dfa_is_else(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ELSE == w->type;
}

// 判断是否为 continue 关键字
static inline int dfa_is_continue(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONTINUE == w->type;
}

// 判断是否为 break 关键字
static inline int dfa_is_break(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_BREAK == w->type;
}

// 判断是否为 return 关键字
static inline int dfa_is_return(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_RETURN == w->type;
}

// 判断是否为 goto 关键字
static inline int dfa_is_goto(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_GOTO == w->type;
}

// 判断是否为 while 关键字
static inline int dfa_is_while(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_WHILE == w->type;
}

// 判断是否为 do 关键字
static inline int dfa_is_do(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DO == w->type;
}

// 判断是否为 for 关键字
static inline int dfa_is_for(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_FOR == w->type;
}

// 判断是否为 switch 关键字
static inline int dfa_is_switch(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_SWITCH == w->type;
}

// 判断是否为 default 关键字
static inline int dfa_is_default(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DEFAULT == w->type;
}

// 判断是否为 case 关键字
static inline int dfa_is_case(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CASE == w->type;
}

// 判断是否为 var 关键字
static inline int dfa_is_var(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VAR == w->type;
}

#endif
