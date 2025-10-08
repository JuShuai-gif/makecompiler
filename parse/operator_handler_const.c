#include "ast.h"
#include "operator_handler_const.h"
#include "type_cast.h"
#include "calculate.h"

typedef struct {
    variable_t **pret; // pret：指向一个 variable_t* 的指针地址
    // 用于在表达式计算过程中，记录/返回计算结果（例如常量值）
} handler_data_t;

// 全局的 handler_data_t 指针，可能在整个表达式求值中被复用
static handler_data_t *gd = NULL;

static int __op_const_call(ast_t *ast, function_t *f, void *data);

/*
作用：

这个函数是“常量求值”遍历时的核心调度器。

如果 node 是一个表达式节点，就找到它对应的操作符，并调用相应的常量处理函数。
*/
static int _op_const_node(ast_t *ast, node_t *node, handler_data_t *d) {
    operator_t *op = node->op;

    if (!op) {
        // 如果节点没有 operator，就根据节点类型查找基础操作符
        op = find_base_operator_by_type(node->type);
        if (!op) {
            loge("\n");
            return -1;
        }
    }
    // 根据操作符类型查找对应的“常量表达式求值函数”
    operator_handler_pt h = find_const_operator_handler(op->type);
    if (!h)
        return -1;
    // 调用该操作符的处理函数
    return h(ast, node->nodes, node->nb_nodes, d);
}

/*
作用：

这是整个“常量表达式计算”的核心递归函数。

会根据表达式树结构，自底向上调用具体的 operator handler，完成表达式求值。

支持左结合和右结合操作符。

对函数调用节点、变量节点、操作符节点分别处理。
*/
static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    if (!node)
        return 0;

    // 特殊情况：函数节点，直接调用 __op_const_call 处理函数调用
    if (FUNCTION == node->type)
        return __op_const_call(ast, (function_t *)node, data);
    // 叶子节点（没有子节点）
    if (0 == node->nb_nodes) {
        // 如果是变量类型并且词法信息存在，打印调试信息
        if (type_is_var(node->type) && node->var->w)
            logd("w: %s\n", node->var->w->text->data);
        // 断言：叶子节点必须是变量、标签或者已经被拆分
        assert(type_is_var(node->type)
               || LABEL == node->type
               || node->split_flag);
        return 0;
    }
    // 非叶子节点必须是操作符节点
    assert(type_is_operator(node->type));
    assert(node->nb_nodes > 0);
    // 如果操作符没有初始化，重新查找
    if (!node->op) {
        node->op = find_base_operator_by_type(node->type);
        if (!node->op) {
            loge("node %p, type: %d, w: %p\n", node, node->type, node->w);
            return -1;
        }
    }

    handler_data_t *d = data;

    int i;
    // 按操作符结合性遍历左右子节点
    if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {
        // 左结合：从左到右依次递归计算
        for (i = 0; i < node->nb_nodes; i++) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                return -1;
            }
        }
        // 计算本节点
        operator_handler_pt h = find_const_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            return -1;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            return -1;
        }
    } else {
        // 右结合：从右到左依次递归计算
        for (i = node->nb_nodes - 1; i >= 0; i--) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                return -1;
            }
        }

        operator_handler_pt h = find_const_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            return -1;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

/*
作用：

处理 create 操作符（可能对应类似“构造”或“函数调用关系分析”）。

将当前函数和被调用函数建立双向调用关系，方便后续优化/依赖分析。
*/
static int _op_const_create(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 3);

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v2 = _operand_get(nodes[2]);
    node_t *parent = nodes[0]->parent;
    // v0/v2 必须是函数指针
    assert(FUNCTION_PTR == v0->type && v0->func_ptr);
    assert(FUNCTION_PTR == v2->type && v2->func_ptr);
    // 回溯找到上层的调用函数
    while (parent && FUNCTION != parent->type)
        parent = parent->parent;

    if (!parent) {
        loge("\n");
        return -1;
    }

    function_t *caller = (function_t *)parent;
    function_t *callee0 = v0->func_ptr;
    function_t *callee2 = v2->func_ptr;
    // 建立 caller → callee0 调用关系
    if (caller != callee0) {
        if (vector_add_unique(caller->callee_functions, callee0) < 0)
            return -1;

        if (vector_add_unique(callee0->caller_functions, caller) < 0)
            return -1;
    }
    // 建立 caller → callee2 调用关系
    if (caller != callee2) {
        if (vector_add_unique(caller->callee_functions, callee2) < 0)
            return -1;

        if (vector_add_unique(callee2->caller_functions, caller) < 0)
            return -1;
    }

    return 0;
}

