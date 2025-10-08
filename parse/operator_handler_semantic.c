#include "ast.h"
#include "operator_handler_semantic.h"
#include "type_cast.h"

// handler_data_t: 传递给操作符语义处理器的上下文数据结构。
// 目前只包含一个成员 pret，类型为 variable_t**。
// pret 用来指向“结果变量指针的位置”，即告诉操作符处理器把计算结果放到哪里。
typedef struct {
    variable_t **pret; // 指向 variable_t* 的指针（通常指向某个 node->result）

} handler_data_t;

/* 前向声明：这两个函数在本文件中其余部分实现（或在别处实现），
   在当前片段中只是声明以便下面的函数能调用它们。 */
static int __op_semantic_call(ast_t *ast, function_t *f, void *data);

static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data);

/*
 * _op_semantic_node
 *
 * 作用：
 *   对单个 AST 节点（node）执行“操作符语义处理”：
 *     1. 确定节点对应的 operator_t（如果 node->op 为 NULL，则以 node->type 查找基础 operator）
 *     2. 根据 operator->type 查找对应的语义处理函数（handler）
 *     3. 设置 handler_data_t 中的 pret 指向 node->result（让 handler 将结果写回 node->result）
 *     4. 调用 handler，之后恢复 pret 的原值
 *
 * 参数：
 *   ast  - 整个语法树/上下文
 *   node - 当前待处理的节点（可能为操作符节点或 operand 节点）
 *   d    - 语义处理器上下文（handler_data_t）
 *
 * 返回：
 *   handler 返回值（0 表示成功，负值表示错误），或在找不到 operator/handler 时返回 -1（并记录错误）
 */
static int _op_semantic_node(ast_t *ast, node_t *node, handler_data_t *d) {
    operator_t *op = node->op;
    // 如果节点没有直接绑定 operator（可能是通过节点类型隐含的基础 operator），则查找基础 operator
    if (!op) {
        op = find_base_operator_by_type(node->type); // 根据 node->type（如 OP_ADD/OP_CALL 等）查找 operator
        if (!op) {
            loge("\n"); // 记录错误（具体日志内容在 loge 内部）
            return -1;  // 无法找到对应的 operator，语义分析无法继续
        }
    }
    // 根据 operator 的类型在语义处理器表中查找对应的处理函数（operator handler）
    operator_handler_pt h = find_semantic_operator_handler(op->type);
    if (!h) {
        loge("\n"); // 找不到语义处理函数，记录错误并返回
        return -1;
    }
    // 保存原来的 pret 指针（可能指向调用方的某个位置），以便在调用后恢复
    variable_t **pret = d->pret;
    // 临时设置 d->pret 为当前节点的 result 字段地址，使 handler 知道将结果写回 node->result
    // 注意：handler 可能会分配 variable 并通过 *d->pret = 新 variable 来返回结果
    d->pret = &node->result;

    // 调用 operator 的语义处理函数：h(ast, node->nodes, node->nb_nodes, d)
    // 约定：handler 接受 ast, 子节点数组, 子节点数, 以及 handler_data_t*（用于传入/返回信息）
    int ret = h(ast, node->nodes, node->nb_nodes, d);

    // 恢复原来的 pret 指针（无论 handler 成功或失败都恢复）
    d->pret = pret;

    return ret;
}

/*
 * _semantic_add_address_of
 *
 * 作用：
 *   给一个节点 src 加上“取地址”操作（&src），生成新的 address_of 节点并返回在 *pp。
 *   具体地：
 *     - 根据 src 的类型构造一个对应的指针类型 variable（nb_pointers + 1）
 *     - 创建 OP_ADDRESS_OF 的节点，并以 src 为其子节点
 *     - 将新节点的 result 指向上述分配的 variable（表示 &src 的类型/值）
 *
 * 参数：
 *   ast - 语法树上下文（用于类型查找）
 *   pp  - 输出参数；成功时 *pp 指向新创建的 address_of 节点
 *   src - 被取地址的源节点
 *
 * 返回：
 *   0 成功，负值表示错误（如找不到 operator、分配失败或类型错误）
 *
 * 重要的内存/所有权约定：
 *   - 该函数会为 address_of->result 分配一个 variable（v）。如果函数成功，v 的所有权交给了 address_of（
 *     即后续 node_free(address_of) 应负责释放 v）。如果失败，函数会释放已分配的资源。
 */
static int _semantic_add_address_of(ast_t *ast, node_t **pp, node_t *src) {
    node_t *parent = src->parent; // 保存原来的 parent 指针（新建节点会接回这个 parent）

    // 找到基础的 OP_ADDRESS_OF operator 描述（操作符表中应存在）
    operator_t *op = find_base_operator_by_type(OP_ADDRESS_OF);
    if (!op)
        return -EINVAL; // 参数不合法或运行环境不正确

    // 从 src 节点获得其操作数对应的 variable（例如：如果 src 是常量节点或变量引用，_operand_get 会返回对应的 variable）
    variable_t *v_src = _operand_get(src);

    // 根据 v_src 的类型在 AST 中查找 type_t 描述（可能把 type id -> type descriptor）
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v_src->type);
    if (ret < 0)
        return ret;
    assert(t);

    // 按照 v_src 的类型创建一个新的 variable：类型相同但指针层级 +1（表示指向 v_src 的指针）
    // 参数含义：VAR_ALLOC_BY_TYPE(词法信息, type_t*, const_flag, nb_pointers, func_ptr)
    variable_t *v = VAR_ALLOC_BY_TYPE(v_src->w, t, v_src->const_flag, v_src->nb_pointers + 1, v_src->func_ptr);
    if (!v)
        return -ENOMEM; // 内存不足

    // 为 address_of 节点分配 node 结构，节点类型为 OP_ADDRESS_OF
    node_t *address_of = node_alloc(NULL, OP_ADDRESS_OF, NULL);
    if (!address_of) {
        variable_free(v); // 失败时释放之前分配的 variable
        return -ENOMEM;
    }
    // 将 src 添加为 address_of 的子节点（表示 & src）
    ret = node_add_child(address_of, src);
    if (ret < 0) {
        // 出错时释放已分配资源
        variable_free(v); // 将新分配的 variable 作为 address_of 的结果类型/值
        node_free(address_of);
        return ret;
    }
    // 绑定 operator、结果 variable、父节点，并把新节点通过输出参数返回
    address_of->op = op;
    address_of->result = v;
    address_of->parent = parent;
    *pp = address_of;
    return 0;
}

/*
 * _semantic_add_type_cast
 *
 * 作用：
 *   构建一个类型转换节点 (OP_TYPE_CAST)，将 src 包裹进一个（目标类型为 v_dst->type）cast 节点中，
 *   返回新构建的 cast 节点在 *pp。
 *
 * 细节：
 *   - 通过 v_dst->type 在 ast 中查找对应的 type_t（目标类型描述）
 *   - 创建一个临时 variable v（按目标类型、指针层级等属性构造）
 *   - 创建一个 dst 节点（该节点表示目标类型的临时“占位”），并将其作为 cast 的第一个子节点
 *   - 将 src 作为 cast 的第二个子节点（即 cast(dst, src) 结构）
 *   - cast->result 指向 dst->var 的一个引用（通过 variable_ref），表明 cast 节点的结果是 dst 类型的 variable
 *
 * 参数：
 *   ast  - AST 上下文
 *   pp   - 输出参数，指向最终构造的 cast 节点
 *   v_dst - 目标 variable（用其 type/nb_pointers/func_ptr 等信息确定转换目标）
 *   src  - 被转换的源节点（将成为 cast 的第二个子节点）
 *
 * 返回：
 *   0 成功，负值出错
 *
 * 内存/所有权：
 *   - v 是临时用来创建 dst 节点用的 variable，创建完 dst（node_alloc）后会释放 v（dst 持有真正的 var）
 *   - cast->result 使用 variable_ref(dst->var)，增加了对 dst->var 的引用（因此释放时需要对应的 variable_free 或 node_free 递归释放）
 */
static int _semantic_add_type_cast(ast_t *ast, node_t **pp, variable_t *v_dst, node_t *src) {
    node_t *parent = src->parent; // 保存原 parent
                                  // 查找基础的类型转换 operator（OP_TYPE_CAST）
    operator_t *op = find_base_operator_by_type(OP_TYPE_CAST);
    if (!op)
        return -EINVAL;
    // 获取目标类型的详细描述 type_t（根据 v_dst->type）
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v_dst->type);
    if (ret < 0)
        return ret;
    assert(t);
    // 获取 src 的实际操作数 variable（源变量信息）
    variable_t *v_src = _operand_get(src);

    // 创建一个新的 variable（v），其类型为 t（目标类型），指针层级使用 v_dst->nb_pointers
    variable_t *v = VAR_ALLOC_BY_TYPE(NULL, t, v_src->const_flag, v_dst->nb_pointers, v_dst->func_ptr);
    if (!v)
        return -ENOMEM;
    // 创建一个表示目标值（临时）的节点 dst（该节点携带 variable v）
    node_t *dst = node_alloc(NULL, v->type, v);
    // dst 拥有 v 的所有权（因此我们可以立即释放局部 v 指针）
    variable_free(v);
    v = NULL;
    if (!dst)
        return -ENOMEM;
    // 创建一个 OP_TYPE_CAST 节点（cast 节点），将 dst 和 src 分别作为其子节点
    node_t *cast = node_alloc(NULL, OP_TYPE_CAST, NULL);
    if (!cast) {
        node_free(dst); // 若 cast 分配失败，释放 dst（及其内部的 variable）
        return -ENOMEM;
    }
    // 先把 dst 加为 cast 的第一个子节点
    ret = node_add_child(cast, dst);
    if (ret < 0) {
        node_free(dst);
        node_free(cast);
        return ret;
    }
    // 再把 src 加为 cast 的第二个子节点
    ret = node_add_child(cast, src);
    if (ret < 0) {
        node_free(cast); // dst 已经是 cast 的子节点，会在 node_free(cast) 时被递归释放
        return ret;
    }
    // 绑定 operator，并把 cast 的结果设为 dst->var 的一个引用（variable_ref 可能增加引用计数）
    cast->op = op;
    cast->result = variable_ref(dst->var); // cast 的结果是目标类型的变量
    cast->parent = parent;
    *pp = cast; // 输出 cast 节点
    return 0;
}

/*
 * _semantic_do_type_cast
 *
 * 作用：
 *   对二元操作数（nodes[0] 和 nodes[1]）进行类型统一（coercion）：
 *     - 通过 find_updated_type 查找两个操作数应该转换为的“共同类型”（例如 int 与 float -> float）
 *     - 若需要，将 cast 节点插入到对应的 nodes[x] 上，以使两边最终具有相同类型
 *
 * 参数：
 *   ast      - AST 上下文
 *   nodes    - 操作数节点数组（至少包含 nodes[0] 和 nodes[1]）
 *   nb_nodes - 节点数量（本函数只关心前两个）
 *   data     - 额外数据（未在本函数中使用）
 *
 * 返回：
 *   0 成功，负值错误（例如类型推导失败或内存分配失败）
 *
 * 注意：
 *   - 本函数会直接修改 nodes[] 内容（当插入 cast 时，会把 nodes[i] 替换为新的 cast 节点）
 *   - nodes 数组中原先的节点在被 node_add_child 时会被重新挂接到新的 cast 节点下，因此父指针关系会被维护
 */
static int _semantic_do_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 获取两个操作数对应的 variable（operand 的实际类型信息）
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    // 找到两个变量应该使用的“更新后类型”或最高精度共同类型（返回值可能是 type id）
    int ret = find_updated_type(ast, v0, v1);
    if (ret < 0) {
        loge("var type update failed, type: %d, %d\n", v0->type, v1->type);
        return -EINVAL;
    }

    // 把 type id 映射为具体的 type_t 结构
    type_t *t = NULL;
    ret = ast_find_type_type(&t, ast, ret);
    if (ret < 0)
        return ret;
    assert(t);

    // 申请一个代表标准目标类型的临时 variable（v_std）
    // 该 v_std 只用于传参给 _semantic_add_type_cast（用以提供目标类型信息），
    // 在函数结束前会被释放。
    variable_t *v_std = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);
    if (!v_std)
        return -ENOMEM;

    // 如果目标类型与 nodes[0] 的类型不同，则为 nodes[0] 插入 cast
    if (t->type != v0->type) {
        // 注意：_semantic_add_type_cast 的第一个参数是 node_t **pp，
        // 因此它会用新的 cast 节点替换 nodes[0]（就地修改 nodes 数组）
        ret = _semantic_add_type_cast(ast, &nodes[0], v_std, nodes[0]);
        if (ret < 0) {
            loge("add type cast failed\n");
            goto end;
        }
    }

    // 同上：若 nodes[1] 的类型与目标类型不同，则插入 cast
    if (t->type != v1->type) {
        ret = _semantic_add_type_cast(ast, &nodes[1], v_std, nodes[1]);
        if (ret < 0) {
            loge("add type cast failed\n");
            goto end;
        }
    }

    // 一切成功
    ret = 0;
end:
    // 释放临时分配的 v_std（如果 _semantic_add_type_cast 已把需要的目标类型信息复制到 dst 节点，则这里释放安全）
    variable_free(v_std);
    return ret;
}

// -----------------------------------------------------------------------------
// 辅助说明（整体）
// 这些函数处理函数调用相关的语义分析：检查变量尺寸、为函数调用创建返回值节点、
// 将被调函数包装为函数指针参数、重载解析、以及为参数插入必要的类型转换（cast）。
// -----------------------------------------------------------------------------

/*
 * _semantic_check_var_size
 *
 * 作用：
 *   确保 node->var（该节点对应的变量）有正确的 size 和 data_size 值。
 *   如果变量的 size 或 data_size 为 0，则用类型描述 t->size 填充。
 *
 * 参数：
 *   ast  - 抽象语法树上下文，用于类型查找
 *   node - 要检查的节点（node->var 应当非 NULL）
 *
 * 说明：
 *   - ast_find_type_type 把 node->type（通常为 type id）映射到具体的 type_t 结构。
 *   - 这里使用 assert 确保 ast_find_type_type 成功；实际工程可改为更友好的错误返回。
 */
static void _semantic_check_var_size(ast_t *ast, node_t *node) {
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, node->type);
    assert(0 == ret);
    assert(t);
    // 如果变量 size 未设置，则使用类型描述的大小
    if (0 == node->var->size) {
        node->var->size = t->size;
        logd("node: %p var: %p, var->size: %d\n", node, node->var, node->var->size);
    }
    // data_size 常用于存储序列化/内存中实际数据的大小（与 size 可能一致）
    if (0 == node->var->data_size)
        node->var->data_size = t->size;
}

