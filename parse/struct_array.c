#include "dfa.h"
#include "parse.h"

// 用于把给定的索引按数组维度展开成多维索引
/*
核心逻辑：

    把线性化索引（1维数组索引）转换成多维索引（适用于 array[i][j][k]）

    使用 % 和 / 运算分解每一维索引

    返回 p 给调用者
*/
static int __reshape_index(dfa_index_t **out, variable_t *array, dfa_index_t *index, int n) {
    assert(array->nb_dimentions > 0); // 数组至少是1维
    assert(n > 0);                    // 输入索引长度必须大于0
                                      // 为最终多维索引分配空间
    dfa_index_t *p = calloc(array->nb_dimentions, sizeof(dfa_index_t));
    if (!p)
        return -ENOMEM;
    // 获取原始索引的最后一个值（线性化索引）
    intptr_t i = index[n - 1].i;

    logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n",
         n, array->nb_dimentions, i);

    int j;
    // 从最后一维开始，将线性索引转换为多维索引
    for (j = array->nb_dimentions - 1; j >= 0; j--) {
        if (array->dimentions[j].num <= 0) {
            logw("array's %d-dimention size not set, file: %s, line: %d\n", j, array->w->file->data, array->w->line);

            free(p);
            return -1;
        }
        // 计算当前维度的索引
        p[j].i = i % array->dimentions[j].num;
        i = i / array->dimentions[j].num; // 更新剩余索引，用于上一维
    }
    // 打印每一维的索引
    for (j = 0; j < array->nb_dimentions; j++)
        logi("\033[32m dim: %d, size: %d, index: %ld\033[0m\n", j, array->dimentions[j].num, p[j].i);

    *out = p; // 输出多维索引
    return 0;
}

// 用于根据索引构造 AST（抽象语法树）节点，表示数组元素访问
/*
核心逻辑：

1. 检查 pnode 是否为空

2. 如果没有提供根节点，则创建一个根节点代表数组

3. 如果提供的索引不足，则调用 __reshape_index 生成完整的多维索引

4. 遍历数组维度，逐层构建 AST 节点：

    每一维生成 OP_ARRAY_INDEX 节点

    第一个子节点是父节点（或数组根节点）

    第二个子节点是索引节点（常量）

5. 最终输出 *pnode 为完整的数组访问 AST 节点
*/
static int __array_member_init(ast_t *ast, lex_word_t *w, variable_t *array, dfa_index_t *index, int n, node_t **pnode) {
    if (!pnode)
        return -1;
    // 获取 int 类型
    type_t *t = block_find_type_type(ast->current_block, VAR_INT);
    node_t *root = *pnode;
    // 如果根节点为空，则创建一个根节点表示数组
    if (!root)
        root = node_alloc(NULL, array->type, array);
    // 如果索引数量少于数组维度，则需要重塑索引
    if (n < array->nb_dimentions) {
        if (n <= 0) {
            loge("number of indexes less than needed, array '%s', file: %s, line: %d\n",
                 array->w->text->data, w->file->data, w->line);
            return -1;
        }

        int ret = __reshape_index(&index, array, index, n);
        if (ret < 0)
            return ret;
    }

    int i;
    for (i = 0; i < array->nb_dimentions; i++) {
        intptr_t k = index[i].i;
        // 越界检查
        if (k >= array->dimentions[i].num) {
            loge("index [%ld] out of size [%d], in dim: %d, file: %s, line: %d\n",
                 k, array->dimentions[i].num, i, w->file->data, w->line);

            if (n < array->nb_dimentions) {
                free(index);
                index = NULL;
            }
            return -1;
        }
        // 为当前索引创建一个常量节点
        variable_t *v_index = variable_alloc(NULL, t);
        v_index->const_flag = 1;
        v_index->const_literal_flag = 1;
        v_index->data.i64 = k;

        node_t *node_index = node_alloc(NULL, v_index->type, v_index); // 索引节点
        node_t *node_op = node_alloc(w, OP_ARRAY_INDEX, NULL);         // 数组访问操作节点

        node_add_child(node_op, root);       // 将 root 添加为操作节点的第一个子节点
        node_add_child(node_op, node_index); // 将索引节点添加为操作节点的第二个子节点
        root = node_op;                      // 更新 root 为当前操作节点
    }
    // 如果进行了索引重塑，释放临时索引
    if (n < array->nb_dimentions) {
        free(index);
        index = NULL;
    }

    *pnode = root;               // 输出最终的 AST 节点
    return array->nb_dimentions; // 返回数组维度
}

