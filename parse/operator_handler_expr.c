#include "ast.h"
#include "operator_handler_const.h"
#include "operator_handler_semantic.h"
#include "type_cast.h"
#include "calculate.h"

/* handler_data_t 用于传递给 operator handler 的上下文数据结构。
 * 目前只包含一个 variable_t** 成员 pret（猜测用于存放/传回计算结果指针数组或返回值位置）。
 */
typedef struct {
    variable_t **pret;
} handler_data_t;

/* 函数原型：根据 operator type 查找对应的处理器函数（类型 operator_handler_pt） */
operator_handler_pt find_expr_operator_handler(const int type);

/*
 * 递归计算/处理表达式 AST 的内部函数。
 * ast: 整个抽象语法树上下文（用于查找类型转换、符号等全局信息）。
 * node: 当前要处理的表达式节点（可能是变量、常量、操作符等）。
 * data: 传入的上下文，期望为 handler_data_t*。
 *
 * 返回值：0 表示成功，负值表示出错（-1 或其他 errno 风格错误码）。
 */
static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    /* 空节点直接视为成功（没有东西要处理） */
    if (!node)
        return 0;
    /* 如果没有子节点，则按“变量/字面量”处理（叶子节点） */
    if (0 == node->nb_nodes) {
        /* 程序假定所有叶子节点的类型都是变量/字面量类型 */
        assert(type_is_var(node->type));
        /* 如果节点关联了词法单元（w），打印调试信息（变量文本） */
        if (type_is_var(node->type) && node->var->w)
            logd("w: %s\n", node->var->w->text->data);

        return 0;
    }
    /* 到这里说明 node 是一个操作符节点，且至少有一个子节点 */
    assert(type_is_operator(node->type));
    assert(node->nb_nodes > 0);
    /* 若 node->op 未设置，从基础表中按类型查找 operator 描述（例如优先级、结合性等） */
    if (!node->op) {
        node->op = find_base_operator_by_type(node->type);
        if (!node->op) {
            /* 找不到对应操作符的元信息，这是不可恢复的错误 */
            loge("node %p, type: %d, w: %p\n", node, node->type, node->w);
            return -1;
        }
    }

    handler_data_t *d = data; /* 把 void* 转为具体类型，便于传给 handler */
    int i;
    /* 根据操作符的结合性决定先处理子节点的顺序 */
    if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {
        /* 左结合：按从左到右顺序先递归处理每个子节点 */
        for (i = 0; i < node->nb_nodes; i++) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }
        /* 处理完所有子节点后，查找并调用该 operator 的处理函数 */
        operator_handler_pt h = find_expr_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }
        /* 调用 operator handler：参数为 ast、子节点数组、子节点数量、上下文 d */
        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            goto _error;
        }
    } else {
        /* 右结合：按从右到左顺序递归处理每个子节点 */
        for (i = node->nb_nodes - 1; i >= 0; i--) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }
        /* 顺序同上：查找 handler 并调用 */
        operator_handler_pt h = find_expr_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            goto _error;
        }
    }
    /* 成功 */
    return 0;

_error:
    /* 统一错误返回点（目前只是返回 -1） */
    return -1;
}

/* 以下为各类 operator 的具体处理函数（作为 operator_handler_pt 的实现） */

/* 指针运算符（例如 *p 或指针解引用/取地址相关操作）的处理器（目前空实现） */
/* 期望 nb_nodes == 2（左操作数和右操作数），仅做断言检查后返回成功。 */
static int _op_expr_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);
    return 0;
}

/* 数组下标运算 a[b] 处理函数 */
static int _op_expr_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); /* a[b] 必须有两个操作数 */

    node_t *parent = nodes[0]->parent; /* 保存父节点（用于错误或后续改写时使用） */

    /* 获取左右操作数对应的 variable_t（封装了类型/常量标志等信息）*/
    variable_t *v0 = _operand_get(nodes[0]); /* 数组/指针操作数 a */
    variable_t *v1 = _operand_get(nodes[1]); /* 下标操作数 b */
                                             /* 要求下标是常量（编译期可知），否则报错 */
    if (!variable_const(v1)) {
        loge("\n");
        return -EINVAL;
    }
    /* 要求被索引的对象要么是常量字面量（const literal），要么是成员（成员访问，如 struct.field）：
     * 如果既不是字面量也不是成员访问，说明此处的语义不允许数组下标（或暂不支持）。
     */
    if (!v0->const_literal_flag && !v0->member_flag) {
        loge("\n");
        return -EINVAL;
    }

    return 0;
}