/*
 * _semantic_add_call_rets
 *
 * 作用：
 *   给调用节点 parent（代表调用表达式）创建、填充并挂载被调函数 f 的返回值节点（f->rets）。
 *   支持函数有多个返回值（多返回值的语言/实现场景）。
 *
 * 参数：
 *   ast    - AST 上下文（用于类型查找）
 *   parent - 表示调用的节点（将承载 result_nodes 向量）
 *   d      - handler_data_t，上下文中可能包含 pret（把结果返回给调用者的位置）
 *   f      - 被调用的 function_t（包含 f->rets 等信息）
 *
 * 返回：
 *   0 成功，负值错误（如内存不足或类型查找失败）
 *
 * 行为细节：
 *   - 如果 parent->result_nodes 为空，分配一个 vector 用于存放所有返回值对应的 node。
 *   - 若已经存在，则先清理（vector_clear，并对每个 node 调用 node_free）。
 *   - 对 f->rets 中的每个 variable（fret），基于其 type 创建对应的临时 variable r，
 *     再把 r 包装成 node 并加入 parent->result_nodes。
 *   - 对第一个返回值（如果存在）且调用方期望结果（d->pret 非空）时，将第一个返回值引用回调给 d->pret。
 *     （*d->pret = variable_ref(r)）
 */
static int _semantic_add_call_rets(ast_t *ast, node_t *parent, handler_data_t *d, function_t *f) {
    variable_t *fret;
    variable_t *r;
    type_t *t;
    node_t *node;
    // 若函数有返回值，确保 parent->result_nodes 存在并为空
    if (f->rets->size > 0) {
        if (!parent->result_nodes) {
            parent->result_nodes = vector_alloc();
            if (!parent->result_nodes)
                return -ENOMEM;
        } else
            // 清理旧的返回结果节点（若存在），用 node_free 递归释放
            vector_clear(parent->result_nodes, (void (*)(void *))node_free);
    }

    int i;
    for (i = 0; i < f->rets->size; i++) {
        fret = f->rets->data[i];
        // 根据 fret->type 查到具体 type_t 描述
        t = NULL;
        int ret = ast_find_type_type(&t, ast, fret->type);
        if (ret < 0)
            return ret;
        assert(t);
        // 为 parent (当前调用上下文) 创建一个与被调返回类型一致的临时 variable r
        // 使用 parent->w 作为词法/位置信息，使得生成节点具有适当的位置信息
        r = VAR_ALLOC_BY_TYPE(parent->w, t, fret->const_flag, fret->nb_pointers, fret->func_ptr);
        // 为这个返回值分配 node（注意：node->result 将指向 r）
        node = node_alloc(r->w, parent->type, NULL);
        // 另一种可能的写法（被注释）： node_alloc(NULL, r->type, r)
        if (!node) {
            loge("\n");
            return -ENOMEM;
        }

        // 设置 node 的结果及一些与调用相关的标志
        node->result = r;            // 把刚创建的 variable 关联为 node 的结果
        node->op = parent->op;       // 继承调用 parent 的 operator（可能用于后续处理）
        node->split_parent = parent; // 记录这是从 parent 拆出的“返回值节点”
        node->split_flag = 1;        // 标记为拆分节点（用于后续生成/优化）
        // 将该返回值 node 放入 parent->result_nodes 向量
        if (vector_add(parent->result_nodes, node) < 0) {
            loge("\n");
            node_free(node);
            return -ENOMEM;
        }
    }
    // 如果调用者希望获取返回值（有 d->pret），则把第一个返回值通过 d->pret 返回（引用计数增加）
    if (d->pret && parent->result_nodes->size > 0) {
        r = _operand_get(parent->result_nodes->data[0]);

        *d->pret = variable_ref(r);
    }

    return 0;
}

/*
 * _semantic_add_call
 *
 * 作用：
 *   把 function_t f 的“函数值/函数指针”包装成一个节点（node_pf），
 *   并把它插入到调用 parent 的参数列表最前面（即 node_pf 作为第 0 个子节点），
 *   同时把 parent 的类型/操作符设置为 OP_CALL（调用操作）。
 *   最后为调用生成返回值节点（调用 _semantic_add_call_rets）。
 *
 * 参数：
 *   ast      - AST 上下文
 *   nodes    - 表示原调用表达式节点数组（nodes[0] 的 parent 就是要修改的 parent）
 *   nb_nodes - nodes 数量（至少 1）
 *   d        - handler_data_t，上下文（用于把返回值传回）
 *   f        - 被调用的函数
 *
 * 返回：
 *   0 成功，负值错误
 *
 * 关键词解释：
 *   - var_pf: 表示“函数指针变量”的 variable；该变量保存对函数 f 的引用（func_ptr 字段）
 *   - node_pf: 包含 var_pf 的节点，将被插入为调用的第 0 个参数（callee）
 *
 * 具体步骤：
 *   1. 为函数指针类型分配 variable（pt 由当前 block 的 FUNCTION_PTR 类型获得）
 *   2. 把该 variable 包装成 node_pf，并将它插入 parent 的 children 最前面（通过 node_add_child + 内部元素右移）
 *   3. 设置 parent->type = OP_CALL 并设置 operator
 *   4. 调用 _semantic_add_call_rets 创建返回值节点
 */
static int _semantic_add_call(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d, function_t *f) {
    assert(nb_nodes >= 1);

    variable_t *var_pf = NULL;
    node_t *node_pf = NULL;
    node_t *node = NULL;
    node_t *parent = nodes[0]->parent;

    // 在当前块中查找 FUNCTION_PTR 类型描述（表示函数指针类型）
    type_t *pt = block_find_type_type(ast->current_block, FUNCTION_PTR);
    // 为函数指针分配一个变量，nb_pointers=1 表示这是一个指针，func_ptr=f 保存被调用函数引用
    var_pf = VAR_ALLOC_BY_TYPE(f->node.w, pt, 1, 1, f);
    if (!var_pf) {
        loge("var alloc error\n");
        return -ENOMEM;
    }

    // 将其标记为常量字面量（函数名作为常量）
    var_pf->const_flag = 1;
    var_pf->const_literal_flag = 1;
    // 为 var_pf 创建节点 node_pf
    node_pf = node_alloc(NULL, var_pf->type, var_pf);
    if (!node_pf) {
        loge("node alloc failed\n");
        return -ENOMEM;
    }
    // 将 parent 标记为调用节点，并绑定 CALL 操作符
    parent->type = OP_CALL;
    parent->op = find_base_operator_by_type(OP_CALL);
    // 把 node_pf 当作新子节点添加到 parent（node_add_child 会把 node_pf 添加到 children 末尾，并调整 nb_nodes）
    node_add_child(parent, node_pf);
    // 将原有 children 向右整体移动 1 格（因为 node_pf 实际要放到索引 0）
    // 注意：parent->nb_nodes 在 node_add_child 后已经增加 1，因此循环范围为 parent->nb_nodes - 2 .. 0
    int i;
    for (i = parent->nb_nodes - 2; i >= 0; i--)
        parent->nodes[i + 1] = parent->nodes[i];

    // 把 node_pf 放到索引 0
    parent->nodes[0] = node_pf;
    // 为调用构造返回值节点并处理 d->pret
    return _semantic_add_call_rets(ast, parent, d, f);
}

/*
 * _semantic_find_proper_function2
 *
 * 作用：
 *   在候选函数集合 fvec 中挑选一个“最合适”的函数（用于重载解析）。
 *   优先选择参数完全一致的函数；若没有完全一致的，再尝试通过可接受的类型转换匹配。
 *
 * 参数：
 *   ast  - AST 上下文（用于 type check）
 *   fvec - 候选函数集合（vector<function_t*>）
 *   argv - 调用时实参类型的 variable 向量（vector<variable_t*>）
 *   pf   - 输出参数，成功后指向选择的 function_t*
 *
 * 返回：
 *   0 成功（*pf 被设置），否则负值（未找到合适函数时返回 -404）
 *
 * 注意：
 *   - function_same_argv 用于判断参数列表是否完全相同（精确匹配）
 *   - type_cast_check 用于判断能否把实际参数隐式转换为形式参数类型
 *   - variable_is_struct_pointer 的存在说明对结构体指针参数有特殊放宽（跳过检查）
 *
 * 实现细节与潜在问题：
 *   - 代码的第一轮循环尝试精确匹配（正确）。
 *   - 第二轮循环试图允许隐式转换：但当前实现中在内层循环体内一旦发现第一个参数可转换就直接
 *     `*pf = f; return 0;`，这会在只检查了第一个参数时就返回——**可能是逻辑错误**。
 *     正确的逻辑通常是：对该 f 的所有参数都检查通过后，才设定 *pf 并返回。
 *   - 建议将 `*pf = f; return 0;` 移到内层循环外，并在循环完成且未遇到 break 的情况下返回。
 */
static int _semantic_find_proper_function2(ast_t *ast, vector_t *fvec, vector_t *argv, function_t **pf) {
    function_t *f;
    variable_t *v0;
    variable_t *v1;

    int i;
    int j;
    // 第 1 步：精确匹配（函数参数完全相同）
    for (i = 0; i < fvec->size; i++) {
        f = fvec->data[i];

        if (function_same_argv(f->argv, argv)) {
            *pf = f;
            return 0;
        }
    }
    // 第 2 步：尝试通过隐式类型转换匹配
    for (i = 0; i < fvec->size; i++) {
        f = fvec->data[i];
        // 逐对比较 f 的形式参数与调用时的实参
        for (j = 0; j < argv->size; j++) {
            v0 = f->argv->data[j];
            v1 = argv->data[j];
            // 如果形式参数是结构体指针，则跳过类型匹配（允许更灵活的匹配策略）
            if (variable_is_struct_pointer(v0))
                continue;
            // 若该参数不能被隐式转换（或检查失败），则中断当前候选函数 f 的检查，继续下一个 f
            if (type_cast_check(ast, v0, v1) < 0)
                break;

            // 注意：这里原代码在内循环里直接返回，可能会在只检查了部分参数时就认为匹配成功。
            // 正确做法应当在外层检查所有参数通过后再返回。下面是更符合意图的伪逻辑（注释）：
            //
            // // 如果 j 到达 argv->size，说明 f 的所有参数都可兼容
            // if (j == argv->size) {
            //     *pf = f;
            //     return 0;
            // }
            //
            // 但为了保持对原代码的注释性，这里不修改实现，仅提示潜在 bug。
            *pf = f;
            return 0;
        }
        // 注意：原代码没有在此处基于整个 inner-loop 的成功来返回；如果修复，应该在这里检查 j==argv->size 并返回。
    }

    return -404; // 表示未找到合适的函数（自定义错误码）
}

/*
 * _semantic_find_proper_function
 *
 * 作用：
 *   在给定类型作用域 t->scope 下，按名字 fname 和参数 argv 查找候选函数，
 *   然后调用 _semantic_find_proper_function2 从候选集中挑选最合适的函数。
 *
 * 参数：
 *   ast   - AST 上下文
 *   t     - 类型描述（其 scope 用于查找函数）
 *   fname - 函数名（或父作用域中存储函数标识的某种标识符）
 *   argv  - 实参数量（vector<variable_t*>）
 *   pf    - 输出，匹配到的 function_t*
 *
 * 返回：
 *   来自 _semantic_find_proper_function2 的返回值（0 表示成功）
 */
static int _semantic_find_proper_function(ast_t *ast, type_t *t, const char *fname, vector_t *argv, function_t **pf) {
    vector_t *fvec = NULL;
    // 在作用域中查找名字和参数形似的函数集合（可能是重载集合）
    int ret = scope_find_like_functions(&fvec, t->scope, fname, argv);
    if (ret < 0)
        return ret;
    // 从候选集合中挑选最合适的一个
    ret = _semantic_find_proper_function2(ast, fvec, argv, pf);
    // 释放候选集合
    vector_free(fvec);
    return ret;
}

/*
 * _semantic_do_overloaded2
 *
 * 作用：
 *   给定已选定的重载函数 f，把实际传入的参数 nodes[]（对应 argv）调整到与 f 的形参类型一致：
 *     - 遇到需要时为某个参数插入类型转换（调用 _semantic_add_type_cast）
 *   然后调用 _semantic_add_call 把函数指针节点插入并创建返回值。
 *
 * 参数：
 *   ast      - AST 上下文
 *   nodes    - 参数节点数组（将可能被就地替换成 cast 节点）
 *   nb_nodes - 节点数量
 *   d        - handler 数据（用于返回第一个返回值等）
 *   argv     - 参数的 variable 向量（与 f->argv 一一对应）
 *   f        - 被选中的重载函数
 *
 * 返回：
 *   0 成功，否则返回错误码
 */
static int _semantic_do_overloaded2(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d, vector_t *argv, function_t *f) {
    variable_t *v0;
    variable_t *v1;

    int i;
    for (i = 0; i < argv->size; i++) {
        v0 = f->argv->data[i]; // 形式参数
        v1 = argv->data[i];    // 实参 variable
                               // 跳过结构体指针的强制转换（保持宽松匹配）
        if (variable_is_struct_pointer(v0))
            continue;
        // 如果类型已经一致就不用插 cast
        if (variable_same_type(v0, v1))
            continue;
        // 否则插入类型转换节点，把 nodes[i]（实参节点）转换为形式参数类型 v0
        int ret = _semantic_add_type_cast(ast, &nodes[i], v0, nodes[i]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }
    // 在所有参数处理完毕后，把函数调用形态构建完成（插入函数指针节点、构造返回值节点等）
    return _semantic_add_call(ast, nodes, nb_nodes, d, f);
}

/*
 * _semantic_do_overloaded
 *
 * 作用：
 *   处理重载函数解析（在不知道具体哪个重载版本时）：
 *     1. 从 nodes[] 中收集实参的 variable（argv）
 *     2. 根据某个结构体指针参数来确定作用域（若存在）
 *     3. 在该作用域中查找匹配的重载候选集合 fvec
 *     4. 在候选集中选出最合适的函数（_semantic_find_proper_function2）
 *     5. 若找到则调用 _semantic_do_overloaded2 来完成参数类型调整并构造调用
 *
 * 参数：
 *   ast      - AST 上下文
 *   nodes    - 参数节点数组（注意 nodes[0] 的 parent 是调用表达式）
 *   nb_nodes - 节点数
 *   d        - handler_data_t
 *
 * 返回：
 *   0 成功，负值失败
 *
 * 说明：
 *   - 当存在结构体指针实参时，使用该结构体类型的 scope 来查找重载函数（支持方法重载在类/结构体作用域的常见场景）
 *   - 查找结果交给 _semantic_find_proper_function2 做最终匹配
 *   - 适当释放中间分配的 vector
 */
static int _semantic_do_overloaded(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    function_t *f;
    variable_t *v;
    vector_t *argv;
    vector_t *fvec = NULL;
    node_t *parent = nodes[0]->parent;
    type_t *t = NULL;
    // argv 用于保存当前实参的 variable 列表
    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    int ret;
    int i;

    for (i = 0; i < nb_nodes; i++) {
        v = _operand_get(nodes[i]); // 获取 nodes[i] 对应的 variable（实参的类型信息）
                                    // 如果还未确定 t，且遇到第一个结构体指针参数，则基于该参数的类型确定作用域
        if (!t && variable_is_struct_pointer(v)) {
            t = NULL;
            ret = ast_find_type_type(&t, ast, v->type);
            if (ret < 0)
                return ret;
            assert(t->scope); // 这里假设结构体类型必然有作用域（即成员/方法所在作用域）
        }
        // 把实参的 variable 加入 argv
        ret = vector_add(argv, v);
        if (ret < 0) {
            vector_free(argv);
            return ret;
        }
    }
    // 在确定的作用域（若有 t，则为 t->scope，否则依赖 scope_find_overloaded_functions 的默认行为）
    // 查找名为 parent->type（作为函数标识）的重载候选集合
    ret = scope_find_overloaded_functions(&fvec, t->scope, parent->type, argv);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }
    // 从候选集合中挑选最合适的函数
    ret = _semantic_find_proper_function2(ast, fvec, argv, &f);
    if (ret < 0)
        loge("\n");
    else
        // 若找到则执行进一步的参数调整并构造调用节点
        ret = _semantic_do_overloaded2(ast, nodes, nb_nodes, d, argv, f);
    // 释放临时分配的向量
    vector_free(fvec);
    vector_free(argv);
    return ret;
}

