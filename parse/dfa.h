#ifndef DFA_H
#define DFA_H

#include "utils_vector.h"
#include "utils_list.h"

// DFA 执行返回值的枚举
enum dfa_retvals {
    DFA_REPEATED = -3, // 状态重复（可能表示检测到死循环或重复匹配）

    DFA_ERROR = -1, // DFA 执行出错

    DFA_OK = 0,          // 正常完成
    DFA_NEXT_SYNTAX = 1, // 切换到下一个语法规则
    DFA_NEXT_WORD = 2,   // 读取下一个词
    DFA_CONTINUE = 3,    // 继续当前 DFA 流程
    DFA_SWITCH_TO = 4,   // 切换到某个新的 DFA 状态/语法
};

// 前向声明，定义 DFA 框架中使用的核心结构体
typedef struct dfa_s dfa_t;               // DFA 本体
typedef struct dfa_node_s dfa_node_t;     // DFA 状态节点
typedef struct dfa_ops_s dfa_ops_t;       // DFA 操作接口集合
typedef struct dfa_module_s dfa_module_t; // DFA 模块
typedef struct dfa_hook_s dfa_hook_t;     // DFA 钩子

// 判断某个输入是否满足节点条件的函数指针
// 参数：dfa 指向 DFA，本体，word 是当前处理的词
typedef int (*dfa_is_pt)(dfa_t *dfa, void *word);

// 在某个节点执行动作的函数指针
// 参数：dfa 本体，words 表示已收集的词序列，data 附加数据
typedef int (*dfa_action_pt)(dfa_t *dfa, vector_t *words, void *data);

// DFA 钩子类型的枚举
enum dfa_hook_types {

    DFA_HOOK_PRE = 0, // 在进入节点之前执行
    DFA_HOOK_POST,    // 在节点处理之后执行
    DFA_HOOK_END,     // 在 DFA 结束时执行

    DFA_HOOK_NB // 钩子类型的总数
};

// DFA 钩子结构，形成链表
struct dfa_hook_s {
    dfa_hook_t *next; // 指向下一个钩子
    dfa_node_t *node; // 钩子绑定的节点
};

// DFA 节点（即有限自动机的状态）
struct dfa_node_s {
    char *name; // 节点名称，方便调试和识别

    dfa_is_pt is;         // 判断当前输入是否匹配该节点的函数
    dfa_action_pt action; // 匹配成功后执行的动作函数

    vector_t *childs; // 子节点集合（DFA 状态转移表）

    int refs; // 引用计数（用于内存管理）

    int module_index; // 该节点属于哪个模块（索引）
};

// DFA 本体
struct dfa_s {
    vector_t *nodes;// 所有节点集合
    vector_t *syntaxes;// DFA 支持的语法集合

    //	 vector_t*       words;

    dfa_hook_t *hooks[DFA_HOOK_NB];// 钩子数组，每类钩子一个链表

    void *priv;// 私有数据指针，供用户自定义

    dfa_ops_t *ops;// 关联的操作接口（词处理相关）
};

// DFA 操作接口集合，定义词的入栈/出栈方式
struct dfa_ops_s {
    const char *name;// 操作集合名称

    void *(*pop_word)(dfa_t *dfa);// 弹出一个词
    int (*push_word)(dfa_t *dfa, void *word);// 压入一个词
    void (*free_word)(void *word);// 释放一个词占用的内存
};

// DFA 模块，封装一套语法或功能
struct dfa_module_s {
    const char *name;// 模块名称

    int (*init_module)(dfa_t *dfa);// 初始化模块
    int (*init_syntax)(dfa_t *dfa);// 初始化语法（通常在加载具体规则时）

    int (*fini_module)(dfa_t *dfa);// 释放模块资源

    int index;// 模块索引（在 dfa_node_t.module_index 中引用）
};

// 一个简单的“入口节点”判断函数
// 永远返回 1，表示所有输入都可以进入该入口节点
static inline int dfa_is_entry(dfa_t *dfa, void *word) {
    return 1;
}

// 入口节点的动作函数
// 如果已经有词(words->size > 0)，继续执行 DFA
// 否则需要请求下一个词
static inline int dfa_action_entry(dfa_t *dfa, vector_t *words, void *data) {
    return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}


// “下一个”节点的动作函数
// 固定要求 DFA 读取下一个词
static inline int dfa_action_next(dfa_t *dfa, vector_t *words, void *data) {
    return DFA_NEXT_WORD;
}