/* 用于处理表达式包装/括号之类的节点（OP_EXPR），把包装去掉并继续处理内部表达式 */
static int _op_expr_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes); /* 包装表达式只有单个子节点 */

    handler_data_t *d = data;

    node_t *n = nodes[0];
    node_t *parent = nodes[0]->parent;
    /* 如果连续多层 OP_EXPR（例如 ((expr))），一直向下找到实际的子表达式 */
    while (OP_EXPR == n->type)
        n = n->nodes[0];
    /* 现在 n 指向真正的子表达式节点。
     * 原 wrapper（nodes[0]）的子指针要从父节点断开，避免在 free 时误释放子节点。
     * n->parent 指向 wrapper，故我们把 wrapper 在其父中的对应指针设为 NULL。
     */
    n->parent->nodes[0] = NULL;

    /* 释放 wrapper 节点本身（它已经不包含子节点了），避免内存泄漏 */
    node_free(nodes[0]);

    /* 将原来的 nodes[0] 指向真正的子表达式节点，并修正 parent 指针关系 */
    nodes[0] = n;
    n->parent = parent;
    /* 递归处理被解包后的表达式（注意此处是把替换后的节点作为新的子节点继续计算） */
    int ret = _expr_calculate_internal(ast, n, d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

/* 地址运算符 (&expr) 处理器（目前空实现） */
static int _op_expr_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

/* 类型转换表达式处理（例如 (int)x 或 (T)expr） */
static int _op_expr_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); /* (dst_type) src 形式：目标类型节点 + 源表达式节点 */

    /* dst、src 都通过 _operand_get 转成 variable_t 用于检查/生成常量等 */
    variable_t *dst = _operand_get(nodes[0]); /* 目标类型的占位 variable（存放目标信息） */
    variable_t *src = _operand_get(nodes[1]); /* 源表达式的变量（需要被转换） */
    node_t *parent = nodes[0]->parent;        /* 父节点，后面用于替换或写回结果 */
                                              /* 情况1：源是常量 —— 可以在编译期做常量折叠/转换 */
    if (variable_const(src)) {
        int dst_type = dst->type;
        /* 如果目标是指针/数组类（通过 nb_pointers + nb_dimentions 判断），把目标类型视为 uintptr */
        if (dst->nb_pointers + dst->nb_dimentions > 0)
            dst_type = VAR_UINTPTR;
        /* 查找从 src->type 到 dst_type 的基础类型转换函数（例如 int->float, char*->uintptr 等） */
        type_cast_t *cast = find_base_type_cast(src->type, dst_type);
        if (cast) {
            variable_t *r = NULL;
            /* 调用具体的转换函数，由 cast->func 填充 r（新的常量 variable） */
            int ret = cast->func(ast, &r, src);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
            /* 填充 r 的属性，使之成为目标语义上的常量结果 */
            r->const_flag = 1;                          /* 标记为常量 */
            r->type = dst->type;                        /* 恢复真实目标类型（可能与 dst_type 不同，dst_type 只是用于找到 cast） */
            r->nb_pointers = variable_nb_pointers(dst); /* 保留目标指针层数信息 */
            r->const_literal_flag = 1;                  /* 标记为常量字面量 */

            /* 如果父节点有关联的词法单元（parent->w），交换 r->w 与 parent->w，
             * 目的是把原来父节点代表的源代码词（位置/文本等）移动到结果 variable 上，便于后续错误/调试信息。
             * XCHG 是交换宏/函数（需查看实现），在这里等价于 swap(r->w, parent->w)。
             */

            if (parent->w)
                XCHG(r->w, parent->w);

            logd("parent: %p\n", parent);
            /* 释放 parent 节点里原有的数据（可能是类型占位、旧 variable 等），准备写入 r */
            node_free_data(parent);
            /* 把 parent 节点转换为常量结果节点：设置类型并把 var 指向 r（注意这里没有复制 r） */
            parent->type = r->type;
            parent->var = r;
        }

        return 0;
    } else if (variable_const_string(src)) {
        /* 情况2：源是常量字符串（字符串常量的特殊处理路径） */
        assert(src == nodes[1]->var); /* 确保节点 var 指向该字符串常量 */
                                      /* 引用计数或浅拷贝字符串 variable，得到 v（并不修改 src 本身） */
        variable_t *v = variable_ref(src);
        assert(v);
        /* 释放 parent 原来的数据（占位等），并把 parent 变为类型转换后的结果 */
        node_free_data(parent);

        logd("parent->result: %p, parent: %p, v->type: %d\n", parent->result, parent, v->type);
        parent->type = v->type;
        parent->var = v;
        /* 其它情况（非常量、非字符串常量），当前实现不做即时处理，返回成功让后续阶段处理 */
        return 0;
    }

    return 0;
}

