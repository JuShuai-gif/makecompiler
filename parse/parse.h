#ifndef PARSE_H
#define PARSE_H

#include"lex.h"
#include"ast.h"
#include"dfa.h"
#include"utils_stack.h"
#include"dwarf.h"

typedef struct parse_s   parse_t;
typedef struct dfa_data_s    dfa_data_t;

#define SHNDX_TEXT   1
#define SHNDX_RODATA 2
#define SHNDX_DATA   3

#define SHNDX_DEBUG_ABBREV 4
#define SHNDX_DEBUG_INFO   5
#define SHNDX_DEBUG_LINE   6
#define SHNDX_DEBUG_STR    7

struct parse_s
{
	lex_t*         lex_list;// 分词器
	lex_t*         lex;

	ast_t*         ast;

	dfa_t*         dfa;
	dfa_data_t*        dfa_data;

	vector_t*      symtab;
	vector_t*      global_consts;

	dwarf_t*       debug;
};

typedef struct {
	lex_word_t*    w;
	intptr_t           i;
} dfa_index_t;

typedef struct {
	expr_t*        expr;

	int                n;        // number of index array
	dfa_index_t        index[0]; // index array

} dfa_init_expr_t;

typedef struct {
	lex_word_t*      identity;
	lex_word_t*      type_w;
	type_t*          type;

	int                  number;
	int                  nb_pointers;
	function_t*      func_ptr;

	uint32_t             const_flag :1;
	uint32_t             extern_flag:1;
	uint32_t             static_flag:1;
	uint32_t             inline_flag:1;

} dfa_identity_t;

struct dfa_data_s {
	void**               module_datas;

	expr_t*          expr;
	int                  expr_local_flag;

	stack_t*         current_identities;
	variable_t*      current_var;
	lex_word_t*      current_var_w;

	int                  nb_sizeofs;
	int                  nb_containers;

	function_t*      current_function;
	int                  argc;

	lex_word_t*      current_async_w;

	type_t*	         root_struct;
	type_t*	         current_struct;

	node_t*          current_node;

	node_t*          current_while;

	node_t*          current_for;
	vector_t*        for_exprs;

	node_t*          current_return;
	node_t*          current_goto;

	node_t*          current_va_start;
	node_t*          current_va_arg;
	node_t*          current_va_end;

	uint32_t             const_flag :1;
	uint32_t             extern_flag:1;
	uint32_t             static_flag:1;
	uint32_t             inline_flag:1;

	uint32_t             var_semicolon_flag:1;

	int              nb_lbs;
	int              nb_rbs;

	int              nb_lss;
	int              nb_rss;

	int              nb_lps;
	int              nb_rps;
};

int parse_dfa_init(parse_t* parse);

int parse_open (parse_t** pparse);
int parse_close(parse_t*   parse);

int parse_file(parse_t* parse, const char* path);

int parse_compile(parse_t* parse, const char* arch, int _3ac);
int parse_to_obj (parse_t* parse, const char* out, const char* arch);

int _find_global_var(node_t* node, void* arg, vector_t* vec);
int _find_function  (node_t* node, void* arg, vector_t* vec);

#endif
