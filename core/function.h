#ifndef FUNCTION_H
#define FUNCTION_H
#include "node.h"

/*
function_s 本质上是编译器/解释器里函数抽象表示的数据结构，保存了从语法、语义分析到代码生成阶段所需的各种信息：
    语义信息：函数作用域、参数、返回值、函数签名。

    代码生成相关：基本块列表、跳转信息、DAG 优化、寄存器和指令操作。

    内存布局：局部变量栈大小、callee 保存的寄存器大小。

    链接相关：重定位信息。

    标志位：标记函数是否是 static/extern/inline、是否有可变参数等。
*/

// 表示函数(Function)的结构体
struct function_s
{
    node_t node;// 基础语法树节点信息，记录函数在 AST 中的通用属性
    scope_t* scope;// 作用域信息，函数的局部变量、符号表等都挂在该作用域下
    string_t* signature;// 函数签名(包含函数名、参数类型、返回类型等字符串)

    list_t list;// 用于范围管理

    vector_t* rets;// 返回值的类型/符号向量
    vector_t* argv;// 参数列表

    int args_int;// 整型参数个数
    int args_float;// float 参数个数
    int args_double;// double 参数个数

    int op_type;// 函数操作类型

    vector_t* callee_functions;// 被当前函数调用的函数列表
    vector_t* caller_functions;// 调用当前函数的函数列表

    list_t basic_block_list_head;// 基本块链表头
    int nb_basic_blocks;// 基本块数量

    vector_t* jmps;// 跳转指令集合(控制流跳转信息)

    list_t dag_list_head;// DAG(有向无环图)链表头，可能用于优化中间代码

    vector_t* bb_loops;// 基本块中循环集合
    vector_t* bb_groups;// 基本块分组集合

    vector_t* text_relas;// 代码段的重定位信息
    vector_t* data_relas;// 数据段的重定位信息

    inst_ops_t* iops;// 指令操作集合（Instruction operations）
    regs_ops_t* rops;// 寄存器分配操作集合（Register operations）

    Efunction* ef;// 外部函数扩展信息（例如目标代码生成时使用）

    _3ac_code_t* init_code;// 初始化的三地址码 (Three-Address Code)
    int init_code_bytes;// 初始化三地址码占用的字节数

    int callee_saved_size; // 被调用者保存寄存器占用的栈大小

    int local_vars_size;// 局部变量占用的栈大小
    int code_bytes;// 函数编译生成的机器码大小

    // 标志位（使用位域存储多个布尔标志）
    uint32_t visited_flag:1;     // 是否已经被访问过（图遍历/优化时使用）
    uint32_t bp_used_flag:1;     // 是否使用了基址指针 (base pointer)

    uint32_t static_flag:1;      // 是否是 static 函数
    uint32_t extern_flag:1;      // 是否是 extern 函数（声明但未定义）
    uint32_t inline_flag:1;      // 是否是 inline 函数
    uint32_t member_flag:1;      // 是否是类成员函数

    uint32_t vargs_flag:1;       // 是否是可变参数函数 (varargs)
    uint32_t void_flag :1;       // 是否是 void 返回类型函数
    uint32_t call_flag :1;       // 是否为调用标记函数
    uint32_t vla_flag  :1;       // 是否使用了 VLA (Variable Length Array)

    uint32_t compile_flag:1;     // 是否已经编译过
    uint32_t native_flag :1;     // 是否为本地(native)函数（如内建或外部库函数）
};

function_t* function_alloc(lex_word_t* w);
void function_free(function_t* f);

int function_same(function_t* f0,function_t* f1);
int function_same_type(function_t* f0,function_t* f1);
int function_same_argv(vector_t* argv0,vector_t* argv1);
int function_like_argv(vector_t* argv0,vector_t* argv1);

#endif