// 定义并注册一个 DFA 节点
// 参数：dfa（DFA 本体）、module（模块名）、node（节点名）、is（匹配函数）、action（动作函数）
//
// 作用：
// 1. 组合模块名和节点名生成唯一的节点名字，例如 "json_entry"
// 2. 调用 dfa_node_alloc 创建节点
// 3. 设置模块索引，并将节点加入 DFA
// 4. 如果失败，直接报错并返回 -1
#define DFA_MODULE_NODE(dfa, module, node, is, action)                            \
    {                                                                             \
        char str[256];                                                            \
        snprintf(str, sizeof(str) - 1, "%s_%s", dfa_module_##module.name, #node); \
        dfa_node_t *node = dfa_node_alloc(str, is, action);                       \
        if (!node) {                                                              \
            printf("%s(),%d, error: \n", __func__, __LINE__);                     \
            return -1;                                                            \
        }                                                                         \
        node->module_index = dfa_module_##module.index;                           \
        dfa_add_node(dfa, node);                                                  \
    }

// 获取某个模块下的节点
// 生成名字 "module_name"，然后查找 DFA 中是否有该节点
// 如果找不到就报错并返回 -1
#define DFA_GET_MODULE_NODE(dfa, module, name, node)          \
    dfa_node_t *node = dfa_find_node(dfa, #module "_" #name); \
    if (!node) {                                              \
        printf("%s(),%d, error: \n", __func__, __LINE__);     \
        return -1;                                            \
    }

// 定义模块的入口节点
// 使用默认的入口判断函数 dfa_is_entry 和入口动作函数 dfa_action_entry
#define DFA_MODULE_ENTRY(dfa, module) \
    DFA_MODULE_NODE(dfa, module, entry, dfa_is_entry, dfa_action_entry)

// 向某个 DFA 节点上挂载一个钩子
// 参数：dfa_node = 钩子绑定的节点，type = 钩子类型（PRE/POST/END）
//
// 步骤：
// 1. 检查节点是否有效并且有判断函数
// 2. 分配一个 dfa_hook_t
// 3. 将其挂到 dfa->hooks[type] 链表的头部
// 4. 返回新建的钩子
#define DFA_PUSH_HOOK(dfa_node, type)                                          \
    ({                                                                         \
        dfa_node_t *dn = (dfa_node);                                           \
        if (!dn || !dn->is) {                                                  \
            printf("%s(), %d, error: invalid dfa node\n", __func__, __LINE__); \
            return DFA_ERROR;                                                  \
        }                                                                      \
        dfa_hook_t *h = calloc(1, sizeof(dfa_hook_t));                         \
        if (!h) {                                                              \
            printf("%s(), %d, error: \n", __func__, __LINE__);                 \
            return DFA_ERROR;                                                  \
        }                                                                      \
        h->node = dn;                                                          \
        h->next = dfa->hooks[type];                                            \
        dfa->hooks[type] = h;                                                  \
        h;                                                                     \
    })

// 分配一个新的节点
// name：节点名字
// is：判断函数
// action：动作函数
dfa_node_t *dfa_node_alloc(const char *name, dfa_is_pt is, dfa_action_pt action);

// 释放一个节点
void dfa_node_free(dfa_node_t *node);

// 打开并初始化一个 DFA
// name：DFA 名称，priv：用户私有数据
int dfa_open(dfa_t **pdfa, const char *name, void *priv);

// 关闭并释放 DFA
void dfa_close(dfa_t *dfa);

// 将一个节点加入 DFA
int dfa_add_node(dfa_t *dfa, dfa_node_t *node);

// 根据名字查找节点
dfa_node_t *dfa_find_node(dfa_t *dfa, const char *name);

// 为某个父节点添加子节点（建立状态转移关系）
int dfa_node_add_child(dfa_node_t *parent, dfa_node_t *child);

// 处理一个输入词
// word：输入词，data：附加数据
// 内部会驱动 DFA 状态流转
int dfa_parse_word(dfa_t *dfa, void *word, void *data);

// 删除某个钩子（从链表中移除）
void dfa_del_hook(dfa_hook_t **pp, dfa_hook_t *sentinel);

// 根据节点名删除钩子
void dfa_del_hook_by_name(dfa_hook_t **pp, const char *name);

#endif