#ifndef BLOCK_H
#define BLOCK_H

#include "node.h"

struct block_s
{
    node_t node;
    scope_t* scope;
    string_t* name;
};

block_t* block_alloc(lex_word_t* w);
block_t* block_alloc_cstr(const char* name);

void block_free(block_t* b);

type_t* block_find_type(block_t* b,const char* name);

type_t* block_find_type_type(block_t* b,const int type);

variable_t* block_find_variable(block_t* b,const char* name);

function_t* block_ding_function(block_t* b,const char* name);

label_t* block_fing_label(block_t* b,const char* name);

#endif