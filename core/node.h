#ifndef NODE_H
#define NODE_H

#include "utils_string.h"
#include "utils_list.h"
#include "utils_vector.h"
#include "utils_graph.h"
#include "lex.h"
#include "variable.h"
#include "operator.h"

#define OP_ASSOCIATIVITY_LEFT	0
#define OP_ASSOCIATIVITY_RIGHT	1

// 抽象语法树(AST)中节点结构体定义
struct node_s {
    int type;       // 节点类型，用于区分不同语法成分
    node_t *parent; // 指向父节点的指针
    int nb_nodes;   // 子节点数量
    node_t **nodes; // 子节点数组(指针数组)

    // 节点可能关联的具体内容，不同类型节点用不同的成员
    union {
        variable_t *var; // 如果是变量节点,指向变量信息
        lex_word_t *w;   // 如果是词法单元节点，指向词法单元
        label_t *label;  // 如果是标签节点，指向标签信息
    };

    lex_word_t *debug_w; // 调试用的词法单元指针(可能用于报错定位)

    int priority;   // 运算优先级(例如表达式里的操作符优先级)
    operator_t *op; // 运算符信息

    variable_t *result;     // 表达式计算结果(存放结果变量)
    vector_t *result_nodes; // 结果相关的子节点集合
    node_t *split_parent;   // 如果该节点是分裂节点，指向其分裂的父节点

    // 一组位域标志(flags),用于快速记录节点的属性
    uint32_t root_flag : 1;   // 节点是否是根节点（根 block）
    uint32_t file_flag : 1;   // 节点是否是文件级 block
    uint32_t enum_flag : 1;   // 节点是否表示枚举类型
    uint32_t class_flag : 1;  // 节点是否表示类类型
    uint32_t union_flag : 1;  // 节点是否表示联合类型
    uint32_t define_flag : 1; // 节点是否表示函数定义（而不仅是声明）
    uint32_t const_flag : 1;  // 节点是否表示常量类型
    uint32_t split_flag : 1;  // 节点是否是其父节点的“分裂”节点
    uint32_t _3ac_done : 1;   // 节点的三地址码（3AC）是否已经生成
    uint32_t semi_flag : 1;   // 节点后面是否跟随分号 ';'
};

// 表示程序中的 标签 的数据结构
struct label_s {
    list_t list; // 用于将多个 label 串联起来的链表节点(典型的 intrucive list)
    int refs;    // 被引用次数(例如有多少条 goto/jump 指令跳向该 label)
    int type;    // 标签类型（可能区分普通标签、循环标签、函数入口等）

    lex_word_t *w; // 标签对应的词法单元（在源代码中的单词信息，便于定位/调试）
    node_t *node;  // 指向 AST（抽象语法树）中的节点，表示该 label 所在的语法位置
};

// 分配节点
node_t *mc_node_alloc(lex_word_t *w, int type, variable_t *var);
// 分配标签节点
node_t *mc_node_alloc_label(label_t *l);
// 节点克隆
node_t *mc_node_clone(node_t *node);
// 节点增加孩子
int mc_node_add_child(node_t *parent, node_t *child);
// 节点删除孩子
void mc_node_del_child(node_t *parent, node_t *child);
// 节点释放
void mc_node_free(node_t *node);
// 节点释放数据
void mc_node_free_data(node_t *node);
// 节点移动数据
void mc_node_move_data(node_t *dst, node_t *src);
// 节点打印
void mc_node_print(node_t *node);
// 节点操作数获取
variable_t *_mc_operand_get(const node_t *node);
// 节点函数获取
function_t *_mc_function_get(node_t *node);
//
typedef int (*mc_node_find_pt)(node_t *node, void *arg, vector_t *results);
// 宽度优先搜索
int mc_node_search_bfs(node_t *root, void *arg, vector_t *results, int max, mc_node_find_pt find);
// 标签分配
label_t *mc_label_alloc(lex_word_t *w);
// 标签释放
void mc_label_free(label_t *l);

#endif