// 处理sizeof运算符的函数
static int _op_expr_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return -1; // 暂不支持sizeof运算，返回错误
}

// 处理一元运算符的通用函数
static int _op_expr_unary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes); // 确保只有一个操作数

    handler_data_t *d = data; // 获取处理器数据
                              // 获取操作数和父节点
    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0); // 确保操作数存在
                // 只有常量才能进行编译时计算
    if (!v0->const_flag)
        return -1;
    // 检查操作数类型是否为数字类型
    if (type_is_number(v0->type)) {
        // 查找对应的计算函数
        calculate_t *cal = find_base_calculate(parent->type, v0->type, v0->type);
        if (!cal) {
            loge("type %d not support\n", v0->type); // 记录类型不支持的错误
            return -EINVAL;
        }
        // 执行一元运算
        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, NULL);
        if (ret < 0) {
            loge("\n"); // 记录运算失败的错误
            return ret;
        }
        r->const_flag = 1; // 标记结果为常量
                           // 交换宽度信息
        XCHG(r->w, parent->w);
        // 释放父节点原有数据，设置新的类型和变量
        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;

    } else {
        loge("type %d not support\n", v0->type); // 记录类型不支持的错误
        return -1;
    }

    return 0; // 成功返回
}

// 负号运算符处理函数
static int _op_expr_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data); // 委托给一元运算通用函数
}
// 按位取反运算符处理函数
static int _op_expr_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data); // 委托给一元运算通用函数
}
// 逻辑非运算符处理函数
static int _op_expr_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data); // 委托给一元运算通用函数
}
// 处理二元运算符的通用函数
static int _op_expr_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 确保有两个操作数

    handler_data_t *d = data; // 获取处理器数据

    // 获取两个操作数和父节点
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    assert(v0); // 确保操作数存在
    assert(v1);
    // 检查两个操作数是否都是数字类型
    if (type_is_number(v0->type) && type_is_number(v1->type)) {
        // 只有两个操作数都是常量才能进行编译时计算
        if (!variable_const(v0) || !variable_const(v1)) {
            return 0; // 如果任一操作数不是常量，返回0（可能表示延迟计算）
        }

        assert(v0->type == v1->type); // 确保类型相同

        // 查找对应的计算函数
        calculate_t *cal = find_base_calculate(parent->type, v0->type, v1->type);
        if (!cal) {
            loge("type %d, %d not support\n", v0->type, v1->type); // 记录类型不支持的错误
            return -EINVAL;
        }
        // 执行二元运算
        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, v1);
        if (ret < 0) {
            loge("\n"); // 记录运算失败的错误
            return ret;
        }
        r->const_flag = 1; // 标记结果为常量
        // 交换宽度信息
        XCHG(r->w, parent->w);
        // 释放父节点原有数据，设置新的类型和变量
        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;

    } else {
        loge("type %d, %d not support\n", v0->type, v1->type); // 记录类型不支持的错误
        return -1;
    }

    return 0; // 成功返回
}
// 加法运算符处理函数
static int _op_expr_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}

// 减法运算符处理函数
static int _op_expr_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}

// 乘法运算符处理函数
static int _op_expr_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}

// 除法运算符处理函数
static int _op_expr_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}