/*
作用：

这里的指针运算在常量求值阶段可能不参与实际计算，所以是空实现。
*/
static int _op_const_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

/*
作用：

处理数组下标访问表达式。

检查被访问的变量是否为数组/指针类型。

对索引表达式递归求值。
*/
static int _op_const_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    variable_t *v0 = _operand_get(nodes[0]);
    assert(v0);
    // 如果 v0 不是指针/数组，报错
    if (variable_nb_pointers(v0) <= 0) {
        loge("index out\n");
        return -1;
    }

    handler_data_t *d = data;
    // 递归计算索引表达式
    int ret = _expr_calculate_internal(ast, nodes[1], d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

/*
作用：

处理代码块（例如 { ... }），在常量求值阶段相当于顺序执行每个节点的计算。

切换/恢复作用域信息。
*/
static int _op_const_block(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (0 == nb_nodes)
        return 0;

    handler_data_t *d = data;
    block_t *up = ast->current_block;
    // 切换当前作用域块
    ast->current_block = (block_t *)(nodes[0]->parent);

    int ret;
    int i;
    // 遍历 block 中的所有节点，依次计算
    for (i = 0; i < nb_nodes; i++) {
        node_t *node = nodes[i];

        if (FUNCTION == node->type)
            ret = __op_const_call(ast, (function_t *)node, data);
        else
            ret = _op_const_node(ast, node, d);

        if (ret < 0) {
            loge("\n");
            ast->current_block = up;
            return -1;
        }
    }
    // 还原上层 block
    ast->current_block = up;
    return 0;
}

/*
作用：

处理 return 语句中的返回值常量计算。

每个 return 表达式都递归计算。
*/
static int _op_const_return(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    int i;
    for (i = 0; i < nb_nodes; i++) {
        expr_t *e = nodes[i];
        variable_t *r = NULL;
        // 计算 return 表达式
        if (_expr_calculate_internal(ast, e, &r) < 0)
            return -1;
    }

    return 0;
}

// 在常量求值阶段，这些控制流相关的语句通常不做实际处理，保留空实现或仅用于标记
static int _op_const_break(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_continue(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_label(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_goto(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

/*
作用：

处理 if 语句的常量求值。

先计算条件表达式（条件是否为常量可能不重要，但必须被计算）。

再计算 if 块中的所有节点。

这里只是“语义遍历”，并没有真正跳过 else 分支等逻辑（可能在上层控制）。
*/
static int _op_const_if(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (nb_nodes < 2) {
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];
    // if 的第一个节点是条件表达式
    assert(OP_EXPR == e->type);
    // 先计算条件表达式
    if (_expr_calculate_internal(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }
    // 然后遍历执行 if 语句块内节点
    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _op_const_node(ast, nodes[i], d);
        if (ret < 0)
            return -1;
    }

    return 0;
}

// 处理do-while循环的常量表达式
static int _op_const_do(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 确保有2个子节点：条件表达式和循环体

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[1]; // 第二个节点是条件表达式

    assert(OP_EXPR == e->type); // 确保是表达式类型

    // 先处理循环体中的常量表达式
    if (_op_const_node(ast, nodes[1], d) < 0)
        return -1;
    // 计算条件表达式的值
    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;

    return 0; // 成功返回
}

// 处理while循环的常量表达式
static int _op_const_while(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes || 1 == nb_nodes); // 确保有1-2个子节点：条件表达式和可选的循环体

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0]; // 第一个节点是条件表达式

    assert(OP_EXPR == e->type); // 确保是表达式类型

    // 计算条件表达式的值
    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;
    // 如果有循环体，处理循环体中的常量表达式
    if (2 == nb_nodes) {
        if (_op_const_node(ast, nodes[1], d) < 0)
            return -1;
    }

    return 0; // 成功返回
}

// 处理switch语句的常量表达式
static int _op_const_switch(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 确保有2个子节点：选择表达式和case块

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0]; // 第一个节点是选择表达式
    expr_t *e2;
    node_t *b = nodes[1]; // 第二个节点是case块
    node_t *child;

    assert(OP_EXPR == e->type); // 确保是表达式类型

    // 计算选择表达式的值
    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;

    // 遍历case块中的所有子节点
    int i;
    for (i = 0; i < b->nb_nodes; i++) {
        child = b->nodes[i];

        if (OP_CASE == child->type) {
            // 处理case标签
            assert(1 == child->nb_nodes); // case节点应该有一个子节点（常量表达式）

            e = child->nodes[0]; // case的常量表达式

            assert(OP_EXPR == e->type); // 确保是表达式类型
            // 计算case常量表达式的值
            if (_expr_calculate_internal(ast, e, &r) < 0)
                return -1;

        } else {
            // 处理case块中的语句
            if (_op_const_node(ast, child, d) < 0)
                return -1;
        }
    }

    return 0; // 成功返回
}

// 处理case标签的常量表达式（空实现）
static int _op_const_case(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // case标签本身在switch中处理，这里不需要额外处理
}
// 处理default标签的常量表达式（空实现）
static int _op_const_default(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // default标签不需要特殊处理
}

// 处理可变长度数组(VLA)分配的常量表达式（空实现）
static int _op_const_vla_alloc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes); // 确保有4个参数

    return 0; // VLA分配通常在运行时处理，编译时无法确定
}

