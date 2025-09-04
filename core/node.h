#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "utils_list.h"
#include "utils_vector.h"
#include "utils_graph.h"
#include "lex.h"
#include "variable.h"
#include "operator.h"

#define OP_ASSOCIATIVITY 0
#define OP_ASSOCIATIVITY 1

struct node_s
{
    int type;
    node_t* parent;
    int nb_nodes;
    node_t** nodes;

    union 
    {
        variable_t* var;
        lex_word_t* w;
        label_t* label;
    };

    lex_word_t* debug_w;


    int priority;
    operator_t* op;

    variable_t* result;
    vector_t* result_nodes;
    node_t* split_parent;

    uint32_t            root_flag   :1; // set when node is root block
	uint32_t            file_flag   :1; // set when node is a file block
	uint32_t            enum_flag   :1; // set when node is a enum type
	uint32_t            class_flag  :1; // set when node is a class type
	uint32_t            union_flag  :1; // set when node is a union type
	uint32_t            define_flag :1; // set when node is a function & is defined not only declared
	uint32_t            const_flag  :1; // set when node is a const type
	uint32_t            split_flag  :1; // set when node is a split node of its parent
	uint32_t            _3ac_done   :1; // set when node's 3ac code is made

	uint32_t            semi_flag   :1; // set when followed by a ';'
    
};


struct label_s
{
    list_t list;
    int refs;
    int type;

    lex_word_t* w;
    node_t* node;
};


node_t* node_alloc(lex_word_t* w,int type,variable_t* var);
node_t* node_alloc_label(label_t* l);

node_t* node_clone(node_t* node);

int node_add_child(node_t* parent,node_t* child);
void node_del_child(node_t* parent,node_t* child);

void node_free(node_t* node);
void node_free_data(node_t* node);
void node_move_data(node_t* dst,node_t* src);

void node_print(node_t* node);

variable_t* _operand_get(const node_t* node);
function_t* _function_get(node_t* node);

typedef int (*node_find_pt)(node_t* node,void* arg,vector_t* results);
int node_search_bfs(node_t* root,void* arg,vector_t* results,int max,node_find_pt find);

label_t* label_alloc(lex_word_t* w);
void label_free(label_t* l);

#endif