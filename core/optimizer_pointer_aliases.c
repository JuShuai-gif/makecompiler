#include "optimizer.h"

static int _bb_prev_find_saves(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *prev_bb;
    dag_node_t *dn;

    int count = 0;
    int ret;
    int j;

    for (j = 0; j < bb->prevs->size; j++) {
        prev_bb = bb->prevs->data[j];

        int k;
        for (k = 0; k < bb->entry_dn_aliases->size; k++) {
            dn = bb->entry_dn_aliases->data[k];

            if (vector_find(prev_bb->exit_dn_aliases, dn))
                continue;

            ret = vector_add(prev_bb->exit_dn_aliases, dn);
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

static int _bb_next_find_saves(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *next_bb;
    dag_node_t *dn;

    int count = 0;
    int ret;
    int j;

    for (j = 0; j < bb->nexts->size; j++) {
        next_bb = bb->nexts->data[j];

        int k;
        for (k = 0; k < next_bb->exit_dn_aliases->size; k++) {
            dn = next_bb->exit_dn_aliases->data[k];

            if (vector_find(bb->exit_dn_aliases, dn))
                continue;

            ret = vector_add(bb->exit_dn_aliases, dn);
            if (ret < 0)
                return ret;
            ++count;
        }

        ret = vector_add(queue, next_bb);
        if (ret < 0)
            return ret;
    }
    return count;
}

static int _bb_prev_find_loads(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *prev_bb;
    dag_node_t *dn;

    int count = 0;
    int ret;
    int j;

    for (j = 0; j < bb->prevs->size; j++) {
        prev_bb = bb->prevs->data[j];

        int k;
        for (k = 0; k < prev_bb->entry_dn_aliases->size; k++) {
            dn = prev_bb->entry_dn_aliases->data[k];

            if (vector_find(bb->entry_dn_aliases, dn))
                continue;

            ret = vector_add(bb->entry_dn_aliases, dn);
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

static int _bb_next_find_loads(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *next_bb;
    dag_node_t *dn;

    int count = 0;
    int ret;
    int j;

    for (j = 0; j < bb->nexts->size; j++) {
        next_bb = bb->nexts->data[j];

        int k;
        for (k = 0; k < bb->entry_dn_aliases->size; k++) {
            dn = bb->entry_dn_aliases->data[k];

            if (vector_find(next_bb->entry_dn_aliases, dn))
                continue;

            ret = vector_add(next_bb->entry_dn_aliases, dn);
            if (ret < 0)
                return ret;
            ++count;
        }

        ret = vector_add(queue, next_bb);
        if (ret < 0)
            return ret;
    }
    return count;
}

static int _optimize_pointer_aliases(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *start;
    basic_block_t *end;

    int count;
    int ret;

    if (list_empty(bb_list_head))
        return 0;

    l = list_head(bb_list_head);
    start = list_data(l, basic_block_t, list);

    l = list_tail(bb_list_head);
    end = list_data(l, basic_block_t, list);
    assert(end->end_flag);

    do {
        ret = basic_block_search_bfs(end, _bb_prev_find_saves, NULL);
        if (ret < 0)
            return ret;
        count = ret;

        ret = basic_block_search_bfs(start, _bb_next_find_saves, NULL);
        if (ret < 0)
            return ret;
        count += ret;

        ret = basic_block_search_bfs(start, _bb_next_find_loads, NULL);
        if (ret < 0)
            return ret;
        count += ret;

        ret = basic_block_search_bfs(end, _bb_prev_find_loads, NULL);
        if (ret < 0)
            return ret;
        count += ret;
    } while (count > 0);

    //	 basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_pointer_aliases =
    {
        .name = "pointer_aliases",

        .optimize = _optimize_pointer_aliases,

        .flags = OPTIMIZER_LOCAL,
};