// 取模运算符处理函数
static int _op_expr_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}
// 检查移位操作的合法性
static int _shift_check_const(node_t **nodes, int nb_nodes) {
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    // 如果移位数量是常量，检查其范围
    if (variable_const(v1)) {
        // 移位数量不能为负数
        if (v1->data.i < 0) {
            loge("shift count %d < 0\n", v1->data.i); // 记录移位数为负的错误
            return -EINVAL;
        }
        // 移位数量不能超过类型的位数
        if (v1->data.i >= v0->size << 3) {
            loge("shift count %d >= type bits: %d\n", v1->data.i, v0->size << 3); // 记录移位过大的错误
            return -EINVAL;
        }
    }

    return 0; // 检查通过
}
// 左移运算符处理函数
static int _op_expr_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 先检查移位操作的合法性
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}
// 右移运算符处理函数
static int _op_expr_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 先检查移位操作的合法性
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_expr_binary(ast, nodes, nb_nodes, data); // 委托给二元运算通用函数
}
// 解析赋值表达式左侧（左值）
static int _expr_left(member_t **pm, node_t *left) {
    variable_t *idx;
    member_t *m;
    // 分配成员结构
    m = member_alloc(NULL);
    if (!m)
        return -ENOMEM; // 内存分配失败
    // 遍历左侧表达式树
    while (left) {
        if (OP_EXPR == left->type) {
            // 普通表达式，继续遍历
        } else if (OP_ARRAY_INDEX == left->type) {
            // 数组索引操作
            idx = _operand_get(left->nodes[1]);
            // 添加数组索引到成员结构
            if (member_add_index(m, NULL, idx->data.i) < 0)
                return -1;

        } else if (OP_POINTER == left->type) {
            // 指针操作
            idx = _operand_get(left->nodes[1]);
            // 添加指针偏移到成员结构
            if (member_add_index(m, idx, 0) < 0)
                return -1;
        } else
            break; // 遇到变量节点，结束遍历

        left = left->nodes[0]; // 移动到左子节点
    }
    // 确保最终节点是变量类型
    assert(type_is_var(left->type));
    // 设置基变量
    m->base = _operand_get(left);

    *pm = m; // 返回构建的成员结构
    return 0;
}
// 解析赋值表达式右侧（右值）
static int _expr_right(member_t **pm, node_t *right) {
    variable_t *idx;
    member_t *m;
    // 分配成员结构
    m = member_alloc(NULL);
    if (!m)
        return -ENOMEM; // 内存分配失败
    // 遍历右侧表达式树
    while (right) {
        if (OP_EXPR == right->type)
            right = right->nodes[0]; // 普通表达式，继续遍历

        else if (OP_ASSIGN == right->type)
            right = right->nodes[1]; // 赋值操作，取右值

        else if (OP_ADDRESS_OF == right->type) {
            // 取地址操作
            if (member_add_index(m, NULL, -OP_ADDRESS_OF) < 0)
                return -1;

            right = right->nodes[0]; // 继续处理内部表达式

        } else if (OP_ARRAY_INDEX == right->type) {
            // 数组索引操作
            idx = _operand_get(right->nodes[1]);
            assert(idx->data.i >= 0); // 确保索引非负
            // 添加数组索引到成员结构
            if (member_add_index(m, NULL, idx->data.i) < 0)
                return -1;

            right = right->nodes[0]; // 继续处理内部表达式

        } else if (OP_POINTER == right->type) {
            // 指针操作
            idx = _operand_get(right->nodes[1]);
            // 添加指针偏移到成员结构
            if (member_add_index(m, idx, 0) < 0)
                return -1;

            right = right->nodes[0]; // 继续处理内部表达式
        } else
            break; // 遇到变量节点，结束遍历
    }
    // 确保最终节点是变量类型
    assert(type_is_var(right->type));
    // 设置基变量
    m->base = _operand_get(right);

    *pm = m; // 返回构建的成员结构
    return 0;
}

