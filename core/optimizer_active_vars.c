

#include "optimizer.h"

static int _bb_prev_find(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *prev_bb;
    dag_node_t *dn;

    int count = 0;
    int ret;
    int j;
    int k;

    for (k = 0; k < bb->exit_dn_actives->size; k++) {
        dn = bb->exit_dn_actives->data[k];

        if (vector_find(bb->entry_dn_inactives, dn))
            continue;

        if (vector_find(bb->entry_dn_actives, dn))
            continue;

        if (vector_find(bb->entry_dn_delivery, dn))
            continue;

        ret = vector_add(bb->entry_dn_delivery, dn);
        if (ret < 0)
            return ret;
        ++count;
    }

    for (j = 0; j < bb->prevs->size; j++) {
        prev_bb = bb->prevs->data[j];

        for (k = 0; k < bb->entry_dn_actives->size; k++) {
            dn = bb->entry_dn_actives->data[k];

            if (vector_find(prev_bb->exit_dn_actives, dn))
                continue;

            ret = vector_add(prev_bb->exit_dn_actives, dn);
            if (ret < 0)
                return ret;
            ++count;
        }

        for (k = 0; k < bb->entry_dn_delivery->size; k++) {
            dn = bb->entry_dn_delivery->data[k];

            if (vector_find(prev_bb->exit_dn_actives, dn))
                continue;

            ret = vector_add(prev_bb->exit_dn_actives, dn);
            if (ret < 0)
                return ret;
            ++count;
        }

        ret = vector_add(queue, prev_bb);
        if (ret < 0)
            return ret;
    }
    return count;
}

static int _optimize_active_vars(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    int count;
    int ret;

    if (list_empty(bb_list_head))
        return 0;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        ret = basic_block_active_vars(bb);
        if (ret < 0)
            return ret;
    }

    do {
        l = list_tail(bb_list_head);
        bb = list_data(l, basic_block_t, list);
        assert(bb->end_flag);

        ret = basic_block_search_bfs(bb, _bb_prev_find, NULL);
        if (ret < 0)
            return ret;
        count = ret;

    } while (count > 0);

    //	  basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_active_vars =
    {
        .name = "active_vars",

        .optimize = _optimize_active_vars,

        .flags = OPTIMIZER_LOCAL,
};
