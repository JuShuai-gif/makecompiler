#ifndef OPERATOR_HANDLER_SEMANTIC_H
#define OPERATOR_HANDLER_SEMANTIC_H

#include "operator_handler.h"

operator_handler_pt find_semantic_operator_handler(const int type);

int function_semantic_analysis(ast_t *ast, function_t *f);

int expr_semantic_analysis(ast_t *ast, expr_t *e);

int semantic_analysis(ast_t *ast);

#endif
