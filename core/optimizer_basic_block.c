#include "optimizer.h"

void __use_function_dag(list_t *h, basic_block_t *bb);

static void __optimize_assign(dag_node_t *assign) {
    dag_node_t *parent;
    dag_node_t *dst;
    dag_node_t *src;
    int i;

    assert(2 == assign->childs->size);

    dst = assign->childs->data[0];
    src = assign->childs->data[1];

    if (OP_ADD == src->type
        || OP_SUB == src->type
        || OP_MUL == src->type
        || OP_DIV == src->type
        || OP_MOD == src->type
        || OP_ADDRESS_OF == src->type) {
        for (i = src->parents->size - 1; i >= 0; i--) {
            parent = src->parents->data[i];

            if (parent == assign)
                break;
        }

        for (--i; i >= 0; i--) {
            parent = src->parents->data[i];

            if (OP_ASSIGN == parent->type)
                continue;

            if (OP_ADD == parent->type
                || OP_SUB == parent->type
                || OP_MUL == parent->type
                || OP_DIV == parent->type
                || OP_MOD == parent->type
                || OP_ADDRESS_OF == parent->type) {
                if (dst != parent->childs->data[0] && dst != parent->childs->data[1])
                    continue;
            }
            break;
        }

        if (i < 0)
            assign->direct = dst;
    }
}

static void __optimize_dn_free(dag_node_t *dn) {
    dag_node_t *dn2;
    int i;

    for (i = 0; i < dn->childs->size;) {
        dn2 = dn->childs->data[i];

        assert(0 == vector_del(dn->childs, dn2));
        assert(0 == vector_del(dn2->parents, dn));

        if (0 == dn2->parents->size) {
            vector_free(dn2->parents);
            dn2->parents = NULL;
        }
    }

    list_del(&dn->list);
    dag_node_free(dn);
    dn = NULL;
}

static int _bb_dag_update(basic_block_t *bb) {
    dag_node_t *dn;
    dag_node_t *dn_bb;
    dag_node_t *dn_func;
    dag_node_t *base;
    list_t *l;

    while (1) {
        int updated = 0;

        for (l = list_tail(&bb->dag_list_head); l != list_sentinel(&bb->dag_list_head);) {
            dn = list_data(l, dag_node_t, list);
            l = list_prev(l);

            if (dn->parents)
                continue;

            if (type_is_var(dn->type))
                continue;

            if (type_is_assign_array_index(dn->type))
                continue;
            if (type_is_assign_dereference(dn->type))
                continue;
            if (type_is_assign_pointer(dn->type))
                continue;

            if (type_is_assign(dn->type)
                || OP_INC == dn->type || OP_DEC == dn->type
                || OP_3AC_INC == dn->type || OP_3AC_DEC == dn->type
                || OP_3AC_SETZ == dn->type || OP_3AC_SETNZ == dn->type
                || OP_3AC_SETLT == dn->type || OP_3AC_SETLE == dn->type
                || OP_3AC_SETGT == dn->type || OP_3AC_SETGE == dn->type
                || OP_ADDRESS_OF == dn->type
                || OP_DEREFERENCE == dn->type) {
                if (!dn->childs) {
                    list_del(&dn->list);
                    dag_node_free(dn);
                    dn = NULL;

                    ++updated;
                    continue;
                }

                assert(1 <= dn->childs->size && dn->childs->size <= 3);
                dn_bb = dn->childs->data[0];

                if (OP_ADDRESS_OF == dn->type || OP_DEREFERENCE == dn->type) {
                    dn_func = dn->old;
                } else {
                    assert(dn_bb->parents && dn_bb->parents->size > 0);

                    if (dn != dn_bb->parents->data[dn_bb->parents->size - 1]) {
                        if (OP_ASSIGN == dn->type)
                            __optimize_assign(dn);
                        continue;
                    }

                    dn_func = dn_bb->old;
                }

                if (!dn_func) {
                    loge("\n");
                    return -1;
                }

                if (vector_find(bb->dn_saves, dn_func)
                    || vector_find(bb->dn_resaves, dn_func)) {
                    if (OP_ASSIGN == dn->type)
                        __optimize_assign(dn);
                    continue;
                }

                __optimize_dn_free(dn);
                ++updated;

            } else if (OP_ADD == dn->type || OP_SUB == dn->type
                       || OP_MUL == dn->type || OP_DIV == dn->type
                       || OP_MOD == dn->type) {
                assert(dn->childs);
                assert(2 == dn->childs->size);

                dn_func = dn->old;

                if (!dn_func) {
                    loge("\n");
                    return -1;
                }

                if (vector_find(bb->dn_saves, dn_func)
                    || vector_find(bb->dn_resaves, dn_func))
                    continue;

                __optimize_dn_free(dn);
                ++updated;
            }
        }
        logd("bb: %p, updated: %d\n\n", bb, updated);

        if (0 == updated)
            break;
    }
    return 0;
}

static int __optimize_basic_block(basic_block_t *bb, function_t *f) {
    dag_node_t *dn;
    vector_t *roots;
    list_t h;

    int ret;
    int i;

    list_init(&h);

    roots = vector_alloc();
    if (!roots)
        return -ENOMEM;

    ret = basic_block_dag(bb, &bb->dag_list_head);
    if (ret < 0)
        goto error;

    ret = _bb_dag_update(bb);
    if (ret < 0)
        goto error;

    ret = dag_find_roots(&bb->dag_list_head, roots);
    if (ret < 0)
        goto error;

    logd("bb: %p, roots->size: %d\n", bb, roots->size);

    for (i = 0; i < roots->size; i++) {
        dn = roots->data[i];

        ret = dag_expr_calculate(&h, dn);
        if (ret < 0) {
            loge("\n");
            list_clear(&h, _3ac_code_t, list, _3ac_code_free);
            goto error;
        }
    }

    list_clear(&bb->code_list_head, _3ac_code_t, list, _3ac_code_free);

    list_mov2(&bb->code_list_head, &h);

error:
    __use_function_dag(&bb->code_list_head, bb);

    dag_node_free_list(&bb->dag_list_head);
    vector_free(roots);

    if (ret >= 0)
        ret = basic_block_active_vars(bb);
    return ret;
}

static int _optimize_basic_block(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    if (list_empty(bb_list_head))
        return 0;

    logd("------- %s() ------\n", f->node.w->text->data);

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        if (bb->jmp_flag
            || bb->end_flag
            || bb->call_flag
            || bb->dump_flag
            || bb->varg_flag) {
            logd("bb: %p, jmp:%d,ret:%d, end: %d, call:%d, varg:%d, dereference_flag: %d\n",
                 bb, bb->jmp_flag, bb->ret_flag, bb->end_flag, bb->call_flag, bb->dereference_flag,
                 bb->varg_flag);
            continue;
        }

        int ret = __optimize_basic_block(bb, f);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    //	  basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_basic_block =
    {
        .name = "basic_block",

        .optimize = _optimize_basic_block,

        .flags = OPTIMIZER_LOCAL,
};
