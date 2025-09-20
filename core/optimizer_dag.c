#include "optimizer.h"

static int _optimize_dag(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    if (list_empty(bb_list_head))
        return 0;

    f->nb_basic_blocks = 0;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        bb->index = f->nb_basic_blocks++;

        int ret = basic_block_dag(bb, &f->dag_list_head);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        ret = basic_block_vars(bb, bb_list_head);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    //	  basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_dag =
    {
        .name = "dag",

        .optimize = _optimize_dag,

        .flags = OPTIMIZER_LOCAL,
};