/*
总结：

构建结构体成员访问的 AST

支持嵌套结构体和数组成员

对索引越界和过多索引进行检查
*/
int struct_member_init(ast_t *ast, lex_word_t *w, variable_t *_struct, dfa_index_t *index, int n, node_t **pnode) {
    if (!pnode)
        return -1;

    variable_t *v = NULL;
    type_t *t = block_find_type_type(ast->current_block, _struct->type);
    node_t *root = *pnode;

    if (!root)
        root = node_alloc(NULL, _struct->type, _struct); // 创建根节点，代表结构体

    int j = 0;
    while (j < n) {
        if (!t->scope) { // 当前类型没有成员
            loge("\n");
            return -1;
        }

        int k;

        // 如果索引是成员名字，则在结构体成员表中查找
        if (index[j].w) {
            for (k = 0; k < t->scope->vars->size; k++) {
                v = t->scope->vars->data[k];

                if (v->w && !strcmp(index[j].w->text->data, v->w->text->data))
                    break;
            }
        } else
            k = index[j].i; // 索引是数字，直接用

        if (k >= t->scope->vars->size) { // 越界检查
            loge("\n");
            return -1;
        }

        v = t->scope->vars->data[k];

        // 构建 AST 节点：结构体访问 -> 成员节点
        node_t *node_op = node_alloc(w, OP_POINTER, NULL);
        node_t *node_v = node_alloc(NULL, v->type, v);

        node_add_child(node_op, root);   // 父节点
        node_add_child(node_op, node_v); // 成员节点
        root = node_op;

        logi("j: %d, k: %d, v: '%s'\n", j, k, v->w->text->data);
        j++;

        // 如果成员是数组，调用 __array_member_init 构建数组索引节点
        if (v->nb_dimentions > 0) {
            int ret = __array_member_init(ast, w, v, index + j, n - j, &root);
            if (ret < 0)
                return -1;

            j += ret;
            logi("struct var member: %s->%s[]\n", _struct->w->text->data, v->w->text->data);
        }

        // 如果成员是基本类型或指针，则不再访问下一级
        if (v->type < STRUCT || v->nb_pointers > 0) {
            // if 'v' is a base type var or a pointer, and of course 'v' isn't an array,
            // we can't get the member of v !!
            // the index must be the last one, and its expr is to init v !
            if (j < n - 1) {
                loge("number of indexes more than needed, struct member: %s->%s, file: %s, line: %d\n",
                     _struct->w->text->data, v->w->text->data, w->file->data, w->line);
                return -1;
            }

            logi("struct var member: %s->%s\n", _struct->w->text->data, v->w->text->data);

            *pnode = root; // 输出 AST 节点
            return n;
        }

        // 成员是嵌套结构体，则继续找到其类型
        type_t *type_v = NULL;

        while (t) {
            type_v = scope_find_type_type(t->scope, v->type);
            if (type_v)
                break;

            // only can use types in this scope, or parent scope
            // can't use types in children scope
            t = t->parent; // 查找父作用域
        }

        if (!type_v) {
            type_v = block_find_type_type(ast->current_block, v->type);

            if (!type_v) {
                loge("\n");
                return -1;
            }
        }

        t = type_v; // 更新当前类型
    }
    // 如果索引少于结构体成员数
    loge("number of indexes less than needed, struct member: %s->%s, file: %s, line: %d\n",
         _struct->w->text->data, v->w->text->data, w->file->data, w->line);
    return -1;
}

/*
总结：

处理数组成员访问

数组元素可能是基本类型、指针或结构体

支持多维数组和结构体嵌套
*/
int array_member_init(ast_t *ast, lex_word_t *w, variable_t *array, dfa_index_t *index, int n, node_t **pnode) {
    node_t *root = NULL;
    // 构建数组访问 AST
    int ret = __array_member_init(ast, w, array, index, n, &root);
    if (ret < 0)
        return ret;
    // 如果数组元素是基本类型或指针，直接返回
    if (array->type < STRUCT || array->nb_pointers > 0) {
        if (ret < n - 1) {
            loge("\n");
            return -1;
        }

        *pnode = root;
        return n;
    }
    // 如果数组元素是结构体，则递归调用 struct_member_init
    ret = struct_member_init(ast, w, array, index + ret, n - ret, &root);
    if (ret < 0)
        return ret;

    *pnode = root;
    return n;
}