// 处理for循环的常量表达式
static int _op_const_for(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes); // 确保有4个子节点：初始化、条件、递增、循环体

    handler_data_t *d = data;
    // 处理初始化表达式
    if (nodes[0]) {
        if (_op_const_node(ast, nodes[0], d) < 0) {
            loge("\n"); // 记录错误
            return -1;
        }
    }

    // 处理条件表达式
    expr_t *e = nodes[1];
    if (e) {
        assert(OP_EXPR == e->type); // 确保是表达式类型

        variable_t *r = NULL;
        // 计算条件表达式的值
        if (_expr_calculate_internal(ast, e, &r) < 0) {
            loge("\n"); // 记录错误
            return -1;
        }
    }
    // 处理递增表达式和循环体
    int i;
    for (i = 2; i < nb_nodes; i++) {
        if (!nodes[i])
            continue; // 跳过空节点

        if (_op_const_node(ast, nodes[i], d) < 0) {
            loge("\n"); // 记录错误
            return -1;
        }
    }

    return 0; // 成功返回
}

// 内部函数：处理函数调用中的常量表达式
static int __op_const_call(ast_t *ast, function_t *f, void *data) {
    logd("f: %p, f->node->w: %s\n", f, f->node.w->text->data); // 调试日志

    handler_data_t *d = data;
    block_t *tmp = ast->current_block; // 保存当前块

    // 切换到被调用函数的块
    ast->current_block = (block_t *)f;

    // 处理函数体中的常量表达式
    if (_op_const_block(ast, f->node.nodes, f->node.nb_nodes, d) < 0) {
        loge("\n"); // 记录错误
        return -1;
    }

    ast->current_block = tmp; // 恢复原来的当前块
    return 0;                 // 成功返回
}

