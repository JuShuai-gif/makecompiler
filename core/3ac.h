#ifndef THREEAC_H
#define THREEAC_H

#include "node.h"
#include "dag.h"
#include "utils_graph.h"
#include "basic_block.h"

// 定义结构体别名
typedef struct _3ac_operator_s _3ac_operator_t;
typedef struct _3ac_operand_s _3ac_operand_t;

// 三地址码的操作符结构体
struct _3ac_operator_s {
    int type;               // 操作符类型（例如加减乘除、赋值、跳转等）
    const char *name;       // 操作符的字符串表示（例如 "+", "-", "jmp"）
};

// 表示操作数(可能是变量、常量、标签、AST节点等)
struct _3ac_operand_s {
    node_t *node;         // 对应的抽象语法树(AST)节点
    dag_node_t *dag_node; // 对应的 DAG 节点(用于优化和代码生成)
    _3ac_code_t * code;    // 若为跳转语句，则指向对应的目标三地址码
    basic_block_t *bb;    // 若为跳转语句，则指向目标基本块

    void *rabi;// 额外信息指针(可能和寄存器分配/ABI 有关)
};

// 三地址码的指令结构体
struct _3ac_code_s {
    list_t list; // 作为链表节点，连接在三地址码指令链表中

    _3ac_operator_t * op; // 指令对应的操作符

    vector_t *dsts; // 目标操作数列表（通常只有函数返回值会用到）
    vector_t *srcs; // 源操作数列表（通常有两个源操作数）

    label_t *label; // 用于跳转（goto）的标签，标记目标位置

    _3ac_code_t * origin;// 记录原始的指令（便于回溯）

    basic_block_t *basic_block;// 当前指令所属的基本块
    uint32_t basic_block_start : 1;// 标记是否是基本块的起始指令
    uint32_t jmp_dst_flag : 1;// 标记是否为跳转目标指令

    vector_t *active_vars;// 活跃变量集合（用于活跃变量分析）
    vector_t *dn_status_initeds;// DAG 节点初始化状态信息

    vector_t *instructions;// 低层机器指令列表
    int inst_bytes;// 指令占用的字节数
    int bb_offset;// 当前指令在基本块内的偏移

    graph_t *rcg;// 可能是寄存器冲突图（Register Conflict Graph）
};


/*
主要提供了分配/释放、克隆、打印、优化、基本块划分等功能
*/
// 分配一个三地址码操作数
_3ac_operand_t * _3ac_operand_alloc();

// 释放一个三地址码操作数
void _3ac_operand_free(_3ac_operand_t * operand);

// 分配一条新的三地址码指令
_3ac_code_t * _3ac_code_alloc();


// 克隆一条三地址码指令
_3ac_code_t * _3ac_code_clone(_3ac_code_t * c);


// 释放一条三地址码指令
void _3ac_code_free(_3ac_code_t * code);


// 打印一条三地址码指令
void _3ac_code_print(_3ac_code_t * c, list_t *sentinel);


// 整个三地址码链表打印
void _3ac_list_print(list_t *h);


// 生成一条跳转指令(例如 goto、if 条件跳转)
_3ac_code_t * _3ac_jmp_code(int type, label_t *l, node_t *err);


// 根据操作符类型查找对应的操作符
_3ac_operator_t * _3ac_find_operator(const int type);


// 将三地址码转为 DAG 表示，用于优化
int _3ac_code_to_dag(_3ac_code_t * c, list_t *dag);


// 根据源操作数创建一条三地址码指令
_3ac_code_t * _3ac_alloc_by_src(int op_type, dag_node_t *src);

// 根据目标操作数创建一条三地址码指令
_3ac_code_t * _3ac_alloc_by_dst(int op_type, dag_node_t *dst);

// 创建一条通用的三地址码(支持多个目标、源)
_3ac_code_t * _3ac_code_NN(int op_type, node_t **dsts, int nb_dsts, node_t **srcs, int nb_srcs);

// 将三地址码链表划分为基本块
int _3ac_split_basic_blocks(list_t *list_head__3ac, function_t *f);

// 判断两条三地址码是否等价
int _3ac_code_same(_3ac_code_t * c0, _3ac_code_t * c1);

#endif
