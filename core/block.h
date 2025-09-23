#ifndef BLOCK_H
#define BLOCK_H

#include "node.h"

// 表示一个 "代码块"(block)，在编译器/解释器里常用于描述作用域
struct block_s
{
    node_t node;        // 基础节点信息（一般存放语法树相关的通用字段）
    scope_t* scope;     // 当前 block 的作用域，存放变量、类型、函数等符号
    string_t* name;     // block 的名字（可能是函数名、结构体名、作用域名）
};

// 根据词法单元(lex_word_t) 分配并初始化一个 block
block_t* block_alloc(lex_word_t* w);

// 根据字符串常量(C 字符串)分配并初始化一个block
block_t* block_alloc_cstr(const char* name);

// 释放 block 及其相关资源
void block_free(block_t* b);

// 在 block 中查找指定名字的类型定义
type_t* block_find_type(block_t* b,const char* name);

// 在 block 中根据类型标识符（整型 ID）查找类型定义
type_t* block_find_type_type(block_t* b,const int type);

// 在 block 中查找指定名字的变量
variable_t* block_find_variable(block_t* b,const char* name);

// 在 block 中查找指定名字的函数
function_t* block_find_function(block_t* b,const char* name);

// 在 block 中查找指定名字的标签（label，用于 goto 等跳转）
label_t* block_find_label(block_t* b,const char* name);

#endif