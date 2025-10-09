#include "optimizer.h"
#include "pointer_alias.h"

/**
 * 在基本块中查找数据流状态对象
 * @param bb 基本块指针
 * @param ds_obj 要查找的数据流状态对象
 * @return 0: 在freed列表中找到, 1: 在malloced列表中找到, 0: 未找到
 */
static int _bb_find_ds(basic_block_t *bb, dn_status_t *ds_obj) {
    // 先在已释放的数据流状态列表中查找
    if (vector_find_cmp(bb->ds_freed, ds_obj, ds_cmp_same_indexes))
        return 0;
    // 在已分配的数据流状态列表中查找
    if (vector_find_cmp(bb->ds_malloced, ds_obj, ds_cmp_same_indexes))
        return 1;
    return 0;
}

/**
 * 将源数据流状态的n个索引追加到目标数据流状态中
 * @param dst 目标数据流状态
 * @param src 源数据流状态
 * @param n 要追加的索引数量
 * @return 成功返回0，失败返回错误码
 */
static int __ds_append_index_n(dn_status_t *dst, dn_status_t *src, int n) {
    dn_index_t *di;
    int j;

    assert(n <= src->dn_indexes->size);
    // 遍历前n个索引
    for (j = 0; j < n; j++) {
        di = src->dn_indexes->data[j];
        // 将索引添加到目标向量的前端
        int ret = vector_add_front(dst->dn_indexes, di);
        if (ret < 0)
            return ret;
        di->refs++; // 增加索引的引用计数
    }

    return 0;
}

/**
 * 查找数据流状态的别名
 * @param aliases 存储找到的别名的向量
 * @param ds_obj 原始数据流状态对象
 * @param c 当前的三地址代码
 * @param bb 当前基本块
 * @param bb_list_head 基本块链表头
 * @return 成功返回0，失败返回错误码
 */
int __bb_find_ds_alias(vector_t *aliases, dn_status_t *ds_obj, mc_3ac_code_t *c, basic_block_t *bb, list_t *bb_list_head) {
    vector_t *tmp;
    dn_status_t *ds2;
    dn_status_t *ds;
    dn_index_t *di;

    // 获取前一条指令
    mc_3ac_code_t *c2 = list_data(list_prev(&c->list), mc_3ac_code_t, list);

    int ndi = 0; // 处理的索引数量
    int ret = 0;
    int j;

    // 分配临时向量
    tmp = vector_alloc();
    if (!tmp)
        return -ENOMEM;

    // 克隆数据流状态
    ds = dn_status_clone(ds_obj);
    if (!ds) {
        vector_free(tmp);
        return -ENOMEM;
    }

    // 循环处理别名
    while (1) {
        // 查找指针别名
        ret = pointer_alias_ds(tmp, ds, c2, bb, bb_list_head);
        if (ret < 0) {
            if (POINTER_NOT_INIT == ret)
                break;
            goto error;
        }

        // 处理找到的别名
        for (j = 0; j < tmp->size; j++) {
            ds2 = tmp->data[j];
            // 交换索引和别名索引，交换DAG节点和别名
            XCHG(ds2->dn_indexes, ds2->alias_indexes);
            XCHG(ds2->dag_node, ds2->alias);
            // 如果原始对象有索引，需要复制到别名中
            if (ds_obj->dn_indexes) {
                if (!ds2->dn_indexes) {
                    ds2->dn_indexes = vector_alloc();
                    if (!ds2->dn_indexes) {
                        ret = -ENOMEM;
                        goto error;
                    }
                }
                // 追加索引
                ret = __ds_append_index_n(ds2, ds_obj, ndi);
                if (ret < 0)
                    goto error;
            }
            // 将别名添加到结果向量
            ret = vector_add(aliases, ds2);
            if (ret < 0)
                goto error;

            dn_status_ref(ds2); // 增加引用计数
            ds2 = NULL;
        }
        // 清空临时向量
        vector_clear(tmp, (void (*)(void *))dn_status_free);
        // 处理下一个索引
        if (ds->dn_indexes) {
            assert(ds->dn_indexes->size > 0);
            di = ds->dn_indexes->data[0];
            // 从向量中删除索引
            assert(0 == vector_del(ds->dn_indexes, di));

            dn_index_free(di); // 释放索引
            di = NULL;

            ndi++; // 增加处理的索引计数
            // 如果所有索引都处理完了
            if (0 == ds->dn_indexes->size) {
                vector_free(ds->dn_indexes);
                ds->dn_indexes = NULL;
            }
        } else
            break; // 没有更多索引，退出循环
    }

    ret = 0;
error:
    // 清理资源
    dn_status_free(ds);
    vector_clear(tmp, (void (*)(void *))dn_status_free);
    vector_free(tmp);
    return ret;
}