/*
 * 将 v1 的值作为常量初始化到 m0 指向的位置上
 *
 * 参数：
 *   m0 - 目标 member（表示被初始化的对象及其基变量）
 *   v1 - 源 variable（包含要拷贝的数据）
 *
 * 行为/假设：
 *   - 若 m0 没有索引（m0->indexes == NULL），表示直接给 base 变量整体赋值（比如全局变量被常量初始化）。
 *   - 若有索引，则 m0 表示对 base 的某个成员或数组元素的初始化（对 base 做部分内存拷贝）。
 *   - base->data 可能是 union（例如有 p 指针或原始数据域），因此对整体/部分内存拷贝时需注意目标内存是否已分配。
 *
 * 返回：
 *   0 表示成功；分配失败返回 -ENOMEM。
 */
static int _expr_init_const(member_t *m0, variable_t *v1) {
    // 被初始化的基变量
    variable_t *base = m0->base;

    if (!m0->indexes) {
        /* 没有索引：直接把 v1 的原始数据拷贝到 base->data。
         * 这里使用 base->data 而非 base->data.p，说明 base->data 既可以表示内联存储（固定宽度）
         * （例如小的整型或结构可能直接存在 union 中），也可能后面通过 base->data.p 动态指针表示大对象。
         * v1->size 是要拷贝的字节数。
         *
         * 注意：这种 memcpy 假定 base->data 的内存足够并且 layout 可直接 memcpy（通常在编译器常量初始化语义中可行）。
         */
        memcpy(&base->data, &v1->data, v1->size);
        return 0;
    }
    /* 下面处理有索引的情况：m0 表示 base 的某个成员或数组元素 */
    assert(variable_is_array(base)
           || variable_is_struct(base));
    // base 必须是数组或结构体，因为有索引说明在对复合类型的部分初始化

    int size = variable_size(base); // base 的总字节大小
    int offset = member_offset(m0); // m0 指向的成员在 base 中的偏移（字节）

    assert(offset < size); // 偏移必须合法

    if (!base->data.p) {
        /* 若 base 的数据区域还未通过指针分配（比如大对象或外部存储），则分配一块 size 字节
         * 并清零（calloc）。这是为了后续把部分成员写入到正确位置。
         */
        base->data.p = calloc(1, size);
        if (!base->data.p)
            return -ENOMEM;
    }
    /* 把 v1 的数据拷贝到 base->data.p + offset 位置（即具体成员的内存位置） */
    memcpy(base->data.p + offset, &v1->data, v1->size);
    return 0;
}

/*
 * 对地址初始化（把某个对象的地址放到全局常量表并建立引用关系）
 *
 * 语义上用于把 m1（源）的地址作为常量保存到 m0（目标）中（例如像 C 里把 &sym 存为常量指针）。
 *
 * 参数：
 *   ast - 抽象语法树/上下文（包含全局常量表 global_consts、全局关系 global_relas 等）
 *   m0  - 目标 member（接收地址的那个位置）
 *   m1  - 源 member（我们要取其地址）
 *
 * 行为：
 *   - 将 m1->base（被取地址的变量）加入 ast->global_consts（唯一化加入）。
 *   - 为了在后面链接时知道哪个常量地址对应哪个目标，建立 ast_rela_t（引用关系）并加入 ast->global_relas。
 *
 * 返回：
 *   0 成功；负值错误（例如内存分配或 vector 操作失败）。
 */
static int _expr_init_address(ast_t *ast, member_t *m0, member_t *m1) {
    int ret = vector_add_unique(ast->global_consts, m1->base);
    if (ret < 0)
        return ret;
    /* 建立一个关系对象，记录“目标 ->（引用）源” */
    ast_rela_t *r = calloc(1, sizeof(ast_rela_t));
    if (!r)
        return -ENOMEM;
    r->ref = m0; // 引用（谁将持有地址）
    r->obj = m1; // 对象（地址来源）

    if (vector_add(ast->global_relas, r) < 0) {
        free(r);
        return -ENOMEM;
    }

    return 0;
}

/*
 * 处理表达式中的赋值操作（OP_ASSIGN）
 *
 * 节点数组 nodes 通常为左值（nodes[0]）和右值（nodes[1]）两部分。
 * 该函数解析左右两侧为 member_t（成员引用/变量引用），根据右侧是否为常量/字符串/函数指针/取地址等，
 * 把初始化工作委托到 _expr_init_const 或 _expr_init_address，或者返回错误。
 *
 * 参数：
 *   ast      - AST / 编译上下文
 *   nodes    - 表达式节点数组（左值、右值）
 *   nb_nodes - 节点数量（通常至少为 2）
 *   data     - 额外上下文（此处未使用）
 *
 * 返回：
 *   0 表示成功初始化（或建立地址关联）；负值表示失败。
 */