/*
总结：

处理数组初始化

支持未设置维度长度的数组

将初始化表达式索引重塑成多维索引

构建 AST 的赋值节点
*/
int array_init(ast_t *ast, lex_word_t *w, variable_t *v, vector_t *init_exprs) {
    dfa_init_expr_t *ie;

    int unset = 0;      // 未设置长度的维度计数
    int unset_dim = -1; // 未设置长度的维度索引
    int capacity = 1;   // 数组总容量
    int i;
    int j;
    // 遍历维度，计算总容量，并检查未设置长度维度
    for (i = 0; i < v->nb_dimentions; i++) {
        assert(v->dimentions);

        logi("dim[%d]: %d\n", i, v->dimentions[i].num);

        if (v->dimentions[i].num < 0) { // 未设置长度
            if (unset > 0) {
                loge("array '%s' should only unset 1-dimention size, file: %s, line: %d\n",
                     v->w->text->data, w->file->data, w->line);
                return -1;
            }

            unset++;
            unset_dim = i;
        } else
            capacity *= v->dimentions[i].num;
    }
    // 如果存在未设置的维度，根据初始化表达式计算长度
    if (unset) {
        int unset_max = -1;

        for (i = 0; i < init_exprs->size; i++) {
            ie = init_exprs->data[i];

            if (unset_dim < ie->n) {
                if (unset_max < ie->index[unset_dim].i)
                    unset_max = ie->index[unset_dim].i;
            }
        }

        if (-1 == unset_max) {
            unset_max = init_exprs->size / capacity;

            v->dimentions[unset_dim].num = unset_max;

            logw("don't set %d-dimention size of array '%s', use '%d' as calculated, file: %s, line: %d\n",
                 unset_dim, v->w->text->data, unset_max, w->file->data, w->line);
        } else
            v->dimentions[unset_dim].num = unset_max + 1;
    }
    // 将初始化表达式的索引重塑成多维索引
    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        if (ie->n < v->nb_dimentions) {
            int n = ie->n;

            void *p = realloc(ie, sizeof(dfa_init_expr_t) + sizeof(dfa_index_t) * v->nb_dimentions);
            if (!p)
                return -ENOMEM;
            init_exprs->data[i] = p;

            ie = p;
            ie->n = v->nb_dimentions;

            intptr_t index = ie->index[n - 1].i;

            logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n", n, v->nb_dimentions, index);

            for (j = v->nb_dimentions - 1; j >= 0; j--) {
                ie->index[j].i = index % v->dimentions[j].num;
                index = index / v->dimentions[j].num;
            }
        }

        for (j = 0; j < v->nb_dimentions; j++)
            logi("\033[32mi: %d, dim: %d, size: %d, index: %ld\033[0m\n", i, j, v->dimentions[j].num, ie->index[j].i);
    }
    // 根据索引生成 AST 赋值节点
    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        logi("#### data init, i: %d, init expr: %p\n", i, ie->expr);

        expr_t *e;
        node_t *assign;
        node_t *node = NULL;

        if (array_member_init(ast, w, v, ie->index, ie->n, &node) < 0) {
            loge("\n");
            return -1;
        }

        e = expr_alloc();
        assign = node_alloc(w, OP_ASSIGN, NULL);

        node_add_child(assign, node);     // 左值
        node_add_child(assign, ie->expr); // 右值
        expr_add_node(e, assign);

        ie->expr = e; // 替换为 AST 表达式
        printf("\n");
    }

    return 0;
}

/*
总结：

处理结构体初始化

遍历每个初始化表达式

调用 struct_member_init 构建 AST 左值

构建赋值表达式节点并存储
*/
int struct_init(ast_t *ast, lex_word_t *w, variable_t *var, vector_t *init_exprs) {
    dfa_init_expr_t *ie;

    int i;
    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        logi("#### struct init, i: %d, init_expr->expr: %p\n", i, ie->expr);

        expr_t *e;
        node_t *assign;
        node_t *node = NULL;

        if (struct_member_init(ast, w, var, ie->index, ie->n, &node) < 0)
            return -1;

        e = expr_alloc();
        assign = node_alloc(w, OP_ASSIGN, NULL);

        node_add_child(assign, node);// 左值：结构体成员
        node_add_child(assign, ie->expr);// 右值：初始化表达式
        expr_add_node(e, assign);

        ie->expr = e;// 保存 AST
        printf("\n");
    }

    return 0;
}