/**
 * 处理重载赋值操作符的语义分析
 * 当遇到自定义类型的赋值操作时，查找并调用相应的重载函数
 *
 * @param ast AST上下文
 * @param nodes 操作数节点数组
 * @param nb_nodes 操作数数量
 * @param d 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _semantic_do_overloaded_assign(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    function_t *f;
    variable_t *v;
    vector_t *argv;        // 函数参数向量
    vector_t *fvec = NULL; // 候选函数向量
    node_t *parent = nodes[0]->parent;
    type_t *t = NULL; // 类型信息

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    int ret;
    int i;

    // 1. 收集操作数并处理结构体类型
    for (i = 0; i < nb_nodes; i++) {
        v = _operand_get(nodes[i]);
        // 如果是结构体类型，需要取地址
        if (variable_is_struct(v)) {
            if (!t) {
                t = NULL;
                // 查找结构体类型定义
                ret = ast_find_type_type(&t, ast, v->type);
                if (ret < 0)
                    return ret;
                assert(t->scope); // 结构体必须有作用域
            }
            // 为结构体添加取地址操作
            ret = _semantic_add_address_of(ast, &nodes[i], nodes[i]);
            if (ret < 0) {
                loge("\n");
                return ret;
            }

            v = _operand_get(nodes[i]); // 重新获取操作数（现在是指针）
        }

        ret = vector_add(argv, v);
        if (ret < 0) {
            vector_free(argv);
            return ret;
        }
    }
    // 2. 在类型作用域中查找重载函数
    ret = scope_find_overloaded_functions(&fvec, t->scope, parent->type, argv);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }
    // 3. 选择最合适的重载函数
    ret = _semantic_find_proper_function2(ast, fvec, argv, &f);
    if (ret < 0)
        loge("\n");
    else
        // 4. 执行重载操作
        ret = _semantic_do_overloaded2(ast, nodes, nb_nodes, d, argv, f);

    vector_free(fvec);
    vector_free(argv);
    return ret;
}

/**
 * 处理对象创建操作的语义分析
 * 用于处理类似C++的new操作或对象构造
 *
 * @param ast AST上下文
 * @param nodes 操作数节点数组
 * @param nb_nodes 操作数数量
 * @param d 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _semantic_do_create(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    variable_t *v0;
    variable_t *v_pf;
    block_t *b;
    type_t *t;
    type_t *pt;
    node_t *parent = nodes[0]->parent;
    node_t *node0 = nodes[0]; // 目标节点
    node_t *node1 = nodes[1]; // 参数节点
    node_t *create = NULL;    // 创建操作节点
    node_t *node_pf = NULL;   // 函数指针节点

    v0 = _operand_get(nodes[0]);
    // 1. 查找目标类型
    t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;
    // 2. 查找函数指针类型
    pt = block_find_type_type(ast->current_block, FUNCTION_PTR);
    assert(t);
    assert(pt);
    // 3. 创建CREATE操作节点
    create = node_alloc(parent->w, OP_CREATE, NULL);
    if (!create)
        return -ENOMEM;
    // 4. 创建函数指针变量
    v_pf = VAR_ALLOC_BY_TYPE(t->w, pt, 1, 1, NULL);
    if (!v_pf)
        return -ENOMEM;
    v_pf->const_literal_flag = 1; // 标记为常量字面量

    node_pf = node_alloc(t->w, v_pf->type, v_pf);
    if (!node_pf)
        return -ENOMEM;
    // 5. 构建CREATE操作树
    ret = node_add_child(create, node_pf); // 函数指针作为第一个子节点
    if (ret < 0)
        return ret;

    ret = node_add_child(create, node1); // 参数作为第二个子节点
    if (ret < 0)
        return ret;
    create->parent = parent;
    parent->nodes[1] = create; // 替换原来的第二个操作数
                               // 6. 创建多返回值块
    b = block_alloc_cstr("multi_rets");
    if (!b)
        return -ENOMEM;

    ret = node_add_child((node_t *)b, node0);
    if (ret < 0)
        return ret;
    parent->nodes[0] = (node_t *)b; // 替换原来的第一个操作数
    b->node.parent = parent;
    // 7. 重新计算表达式
    ret = _expr_calculate_internal(ast, parent, d);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    return 0;
}

/**
 * 表达式计算的内部递归函数
 * 按照运算符的结合性遍历表达式树并计算结果
 *
 * @param ast AST上下文
 * @param node 当前节点
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    if (!node)
        return 0;
    // 如果是函数节点，直接处理函数调用
    if (FUNCTION == node->type)
        return __op_semantic_call(ast, (function_t *)node, data);
    // 叶子节点：变量或标签
    if (0 == node->nb_nodes) {
        if (type_is_var(node->type))
            _semantic_check_var_size(ast, node); // 检查变量大小

        logd("node->type: %d, %p, %p\n", node->type, _ operand_get(node), node);
        assert(type_is_var(node->type) || LABEL == node->type);
        return 0;
    }
    // 操作符节点必须有效
    assert(type_is_operator(node->type));
    assert(node->nb_nodes > 0);
    // 1. 查找操作符处理器
    if (!node->op) {
        node->op = find_base_operator_by_type(node->type);
        if (!node->op) {
            loge("node %p, type: %d, w: %p\n", node, node->type, node->w);
            return -1;
        }
    }
    // 2. 清理之前的结果
    if (node->result) {
        variable_free(node->result);
        node->result = NULL;
    }

    if (node->result_nodes) {
        vector_clear(node->result_nodes, (void (*)(void *))node_free);
        vector_free(node->result_nodes);
        node->result_nodes = NULL;
    }

    operator_handler_pt h;
    handler_data_t *d = data;
    variable_t **pret = d->pret; // 保存原始的pret，返回前恢复

    int i;
    // 3. 根据结合性递归计算子节点
    if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {
        // left associativity, from 0 to N -1
        // 左结合：从左到右计算（如：a + b + c）
        for (i = 0; i < node->nb_nodes; i++) {
            d->pret = &(node->nodes[i]->result); // 设置子节点的结果位置

            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }
        // 查找语义处理器
        h = find_semantic_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        d->pret = &node->result; // 设置当前节点的结果位置
                                 // 执行操作符语义处理
        if (h(ast, node->nodes, node->nb_nodes, d) < 0)
            goto _error;

    } else {
        // right associativity, from N - 1 to 0
        // 右结合：从右到左计算（如：a = b = c）
        for (i = node->nb_nodes - 1; i >= 0; i--) {
            d->pret = &(node->nodes[i]->result);

            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }

        h = find_semantic_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        d->pret = &node->result;

        if (h(ast, node->nodes, node->nb_nodes, d) < 0)
            goto _error;
    }

    d->pret = pret; // 恢复原始的pret
    return 0;

_error:
    d->pret = pret; // 错误时也要恢复pret
    return -1;
}

/**
 * 表达式计算的入口函数
 * 处理简单变量引用和复杂表达式计算
 *
 * @param ast AST上下文
 * @param e 表达式
 * @param pret 结果变量指针（输出参数）
 * @return 成功返回0，失败返回错误码
 */
static int _expr_calculate(ast_t *ast, expr_t *e, variable_t **pret) {
    assert(e);
    assert(e->nodes);

    node_t *root = e->nodes[0];
    // 1. 简单情况：单个变量引用
    if (type_is_var(root->type)) {
        logd("root: %p var: %p\n", root, root->var);

        _semantic_check_var_size(ast, root); // 检查变量大小

        root->result = variable_ref(root->var); // 创建变量的引用

        if (pret)
            *pret = variable_ref(root->var); // 输出结果
        return 0;
    }
    // 2. 复杂表达式：递归计算
    handler_data_t d = {0};
    d.pret = &root->result; // 设置根节点的结果位置

    if (_expr_calculate_internal(ast, root, &d) < 0) {
        loge("\n");
        return -1;
    }

    if (pret) {
        *pret = variable_ref(root->result); // 输出计算结果
    }
    return 0;
}

/**
 * 创建变量节点并添加到AST中
 *
 * @param pp 输出的节点指针
 * @param ast AST上下文
 * @param parent 父节点
 * @param w 词法单词（包含位置信息）
 * @param type 变量类型
 * @param const_ 是否为常量
 * @param nb_pointers_ 指针级数
 * @param func_ptr_ 函数指针（如果是函数指针类型）
 * @return 成功返回0，失败返回错误码
 */
