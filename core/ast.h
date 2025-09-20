#ifndef AST_H
#define AST_H

#include "type.h"
#include "variable.h"
#include "node.h"
#include "expr.h"
#include "scope.h"
#include "block.h"
#include "function.h"

/*
临时修改 type_t 的属性(const、指针层数、函数指针)

调用 variable_alloc 创建变量

再把 type_t 恢复成原始状态
*/
#define VAR_ALLOC_BY_TYPE(w, t, const_flag_, nb_pointers_, func_ptr_) \
    ({                                                                \
        uint32_t tmp_const_flag = (t)->node.const_flag;               \
        int tmp_nb_pointers = (t)->nb_pointers;                       \
        function_t *tmp_func_ptr = (t)->func_ptr;                     \
                                                                      \
        (t)->node.const_flag = (const_flag_);                         \
        (t)->nb_pointers = (nb_pointers_);                            \
        (t)->func_ptr = (func_ptr_);                                  \
                                                                      \
        variable_t *var = variable_alloc((w), (t));                   \
                                                                      \
        (t)->node.const_flag = tmp_const_flag;                        \
        (t)->nb_pointers = tmp_nb_pointers;                           \
        (t)->func_ptr = tmp_func_ptr;                                 \
        var;                                                          \
    })

// ast_rela_t 主要用于处理 全局常量/全局变量 之间的引用关系
typedef struct ast_rela_s ast_rela_t;
typedef struct ast_s ast_t;

struct ast_rela_s {
    member_t *ref; // ref should point to obj's address
    member_t *obj;
};

// 抽象语法树(AST)结构体
// ast_t 是编译器的核心结构，表示整个 AST 的状态，包括代码块栈、全局变量、函数等
struct ast_s {
    block_t *root_block;// 根代码块(程序入口)
    block_t *current_block;// 当前正在处理的代码块

    int nb_structs;// 程序中结构体的数量
    int nb_functions;// 程序中函数的数量

    vector_t *global_consts;// 全局常量集合
    vector_t *global_relas;// 全局引用集合

    Eboard *board;// 可能用于错误记录/编译状态管理的上下文
};

// 这些函数主要用于 常量折叠、类型推断和调试打印
// 表达式求值，返回变量结果
int expr_calculate(ast_t *ast, expr_t *expr, variable_t **pret);

// 内部表达式求值函数
int expr_calculate_internal(ast_t *ast, node_t *node, void *data);

// 获取变量的类型名称
string_t *variable_type_name(ast_t *ast, variable_t *v);


// 这些内联函数是 作用域/块管理工具
// 将新 block 压入 AST 栈中
static inline void ast_push_block(ast_t *ast, block_t *b) {
    node_add_child((node_t *)ast->current_block, (node_t *)b);
    ast->current_block = b;
}

// 将当前 block 弹出，返回到父 block
static inline void ast_pop_block(ast_t *ast) {
    ast->current_block = (block_t *)(ast->current_block->node.parent);
}

// 获取父 block (跳过非 block/function 类型节点)
static inline block_t *ast_parent_block(ast_t *ast) {
    node_t *parent = ast->current_block->node.parent;

    while (parent && parent->type != OP_BLOCK && parent->type != FUNCTION)
        parent = parent->parent;

    return (block_t *)parent;
}

// 获取当前作用域
static inline scope_t *ast_current_scope(ast_t *ast) {
    return ast->current_block->scope;
}

// global 版本查找全局定义
// 非 global 版本查找当前作用域或可见范围内的定义
// 查找全局函数 / 变量 / 类型
int ast_find_global_function(function_t **pf, ast_t *ast, char *name);
int ast_find_global_variable(variable_t **pv, ast_t *ast, char *name);
int ast_find_global_type(type_t **pt, ast_t *ast, char *name);
int ast_find_global_type_type(type_t **pt, ast_t *ast, int type);

// 在当前 AST 中查找函数 / 变量 / 类型
int ast_find_function(function_t **pf, ast_t *ast, char *name);
int ast_find_variable(variable_t **pv, ast_t *ast, char *name);
int ast_find_type(type_t **pt, ast_t *ast, char *name);
int ast_find_type_type(type_t **pt, ast_t *ast, int type);

// 函数调用处理(生成三地址码)，用于处理函数调用，包括：参数检查、返回值生成、并在_3ac_list_head 中插入相应的三地址码 IR
int operator_function_call(ast_t *ast, function_t *f, const int argc, const variable_t **argv, variable_t **pret, list_t *__3ac_list_head);

// 打开/关闭 AST
int ast_open(ast_t **past);
int ast_close(ast_t *ast);

// 在 AST 中添加基础类型（int/float/...）
int ast_add_base_type(ast_t *ast, base_type_t *base_type);

// 添加一个文件级代码块
int ast_add_file_block(ast_t *ast, const char *path);

// 生成函数签名
int function_signature(ast_t *ast, function_t *f);

// 向 AST 添加常量字符串
int ast_add_const_str(ast_t *ast, node_t *parent, lex_word_t *w);

// 向 AST 添加常量变量（整数/浮点等）
int ast_add_const_var(ast_t *ast, node_t *parent, int type, const uint64_t u64);

#endif
