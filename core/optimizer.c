#include "optimizer.h"

extern optimizer_t optimizer_inline;
extern optimizer_t optimizer_split_call;

extern optimizer_t optimizer_dag;

extern optimizer_t optimizer_call;
extern optimizer_t optimizer_common_expr;

extern optimizer_t optimizer_pointer_alias;
extern optimizer_t optimizer_active_vars;

extern optimizer_t optimizer_pointer_aliases;
extern optimizer_t optimizer_loads_saves;

extern optimizer_t optimizer_dominators;
extern optimizer_t optimizer_dominators_reverse;

extern optimizer_t optimizer_auto_gc_find;
extern optimizer_t optimizer_auto_gc;

extern optimizer_t optimizer_basic_block;

extern optimizer_t optimizer_const_teq;

extern optimizer_t optimizer_loop;

extern optimizer_t optimizer_vla;

extern optimizer_t optimizer_group;
extern optimizer_t optimizer_generate_loads_saves;

static optimizer_t *optimizers[] =
    {
        &optimizer_inline, // global optimizer
        &optimizer_split_call,

        &optimizer_dag,

        &optimizer_call,
        &optimizer_common_expr,

        &optimizer_pointer_alias,
        &optimizer_active_vars,

        &optimizer_pointer_aliases,
        &optimizer_loads_saves,

        &optimizer_auto_gc_find, // global optimizer

        &optimizer_dominators,

        &optimizer_auto_gc,

        &optimizer_basic_block,
        &optimizer_const_teq,

        &optimizer_dominators,
        &optimizer_loop,
        &optimizer_vla,
        &optimizer_group,

        &optimizer_generate_loads_saves,

        &optimizer_dominators_reverse,
};

int optimize(ast_t *ast, vector_t *functions) {
    optimizer_t *opt;
    function_t *f;
    bb_group_t *bbg;

    int n = sizeof(optimizers) / sizeof(optimizers[0]);
    int i;
    int j;

    for (i = 0; i < n; i++) {
        opt = optimizers[i];

        if (OPTIMIZER_GLOBAL == opt->flags) {
            int ret = opt->optimize(ast, NULL, functions);
            if (ret < 0) {
                loge("optimizer: %s\n", opt->name);
                return ret;
            }
            continue;
        }

        for (j = 0; j < functions->size; j++) {
            f = functions->data[j];

            if (!f->node.define_flag)
                continue;

            int ret = opt->optimize(ast, f, NULL);
            if (ret < 0) {
                loge("optimizer: %s\n", opt->name);
                return ret;
            }
        }
    }

#if 1
    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;

        printf("\n");
        logi("------- %s() ------\n", f->node.w->text->data);

        basic_block_print_list(&f->basic_block_list_head);

        for (j = 0; j < f->bb_loops->size; j++) {
            bbg = f->bb_loops->data[j];

            bb_loop_print(bbg);
        }

        for (j = 0; j < f->bb_groups->size; j++) {
            bbg = f->bb_groups->data[j];

            bb_group_print(bbg);
        }
    }
#endif
    return 0;
}
