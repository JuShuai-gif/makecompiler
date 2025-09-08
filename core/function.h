#ifndef FUNCTION_H
#define FUNCTION_H
#include "node.h"

struct function_s
{
    node_t node;
    scope_t* scope;
    string_t* signature;

    list_t list;// 为了范围

    vector_t* rets;// 返回值
    vector_t* argv;// 参数向量

    int args_int;
    int args_float;
    int args_double;

    int op_type;

    vector_t* callee_functions;
    vector_t* caller_functions;

    list_t basic_block_list_head;
    int nb_basic_blocks;

    vector_t* jmps;

    list_t dag_list_head;

    vector_t* bb_loops;
    vector_t* bb_groups;

    vector_t* text_relas;
    vector_t* data_relas;

    inst_ops_t* iops;
    regs_ops_t* rops;

    Efunction* ef;

    str_3ac_code_t* init_code;
    int init_code_bytes;

    int callee_saved_size;

    int local_vars_size;
    int code_bytes;

    uint32_t          visited_flag:1;
	uint32_t          bp_used_flag:1;

	uint32_t          static_flag:1;
	uint32_t          extern_flag:1;
	uint32_t          inline_flag:1;
	uint32_t          member_flag:1;

	uint32_t          vargs_flag:1;
	uint32_t          void_flag :1;
	uint32_t          call_flag :1;
	uint32_t          vla_flag  :1;

	uint32_t          compile_flag:1;
	uint32_t          native_flag :1;
};

function_t* function_alloc(lex_word_t* w);
void function_free(function_t* f);

int function_same(function_t* f0,function_t* f1);
int function_same_type(function_t* f0,function_t* f1);
int function_same_argv(vector_t* argv0,vector_t* argv1);
int function_like_argv(vector_t* argv0,vector_t* argv1);

#endif