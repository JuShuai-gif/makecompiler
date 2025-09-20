#include "optimizer.h"

static int _optimize_loads_saves(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    if (list_empty(bb_list_head))
        return 0;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        int ret = basic_block_loads_saves(bb, bb_list_head);
        if (ret < 0)
            return ret;
    }

    return 0;
}

optimizer_t optimizer_loads_saves =
    {
        .name = "loads_saves",

        .optimize = _optimize_loads_saves,

        .flags = OPTIMIZER_LOCAL,
};
