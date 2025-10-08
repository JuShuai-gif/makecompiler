#include "parse.h"

// -----------------------------------------------------------------------------
// 函数: _find_function
// 作用: 从抽象语法树 (AST) 的节点中查找函数节点，并将其添加到结果向量中。
// 参数:
//   node - 当前遍历到的语法树节点
//   arg  - 附加参数（此处未使用）
//   vec  - 用于存放找到的函数节点的 vector 容器
// 返回:
//   若找到函数节点并成功添加到 vec，则返回 vector_add 的返回值 (通常为 0)
//   否则返回 0
// -----------------------------------------------------------------------------
int _find_function(node_t *node, void *arg, vector_t *vec) {
    // 判断该节点是否为函数类型
    if (FUNCTION == node->type) {
        // 将通用 node_t 指针强制转换为 function_t 类型指针
        function_t *f = (function_t *)node;
        // 将该函数节点加入结果向量 vec 中
        return vector_add(vec, f);
    }
    // 若该节点不是函数类型，返回 0 表示未找到匹配项
    return 0;
}

// -----------------------------------------------------------------------------
// 函数: _find_global_var
// 作用: 遍历 AST 节点，查找其中的全局变量或静态变量，并将其加入结果向量 vec。
// 参数:
//   node - 当前语法树节点
//   arg  - 附加参数（未使用）
//   vec  - 用于收集所有符合条件的变量的 vector
// 返回:
//   成功返回 0；若 vector_add 失败则返回错误码
// -----------------------------------------------------------------------------
int _find_global_var(node_t *node, void *arg, vector_t *vec) {
    // 判断该节点是否为代码块 (OP_BLOCK)，
    // 或者为结构体（STRUCT 及以上类型）且具有 class_flag（类或结构体的标志）
    if (OP_BLOCK == node->type
        || (node->type >= STRUCT && node->class_flag)) {
        // 将 node 强制转换为 block_t 类型（代码块节点）
        block_t *b = (block_t *)node;

        // 若该块没有作用域 (scope) 或该作用域中没有变量表 (vars)，则直接返回
        if (!b->scope || !b->scope->vars)
            return 0;

        int i;
        // 遍历当前作用域中的所有变量
        for (i = 0; i < b->scope->vars->size; i++) {
            variable_t *v = b->scope->vars->data[i];
            // 若该变量为全局变量或静态变量
            if (v->global_flag || v->static_flag) {
                // 将该变量加入结果向量 vec
                int ret = vector_add(vec, v);
                if (ret < 0)
                    return ret; // 添加失败则返回错误码
            }
        }
    }
    // 若该节点不符合条件或处理完毕，则返回 0 表示正常
    return 0;
}