static int _semantic_add_var(node_t **pp, ast_t *ast, node_t *parent,
                             lex_word_t *w, int type, int const_, int nb_pointers_, function_t *func_ptr_) {
    node_t *node;
    type_t *t;
    variable_t *v;
    // 1. 查找类型定义
    t = NULL;
    int ret = ast_find_type_type(&t, ast, type);
    if (ret < 0)
        return ret;
    if (!t)
        return -ENOMEM;
    // 2. 创建变量
    v = VAR_ALLOC_BY_TYPE(w, t, const_, nb_pointers_, func_ptr_);
    if (!v)
        return -ENOMEM;
    // 3. 创建变量节点
    node = node_alloc(v->w, v->type, v);
    if (!node) {
        variable_free(v);
        return -ENOMEM;
    }
    // 4. 添加到父节点
    if (parent) {
        int ret = node_add_child(parent, node);
        if (ret < 0) {
            node_free(node);
            variable_free(v);
            return ret;
        }
    }

    *pp = node;
    return 0;
}
/**
 * 处理对象创建操作符的语义分析（类似C++的new操作）
 * 生成：对象内存分配 + 构造函数调用
 *
 * @param ast AST上下文
 * @param nodes 操作数节点数组
 * @param nb_nodes 操作数数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_create(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes >= 1);

    handler_data_t *d = data;
    variable_t **pret = NULL;

    int ret;
    int i;

    variable_t *v0;
    variable_t *v1;
    variable_t *v2;
    vector_t *argv; // 构造函数参数
    type_t *class;  // 类类型
    type_t *t;
    node_t *parent = nodes[0]->parent;
    node_t *ninit = nodes[0]; // 初始化节点（构造函数指针）

    function_t *fmalloc; // 内存分配函数
    function_t *finit;   // 构造函数
    node_t *nmalloc;     // malloc调用节点
    node_t *nsize;       // 大小参数节点
    node_t *nthis;       // this 指针节点
    node_t *nerr;        // 错误节点

    // 1. 获取构造函数指针
    v0 = _operand_get(nodes[0]);
    assert(v0 && FUNCTION_PTR == v0->type);

    // 2. 查找类类型
    class = NULL;
    ret = ast_find_type(&class, ast, v0->w->text->data);
    if (ret < 0)
        return ret;
    assert(class);

    // 3. 查找内存分配函数
    fmalloc = NULL;
    ret = ast_find_function(&fmalloc, ast, "  _auto_malloc");
    if (ret < 0)
        return ret;
    if (!fmalloc) {
        loge("\n");
        return -EINVAL;
    }

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;
    // 4. 创建this指针变量
    ret = _semantic_add_var(&nthis, ast, NULL, v0->w, class->type, 0, 1, NULL);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }

    ret = vector_add(argv, nthis->var);
    if (ret < 0) {
        vector_free(argv);
        node_free(nthis);
        return ret;
    }
    // 5. 处理构造函数参数
    for (i = 1; i < nb_nodes; i++) {
        pret = d->pret;
        d->pret = &(nodes[i]->result);
        ret = _expr_calculate_internal(ast, nodes[i], d);
        d->pret = pret;

        if (ret < 0) {
            vector_free(argv);
            node_free(nthis);
            return ret;
        }

        ret = vector_add(argv, _operand_get(nodes[i]));
        if (ret < 0) {
            vector_free(argv);
            node_free(nthis);
            return ret;
        }
    }
    // 6. 查找匹配的构造函数
    ret = _semantic_find_proper_function(ast, class, "__init", argv, &finit);
    vector_free(argv);

    if (ret < 0) {
        loge("init function of class '%s' not found\n", v0->w->text->data);
        node_free(nthis);
        return -1;
    }
    v0->func_ptr = finit; // 设置函数指针
                          // 7. 创建大小参数节点
    ret = _semantic_add_var(&nsize, ast, parent, v0->w, VAR_INT, 1, 0, NULL);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }
    nsize->var->const_literal_flag = 1;
    nsize->var->data.i64 = class->size; // 设置类的大小
                                        // 8. 创建malloc函数指针节点
    ret = _semantic_add_var(&nmalloc, ast, parent, fmalloc->node.w, FUNCTION_PTR, 1, 1, fmalloc);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }
    nmalloc->var->const_literal_flag = 1;
    // 9. 重新组织AST节点结构
    ret = node_add_child(parent, nthis);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }
    // 调整父节点的子节点顺序：malloc, size, init, this, args...
    for (i = parent->nb_nodes - 4; i >= 0; i--)
        parent->nodes[i + 3] = parent->nodes[i];
    parent->nodes[0] = nmalloc;
    parent->nodes[1] = nsize;
    parent->nodes[2] = ninit;
    parent->nodes[3] = nthis;

    assert(parent->nb_nodes - 3 == finit->argv->size);
    // 10. 参数类型检查和转换
    for (i = 0; i < finit->argv->size; i++) {
        v1 = finit->argv->data[i]; // 构造函数参数类型

        v2 = _operand_get(parent->nodes[i + 3]); // 实际参数

        if (variable_is_struct_pointer(v1)) // 结构体指针跳过检查
            continue;

        if (variable_same_type(v1, v2)) // 类型相同跳过
            continue;
        // 添加类型转换
        ret = _semantic_add_type_cast(ast, &parent->nodes[i + 3], v1, parent->nodes[i + 3]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }
    // 11. 更新词法单词信息
    if (v0->w)
        lex_word_free(v0->w);
    v0->w = lex_word_clone(v0->func_ptr->node.w);
    // 12. 设置多返回值
    if (!parent->result_nodes) {
        parent->result_nodes = vector_alloc();
        if (!parent->result_nodes) {
            node_free(nthis);
            return -ENOMEM;
        }
    } else
        vector_clear(parent->result_nodes, (void (*)(void *))node_free);

    if (vector_add(parent->result_nodes, nthis) < 0) {
        node_free(nthis);
        return ret;
    }
    // 13. 添加错误返回值
    ret = _semantic_add_var(&nerr, ast, NULL, parent->w, VAR_INT, 0, 0, NULL);
    if (ret < 0)
        return ret;

    if (vector_add(parent->result_nodes, nerr) < 0) {
        node_free(nerr);
        return ret;
    }
    // 14. 设置分割节点信息（用于代码生成）
    nthis->op = parent->op;
    nthis->split_parent = parent;
    nthis->split_flag = 1;

    nerr->op = parent->op;
    nerr->split_parent = parent;
    nerr->split_flag = 1;

    *d->pret = variable_ref(nthis->var); // 设置返回值
    return 0;
}

/**
 * 处理指针操作符的语义分析（->操作符）
 *
 * @param ast AST上下文
 * @param nodes 操作数节点数组
 * @param nb_nodes 操作数数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]); // 结构体指针
    variable_t *v1 = _operand_get(nodes[1]); // 成员名称

    assert(v0);
    assert(v1);
    assert(v0->type >= STRUCT); // 必须是结构体或类类型
                                // 1. 查找成员类型
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v1->type);
    if (ret < 0)
        return ret;
    // 2. 创建结果变量（继承成员的const和指针属性）
    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v1->const_flag, v1->nb_pointers, v1->func_ptr);
    if (!r)
        return -ENOMEM;

    r->member_flag = v1->member_flag; // 继承成员标志
                                      // 3. 复制数组维度信息
    int i;
    for (i = 0; i < v1->nb_dimentions; i++)
        variable_add_array_dimention(r, v1->dimentions[i].num, NULL);

    *d->pret = r; // 设置结果
    return 0;
}

/**
 * 处理数组索引操作符的语义分析（[]操作符）
 *
 * @param ast AST上下文
 * @param nodes 操作数节点数组
 * @param nb_nodes 操作数数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    variable_t *v0 = _operand_get(nodes[0]); // 数组变量
    assert(v0);

    handler_data_t *d = data;
    variable_t **pret = d->pret;
    // 1. 计算索引表达式
    d->pret = &(nodes[1]->result);
    int ret = _expr_calculate_internal(ast, nodes[1], d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    variable_t *v1 = _operand_get(nodes[1]); // 索引值
                                             // 2. 如果是结构体指针，尝试重载操作符
    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret) // 重载成功
            return 0;

        if (-404 != ret) { // -404表示未找到重载，是正常情况
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 3. 检查索引类型
    if (!variable_integer(v1)) {
        loge("array index should be an interger\n");
        return -1;
    }

    int nb_pointers = 0;
    // 4. 根据数组类型确定结果指针级数
    if (v0->nb_dimentions > 0) {
        // 多维数组或定长数组
        if (v0->dimentions[0].num < 0 && !v0->dimentions[0].vla) {
            loge("\n");
            return -1;
        }

        nb_pointers = v0->nb_pointers;
        // 5. 常量索引边界检查
        if (variable_const(v1)) {
            if (v1->data.i < 0) {
                loge("array index '%s' < 0, real: %d, file: %s, line: %d\n",
                     v1->w->text->data, v1->data.i, v1->w->file->data, v1->w->line);
                return -1;
            }
            // 检查数组越界
            if (v1->data.i >= v0->dimentions[0].num && !v0->dimentions[0].vla) {
                if (!v0->member_flag) {
                    // 普通数组越界错误
                    loge("array index '%s' >= size %d, real: %d, file: %s, line: %d\n",
                         v1->w->text->data, v0->dimentions[0].num, v1->data.i, v1->w->file->data, v1->w->line);
                    return -1;
                }
                // 结构体末尾的零长度数组，警告但允许
                logw("array index '%s' >= size %d, real: %d, confirm it for a zero-array end of a struct? file: %s, line: %d\n",
                     v1->w->text->data, v0->dimentions[0].num, v1->data.i, v1->w->file->data, v1->w->line);
            }
        }
    } else if (0 == v0->nb_dimentions && v0->nb_pointers > 0) {
        // 指针运算：指针级数减1
        nb_pointers = v0->nb_pointers - 1;
    } else {
        loge("index out, v0: %s, v0->nb_dimentions: %d, v0->nb_pointers: %d, v0->arg_flag: %d\n",
             v0->w->text->data, v0->nb_dimentions, v0->nb_pointers, v0->arg_flag);
        return -1;
    }
    // 6. 查找元素类型
    type_t *t = NULL;
    ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;
    // 7. 创建结果变量
    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 0, nb_pointers, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    r->member_flag = v0->member_flag;
    // 8. 复制剩余的数组维度（对于多维数组）
    int i;
    for (i = 1; i < v0->nb_dimentions; i++) {
        expr_t *vla = NULL;

        if (v0->dimentions[i].vla) {
            vla = expr_clone(v0->dimentions[i].vla); // 克隆VLA表达式
            if (!vla) {
                variable_free(r);
                return -ENOMEM;
            }
        }

        variable_add_array_dimention(r, v0->dimentions[i].num, vla);
    }

    *d->pret = r; // 设置结果
    return 0;
}

/**
 * 处理代码块的语义分析（{}内的语句序列）
 *
 * @param ast AST上下文
 * @param nodes 代码块中的节点数组
 * @param nb_nodes 节点数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_block(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (0 == nb_nodes) // 空代码块
        return 0;

    handler_data_t *d = data;
    block_t *up = ast->current_block; // 保存当前作用域

    variable_t **pret;
    node_t *node;

    int ret;
    int i;
    // 1. 设置新的当前作用域
    ast->current_block = (block_t *)(nodes[0]->parent);
    // 2. 处理代码块中的所有语句
    for (i = 0; i < nb_nodes; i++) {
        node = nodes[i];

        if (type_is_var(node->type)) // 变量声明，跳过
            continue;

        if (FUNCTION == node->type) {
            // 函数调用
            pret = d->pret;
            ret = __op_semantic_call(ast, (function_t *)node, data);
            d->pret = pret;
        } else
            // 其他操作符
            ret = _op_semantic_node(ast, node, d);

        if (ret < 0) {
            ast->current_block = up; // 恢复作用域
            return -1;
        }
    }

    ast->current_block = up; // 恢复作用域
    return 0;
}

/**
 * 处理return语句的语义分析
 *
 * @param ast AST上下文
 * @param nodes 返回值节点数组
 * @param nb_nodes 返回值数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_return(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;
    // 1. 查找当前函数
    function_t *f = (function_t *)ast->current_block;

    while (f && FUNCTION != f->node.type)
        f = (function_t *)f->node.parent;

    if (!f) {
        loge("\n");
        return -1;
    }
    // 2. 检查返回值数量
    if (nb_nodes > f->rets->size) {
        loge("\n");
        return -1;
    }

    int i;
    for (i = 0; i < nb_nodes; i++) {
        assert(nodes);

        variable_t *fret = f->rets->data[i]; // 函数返回类型
        variable_t *r = NULL;
        expr_t *e = nodes[i]; // 返回表达式
                              // 3. 检查void函数
        if (VAR_VOID == fret->type && 0 == fret->nb_pointers) {
            loge("void function needs no return value, file: %s, line: %d\n", e->parent->w->file->data, e->parent->w->line);
            return -1;
        }
        // 4. 计算返回表达式
        if (_expr_calculate(ast, e, &r) < 0) {
            loge("\n");
            return -1;
        }

        int same = variable_same_type(r, fret); // 检查类型匹配

        variable_free(r);
        r = NULL;

        if (!same) {
            // 5. 添加类型转换
            int ret = _semantic_add_type_cast(ast, &(e->nodes[0]), fret, e->nodes[0]);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
        }
    }

    return 0;
}

/**
 * 处理break语句的语义分析
 * 检查break是否在合法的循环或switch语句中
 *
 * @param ast AST上下文
 * @param nodes 节点数组（break没有操作数）
 * @param nb_nodes 节点数量
 * @param data 处理程序数据
 * @return 成功返回0，失败返回错误码
 */
static int _op_semantic_break(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;
    // 1. 向上查找包含break的循环或switch语句
    node_t *n = (node_t *)ast->current_block;

    while (n
           && OP_WHILE != n->type
           && OP_SWITCH != n->type
           && OP_DO != n->type
           && OP_FOR != n->type)
        n = n->parent;
    // 2. 检查是否找到合法的break目标
    if (!n) {
        loge("\n");
        return -1;
    }

    if (!n->parent) {
        loge("\n");
        return -1;
    }

    return 0;
}

/*
 * 功能: 处理 continue 语义检查
 * 参数:
 *   ast      - 当前 AST 上下文（包含当前块、符号表等）
 *   nodes    - 节点数组（未使用）
 *   nb_nodes - 节点个数（未使用）
 *   data     - handler_data_t 指针（未使用）
 * 返回值:
 *   0  成功（continue 在某个循环内部）
 *  -1  失败（在非循环上下文使用 continue）
 * 其他: 会在找不到所属循环时打印错误日志
 */
static int _op_semantic_continue(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 虽然赋值了，但在此函数中未使用，通常用于传递语义处理上下文
    handler_data_t *d = data;
    // 从当前块的 node（ast->current_block）开始向上查找父节点，
    // 直到找到一个类型为 OP_WHILE 或 OP_FOR 的循环节点，表示 continue 合法
    node_t *n = (node_t *)ast->current_block;
    // 向上遍历父链，直到遇到 while 或 for
    while (n && (OP_WHILE != n->type && OP_FOR != n->type)) {
        n = n->parent;
    }
    // 如果遍历完仍然没有找到循环，就报错（在不在循环内部使用 continue）
    if (!n) {
        loge("\n");
        return -1;
    }

    // 断言：此处应为循环节点（开发期保证）
    assert(OP_WHILE == n->type || OP_FOR == n->type);
    return 0;
}

/*
 * 功能: 处理 label（标签）语义（当前实现为空）
 * 说明: 可能保留给将来做更严格的标签语义检查或注册标签到符号表
 * 返回值:
 *   0 短路成功
 */
static int _op_semantic_label(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

/*
 * 功能: 处理 goto 语义检查 —— 检查目标标签是否存在
 * 参数:
 *   nodes[0] 应当是 LABEL 类型节点（goto 的目标）
 * 返回值:
 *   0 成功（标签存在）
 *  -1 失败（标签未找到或其它错误）
 */
static int _op_semantic_goto(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // goto 节点应有一个参数：标签
    assert(1 == nb_nodes);
    // handler 上下文（未在函数中使用）
    handler_data_t *d = data;

    node_t *nl = nodes[0];
    // 确保传入的是 LABEL 节点
    assert(LABEL == nl->type);
    // 从节点取出 label 结构
    label_t *l = nl->label;
    // label->w（词/标识符信息）应存在
    assert(l->w);
    // 在当前 block 向上查找目标标签（可能实现为在块的链表或符号表中查找）
    label_t *l2 = block_find_label(ast->current_block, l->w->text->data);
    if (!l2) {
        // 未找到标签，报错
        loge("label '%s' not found\n", l->w->text->data);
        return -1;
    }
    // 找到则成功（实际可能需要进一步合法性检查）
    return 0;
}

/*
 * 功能: 处理 if 语义
 * 参数:
 *   nodes[0] : condition 表达式 (expr_t*)
 *   nodes[1..] : if-body 的若干节点（语句/块）
 * 要点:
 *   - 计算条件表达式并检查其为整型（或可作为真值使用）
 *   - 递归检查 if-body 中的每个节点的语义
 * 返回值:
 *   0 成功
 *  -1 失败（表达式检查或某个子节点语义检查失败）
 */
static int _op_semantic_if(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (nb_nodes < 2) {
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0]; // 条件表达式

    assert(OP_EXPR == e->type); // 条件必须是表达式类型
    // 计算表达式的常量值或静态值（_expr_calculate 可能用于常量折叠 / 运行时类型推断）
    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }
    // 检查表达式计算结果存在且是整数型（可作为 if 的条件）
    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }
    variable_free(r); // 释放临时计算结果
    r = NULL;
    // 对 if 的主体（从 nodes[1] 开始）逐个执行语义检查
    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _op_semantic_node(ast, nodes[i], d);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

/*
 * 功能: 处理 do ... while 语义
 * 参数:
 *   nodes[0] : do-body 的节点（语句或块）
 *   nodes[1] : while 的条件表达式（expr_t）
 * 要点:
 *   - 先检查 do-body 的语义
 *   - 然后计算并验证条件表达式为整数（真值测试）
 * 返回值:
 *   0 成功
 *  -1 失败
 */
static int _op_semantic_do(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // do/while 期望正好两个节点

    handler_data_t *d = data;
    variable_t *r = NULL;
    node_t *node = nodes[0]; // do-body（执行一次）
    expr_t *e = nodes[1];    // while 条件表达式

    assert(OP_EXPR == e->type);
    // 先对 do-body 的语义做检查（do-body 内可能包含 break/continue/label 等）
    int ret = _op_semantic_node(ast, node, d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }
    // 计算 while 条件表达式
    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }
    // 条件必须存在且为整数（能够用于真值判断）
    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }
    // 释放临时变量
    variable_free(r);
    r = NULL;

    return 0;
}

/*
 * 功能: 处理 while 语义
 * 参数:
 *   nodes[0] : 条件表达式（expr_t）
 *   nodes[1] : 可选的循环体节点（当 nb_nodes == 2 时存在）
 * 要点:
 *   - 计算条件表达式，确保为整数
 *   - 若存在循环体，则对其进行语义检查（递归）
 * 返回值:
 *   0 成功
 *  -1 失败
 */
