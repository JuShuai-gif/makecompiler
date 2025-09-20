#ifndef OPERATOR_HANDLER_CONST_H
#define OPERATOR_HANDLER_CONST_H

#include"operator_handler.h"

operator_handler_pt  find_const_operator_handler(const int type);

int function_const_opt(ast_t* ast, function_t* f);

#endif
