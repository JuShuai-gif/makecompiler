#ifndef OPERATOR_H
#define OPERATOR_H

#include "utils_def.h"
#include "core_types.h"

#define OP_ASSOCIATIVITY_LEFT 0
#define OP_ASSOCIATIVITY_RIGHT 1

// 运算符信息
struct operator_s
{
    char* name;// 名字
    char* signature;// 符号
    int type;// 类型

    int priority;// 优先级
    int nb_operands;// 
    int associativity;// 关联性
};

// 寻找基础操作
operator_t* find_base_operator(const char* name,const int nb_operands);

// 寻找基础操作通过类型
operator_t* find_base_operator_by_type(const int type);


#endif