static int _op_expr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    variable_t *v0 = _operand_get(nodes[0]); // 左操作数对应的 variable（可能为 member->base）
    variable_t *v1 = _operand_get(nodes[1]); // 右操作数对应的 variable
    member_t *m0 = NULL;
    member_t *m1 = NULL;

    logd("v0->type: %d, v1->type: %d\n", v0->type, v1->type);

    /* 如果右值不是常量字符串，则要求左右类型相等（静态类型一致性检测）
     * variable_const_string(v1) 为真说明 v1 是字符串字面量（在某些语言中，字符串字面量赋给不同类型的指针可能被允许）
     */
    if (!variable_const_string(v1))
        assert(v0->type == v1->type);

    assert(v0->size == v1->size);
    /* 不允许把整个数组或结构体直接作为右值（目前不支持此类初始化路径） */
    if (variable_is_array(v1) || variable_is_struct(v1)) {
        loge("\n");
        return -1;
    }
    /* 解析左侧（nodes[0]）为 member（可能会分配并返回在 m0 中） */
    if (_expr_left(&m0, nodes[0]) < 0) {
        loge("\n");
        return -1;
    }
    /* 解析右侧（nodes[1]）为 member（可能为直接变量、带索引的表达式、地址表达式等） */
    if (_expr_right(&m1, nodes[1]) < 0) {
        loge("\n");
        member_free(m0); // 记得释放之前分配的 m0
        return -1;
    }

    int ret = -1;
    /* 当右侧没有索引（即不是 a[i] 这样的形式）时根据右侧 base 的常量性作不同处理 */
    if (!m1->indexes) {
        if (variable_const_string(m1->base)) {
            /* 右侧是字符串字面量：把字符串常量的地址或内容与左侧建立关联
             * 具体行为交给 _expr_init_address（把 m1->base 注册为全局常量并建立引用关系）
             */
            ret = _expr_init_address(ast, m0, m1);

        } else if (variable_const(m1->base)) {
            /* 右侧是编译时常量（非字符串） */
            if (FUNCTION_PTR == m1->base->type) {
                /* 如果常量是函数指针，把函数地址当做常量地址处理 */
                ret = _expr_init_address(ast, m0, m1);
            } else {
                /* 普通常量（整型、浮点、聚合中的常量子对象等）通过 _expr_init_const 直接把常量数据写入目标 */
                ret = _expr_init_const(m0, m1->base);
                /* _expr_init_const 完成实际拷贝后这里释放成员结构，返回 ret */
                member_free(m0);
                member_free(m1);
                return ret;
            }
        } else {
            /* 其他情况（右侧既不是字符串字面量也不是编译期常量）——这在初始化语义里可能不被允许 */
            variable_t *v = m1->base;
            loge("v: %d/%s\n", v->w->line, v->w->text->data);
            return -1;
        }
    } else {
        /* 右侧有索引的情况，例如 &array 或 &struct.member? 这段代码在处理像 (&x)[...] 或取地址后索引的特殊语义。
         *
         * 具体来看：
         *  - 断言 m1->indexes->size >= 1（至少有一个索引）
         *  - 取出第一个索引 idx，断言其 index == -OP_ADDRESS_OF —— 这说明索引被用作“地址操作”的一种内部表示（由语法解析阶段留下的哨兵）。
         *  - 然后把这个索引从 m1 的索引向量中删除并释放 idx。
         *  - 最后把 m1 当作一个“取地址的 member”交给 _expr_init_address（建立地址引用关系）。
         *
         * 这段实现依赖于解析阶段如何把取地址表达式映射为 indexes 向量的约定（把地址符号用负数或特殊 idx 表示）。
         */
        index_t *idx;

        assert(m1->indexes->size >= 1);

        idx = m1->indexes->data[0];

        assert(-OP_ADDRESS_OF == idx->index);

        assert(0 == vector_del(m1->indexes, idx));

        free(idx);
        idx = NULL;

        ret = _expr_init_address(ast, m0, m1);
    }

    if (ret < 0) {
        loge("\n");
        member_free(m0);
        member_free(m1);
    }
    return ret;
}

