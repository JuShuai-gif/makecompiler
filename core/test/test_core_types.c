#include <stdio.h>
#include "core_types.h"

int main(){
    enum core_types type1 = OP_ADD;
    enum core_types type2 = STRUCT;
    printf("type1: %d\n",type1);
    printf("type2: %d\n",type2);

    // 赋值操作
    enum core_types type3 = OP_ADD_ASSIGN;
    int is_assign = type_is_assign(type3);
    printf("is assign: %d\n",is_assign);


    // 是否带二元运算的赋值操作
    enum core_types type4 = OP_ADD_ASSIGN;
    int is_binary_assign = type_is_binary_assign(type4);
    printf("is_binary_assign: %d\n",is_binary_assign);

    // 判断是否为解引用赋值操作
    enum core_types type5 = OP_3AC_ASSIGN_DEREFERENCE;
    int is_assign_dereference = type_is_assign_dereference(type5);
    printf("is_assign_dereference: %d\n",is_assign_dereference);


    
}









