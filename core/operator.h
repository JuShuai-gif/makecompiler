#ifndef OPERATOR_H
#define OPERATOR_H

#include "utils_def.h"
#include "core_types.h"

#define OP_ASSOCIATIVITY_LEFT 0
#define OP_ASSOCIATIVITY_RIGHT 1

struct operator_s
{
    char* name;
    char* signature;
    int type;

    int priority;
    int nb_operands;
    int associativity;
};


operator_t* find_base_operator(const char* name,const int nb_operands);

operator_t* find_base_operator_by_type(const int type);


#endif