// 处理函数调用的常量表达式
static int _op_const_call(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 0); // 确保至少有一个节点（函数名）
                          // 获取函数指针变量
    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(FUNCTION_PTR == v0->type && v0->func_ptr); // 确保是函数指针类型
                                                      // 向上查找函数定义节点
    while (parent && FUNCTION != parent->type)
        parent = parent->parent;

    if (!parent) {
        loge("\n"); // 记录错误：找不到函数定义
        return -1;
    }

    function_t *caller = (function_t *)parent; // 调用者函数
    function_t *callee = v0->func_ptr;         // 被调用函数

    // 建立调用关系
    if (caller != callee) {
        // 将callee添加到caller的callee列表中
        if (vector_add_unique(caller->callee_functions, callee) < 0)
            return -1;

        // 将caller添加到callee的caller列表中
        if (vector_add_unique(callee->caller_functions, caller) < 0)
            return -1;
    }
    // 处理函数参数中的常量表达式
    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _expr_calculate_internal(ast, nodes[i], data);
        if (ret < 0) {
            loge("\n"); // 记录错误
            return -1;
        }
    }

    return 0; // 成功返回
}

// 处理表达式的常量计算
static int _op_const_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes); // 确保只有一个子节点（表达式）

    node_t *parent = nodes[0]->parent;
    node_t *node;
    // 简化表达式
    expr_simplify(&nodes[0]);

    node = nodes[0];
    // 计算表达式的值
    int ret = _expr_calculate_internal(ast, node, data);
    if (ret < 0) {
        loge("\n"); // 记录错误
        return -1;
    }
    // 释放父节点原有的结果
    if (parent->result)
        variable_free(parent->result);
    // 获取计算结果并设置到父节点
    variable_t *v = _operand_get(node);
    if (v)
        parent->result = variable_ref(v); // 引用计数增加
    else
        parent->result = NULL;

    return 0; // 成功返回
}

// 以下函数处理各种运算符，但目前都是空实现或简单实现：

// 前置递增运算符（空实现）
static int _op_const_inc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 递增操作通常会影响变量，不能作为常量处理
}

// 后置递增运算符（空实现）
static int _op_const_inc_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 递增操作通常会影响变量，不能作为常量处理
}

// 前置递减运算符（空实现）
static int _op_const_dec(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 递减操作通常会影响变量，不能作为常量处理
}

// 后置递减运算符（空实现）
static int _op_const_dec_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 递减操作通常会影响变量，不能作为常量处理
}

// 正号运算符（空实现）
static int _op_const_positive(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 正号操作不改变值，可能在表达式简化中处理
}

// 解引用运算符（指针解引用）
static int _op_const_dereference(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 获取解引用操作的结果变量
    variable_t *v = _operand_get(nodes[0]->parent);
    // 解引用操作的结果通常不是常量（因为指向的内存可能变化）
    v->const_flag = 0;
    return 0; // 成功返回
}

// 取地址运算符（空实现）
static int _op_const_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // 地址操作通常在编译时无法确定具体值
}

