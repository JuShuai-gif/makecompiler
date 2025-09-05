#ifndef EXPR_H
#define EXPR_H

#include "node.h"

expr_t* expr_alloc();
expr_t* expr_clone(expr_t* e);
void expr_free(expr_t* e);

int expr_add_node(expr_t* e,node_t* node);
void expr_simplify(expr_t** pe);

#endif