static int _op_semantic_while(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // while 可以只有条件（例如空体或编译器内部中间结构）
    assert(2 == nb_nodes || 1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0]; // 条件表达式

    assert(OP_EXPR == e->type);
    // 计算条件表达式的值（静态/常量计算或类型检查）
    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }
    // 条件必须能够被解释为整数
    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }
    variable_free(r);
    r = NULL;

    // 如果有循环体，则递归检查循环体内语义
    if (2 == nb_nodes) {
        int ret = _op_semantic_node(ast, nodes[1], d);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

/*
 * 函数名: __switch_for_string
 * 功能:
 *   专门处理 switch-case 中 case 为字符串常量的情形（通过把字符串比较转换为 strcmp() 调用来实现）
 * 参数:
 *   ast    - AST 上下文
 *   parent - switch 的顶层节点（case 所在的父节点）
 *   child  - 当前 case 的节点（将被改写/替换其 nodes[0]）
 *   e      - 被 switch 的表达式（可能为要比较的目标表达式）
 *   e1     - case 分支中的常量字符串表达式（右操作数）
 *   d      - handler 数据，包含临时结果指针等
 * 返回值:
 *   >=0 成功（通常返回 _semantic_add_call 的返回值）
 *   <0  错误代码（找不到 strcmp、内存分配失败或语义检查失败）
 *
 * 处理逻辑概述:
 *   - 查找并引用 strcmp 函数定义（用于字符串比较）
 *   - 克隆被比较的表达式 e（产生 e2），并对其求值 / 类型检查（_expr_calculate）
 *   - 构造一个新的表达式 e3，包含 (e2, e1) 两个子节点，表示 strcmp(e2, e1)
 *   - 将 e3 包装进 e4（作为调用表达式节点），并把 child->nodes[0] 指向 e4
 *   - 将 e3->result 的地址赋给 d->pret（用于获取调用的返回值）
 *   - 最后调用 _semantic_add_call 来处理函数调用的语义与生成
 */
static int __switch_for_string(ast_t *ast, node_t *parent, node_t *child, expr_t *e, expr_t *e1, handler_data_t *d) {
    function_t *f = NULL;
    variable_t *v = NULL;
    expr_t *e2;
    expr_t *e3;
    expr_t *e4;
    // 在 AST 或符号表中查找 strcmp 函数（用于字符串比较）
    int ret = ast_find_function(&f, ast, "strcmp");
    if (ret < 0)
        return ret;
    // 如果未找到 strcmp 的函数描述（符号表中缺失），报错并返回
    if (!f) {
        loge("can't find function 'strcmp()' for compare const string, file: %s, line: %d\n",
             parent->w->file->data, parent->w->line);
        return -1;
    }
    // 克隆原始表达式 e（被 switch 的表达式），产生 e2（在后续作为 strcmp 的第一个参数）
    e2 = expr_clone(e);

    // 注意：代码中紧接着检查 e1 是否为 NULL，这里顺序上看起来有点奇怪（通常应先检查 e2 再检查 e1）；
    //       但照现有代码逻辑：如果 e1 为 NULL 则直接返回 ENOMEM（可能意图为参数缺失或内存错误）
    if (!e1)
        return -ENOMEM;
    // 对克隆后的表达式 e2 做一次计算/验证（确保表达式可以被处理，得到临时 variable v）
    if (_expr_calculate(ast, e2, &v) < 0) {
        // 如果失败，释放 e2 并返回错误
        expr_free(e2);
        return -1;
    }
    // 释放由 _expr_calculate 返回的临时 variable
    variable_free(v);
    v = NULL;
    // 构造 e3：这是一个将作为函数调用参数容器的表达式节点（或等价的中间节点）
    e3 = expr_alloc();
    if (!e3) {
        expr_free(e2); // 分配失败要释放之前分配的资源
        return -ENOMEM;
    }
    // 将 e2（克隆并计算过的目标表达式）作为 e3 的第一个子节点
    ret = node_add_child(e3, e2);
    if (ret < 0) {
        expr_free(e2);
        expr_free(e3);
        return ret;
    }
    e2 = NULL; // 已经转移给了 e3，避免重复释放
               // 再把 e1（case 的字符串表达式）作为 e3 的第二个子节点
    ret = node_add_child(e3, e1);
    if (ret < 0) {
        expr_free(e3);
        return ret;
    }

    // 注意：此时 e1 的生命周期由 e3 管理（取决于 node_add_child 的实现）

    // 将 child->nodes[0] 清空（先移除原来的内容，准备替换为 strcmp 调用表达式）
    child->nodes[0] = NULL;
    // 为函数调用再包装一层表达式 e4（可能是函数调用节点的容器）
    e4 = expr_alloc();
    if (!e4) {
        expr_free(e3);
        return -ENOMEM;
    }
    // 把 e3 作为 e4 的子节点（即 e4 包含 e3，e3 是传递给 _semantic_add_call 的调用参数结构）
    ret = node_add_child(e4, e3);
    if (ret < 0) {
        expr_free(e3);
        expr_free(e4);
        return ret;
    }
    // 把 child->nodes[0] 指向新构造的表达式 e4（替换原来的 case 表达式，使其变为 strcmp 调用）
    child->nodes[0] = e4;
    e4->parent = child; // 设置父指针，维持 AST 正确性
                        // d->pret 指向 e3->result（将来 _semantic_add_call 或其他处通过 d->pret 获取 strcmp 的返回值）
    d->pret = &e3->result;
    // 最后把 e3 的节点（参数）传给语义层来处理此函数调用，
    // _semantic_add_call 很可能会把 f（strcmp）与 e3（参数）结合进行类型检查、调用语义生成等。
    return _semantic_add_call(ast, e3->nodes, e3->nb_nodes, d, f);
}

// 处理 switch 语句的语义分析
static int _op_semantic_switch(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    // 确保有两个节点：表达式和代码块
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t **pret = d->pret;       // 保存之前的返回值指针
    variable_t *v0 = NULL;             // switch 表达式的结果
    variable_t *v1 = NULL;             // case 表达式的结果
    block_t *tmp = ast->current_block; // 保存当前代码块
    expr_t *e = nodes[0];              // switch 的表达式节点
    node_t *b = nodes[1];              // switch 的代码块节点
    node_t *parent = nodes[0]->parent; // 父节点
    node_t *child;                     // 子节点
    expr_t *e1;                        // case 表达式

    // 验证节点类型
    assert(OP_EXPR == e->type);
    assert(OP_BLOCK == b->type);
    // 计算 switch 表达式的结果
    if (_expr_calculate(ast, e, &v0) < 0)
        return -1;
    // 检查 switch 表达式结果类型：必须是整数或字符串
    if (!variable_integer(v0) && !variable_string(v0)) {
        loge("result of switch expr should be an integer or string, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
        variable_free(v0);
        return -1;
    }
    // 设置当前代码块为 switch 的代码块
    ast->current_block = (block_t *)b;

    int ret = -1;
    int i;
    // 遍历 switch 代码块中的所有节点
    for (i = 0; i < b->nb_nodes; i++) {
        child = b->nodes[i];

        if (OP_CASE == child->type) { // 处理 case 语句
            assert(1 == child->nb_nodes);

            e1 = child->nodes[0]; // case 的表达式

            assert(OP_EXPR == e1->type);
            // 计算 case 表达式的结果
            ret = _expr_calculate(ast, e1, &v1);
            if (ret < 0) {
                variable_free(v0);
                return ret;
            }
            // 检查 case 表达式必须是常量整数或常量字符串
            if (!variable_const_integer(v1) && !variable_const_string(v1)) {
                ret = -1;
                loge("result of case expr should be const integer or const string, file: %s, line: %d\n", child->w->file->data, child->w->line);
                goto error;
            }
            // 检查 switch 表达式和 case 表达式类型是否匹配
            if (!variable_type_like(v0, v1)) {
                // 尝试类型转换
                if (type_cast_check(ast, v0, v1) < 0) {
                    ret = -1;
                    loge("type of switch's expr is NOT same to the case's, file: %s, line: %d\n", child->w->file->data, child->w->line);
                    goto error;
                }
                // 添加类型转换
                ret = _semantic_add_type_cast(ast, &(e1->nodes[0]), v0, e1->nodes[0]);
                if (ret < 0)
                    goto error;
            }
            // 如果是字符串类型的 switch，特殊处理
            if (variable_const_string(v1)) {
                ret = __switch_for_string(ast, parent, child, e, e1, d);
                if (ret < 0)
                    goto error;
            }

            variable_free(v1);
            v1 = NULL;

        } else { // 处理其他类型的节点（如 default 语句）
            ret = _op_semantic_node(ast, child, d);
            if (ret < 0) {
                variable_free(v0);
                return -1;
            }
        }
    }
    // 恢复之前的代码块
    ast->current_block = tmp;

    variable_free(v0);

    d->pret = pret;
    return 0;

error:
    variable_free(v0);
    variable_free(v1);
    d->pret = pret;
    return ret;
}

// 处理 case 语句的语义分析（目前未实现）
static int _op_semantic_case(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    loge("\n");
    return -1;
}

// 处理 default 语句的语义分析
static int _op_semantic_default(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0; // default 语句不需要特殊处理
}

// 处理可变长度数组分配的语义分析
static int _op_semantic_vla_alloc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes); // 验证参数数量

    logw("\n"); // 输出警告日志
    return 0;   // 暂未实现
}

// 处理 for 循环的语义分析
static int _op_semantic_for(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes); // 四个部分：初始化、条件、迭代、循环体

    handler_data_t *d = data;
    int ret = 0;

    // 处理初始化表达式
    if (nodes[0]) {
        ret = _op_semantic_node(ast, nodes[0], d);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }
    // 处理条件表达式
    expr_t *e = nodes[1];
    if (e) {
        assert(OP_EXPR == e->type);

        variable_t *r = NULL;
        // 计算条件表达式
        if (_expr_calculate(ast, e, &r) < 0) {
            loge("\n");
            return -1;
        }
        // 检查条件表达式必须是整数类型
        if (!r || !variable_integer(r)) {
            loge("\n");
            return -1;
        }
        variable_free(r);
        r = NULL;
    }
    // 处理迭代表达式和循环体
    int i;
    for (i = 2; i < nb_nodes; i++) {
        if (!nodes[i])
            continue;

        ret = _op_semantic_node(ast, nodes[i], d);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    return 0;
}

// 内部函数：处理函数调用的语义分析
static int __op_semantic_call(ast_t *ast, function_t *f, void *data) {
    logd("f: %p, f->node->w: %s\n", f, f->node.w->text->data);

    handler_data_t *d = data;
    block_t *tmp = ast->current_block;

    // 改变当前代码块为函数体
    ast->current_block = (block_t *)f;
    // 分析函数体
    if (_op_semantic_block(ast, f->node.nodes, f->node.nb_nodes, d) < 0)
        return -1;

    ast->current_block = tmp; // 恢复之前的代码块
    return 0;
}

// 处理函数调用的语义分析
static int _op_semantic_call(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 0);

    handler_data_t *d = data;
    variable_t **pret = d->pret;
    variable_t *v0; // 函数指针变量
    variable_t *v1; // 参数变量
    function_t *f;  // 函数定义
    node_t *parent = nodes[0]->parent;

    // 计算函数表达式（获取函数指针）
    d->pret = &nodes[0]->result;
    int ret = _expr_calculate_internal(ast, nodes[0], d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    v0 = _operand_get(nodes[0]);

    // 验证是否是函数指针
    if (FUNCTION_PTR != v0->type || !v0->func_ptr) {
        loge("\n");
        return -1;
    }

    f = v0->func_ptr;

    // 检查参数数量
    if (f->vargs_flag) { // 可变参数函数
        if (f->argv->size > nb_nodes - 1) {
            loge("number of args pass to '%s()' at least needs %d, real: %d, file: %s, line: %d\n",
                 f->node.w->text->data, f->argv->size, nb_nodes - 1, parent->w->file->data, parent->w->line);
            return -1;
        }
    } else if (f->argv->size != nb_nodes - 1) { // 固定参数函数
        loge("number of args pass to '%s()' needs %d, real: %d, file: %s, line: %d\n",
             f->node.w->text->data, f->argv->size, nb_nodes - 1, parent->w->file->data, parent->w->line);
        return -1;
    }

    // 检查每个参数的类型匹配
    int i;
    for (i = 0; i < f->argv->size; i++) {
        v0 = f->argv->data[i]; // 函数定义的参数类型

        v1 = _operand_get(nodes[i + 1]); // 实际传递的参数
                                         // 检查 void 类型必须是指针
        if (VAR_VOID == v1->type && 0 == v1->nb_pointers) {
            loge("void var should be a pointer\n");
            return -1;
        }
        // 如果类型匹配则继续
        if (variable_type_like(v0, v1))
            continue;
        // 检查类型转换是否可行
        if (type_cast_check(ast, v0, v1) < 0) {
            loge("f: %s, arg var not same type, i: %d, line: %d\n",
                 f->node.w->text->data, i, parent->debug_w->line);
            return -1;
        }
        // 添加类型转换
        ret = _semantic_add_type_cast(ast, &nodes[i + 1], v0, nodes[i + 1]);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }
    // 处理函数返回值
    return _semantic_add_call_rets(ast, parent, d, f);
}

// 处理表达式的语义分析
static int _op_semantic_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    node_t *n = nodes[0];
    // 清理之前的结果
    if (n->result) {
        variable_free(n->result);
        n->result = 0;
    }

    variable_t **pret = d->pret;
    // 计算表达式结果
    d->pret = &n->result;
    int ret = _expr_calculate_internal(ast, n, d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }
    // 设置返回值
    if (type_is_var(n->type)) { // 变量类型
        assert(n->var);
        if (d->pret)
            *d->pret = variable_ref(n->var);

    } else { // 表达式结果
        if (n->result && d->pret)
            *d->pret = variable_ref(n->result);
    }

    return 0;
}