// 类型转换操作符的常量处理
static int _op_const_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 确保类型转换语法树节点有 2 个子节点： [0] 目标类型 [1] 源表达式
    assert(2 == nb_nodes);

    node_t *child = nodes[1];
    node_t *parent = nodes[0]->parent;

    variable_t *dst = _operand_get(nodes[0]);  // 目标类型变量
    variable_t *src = _operand_get(nodes[1]);  // 源变量
    variable_t *result = _operand_get(parent); // 结果变量
    variable_t *r = NULL;

    // 如果源是常量，则尝试直接计算类型转换结果
    if (variable_const(src)) {
        // 函数指针或数组类型的常量，不做数值转换，直接引用
        if (FUNCTION_PTR == src->type || src->nb_dimentions > 0) {
            r = variable_ref(src); // 创建引用

            node_free_data(parent); // 清空父节点旧数据

            parent->type = r->type; // 更新父节点类型
            parent->var = r;        // 更新父节点变量
            return 0;
        }

        int dst_type = dst->type;
        // 如果目标类型是指针或数组，则统一转换为 uintptr 类型
        if (dst->nb_pointers + dst->nb_dimentions > 0)
            dst_type = VAR_UINTPTR;
        // 查找类型转换函数
        type_cast_t *cast = find_base_type_cast(src->type, dst_type);
        if (cast) {
            // 执行类型转换
            int ret = cast->func(ast, &r, src);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
            r->const_flag = 1; // 结果是常量
                               // 交换词法单元信息
            if (parent->w)
                XCHG(r->w, parent->w);
            // 用新结果替换 AST 父节点的数据
            node_free_data(parent);
            parent->type = r->type;
            parent->var = r;
        }

        return 0;
    } else
        // 如果源不是常量，则结果也不是常量
        result->const_flag = 0;

    // 如果源和目标都是整数类型，且目标能完全容纳源的大小
    // 则直接用源节点的数据替换父节点（减少 AST 层级）
    if (variable_integer(src) && variable_integer(dst)) {
        int size;
        if (src->nb_dimentions > 0)
            size = sizeof(void *);
        else
            size = src->size;

        assert(0 == dst->nb_dimentions);

        if (variable_size(dst) <= size) {
            node_t *child = nodes[1];

            logd("child: %d/%s, size: %d, dst size: %d\n", src->w->line, src->w->text->data,
                 size, variable_size(dst));

            nodes[1] = NULL;
            node_free_data(parent);
            node_move_data(parent, child);
        }
    }

    return 0;
}

// sizeof 运算的常量处理（目前尚未实现）
static int _op_const_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return -1;
}

// 一元运算（负号、逻辑非、按位取反等）的常量计算
static int _op_const_unary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    // 常量 + 非指针 + 非数组才能折叠
    int const_flag = v0->const_flag && 0 == v0->nb_pointers && 0 == v0->nb_dimentions;
    if (!const_flag)
        return 0;
    // 数值类型才支持一元运算
    if (type_is_number(v0->type)) {
        calculate_t *cal = find_base_calculate(parent->type, v0->type, v0->type);
        if (!cal) {
            loge("type %d not support\n", v0->type);
            return -EINVAL;
        }

        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, NULL); // 一元运算
        if (ret < 0) {
            loge("\n");
            return ret;
        }
        r->const_flag = 1;

        XCHG(r->w, parent->w); // 交换词法单元信息

        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;

    } else {
        loge("type %d not support\n", v0->type);
        return -1;
    }

    return 0;
}

// 一元负号、按位取反、逻辑非 操作都使用统一的一元处理函数
static int _op_const_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

static int _op_const_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

// 二元运算（加减乘除等）的常量折叠
static int _op_const_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    // 仅当左右都是数值常量时才计算
    if (type_is_number(v0->type) && type_is_number(v1->type)) {
        if (!variable_const(v0) || !variable_const(v1))
            return 0;

        assert(v0->type == v1->type);
        // 查找对应二元运算计算器
        calculate_t *cal = find_base_calculate(parent->type, v0->type, v1->type);
        if (!cal) {
            loge("type %d, %d not support, line: %d\n", v0->type, v1->type, parent->w->line);
            return -EINVAL;
        }

        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, v1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
        r->const_flag = 1;

        XCHG(r->w, parent->w);

        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;
    }

    return 0;
}

// 各种算术/逻辑/位运算都复用 _op_const_binary
static int _op_const_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

// 左右移时检查移位常量合法性
static int _shift_check_const(node_t **nodes, int nb_nodes) {
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    if (variable_const(v1)) {
        if (v1->data.i < 0) {
            loge("shift count %d < 0\n", v1->data.i);
            return -EINVAL;
        }

        if (v1->data.i >= v0->size << 3) {
            loge("shift count %d >= type bits: %d\n", v1->data.i, v0->size << 3);
            return -EINVAL;
        }
    }

    return 0;
}

// 移位操作的常量折叠
static int _op_const_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

// 各种赋值类操作（目前为空实现）
static int _op_const_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_add_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_sub_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_mul_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_div_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_mod_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_shl_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_shr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_and_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_or_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

