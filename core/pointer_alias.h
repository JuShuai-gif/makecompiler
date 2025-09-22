#ifndef POINTER_ALIAS_H
#define POINTER_ALIAS_H

#include"function.h"
#include"basic_block.h"
#include"3ac.h"

#define POINTER_NOT_INIT -1000

int _bb_copy_aliases (basic_block_t* bb, dag_node_t* dn_pointer, dag_node_t* dn_dereference, vector_t* aliases);

int _bb_copy_aliases2(basic_block_t* bb, vector_t* aliases);

int __alias_dereference(vector_t* aliases, dag_node_t* dn_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head);

int pointer_alias   (vector_t* aliases, dag_node_t*  dn_alias,   _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head);
int pointer_alias_ds(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head);

int pointer_alias_ds_leak(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head);

#endif