// 处理负号操作的语义分析
static int _op_semantic_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]); // 获取操作数

    assert(v0);
    // 检查是否是结构体指针，尝试重载操作
    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret) // 重载成功
            return 0;

        if (-404 != ret) { // 重载出错（-404 表示未找到重载）
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 检查操作数类型：整数或浮点数
    if (variable_integer(v0) || variable_float(v0)) {
        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
        // 创建结果变量
        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

// 处理自增操作的语义分析
static int _op_semantic_inc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    // 检查操作数不能是常量
    if (variable_const(v0) || variable_const_string(v0)) {
        loge("line: %d\n", parent->w->line);
        return -1;
    }
    // 检查是否是结构体指针，尝试重载操作
    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 检查操作数必须是整数类型
    if (variable_integer(v0)) {
        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
        // 创建结果变量
        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

/*
 * _op_semantic_inc_post / _op_semantic_dec / _op_semantic_dec_post
 *
 * 功能:
 *   这三个函数都直接将工作委派给 _op_semantic_inc。
 *   意味着：语义检查/处理逻辑在 _op_semantic_inc 中实现，
 *   并且该实现应能根据上下文处理前置/后置、自增/自减的差异。
 *
 * 注意:
 *   代码里 dec/dec_post 调用 inc 的实现是常见的简化（复用逻辑）做法，
 *   但要确保 _op_semantic_inc 能正确区分增/减（例如通过 node/op 标志等）。
 */
static int _op_semantic_inc_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

static int _op_semantic_dec(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

static int _op_semantic_dec_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

/*
 * _op_semantic_positive
 *
 * 功能:
 *   处理一元正号运算（+expr）。通常 +expr 是无操作（no-op），
 *   这里通过把子节点的数据“上提”到父节点并释放子节点，实现消除一元+节点。
 *
 * 参数:
 *   nodes[0]：子节点（表达式）
 *
 * 实现要点:
 *   - 断言只接收 1 个节点
 *   - 将 parent 节点的数据清除并移动 child 的数据到 parent（node_move_data）
 *   - 释放 child 节点
 *
 * 返回:
 *   0 成功
 */
static int _op_semantic_positive(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    node_t *child = nodes[0];
    node_t *parent = nodes[0]->parent;
    // 断开外部对 nodes[0] 的引用（避免后续重复使用）
    nodes[0] = NULL;
    // 清理 parent 原有的数据（lex/var/类型信息等）
    node_free_data(parent);
    // 把 child 的数据整体移动到 parent（转移所有权）
    node_move_data(parent, child);
    // 释放 child 节点本身（其数据已被移动）
    node_free(child);

    return 0;
}

/*
 * _op_semantic_dereference
 *
 * 功能:
 *   处理解引用操作（*p），返回解引用后的变量类型信息。
 *
 * 流程:
 *   - 通过 _operand_get 获取被解引用表达式的变量描述 v0
 *   - 校验 v0 必须是指针（nb_pointers > 0）
 *   - 查找 v0 的基础 type_t（ast_find_type_type），
 *   - 根据基础类型和指针层级（nb_pointers - 1）分配一个新的 variable 描述 r
 *   - 将结果写入到 d->pret 指向的位置（这是调用方期望接收返回变量的位置）
 *
 * 注意:
 *   - 当 nb_pointers == 1 时，解引用后的 nb_pointers 变为 0（即得到实际对象类型）
 *   - 这里把 const_flag 直接传为 0（代码中使用 VAR_ALLOC_BY_TYPE(..., 0, v0->nb_pointers -1, ...)）
 *     —— 视设计可能需要考虑 const 传播，但当前实现选择清除 const（或由 VAR_ALLOC_BY_TYPE 的语义处理）
 *
 * 返回:
 *   0 成功，-EINVAL/ -ENOMEM / 其它 ast_find_type_type 返回的错误码 表示失败
 */
static int _op_semantic_dereference(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    // 获取被解引用表达式的变量信息
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (v0->nb_pointers <= 0) {
        loge("var is not a pointer\n");
        return -EINVAL; // 不能对非指针做解引用
    }

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type); // 查找变量的基础类型描述
    if (ret < 0)
        return ret;

    // 为解引用结果分配 variable 描述：
    // - 使用同一个基础 type_t
    // - 指针层数减 1
    // - 这里 const_flag 使用 0（注意：按需要可调整）
    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 0, v0->nb_pointers - 1, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    *d->pret = r; // 把结果交给调用者（写入 d->pret 指向处）
    return 0;
}

/*
 * _op_semantic_address_of
 *
 * 功能:
 *   处理取地址操作（&expr）。
 *
 * 流程:
 *   - 取得被取地址表达式的 variable 描述 v0
 *   - 禁止对常量字面量取地址（const_literal_flag）
 *   - 查找基础类型 t，然后分配一个 nb_pointers + 1 的变量描述 r（表示“指向该对象的指针”）
 *   - 保留原始的 const 标志（v0->const_flag）作为 r 的 const_flag
 *   - 把 r 写入 d->pret
 *
 * 注意:
 *   - 取地址后指针层级增加
 *   - 禁止对立即量取地址是合理的（例如对字符串字面量/整数常量取地址通常非法或未定义）
 *
 * 返回:
 *   0 成功，错误码表示失败
 */
static int _op_semantic_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (v0->const_literal_flag) {
        loge("\n");
        return -EINVAL; // 对字面量取地址被视为错误
    }

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    // 分配表示指针的新 variable，指针层级 +1，保留 const_flag
    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers + 1, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    *d->pret = r;
    return 0;
}

/*
 * _op_semantic_type_cast
 *
 * 功能:
 *   处理强制类型转换语义（dst_type(expr) 或类似语法）。
 *
 * 参数约定:
 *   nodes[0] : 目标类型（通常是一个类型描述或类型字面）
 *   nodes[1] : 源表达式（要被转换的表达式）
 *
 * 实现要点:
 *   - 通过 _operand_get 获得 dst 与 src 的变量描述（dst 描述通常代表目标类型）
 *   - 如果 handler_data_t 提供了 d->pret（调用方期望接收结果的位置），
 *     就将 dst 的引用（variable_ref(dst)）赋给 d->pret
 *
 * 说明:
 *   - 这里并没有真正执行“值”的转换逻辑（例如位宽缩放、符号扩展等），
 *     该函数把结果类型固定为 dst，具体的数据转换可能在后续 codegen/ops 层实现。
 *
 * 返回:
 *   0 成功
 */
static int _op_semantic_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *dst = _operand_get(nodes[0]); // 目标类型的变量描述
    variable_t *src = _operand_get(nodes[1]); // 源表达式的变量描述

    assert(dst);
    assert(src);

    if (d->pret) {
        // 将 dst 的引用传回（variable_ref 通常表示增加引用或复制描述）
        *d->pret = variable_ref(dst);
    }

    return 0;
}

/*
 * _op_semantic_container
 *
 * 功能:
 *   实现类似 container_of / offsetof 语义的处理：
 *   给定一个指向结构体成员的指针，计算出包含该成员的结构体的指针。
 *
 * 典型调用形式（概念层面）:
 *   container(ptr_to_member, container_type, member_info)
 *
 * 参数含义（根据变量名与用法推断）:
 *   nodes[0] = e      : 表示原始的 member 指针表达式（应为表达式节点）
 *   nodes[1] = node1  : 表示容器类型（或容器类型的变量描述）
 *   nodes[2] = node2  : 表示 member 的描述（包含 member->offset 等信息）
 *
 * 处理流程:
 *   1) 检查前置条件：v0（member ptr）是指针，v1 是结构体指针相关，v2 带 member_flag
 *   2) 把 parent 转换成 OP_TYPE_CAST，parent->result 设置为 container 类型（variable_ref(v1)）
 *      并把 parent 的子节点调整为 [node1, e]
 *   3) 如果 member 的 offset 为 0（成员在结构体偏移为 0），则直接返回（只需做类型转换）
 *   4) 否则，需要通过指针算术恢复容器基址：
 *         - 首先把 member 指针（e->nodes[0]）转换为 u8*（字节指针 u8）——方便做按字节偏移
 *         - 构造一个常量 offset（uintptr 类型），值为 member 的偏移量 v2->offset
 *         - 将 node2 替换为该常量 offset，并把它加入到 e 的子节点
 *         - 把 e 的操作设为 OP_SUB（即 e = (u8*)member_ptr - offset）
 *         - 最后 parent（先前设置为类型转换）会把结果 cast 回 container 指针类型
 *
 * 举例（伪代码）:
 *   parent = (container_type *) ((u8 *)member_ptr - offsetof(container_type, member));
 *
 * 内存/所有权说明:
 *   - _semantic_add_type_cast 会修改 e->nodes[0] 的节点（将其 cast 为 u8*）
 *   - node2 原来是成员的变量描述（包含 offset），如果 offset != 0 则会被替换为常量 offset
 *   - 在替换前需释放 node2->var 与 e->result，随后把 offset 绑定到 node2->var
 *
 * 返回:
 *   0 成功，<0 表示错误（包括内存分配失败或 _semantic_add_type_cast 的错误）
 */
static int _op_semantic_container(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(3 == nb_nodes);

    handler_data_t *d = data;

    node_t *e = nodes[0];
    node_t *node1 = nodes[1];
    node_t *node2 = nodes[2];
    node_t *parent = e->parent;

    variable_t *v0 = _operand_get(e);     // member 指针表达式的变量描述
    variable_t *v1 = _operand_get(node1); // container（结构体）类型的变量描述
    variable_t *v2 = _operand_get(node2); // 成员描述（含 offset）
    variable_t *u8;
    variable_t *offset;
    type_t *t;

    assert(v0);
    assert(OP_EXPR == e->type);
    // 确保调用方期待 parent->result（d->pret 指向 parent->result）
    assert(d->pret == &parent->result);

    // 一些语义约束（依据代码推断）
    assert(v0->nb_pointers > 0); // 被传入的 member 是指针
    assert(v1->nb_pointers > 0); // container 相关也应是指针（容器的指针）
    assert(v1->type >= STRUCT);  // v1 的类型应为结构体类型或以后枚举
    assert(v2->member_flag);     // node2 标记为成员信息

    // 将 parent 变为类型转换节点：parent = (container_type *) ...
    parent->type = OP_TYPE_CAST;
    parent->result = variable_ref(v1); // cast 后的结果类型（container 指针）
    parent->nodes[0] = node1;          // 第一个子节点是 container 类型描述
    parent->nodes[1] = e;              // 第二个子节点是 member 指针表达式
    parent->nodes[2] = NULL;
    parent->nb_nodes = 2;
    parent->op = find_base_operator_by_type(OP_TYPE_CAST);

    // 如果成员偏移为 0，则无需指针算术，直接完成类型转换
    if (0 == v2->offset) {
        node_free(node2); // 释放 node2（不会再需要）
        return 0;
    }

    // 否则需要通过按字节偏移（uintptr/u8）来恢复容器基址

    // 1) 准备 u8* 类型的变量描述（用于把 member_ptr 转为 u8*）
    t = block_find_type_type(ast->current_block, VAR_U8);
    u8 = VAR_ALLOC_BY_TYPE(NULL, t, 0, 1, NULL); // u8 指针（nb_pointers = 1）
    // 注意：这里没有检查 u8 是否为 NULL（如果 VAR_ALLOC_BY_TYPE 失败需考虑错误处理）
    // 2) 把 e->nodes[0] 强制转换为 u8*（即在语义层插入类型转换）
    int ret = _semantic_add_type_cast(ast, &e->nodes[0], u8, e->nodes[0]);
    if (ret < 0)
        return ret;
    // 3) 创建一个常量 offset（类型为 uintptr），值为 v2->offset
    t = block_find_type_type(ast->current_block, VAR_UINTPTR);
    offset = VAR_ALLOC_BY_TYPE(v2->w, t, 1, 0, NULL); // const 的 uintptr

    offset->data.u64 = v2->offset;
    // 4) 把 node2 的原有 variable/free 掉并替换为常量 offset
    assert(!node2->result);

    variable_free(node2->var); // 释放 node2 原本持有的 var
    variable_free(e->result);  // 释放 e（member_ptr）原本的 result（因为我们会改写 e）

    node2->type = offset->type; // 把 node2 变为常量节点
    node2->var = offset;

    // 5) 把常量 offset 加入到 e 的子节点，构成 (u8*)member_ptr - offset
    node_add_child(e, node2);

    // 6) 将 e 改为减法操作，结果类型使用 u8（即字节指针）
    e->type = OP_SUB;
    e->result = u8;
    e->op = find_base_operator_by_type(OP_SUB);

    return 0;
}

/*
 * _op_semantic_sizeof
 *
 * 功能:
 *   处理 sizeof(expr) 或 sizeof(type) 语义：计算所占字节数并返回一个 const 的整型字面（使用 VAR_INTPTR 类型）
 *
 * 流程:
 *   - 取得 operand 的 variable 描述 v0
 *   - 用 variable_size(v0) 获取大小（以字节为单位）
 *   - 分配一个 VAR_INTPTR 类型的常量 variable r，并把 size 填入 r->data.i64，设置 const_flag
 *   - 用 XCHG(r->w, parent->w) 把词位置信息/语法词绑定给 r（保留源位置信息）
 *   - 清理 parent 原有数据并把 parent 变为常量节点（parent->var = r）
 *
 * 注意:
 *   - variable_size 的行为（是否对数组/指针/结构体返回不同值）依赖于其实现
 *   - 返回值类型选为 VAR_INTPTR（目标平台指针宽度整型），这在 sizeof 返回值的语义设计中是常见选项
 *
 * 返回:
 *   0 成功，负数表示失败
 */
static int _op_semantic_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(OP_EXPR == nodes[0]->type);
    assert(d->pret == &parent->result); // 期望 d->pret 指向 parent->result

    int size = variable_size(v0);
    if (size < 0)
        return size;

    type_t *t = block_find_type_type(ast->current_block, VAR_INTPTR);

    lex_word_t *w = parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 1, 0, NULL); // 常量类型（const_flag=1）
    if (!r)
        return -ENOMEM;

    r->data.i64 = size;
    r->const_flag = 1;

    // 交换词信息：保持 source location 在 parent/return 上的可追溯性
    XCHG(r->w, parent->w);

    // 清除 parent 上原来的数据并把 parent 设置为常量节点（持有 r）
    node_free_data(parent);
    parent->type = r->type;
    parent->var = r;

    return 0;
}

/*
 * _op_semantic_logic_not
 *
 * 功能:
 *   处理逻辑非操作（!expr）。
 *
 * 流程:
 *   1) 如果操作数是“结构体指针”的语义（variable_is_struct_pointer），
 *      优先尝试查找并执行重载操作（_semantic_do_overloaded）。
 *      - 若重载处理成功返回 0
 *      - 若返回 -404 表示没有找到重载（继续后续内置处理）
 *      - 若返回其他负值则视为错误并返回
 *   2) 若 v0 是整型（variable_integer），则创建一个整型变量作为结果
 *      - const_flag 的计算：只有在 v0 是常量且不是指针/数组时才视为 const
 *      - 结果类型使用 VAR_INT（通过 block_find_type_type 找到）
 *   3) 否则报错（不支持的操作数类型）
 *
 * 返回:
 *   0 成功，负值表示错误
 */
static int _op_semantic_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    // 如果是结构体指针类型，先尝试调用用户定义/重载逻辑
    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) { // -404 这里表示“未找到重载”，其它错误则上报
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    // 内置逻辑：操作数是整型时，!expr 的类型为整型（通常是 int）
    if (variable_integer(v0)) {
        // 如果 v0 是常量，且既不是指针也不是数组/维度，则结果也是常量
        int const_flag = v0->const_flag && 0 == v0->nb_pointers && 0 == v0->nb_dimentions;

        type_t *t = block_find_type_type(ast->current_block, VAR_INT);

        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, 0, NULL);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    // 其它类型未实现
    loge("v0: %d/%s\n", v0->w->line, v0->w->text->data);
    return -1;
}

/*
 * _op_semantic_bit_not
 *
 * 功能:
 *   处理按位取反操作（~expr）。
 *
 * 流程:
 *   1) 如果是结构体指针，尝试重载处理（同 logic_not）
 *   2) 决定结果类型:
 *       - 如果 operand 是指针或有维度（nb_pointers + nb_dimentions > 0），
 *         则结果类型选择 VAR_UINTPTR（无符号指针整型），表示对指针按位取反得到 uintptr 类型
 *       - 否则如果 operand 是整数类型（type_is_integer），则结果类型与 operand 相同
 *       - 其他类型视为不支持并报错
 *   3) 根据决定的类型分配 result variable，并保留 operand 的 const_flag
 *
 * 说明:
 *   - 保留 const_flag，说明 ~ 运算并不会使常量性丢失（取决于语言语义）
 *   - 对指针做 ~ 通常语义上少见，此处将其返回 uintptr 可视为内部实现选择
 *
 * 返回:
 *   0 成功，负值表示错误
 */
static int _op_semantic_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    // 先尝试结构体重载
    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    type_t *t;

    // 如果是指针或数组维度存在，就把结果定为 uintptr（按位操作在指针上以 uintptr 表示）
    if (v0->nb_pointers + v0->nb_dimentions > 0) {
        t = block_find_type_type(ast->current_block, VAR_UINTPTR);

    } else if (type_is_integer(v0->type)) {
        // 否则如果是整型，则保持相同整型（查找该整型的 type_t）
        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
    } else {
        // 其它类型不支持按位取反
        loge("\n");
        return -1;
    }

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, 0, NULL);
    if (!r)
        return -ENOMEM;

    *d->pret = r;
    return 0;
}

