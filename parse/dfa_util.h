#ifndef DFA_UTIL_H
#define DFA_UTIL_H

#include"lex_word.h"
#include"dfa.h"

static inline int dfa_is_hash(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_HASH == w->type;
}

static inline int dfa_is_LF(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LF == w->type;
}

static inline int dfa_is_lp(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LP == w->type;
}

static inline int dfa_is_rp(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RP == w->type;
}

static inline int dfa_is_ls(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LS == w->type;
}

static inline int dfa_is_rs(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RS == w->type;
}

static inline int dfa_is_lb(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_LB == w->type;
}

static inline int dfa_is_rb(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RB == w->type;
}

static inline int dfa_is_range(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_RANGE == w->type;
}

static inline int dfa_is_dot(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_DOT == w->type;
}

static inline int dfa_is_comma(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_COMMA == w->type;
}

static inline int dfa_is_semicolon(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_SEMICOLON == w->type;
}

static inline int dfa_is_enum(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ENUM == w->type;
}

static inline int dfa_is_union(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_UNION == w->type;
}

static inline int dfa_is_colon(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_COLON == w->type;
}

static inline int dfa_is_star(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_STAR == w->type;
}

static inline int dfa_is_assign(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_ASSIGN == w->type;
}

static inline int dfa_is_identity(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_identity(w);
}

static inline int dfa_is_const_integer(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_const_integer(w);
}

static inline int dfa_is_base_type(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_base_type(w);
}

static inline int dfa_is_sizeof(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_SIZEOF == w->type;
}

static inline int dfa_is_include(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_INCLUDE == w->type;
}

static inline int dfa_is_define(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DEFINE == w->type;
}

static inline int dfa_is_vargs(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_VAR_ARGS == w->type;
}

static inline int dfa_is_const(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONST == w->type;
}

static inline int dfa_is_static(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_STATIC == w->type;
}

static inline int dfa_is_extern(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_EXTERN == w->type;
}

static inline int dfa_is_inline(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_INLINE == w->type;
}

static inline int dfa_is_va_start(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_START == w->type;
}

static inline int dfa_is_va_arg(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_ARG == w->type;
}

static inline int dfa_is_va_end(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VA_END == w->type;
}

static inline int dfa_is_container(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONTAINER == w->type;
}

static inline int dfa_is_const_string(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_CONST_STRING == w->type;
}

static inline int dfa_is_if(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_IF == w->type;
}

static inline int dfa_is_else(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ELSE == w->type;
}

static inline int dfa_is_continue(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONTINUE == w->type;
}

static inline int dfa_is_break(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_BREAK == w->type;
}

static inline int dfa_is_return(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_RETURN == w->type;
}

static inline int dfa_is_goto(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_GOTO == w->type;
}

static inline int dfa_is_while(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_WHILE == w->type;
}

static inline int dfa_is_do(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DO == w->type;
}

static inline int dfa_is_for(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_FOR == w->type;
}

static inline int dfa_is_switch(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_SWITCH == w->type;
}

static inline int dfa_is_default(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_DEFAULT == w->type;
}

static inline int dfa_is_case(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CASE == w->type;
}

static inline int dfa_is_var(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_VAR == w->type;
}

#endif