/**
 * 查找数据流状态的别名并检查是否需要保留
 * @param ds_obj 数据流状态对象
 * @param c 三地址代码
 * @param bb 基本块
 * @param bb_list_head 基本块链表头
 * @return 1: 需要保留, 0: 不需要保留, 负数: 错误码
 */
static int _bb_find_ds_alias(dn_status_t *ds_obj, mc_3ac_code_t *c, basic_block_t *bb, list_t *bb_list_head) {
    dn_status_t *ds_obj2;
    dn_status_t *ds;
    dn_index_t *di;
    vector_t *aliases;
    int i;
    // 分配别名向量
    aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;
    // 查找所有别名
    int ret = __bb_find_ds_alias(aliases, ds_obj, c, bb, bb_list_head);
    if (ret < 0)
        return ret;

    int need = 0; // 是否需要保留的标志
                  // 检查别名是否需要保留
    for (i = 0; i < aliases->size; i++) {
        ds = aliases->data[i];

        logd("ds: %#lx, ds->refs: %d\n", 0xffff & (uintptr_t)ds, ds->refs);
        if (!ds->dag_node)
            continue;
        // 如果在malloced列表中找到且不在freed列表中
        if (vector_find_cmp(bb->ds_malloced, ds, ds_cmp_same_indexes)
            && !vector_find_cmp(bb->ds_freed, ds, ds_cmp_same_indexes)) {
            need = 1;
            break;
        }
    }
    // 检查原始对象是否需要保留
    if (vector_find_cmp(bb->ds_malloced, ds_obj, ds_cmp_same_indexes)
        && !vector_find_cmp(bb->ds_freed, ds_obj, ds_cmp_same_indexes)) {
        need = 1;
    }

    ret = need;
error:
    // 清理资源
    vector_clear(aliases, (void (*)(void *))dn_status_free);
    vector_free(aliases);
    return ret;
}

/**
 * 在函数参数中查找需要自动垃圾收集的变量
 * @param cur_bb 当前基本块
 * @param c 三地址代码
 * @return 成功返回0，失败返回错误码
 */
static int _auto_gc_find_argv_in(basic_block_t *cur_bb, mc_3ac_code_t *c) {
    mc_3ac_operand_t *src;
    dag_node_t *dn;
    variable_t *v;

    int i;
    // 遍历所有源操作数（跳过第一个，可能是函数本身）
    for (i = 1; i < c->srcs->size; i++) {
        src = c->srcs->data[i];

        dn = src->dag_node;
        // 遍历DAG节点，跳过类型转换和表达式节点
        while (dn) {
            if (OP_TYPE_CAST == dn->type)
                dn = dn->childs->data[0];

            else if (OP_EXPR == dn->type)
                dn = dn->childs->data[0];

            else if (OP_POINTER == dn->type)
                dn = dn->childs->data[0];
            else
                break;
        }

        v = dn->var;
        // 检查变量是否为复杂类型（指针、数组、结构体）
        if (v->nb_pointers + v->nb_dimentions + (v->type >= STRUCT) < 2)
            continue;

        logd("v: %s\n", v->w->text->data);
        // 添加到入口活跃节点列表
        if (vector_add_unique(cur_bb->entry_dn_actives, dn) < 0)
            return -ENOMEM;
    }

    return 0;
}