// 处理指针加法运算的语义分析（指针 + 整数）
static int _semantic_pointer_add(ast_t *ast, node_t *parent, node_t *pointer, node_t *index) {
    variable_t *r;
    variable_t *v = _operand_get(pointer); // 获取指针变量
    type_t *t = NULL;
    node_t *add;
    node_t *neg;

    // 查找指针指向的类型
    int ret = ast_find_type_type(&t, ast, v->type);
    if (ret < 0)
        return ret;

    // 创建数组索引操作节点（指针运算通常转换为数组索引）
    add = node_alloc(parent->w, OP_ARRAY_INDEX, NULL);
    if (!add)
        return -ENOMEM;

    // 获取指针的层级数
    int nb_pointers = variable_nb_pointers(v);

    // 创建结果变量：类型不变，指针层级减1（因为解引用）
    r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, nb_pointers - 1, v->func_ptr);
    if (!r) {
        node_free(add);
        return -ENOMEM;
    }
    r->local_flag = 1; // 标记为局部变量
    r->tmp_flag = 1;   // 标记为临时变量

    add->result = r; // 设置操作结果
    r = NULL;

    // 添加指针作为第一个操作数
    ret = node_add_child(add, pointer);
    if (ret < 0) {
        node_free(add);
        return ret;
    }

    // 添加索引作为第二个操作数
    ret = node_add_child(add, index);
    if (ret < 0) {
        pointer->parent = parent; // 恢复父节点指针

        add->nb_nodes = 0;
        node_free(add);
        return ret;
    }

    add->parent = parent;

    // 重构父节点：用新的数组索引节点替换原来的操作
    parent->nodes[0] = add;
    parent->nodes[1] = NULL;
    parent->nb_nodes = 1;

    // 如果是减法操作，需要处理负号
    if (OP_SUB == parent->type) {
        neg = node_alloc(parent->w, OP_NEG, NULL); // 创建负号节点
        if (!neg) {
            ret = -ENOMEM;
            goto error;
        }

        v = _operand_get(index); // 获取索引变量
                                 // 查找索引变量的类型
        ret = ast_find_type_type(&t, ast, v->type);
        if (ret < 0)
            goto error;
        // 创建临时变量存储负值
        r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, 0, NULL);
        if (!r) {
            node_free(neg);
            goto error;
        }
        r->local_flag = 1;
        r->tmp_flag = 1;

        neg->result = r;
        r = NULL;
        // 添加索引作为负号节点的操作数
        ret = node_add_child(neg, index);
        if (ret < 0) {
            node_free(neg);
            goto error;
        }
        // 用负号节点替换原来的索引
        add->nodes[1] = neg;
        neg->parent = add;
    }

    ret = 0;
error:
    // 将操作类型改为取地址操作
    parent->op = find_base_operator_by_type(OP_ADDRESS_OF);
    parent->type = OP_ADDRESS_OF;
    return ret;
}

// 处理指针复合赋值运算（+=, -=）
static int _semantic_pointer_add_assign(ast_t *ast, node_t *parent, node_t *pointer, node_t *index) {
    variable_t *v = _operand_get(pointer);
    variable_t *r = NULL;
    type_t *t = NULL;
    node_t *p2;
    node_t *add;

    // 查找指针指向的类型
    int ret = ast_find_type_type(&t, ast, v->type);
    if (ret < 0)
        return ret;

    // 根据操作类型创建加法或减法节点
    if (OP_ADD_ASSIGN == parent->type)
        add = node_alloc(parent->w, OP_ADD, NULL);
    else
        add = node_alloc(parent->w, OP_SUB, NULL);
    if (!add)
        return -ENOMEM;
    // 创建临时变量存储运算结果
    r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, variable_nb_pointers(v), v->func_ptr);
    if (!r) {
        node_free(add);
        return -ENOMEM;
    }
    r->local_flag = 1;
    r->tmp_flag = 1;

    add->result = r;
    r = NULL;
    // 克隆指针表达式（避免修改原始节点）
    p2 = expr_clone(pointer);
    if (!p2) {
        node_free(add);
        return -ENOMEM;
    }
    // 复制变量引用
    if (type_is_var(pointer->type))
        p2->var = variable_ref(pointer->var);

    else if (type_is_operator(pointer->type))
        p2->result = variable_ref(pointer->result);
    // 添加克隆的指针作为第一个操作数
    ret = node_add_child(add, p2);
    if (ret < 0) {
        node_free(p2);
        node_free(add);
        return ret;
    }
    // 添加索引作为第二个操作数
    ret = node_add_child(add, index);
    if (ret < 0) {
        node_free(add);
        return ret;
    }

    parent->nodes[1] = NULL;
    // 处理指针加法运算
    ret = _semantic_pointer_add(ast, add, p2, index);
    if (ret < 0) {
        node_free(add);
        return ret;
    }
    // 将加法节点设置为赋值操作的右值
    parent->nodes[1] = add;
    add->parent = parent;

    // 将操作类型改为普通赋值
    parent->op = find_base_operator_by_type(OP_ASSIGN);
    parent->type = OP_ASSIGN;
    return 0;
}

// 处理二元赋值运算（=, +=, -= 等）
static int _op_semantic_binary_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 左值和右值

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]); // 左值
    variable_t *v1 = _operand_get(nodes[1]); // 右值
    variable_t *v2 = NULL;
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    // 检查左值是否可修改（不能是常量或数组）
    if (v0->const_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;
    }
    // 检查是否是结构体指针，尝试重载操作
    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret) // 重载成功
            return 0;

        if (-404 != ret) { // 重载出错
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 处理整数或浮点数类型的赋值
    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            type_t *t = NULL;

            int nb_pointers0 = variable_nb_pointers(v0); // 左值指针层级
            int nb_pointers1 = variable_nb_pointers(v1); // 右值指针层级

            // 情况1：左值是指针
            if (nb_pointers0 > 0) {
                if (nb_pointers1 > 0) {
                    // 指针赋值给指针：检查类型是否相同
                    if (!variable_same_type(v0, v1)) {
                        loge("different type pointer, type: %d,%d, nb_pointers: %d,%d\n",
                             v0->type, v1->type, nb_pointers0, nb_pointers1);
                        return -EINVAL;
                    }

                } else if (!variable_integer(v1)) {
                    // 非整数赋值给指针：错误
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;
                } else {
                    // 整数赋值给指针：需要类型转换
                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v1->w, t, v1->const_flag, 0, NULL);
                    // 添加类型转换
                    int ret = _semantic_add_type_cast(ast, &nodes[1], v2, nodes[1]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }
                    // 处理指针复合赋值（+=, -=）
                    if (OP_ADD_ASSIGN == parent->type || OP_SUB_ASSIGN == parent->type) {
                        ret = _semantic_pointer_add_assign(ast, parent, nodes[0], nodes[1]);
                        if (ret < 0)
                            return ret;
                    }
                }
            } else if (nb_pointers1 > 0) { // 情况2：右值是指针但左值不是
                loge("assign a pointer to an integer NOT with a type cast, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                return -1;

            } else if (v0->type != v1->type) { // 情况3：类型不匹配但都不是指针
                // 检查类型转换是否可行
                if (type_cast_check(ast, v0, v1) < 0) {
                    loge("type cast failed, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                    return -1;
                }
                // 添加类型转换
                int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
                if (ret < 0) {
                    loge("add type cast failed\n");
                    return ret;
                }
            }
            // 创建结果变量
            int ret = ast_find_type_type(&t, ast, v0->type);
            if (ret < 0)
                return ret;
            assert(t);

            lex_word_t *w = nodes[0]->parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
            if (!r)
                return -ENOMEM;

            *d->pret = r; // 设置返回值
            return 0;
        }
    }

    loge("type %d, %d not support\n", v0->type, v1->type);
    return -1;
}

// 处理二元运算的语义分析（+, -, *, / 等）
static int _op_semantic_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 两个操作数

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]); // 左操作数
    variable_t *v1 = _operand_get(nodes[1]); // 右操作数
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);
    // 检查是否是结构体指针，尝试重载操作
    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 处理整数或浮点数运算
    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            function_t *func_ptr = NULL;
            variable_t *v2 = NULL;
            type_t *t = NULL;

            int const_flag = 0;
            int nb_pointers = 0;
            int nb_pointers0 = variable_nb_pointers(v0); // 左操作数指针层级
            int nb_pointers1 = variable_nb_pointers(v1); // 右操作数指针层级
            int add_flag = 0;                            // 标记是否进行了指针加法转换
                                                         // 情况1：左操作数是指针
            if (nb_pointers0 > 0) {
                if (nb_pointers1 > 0) {
                    // 指针与指针运算：检查类型是否相同
                    if (!variable_same_type(v0, v1)) {
                        loge("different type pointer, type: %d,%d, nb_pointers: %d,%d\n",
                             v0->type, v1->type, nb_pointers0, nb_pointers1);
                        return -EINVAL;
                    }

                } else if (!variable_integer(v1)) {
                    // 指针与非整数运算：错误
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;
                } else {
                    // 指针与整数运算：需要类型转换
                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v1->w, t, v1->const_flag, 0, NULL);
                    // 添加类型转换
                    int ret = _semantic_add_type_cast(ast, &nodes[1], v2, nodes[1]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }
                    // 处理指针加法/减法
                    if (OP_ADD == parent->type || OP_SUB == parent->type) {
                        ret = _semantic_pointer_add(ast, parent, nodes[0], nodes[1]);
                        if (ret < 0)
                            return ret;
                        add_flag = 1; // 标记已进行指针运算转换
                    }
                }
                // 设置结果类型为指针指向的类型
                t = NULL;
                int ret = ast_find_type_type(&t, ast, v0->type);
                if (ret < 0)
                    return ret;

                const_flag = v0->const_flag;
                nb_pointers = nb_pointers0;
                func_ptr = v0->func_ptr;

            } else if (nb_pointers1 > 0) { // 情况2：右操作数是指针
                if (!variable_integer(v0)) {
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;

                } else { // 不允许整数减指针
                    if (OP_SUB == parent->type) {
                        loge("only a pointer sub an integer, NOT reverse, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                        return -1;
                    }
                    // 整数与指针运算：需要类型转换
                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v0->w, t, v0->const_flag, 0, NULL);
                    // 添加类型转换
                    int ret = _semantic_add_type_cast(ast, &nodes[0], v2, nodes[0]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }
                    // 处理指针加法
                    if (OP_ADD == parent->type) {
                        ret = _semantic_pointer_add(ast, parent, nodes[1], nodes[0]);
                        if (ret < 0)
                            return ret;

                        add_flag = 1; // 标记已进行指针运算转换
                    }
                }
                // 设置结果类型为指针指向的类型
                t = NULL;
                int ret = ast_find_type_type(&t, ast, v1->type);
                if (ret < 0)
                    return ret;

                const_flag = v1->const_flag;
                nb_pointers = nb_pointers1;
                func_ptr = v1->func_ptr;

            } else if (v0->type == v1->type) { // 情况3：两个操作数类型相同（都不是指针）
                t = NULL;
                int ret = ast_find_type_type(&t, ast, v0->type);
                if (ret < 0)
                    return ret;

                const_flag = v0->const_flag && v1->const_flag; // 只有两个都是常量结果才是常量
                nb_pointers = 0;
                func_ptr = NULL;

            } else { // 情况4：类型不同但可以转换
                     // 查找提升后的类型
                int ret = find_updated_type(ast, v0, v1);
                if (ret < 0) {
                    loge("var type update failed, type: %d, %d\n", v0->type, v1->type);
                    return -EINVAL;
                }

                t = NULL;
                ret = ast_find_type_type(&t, ast, ret);
                if (ret < 0)
                    return ret;
                // 添加必要的类型转换
                if (t->type != v0->type)
                    ret = _semantic_add_type_cast(ast, &nodes[0], v1, nodes[0]);
                else
                    ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);

                if (ret < 0) {
                    loge("add type cast failed\n");
                    return ret;
                }

                const_flag = v0->const_flag && v1->const_flag;
                nb_pointers = 0;
                func_ptr = NULL;
            }
            // 创建结果变量
            lex_word_t *w = parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, nb_pointers, func_ptr);
            if (!r)
                return -ENOMEM;

            r->tmp_flag = add_flag; // 标记是否为临时结果

            *d->pret = r; // 设置返回值
            return 0;
        }
    }

    loge("type %d, %d not support\n", v0->type, v1->type);
    return -1;
}

// ---------------------------
// 基本算术运算符的语义处理函数
// ---------------------------

// 加法语义检查：直接委托给通用的二元操作语义函数
static int _op_semantic_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

// 减法语义检查
static int _op_semantic_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

// 乘法语义检查
static int _op_semantic_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

// 除法语义检查
static int _op_semantic_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

// ------------------------------------
// 仅限于整数类型的二元运算（如 %、<<、>>、&、|）
// ------------------------------------
static int _op_semantic_binary_interger(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;
    // 获取操作数（左右操作节点对应的变量）
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;
    lex_word_t *w = parent->w;

    type_t *t;
    variable_t *r;

    assert(v0);
    assert(v1);
    // 如果任一操作数是结构体指针类型，尝试执行运算符重载函数
    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0; // 成功找到并执行重载

        if (-404 != ret) { // -404 表示“未找到重载”，其他负值为错误
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 检查是否为整数类型
    if (variable_integer(v0) && variable_integer(v1)) {
        int const_flag = v0->const_flag && v1->const_flag;
        // 若类型不同，执行类型自动转换
        if (!variable_same_type(v0, v1)) {
            int ret = _semantic_do_type_cast(ast, nodes, nb_nodes, data);
            if (ret < 0) {
                loge("semantic do type cast failed, line: %d\n", parent->w->line);
                return ret;
            }
        }
        // 转换完成后重新获取节点类型（因为节点结构可能被替换）
        v0 = _operand_get(nodes[0]);
        // 在 AST 类型系统中查找该类型定义
        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
        // 按类型分配结果变量（即运算结果）
        r = VAR_ALLOC_BY_TYPE(w, t, const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r) {
            loge("var alloc failed\n");
            return -ENOMEM;
        }
        // 将结果放入 handler_data 的输出变量中
        *d->pret = r;
        return 0;
    }
    // 若不是整数类型，则报错
    loge("\n");
    return -1;
}

// 以下运算符都使用 _op_semantic_binary_interger() 作为语义分析逻辑
static int _op_semantic_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

// 处理多返回值赋值的语义分析（如：a, b = func()）
static int _semantic_multi_rets_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 左值列表和函数调用

    handler_data_t *d = data;

    node_t *parent = nodes[0]->parent; // 赋值操作节点
    node_t *gp = parent->parent;       // 祖父节点

    assert(OP_BLOCK == nodes[0]->type); // 左值应该是代码块（多个变量）

    // 向上查找非表达式节点
    while (OP_EXPR == gp->type)
        gp = gp->parent;
#if 1
    // 检查多返回值调用必须在代码块或函数中
    if (gp->type != OP_BLOCK && gp->type != FUNCTION) {
        loge("a call to multi-return-values function MUST be in block, line: %d, gp->type: %d, b: %d, f: %d\n",
             parent->w->line, gp->type, OP_BLOCK, FUNCTION);
        return -1;
    }
#endif

    node_t *rets = nodes[0]; // 返回值接收列表
    node_t *call = nodes[1]; // 函数调用
    node_t *ret;

    // 解包函数调用表达式
    while (call) {
        if (OP_EXPR == call->type)
            call = call->nodes[0];
        else
            break;
    }

    // 验证必须是函数调用或对象创建
    if (OP_CALL != call->type && OP_CREATE != call->type) {
        loge("\n");
        return -1;
    }

    // 检查返回值数量匹配
    assert(call->nb_nodes > 0);

    logd("rets->nb_nodes: %d, call->result_nodes: %p\n", rets->nb_nodes, call->result_nodes);
    logd("rets->nb_nodes: %d, call->result_nodes->size: %d\n", rets->nb_nodes, call->result_nodes->size);

    assert(rets->nb_nodes <= call->result_nodes->size);

    int i;
    // 为每个返回值创建赋值操作
    for (i = 0; i < rets->nb_nodes; i++) {
        variable_t *v0 = _operand_get(rets->nodes[i]);              // 接收变量
        variable_t *v1 = _operand_get(call->result_nodes->data[i]); // 返回值

        // 检查类型匹配
        if (!variable_same_type(v0, v1)) {
            loge("\n");
            return -1;
        }
        // 检查接收变量是否可修改
        if (v0->const_flag) {
            loge("\n");
            return -1;
        }
        // 创建赋值节点
        node_t *assign = node_alloc(parent->w, OP_ASSIGN, NULL);
        if (!assign)
            return -ENOMEM;
        // 添加左右操作数
        node_add_child(assign, rets->nodes[i]);
        node_add_child(assign, call->result_nodes->data[i]);
        // 用赋值节点替换原来的接收变量
        rets->nodes[i] = assign;
    }
    // 重构AST：将函数调用插入到返回值块中
    node_add_child(rets, nodes[1]);
    // 调整节点顺序：函数调用在前，赋值操作在后
    for (i = rets->nb_nodes - 2; i >= 0; i--)
        rets->nodes[i + 1] = rets->nodes[i];
    rets->nodes[0] = nodes[1];
    // 修改父节点类型为表达式
    parent->type = OP_EXPR;
    parent->nb_nodes = 1;
    parent->nodes[0] = rets;

    return 0;
}

