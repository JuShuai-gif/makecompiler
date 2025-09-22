#ifndef SCOPE_H
#define SCOPE_H

#include "utils_list.h"
#include "utils_vector.h"
#include "lex_word.h"
#include "core_types.h"
// -------------------------
// 实现作用域管理，可以理解为一个符号表(Symbol Table)
// 作用域 (Scope) 定义   语义分析的核心阶段，保证变量/函数/类型的查找规则符合语言语义
// -------------------------

// 作用域结构体，表示一个“变量/类型/函数/标签”等符号的可见范围
struct scope_s
{
    list_t list;// 链表节点，用于把多个作用域链接起来（比如作用域栈）
    string_t* name;// 作用域名字（例如函数名、类名，或者匿名作用域）

    lex_word_t* w;// 对应的词法单元（方便调试/错误提示）

    vector_t* vars; // 变量列表（当前作用域声明的变量）
    list_t type_list_head;// 类型链表（当前作用域声明的类型，比如 struct、typedef）
    list_t operator_list_head;// 运算符重载列表（如果语言支持 operator overloading）
    list_t function_list_head;// 函数链表（当前作用域定义的函数）
    list_t label_list_head;// 标签链表（如 goto 标签）
};


// -------------------------
// 作用域的创建与销毁
// -------------------------

// 创建一个新的作用域（传入词法单元 w 和作用域名 name）
scope_t* scope_alloc(lex_word_t* w,const char* name);

// 释放一个作用域及其内容
void scope_free(scope_t* scope);

// -------------------------
// 向作用域中添加符号
// -------------------------

// 添加一个变量到作用域中
void scope_push_var(scope_t* scope,variable_t* var);


// 添加一个类型到作用域中
void scope_push_type(scope_t* scope,type_t* t);


// 添加一个运算符重载到作用域中
void scope_push_operator(scope_t* scope,function_t* op);


// 添加一个函数到作用域中
void scope_push_function(scope_t* scope,function_t* f);


// -------------------------
// 在作用域中查找符号
// -------------------------

// 按名字查找类型
type_t* scope_find_type(scope_t* scope,const char* name);

// 按内部 type id 查找类型
type_t* scope_find_type_type(scope_t* scope,const int type);

// 按名字查找变量
variable_t* scope_find_variable(scope_t* scope, const char* name);

// 按名字查找函数
function_t* scope_find_function(scope_t* scope, const char* name);

// 查找是否存在相同函数（匹配函数签名）
function_t* scope_find_same_function(scope_t* scope, function_t* f0);

// 查找最合适的函数（函数重载匹配：根据函数名和参数类型匹配）
function_t* scope_find_proper_function(scope_t* scope, const char* name, vector_t* argv);

// 查找所有重载函数（返回多个可能的匹配）
int scope_find_overloaded_functions(vector_t** pfunctions, scope_t* scope, const int op_type, vector_t* argv);

// 查找相似的函数（可能参数类型可转换）
int scope_find_like_functions(vector_t** pfunctions, scope_t* scope, const char* name, vector_t* argv);

// -------------------------
// 标签管理
// -------------------------

// 向作用域中添加一个 label
void scope_push_label(scope_t* scope, label_t* l);

// 按名字查找 label
label_t* scope_find_label(scope_t* scope, const char* name);

#endif