/**
 * 在基本块的后继中传播数据流状态
 * @param bb 当前基本块
 * @param data 未使用的参数
 * @param queue 处理队列
 * @return 更新的状态数量，错误返回负数
 */
static int _auto_gc_bb_next_find(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *next_bb;
    dn_status_t *ds;
    dn_status_t *ds2;

    int count = 0; // 更新计数
    int ret;
    int j;

    // 遍历所有后继基本块
    for (j = 0; j < bb->nexts->size; j++) {
        next_bb = bb->nexts->data[j];

        int k;
        // 遍历当前块的所有malloced数据流状态
        for (k = 0; k < bb->ds_malloced->size; k++) {
            ds = bb->ds_malloced->data[k];
            // 跳过在当前块或后继块中已释放的状态
            if (vector_find_cmp(bb->ds_freed, ds, ds_cmp_same_indexes))
                continue;

            if (vector_find_cmp(next_bb->ds_freed, ds, ds_cmp_same_indexes))
                continue;
            // 在后继块中查找相似的数据流状态
            ds2 = vector_find_cmp(next_bb->ds_malloced, ds, ds_cmp_like_indexes);
            if (ds2) {
                uint32_t tmp = ds2->ret_flag;
                // 合并返回标志
                ds2->ret_flag |= ds->ret_flag;
                // 如果标志有变化，记录并更新返回索引
                if (tmp != ds2->ret_flag) {
                    logd("*** ds2: %#lx, ret_index: %d, ret_flag: %d, ds: %#lx, ret_index: %d, ret_flag: %d\n",
                         0xffff & (uintptr_t)ds2, ds2->ret_index, ds2->ret_flag,
                         0xffff & (uintptr_t)ds, ds->ret_index, ds->ret_flag);

                    count++;
                    ds2->ret_index = ds->ret_index;
                }
                continue;
            }
            // 克隆数据流状态到后继块
            ds2 = dn_status_clone(ds);
            if (!ds2)
                return -ENOMEM;

            ret = vector_add(next_bb->ds_malloced, ds2);
            if (ret < 0) {
                dn_status_free(ds2);
                return ret;
            }
            ++count;
        }
        // 将后继块加入处理队列
        ret = vector_add(queue, next_bb);
        if (ret < 0)
            return ret;
    }
    return count;
}

/**
 * 对函数进行BFS排序，从malloc函数开始
 * @param fqueue 存储排序后函数的队列
 * @param functions 所有函数的向量
 * @return 成功返回0，失败返回错误码
 */
static int _bfs_sort_function(vector_t *fqueue, vector_t *functions) {
    function_t *fmalloc = NULL;
    function_t *f;
    function_t *f2;
    int i;
    int j;
    // 初始化访问标志并查找malloc函数
    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        f->visited_flag = 0;

        if (!fmalloc && !strcmp(f->node.w->text->data, " _auto_malloc"))
            fmalloc = f;
    }

    if (!fmalloc)
        return 0;
    // 从malloc函数开始BFS
    int ret = vector_add(fqueue, fmalloc);
    if (ret < 0)
        return ret;

    for (i = 0; i < fqueue->size; i++) {
        f = fqueue->data[i];

        if (f->visited_flag)
            continue;

        logd("f: %p, %s\n", f, f->node.w->text->data);

        f->visited_flag = 1;
        // 遍历调用者函数
        for (j = 0; j < f->caller_functions->size; j++) {
            f2 = f->caller_functions->data[j];

            if (f2->visited_flag)
                continue;

            ret = vector_add(fqueue, f2);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}