// 处理赋值操作的语义分析
static int _op_semantic_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 左值和右值

    handler_data_t *d = data;

    node_t *parent = nodes[0]->parent;
    node_t *call = nodes[1]; // 右值（可能是函数调用）
    // 解包右值表达式
    while (call) {
        if (OP_EXPR == call->type)
            call = call->nodes[0];
        else
            break;
    }
    // 检查是否是多返回值的函数调用
    if (OP_CALL == call->type
        && call->result_nodes->size > 1) {
        // 如果左值不是代码块，需要包装成代码块
        if (OP_BLOCK != nodes[0]->type) {
            block_t *b = block_alloc_cstr("multi_rets");
            if (!b)
                return -ENOMEM;

            int ret = node_add_child((node_t *)b, nodes[0]);
            if (ret < 0) {
                block_free(b);
                return ret;
            }
            parent->nodes[0] = (node_t *)b;
            b->node.parent = parent;
        }
    }
    // 如果是多返回值赋值，调用专门的处理函数
    if (OP_BLOCK == nodes[0]->type) {
        return _semantic_multi_rets_assign(ast, nodes, nb_nodes, data);
    }
    // 普通赋值操作的处理
    variable_t *v0 = _operand_get(nodes[0]); // 左值
    variable_t *v1 = _operand_get(nodes[1]); // 右值

    assert(v0);
    assert(v1);
    // 检查void类型必须是指针
    if (VAR_VOID == v0->type && 0 == v0->nb_pointers) {
        loge("var '%s' can't be 'void' type\n", v0->w->text->data);
        return -1;
    }

    if (VAR_VOID == v1->type && 0 == v1->nb_pointers) {
        loge("var '%s' can't be 'void' type\n", v1->w->text->data);
        return -1;
    }
    // 检查左值是否可修改
    if (v0->const_literal_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;

    } else if (v0->const_flag) {
        logw("const var '%s' can't be assigned\n", v0->w->text->data);
    }
    // 处理结构体赋值
    if (variable_is_struct(v0) || variable_is_struct(v1)) {
        int size = variable_size(v0);
        // 尝试操作符重载
        int ret = _semantic_do_overloaded_assign(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) { // -404表示未找到重载
            loge("semantic do overloaded error, ret: %d\n", ret);
            return -1;
        }
        // 相同类型的结构体赋值：使用memcpy
        if (variable_same_type(v0, v1)) {
            function_t *f = NULL;
            ret = ast_find_function(&f, ast, "  _memcpy");
            if (ret < 0)
                return ret;

            if (!f) {
                loge("semantic do overloaded error: default '  _memcpy' NOT found\n");
                return -1;
            }
            // 创建size参数
            type_t *t = block_find_type_type(ast->current_block, VAR_INTPTR);
            variable_t *v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 0, NULL);
            if (!v) {
                loge("var alloc failed\n");
                return -ENOMEM;
            }
            v->data.i64 = size;

            node_t *node_size = node_alloc(NULL, v->type, v);
            if (!node_size) {
                loge("node alloc failed\n");
                return -ENOMEM;
            }
            // 添加size参数
            node_add_child(parent, node_size);
            // 转换为memcpy调用
            return _semantic_add_call(ast, parent->nodes, parent->nb_nodes, d, f);
        }

        loge("semantic do overloaded error\n");
        return -1;
    }
    // 类型不匹配的处理
    if (!variable_same_type(v0, v1)) {
        // 处理结构体指针赋值为NULL的情况
        if (variable_is_struct_pointer(v0) && v1->w && strcmp(v1->w->text->data, "NULL")) {
            type_t *t = NULL;
            int ret = ast_find_type_type(&t, ast, v0->type);
            if (ret < 0)
                return ret;
            assert(t);
            // 检查是否有初始化函数
            if (scope_find_function(t->scope, "__init")) {
                int ret = _semantic_do_create(ast, nodes, nb_nodes, d);
                if (0 == ret)
                    return 0;

                if (-404 != ret) {
                    loge("semantic do overloaded error, ret: %d\n", ret);
                    return -1;
                }
            }
        }

        logd("v0: v_%d_%d/%s\n", v0->w->line, v0->w->pos, v0->w->text->data);
        // 检查类型转换是否可行
        if (type_cast_check(ast, v0, v1) < 0) {
            loge("\n");
            return -1;
        }
        // 添加类型转换
        int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
        if (ret < 0) {
            loge("add type cast failed\n");
            return ret;
        }
    }
    // 创建结果变量
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
    if (!r) {
        loge("var alloc failed\n");
        return -1;
    }

    *d->pret = r; // 设置返回值
    return 0;
}

// 处理整数二元赋值运算的语义分析（%=, <<=, >>=, &=, |=）
static int _op_semantic_binary_interger_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]); // 左值
    variable_t *v1 = _operand_get(nodes[1]); // 右值
    node_t *parent = nodes[0]->parent;
    lex_word_t *w = parent->w;

    type_t *t;
    variable_t *r;

    assert(v0);
    assert(v1);
    // 检查左值是否可修改
    if (v0->const_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;
    }
    // 尝试操作符重载
    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }
    // 处理整数运算
    if (variable_integer(v0) && variable_integer(v1)) {
        // 类型不匹配时添加类型转换
        if (!variable_same_type(v0, v1)) {
            if (type_cast_check(ast, v0, v1) < 0) {
                loge("\n");
                return -1;
            }

            int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
            if (ret < 0) {
                loge("add type cast failed\n");
                return ret;
            }
        }
        // 创建结果变量
        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
        assert(t);

        r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r) {
            loge("var alloc failed\n");
            return -ENOMEM;
        }

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

// 各种复合赋值运算的语义分析函数（转发到通用处理函数）
static int _op_semantic_add_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_sub_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_mul_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_div_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_mod_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shl_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_and_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_or_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

// 处理比较运算的语义分析
static int _op_semantic_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes); // 两个操作数

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]); // 左操作数
    variable_t *v1 = _operand_get(nodes[1]); // 右操作数

    assert(v0);
    assert(v1);

    // 尝试操作符重载
    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    // 处理整数或浮点数比较
    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            int const_flag = v0->const_flag && v1->const_flag; // 只有两个都是常量结果才是常量
            // 类型不匹配时进行类型转换
            if (!variable_same_type(v0, v1)) {
                int ret = _semantic_do_type_cast(ast, nodes, nb_nodes, data);
                if (ret < 0) {
                    loge("semantic do type cast failed\n");
                    return ret;
                }
            }

            v0 = _operand_get(nodes[0]); // 重新获取转换后的操作数
            // 比较运算的结果总是整数类型（布尔值）
            type_t *t = block_find_type_type(ast->current_block, VAR_INT);

            lex_word_t *w = nodes[0]->parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, 0, NULL);
            if (!r) {
                loge("var alloc failed\n");
                return -ENOMEM;
            }

            *d->pret = r; // 设置比较结果
            return 0;
        }
    }

    loge("\n");
    return -1;
}

// 宏定义：检查浮点数不能进行相等性比较（由于精度问题）
static int _op_semantic_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
#define CMPEQ_CHECK_FLOAT()                             \
    do {                                                \
        assert(2 == nb_nodes);                          \
        variable_t *v0 = _operand_get(nodes[0]);        \
        variable_t *v1 = _operand_get(nodes[1]);        \
                                                        \
        if (variable_float(v0) || variable_float(v1)) { \
            loge("float type can't cmp equal\n");       \
            return -EINVAL;                             \
        }                                               \
    } while (0)

    CMPEQ_CHECK_FLOAT();

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

// 各种比较运算的语义分析函数
static int _op_semantic_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT(); // 检查浮点数相等比较

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT(); // 检查浮点数大于等于比较

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT(); // 检查浮点数小于等于比较

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

// 逻辑运算的语义分析
static int _op_semantic_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

// 可变参数处理的语义分析
static int _op_semantic_va_start(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (2 != nb_nodes) { // va_start需要两个参数
        loge("\n");
        return -1;
    }
    return 0;
}

static int _op_semantic_va_arg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (2 != nb_nodes) { // va_arg需要两个参数
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;

    if (d->pret) {
        // 获取类型参数
        variable_t *v = _operand_get(nodes[1]);

        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v->type);
        if (ret < 0)
            return ret;
        assert(t);
        // 创建结果变量
        variable_t *r = VAR_ALLOC_BY_TYPE(nodes[0]->parent->w, t, 0, v->nb_pointers, v->func_ptr);

        if (!r)
            return -ENOMEM;

        *d->pret = r; // 设置va_arg的结果
    }
    return 0;
}

static int _op_semantic_va_end(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (1 != nb_nodes) { // va_end需要一个参数
        loge("\n");
        return -1;
    }
    return 0;
}

// 语义分析操作符处理函数表
operator_handler_pt semantic_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_semantic_expr, // 表达式
        [OP_CALL] = _op_semantic_call, // 函数调用

        [OP_ARRAY_INDEX] = _op_semantic_array_index, // 数组索引
        [OP_POINTER] = _op_semantic_pointer,         // 指针操作
        [OP_CREATE] = _op_semantic_create,           // 对象创建

        [OP_VA_START] = _op_semantic_va_start, // 可变参数开始
        [OP_VA_ARG] = _op_semantic_va_arg,     // 可变参数获取
        [OP_VA_END] = _op_semantic_va_end,     // 可变参数结束

        [OP_CONTAINER] = _op_semantic_container, // 容器操作

        [OP_SIZEOF] = _op_semantic_sizeof,       // sizeof 操作
        [OP_TYPE_CAST] = _op_semantic_type_cast, // 类型转换
        [OP_LOGIC_NOT] = _op_semantic_logic_not, // 逻辑非
        [OP_BIT_NOT] = _op_semantic_bit_not,     // 按位取反
        [OP_NEG] = _op_semantic_neg,             // 算术负号
        [OP_POSITIVE] = _op_semantic_positive,   // 算术正号

        [OP_INC] = _op_semantic_inc, // 前置自增
        [OP_DEC] = _op_semantic_dec, // 前置自减

        [OP_INC_POST] = _op_semantic_inc_post, // 后置自增
        [OP_DEC_POST] = _op_semantic_dec_post, // 后置自减

        [OP_DEREFERENCE] = _op_semantic_dereference, // 解引用
        [OP_ADDRESS_OF] = _op_semantic_address_of,   // 取地址

        [OP_MUL] = _op_semantic_mul, // 乘法
        [OP_DIV] = _op_semantic_div, // 除法
        [OP_MOD] = _op_semantic_mod, // 取模

        [OP_ADD] = _op_semantic_add, // 加法
        [OP_SUB] = _op_semantic_sub, // 减法

        [OP_SHL] = _op_semantic_shl, // 左移
        [OP_SHR] = _op_semantic_shr, // 右移

        [OP_BIT_AND] = _op_semantic_bit_and, // 按位与
        [OP_BIT_OR] = _op_semantic_bit_or,   // 按位或

        [OP_EQ] = _op_semantic_eq, // 等于
        [OP_NE] = _op_semantic_ne, // 不等于
        [OP_GT] = _op_semantic_gt, // 大于
        [OP_LT] = _op_semantic_lt, // 小于
        [OP_GE] = _op_semantic_ge, // 大于等于
        [OP_LE] = _op_semantic_le, // 小于等于

        [OP_LOGIC_AND] = _op_semantic_logic_and, // 逻辑与
        [OP_LOGIC_OR] = _op_semantic_logic_or,   // 逻辑或

        [OP_ASSIGN] = _op_semantic_assign,         // 赋值
        [OP_ADD_ASSIGN] = _op_semantic_add_assign, // 加赋值
        [OP_SUB_ASSIGN] = _op_semantic_sub_assign, // 减赋值
        [OP_MUL_ASSIGN] = _op_semantic_mul_assign, // 乘赋值
        [OP_DIV_ASSIGN] = _op_semantic_div_assign, // 除赋值
        [OP_MOD_ASSIGN] = _op_semantic_mod_assign, // 模赋值
        [OP_SHL_ASSIGN] = _op_semantic_shl_assign, // 左移赋值
        [OP_SHR_ASSIGN] = _op_semantic_shr_assign, // 右移赋值
        [OP_AND_ASSIGN] = _op_semantic_and_assign, // 与赋值
        [OP_OR_ASSIGN] = _op_semantic_or_assign,   // 或赋值

        [OP_BLOCK] = _op_semantic_block,       // 代码块
        [OP_RETURN] = _op_semantic_return,     // 返回语句
        [OP_BREAK] = _op_semantic_break,       // break语句
        [OP_CONTINUE] = _op_semantic_continue, // continue语句
        [OP_GOTO] = _op_semantic_goto,         // goto语句
        [LABEL] = _op_semantic_label,          // 标签

        [OP_IF] = _op_semantic_if,       // if语句
        [OP_WHILE] = _op_semantic_while, // while循环
        [OP_DO] = _op_semantic_do,       // do-while循环
        [OP_FOR] = _op_semantic_for,     // for循环

        [OP_SWITCH] = _op_semantic_switch,   // switch语句
        [OP_CASE] = _op_semantic_case,       // case标签
        [OP_DEFAULT] = _op_semantic_default, // default标签

        [OP_VLA_ALLOC] = _op_semantic_vla_alloc, // 可变长度数组分配
};

// 根据操作符类型查找对应的语义分析处理函数
operator_handler_pt find_semantic_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return semantic_operator_handlers[type];
}

// 函数语义分析入口
int function_semantic_analysis(ast_t *ast, function_t *f) {
    handler_data_t d = {0};

    // 调用内部函数调用语义分析
    int ret = __op_semantic_call(ast, f, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

// 表达式语义分析入口
int expr_semantic_analysis(ast_t *ast, expr_t *e) {
    handler_data_t d = {0};

    if (!e->nodes || e->nb_nodes != 1) {
        loge("\n");
        return -1;
    }

    // 计算表达式
    int ret = _expr_calculate_internal(ast, e->nodes[0], &d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

// 整个AST的语义分析入口
int semantic_analysis(ast_t *ast) {
    handler_data_t d = {0};

    // 从根代码块开始语义分析
    int ret = _expr_calculate_internal(ast, (node_t *)ast->root_block, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}