/* 简单地把比较操作都交给 _op_expr_binary（或通用比较处理）——这里是语义层面的包装 */
static int _op_expr_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

/* 逻辑与/或、位运算等也统一交给通用的二元表达式处理器（_op_expr_binary） */
static int _op_expr_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

/* --------- 操作符处理函数表（将操作符枚举值映射到相应的处理函数） --------- */
/* 这种方式方便通过操作符类型直接查找处理器，避免长 switch-case */
operator_handler_pt expr_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_expr_expr,

        [OP_ARRAY_INDEX] = _op_expr_array_index,
        [OP_POINTER] = _op_expr_pointer,

        [OP_SIZEOF] = _op_expr_sizeof,
        [OP_TYPE_CAST] = _op_expr_type_cast,
        [OP_LOGIC_NOT] = _op_expr_logic_not,
        [OP_BIT_NOT] = _op_expr_bit_not,
        [OP_NEG] = _op_expr_neg,

        [OP_ADDRESS_OF] = _op_expr_address_of,

        [OP_MUL] = _op_expr_mul,
        [OP_DIV] = _op_expr_div,
        [OP_MOD] = _op_expr_mod,

        [OP_ADD] = _op_expr_add,
        [OP_SUB] = _op_expr_sub,

        [OP_SHL] = _op_expr_shl,
        [OP_SHR] = _op_expr_shr,

        [OP_BIT_AND] = _op_expr_bit_and,
        [OP_BIT_OR] = _op_expr_bit_or,

        [OP_EQ] = _op_expr_eq,
        [OP_NE] = _op_expr_ne,
        [OP_GT] = _op_expr_gt,
        [OP_LT] = _op_expr_lt,
        [OP_GE] = _op_expr_ge,
        [OP_LE] = _op_expr_le,

        [OP_LOGIC_AND] = _op_expr_logic_and,
        [OP_LOGIC_OR] = _op_expr_logic_or,

        [OP_ASSIGN] = _op_expr_assign,
};

/* 查找对应类型的表达式处理器（安全检查 type 范围） */
operator_handler_pt find_expr_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return expr_operator_handlers[type];
}

/*
 * 对表达式 e 做计算（或求值），并把结果 variable 的引用放入 pret（如果 pret 非空）
 *
 * 语义：
 *   - 首先做语义分析 expr_semantic_analysis（比如类型检查、常量折叠准备、重写等），失败则直接返回错误。
 *   - 然后若表达式最外层节点不是变量类型（即不是立即可用的 variable），会调用 _expr_calculate_internal 递归计算（可能进行常量求值或中间临时值生成）。
 *   - 最终通过 _operand_get 获取到最终的 variable 指针 v。
 *   - 如果用户要求返回结果（pret != NULL），则通过 variable_ref 返回一个引用（可能是增加引用计数）。
 *
 * 注意：
 *   - 这里对 e->nodes[0] 的类型检查用的是 type_is_var —— 表示若该节点已经代表一个 variable（例如命名的变量或已计算的常量），则无需调用内部计算。
 *   - _expr_calculate_internal 的实现不在片段中，但从调用格式看它会为表达式生成/填充临时变量或计算结果。
 */
int expr_calculate(ast_t *ast, expr_t *e, variable_t **pret) {
    if (!e || !e->nodes || e->nb_nodes <= 0)
        return -1;

    if (expr_semantic_analysis(ast, e) < 0)
        return -1;

    handler_data_t d = {0}; // 传入 _expr_calculate_internal 的上下文/输出结构
    variable_t *v;

    /* 如果表达式的最外层节点还不是 variable（可能是运算节点），就递归计算它 */
    if (!type_is_var(e->nodes[0]->type)) {
        if (_expr_calculate_internal(ast, e->nodes[0], &d) < 0) {
            loge("\n");
            return -1;
        }
    }
    /* 取得表达式结果所对应的 variable（可能是原变量或 _expr_calculate_internal 创建的临时变量） */
    v = _operand_get(e->nodes[0]);

    if (pret)
        *pret = variable_ref(v); // 返回一个引用（假定 variable_ref 做引用计数或复制）

    return 0;
}
