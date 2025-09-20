#include "optimizer.h"

void __use_function_dag(list_t *h, basic_block_t *bb) {
    _3ac_operand_t *src;
    _3ac_operand_t *dst;
    _3ac_code_t *c;
    dag_node_t *dn;
    list_t *l;

    int i;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        c = list_data(l, _3ac_code_t, list);

        if (c->dsts) {
            dst = c->dsts->data[0];
            dn = dst->dag_node->old;

            dst->node = dn->node;
            dst->dag_node = dn;
        }

        if (c->srcs) {
            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                dn = src->dag_node->old;

                src->node = dn->node;
                src->dag_node = dn;
            }
        }

        c->basic_block = bb;
    }
}

int __optimize_common_expr(basic_block_t *bb, function_t *f) {
    dag_node_t *dn;
    vector_t *roots;
    list_t *l;
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

    ret = dag_find_roots(&bb->dag_list_head, roots);
    if (ret < 0)
        goto error;

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

    ret = 0;
error:
    __use_function_dag(&bb->code_list_head, bb);

    dag_node_free_list(&bb->dag_list_head);
    vector_free(roots);
    return ret;
}

static int _optimize_common_expr(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    basic_block_t *bb;
    list_t *l;
    list_t *bb_list_head = &f->basic_block_list_head;

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

        int ret = __optimize_common_expr(bb, f);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        ret = basic_block_vars(bb, bb_list_head);
        if (ret < 0)
            return ret;
    }

    return 0;
}

optimizer_t optimizer_common_expr =
    {
        .name = "common_expr",

        .optimize = _optimize_common_expr,

        .flags = OPTIMIZER_LOCAL,
};