// 比较运算的常量折叠
static int _op_const_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

// 逻辑和位运算也直接调用二元计算
static int _op_const_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

// 可变参数操作符（未实现）
static int _op_const_va_start(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_va_arg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_va_end(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

// 所有操作符对应的常量折叠处理函数表
operator_handler_pt const_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_const_expr,
        [OP_CALL] = _op_const_call,

        [OP_ARRAY_INDEX] = _op_const_array_index,
        [OP_POINTER] = _op_const_pointer,
        [OP_CREATE] = _op_const_create,

        [OP_VA_START] = _op_const_va_start,
        [OP_VA_ARG] = _op_const_va_arg,
        [OP_VA_END] = _op_const_va_end,

        [OP_SIZEOF] = _op_const_sizeof,
        [OP_TYPE_CAST] = _op_const_type_cast,
        [OP_LOGIC_NOT] = _op_const_logic_not,
        [OP_BIT_NOT] = _op_const_bit_not,
        [OP_NEG] = _op_const_neg,
        [OP_POSITIVE] = _op_const_positive,

        [OP_INC] = _op_const_inc,
        [OP_DEC] = _op_const_dec,

        [OP_INC_POST] = _op_const_inc_post,
        [OP_DEC_POST] = _op_const_dec_post,

        [OP_DEREFERENCE] = _op_const_dereference,
        [OP_ADDRESS_OF] = _op_const_address_of,

        [OP_MUL] = _op_const_mul,
        [OP_DIV] = _op_const_div,
        [OP_MOD] = _op_const_mod,

        [OP_ADD] = _op_const_add,
        [OP_SUB] = _op_const_sub,

        [OP_SHL] = _op_const_shl,
        [OP_SHR] = _op_const_shr,

        [OP_BIT_AND] = _op_const_bit_and,
        [OP_BIT_OR] = _op_const_bit_or,

        [OP_EQ] = _op_const_eq,
        [OP_NE] = _op_const_ne,
        [OP_GT] = _op_const_gt,
        [OP_LT] = _op_const_lt,
        [OP_GE] = _op_const_ge,
        [OP_LE] = _op_const_le,

        [OP_LOGIC_AND] = _op_const_logic_and,
        [OP_LOGIC_OR] = _op_const_logic_or,

        [OP_ASSIGN] = _op_const_assign,
        [OP_ADD_ASSIGN] = _op_const_add_assign,
        [OP_SUB_ASSIGN] = _op_const_sub_assign,
        [OP_MUL_ASSIGN] = _op_const_mul_assign,
        [OP_DIV_ASSIGN] = _op_const_div_assign,
        [OP_MOD_ASSIGN] = _op_const_mod_assign,
        [OP_SHL_ASSIGN] = _op_const_shl_assign,
        [OP_SHR_ASSIGN] = _op_const_shr_assign,
        [OP_AND_ASSIGN] = _op_const_and_assign,
        [OP_OR_ASSIGN] = _op_const_or_assign,

        [OP_BLOCK] = _op_const_block,
        [OP_RETURN] = _op_const_return,
        [OP_BREAK] = _op_const_break,
        [OP_CONTINUE] = _op_const_continue,
        [OP_GOTO] = _op_const_goto,
        [LABEL] = _op_const_label,

        [OP_IF] = _op_const_if,
        [OP_WHILE] = _op_const_while,
        [OP_DO] = _op_const_do,
        [OP_FOR] = _op_const_for,

        [OP_SWITCH] = _op_const_switch,
        [OP_CASE] = _op_const_case,
        [OP_DEFAULT] = _op_const_default,

        [OP_VLA_ALLOC] = _op_const_vla_alloc,
};

// 根据操作符类型查找对应的常量计算处理函数
operator_handler_pt find_const_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return const_operator_handlers[type];
}

// 对函数中的 AST 进行常量折叠优化
int function_const_opt(ast_t *ast, function_t *f) {
    handler_data_t d = {0};

    int ret = __op_const_call(ast, f, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}
