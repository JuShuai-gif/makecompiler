#ifndef OPERATOR_HANDLER_H
#define OPERATOR_HANDLER_H

#include "ast.h"

typedef int (*operator_handler_pt)(ast_t *ast, node_t **nodes, int nb_nodes, void *data);

operator_handler_pt find_3ac_operator_handler(const int type);

int function_to_3ac(ast_t *ast, function_t *f, list_t *_3ac_list_head);

#endif