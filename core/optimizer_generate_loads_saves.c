#include "optimizer.h"

static int _bb_index_cmp(const void *p0, const void *p1) {
    const basic_block_t *bb0 = *(basic_block_t **)p0;
    const basic_block_t *bb1 = *(basic_block_t **)p1;

    if (bb0->index < bb1->index)
        return -1;

    if (bb0->index > bb1->index)
        return 1;
    return 0;
}

static int _optimize_generate_loads_saves(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;

    if (list_empty(bb_list_head))
        return 0;

    list_t *l;
    basic_block_t *bb;
    basic_block_t *bb_auto_gc;
    basic_block_t *post;
    bb_group_t *bbg;

    dn_status_t *ds;
    dag_node_t *dn;
    _3ac_code_t *load;
    _3ac_code_t *save;
    _3ac_code_t *c;

    int i;
    int j;
    int k;
    int ret;

#define OPTIMIZER_LOAD(load_type, h)            \
    do {                                        \
        load = _3ac_alloc_by_dst(load_type, dn); \
        if (!load) {                            \
            loge("\n");                         \
            return -ENOMEM;                     \
        }                                       \
        load->basic_block = bb;                 \
        list_add_front(h, &load->list);         \
    } while (0)

#define OPTIMIZER_SAVE(save_type, h)            \
    do {                                        \
        save = _3ac_alloc_by_src(save_type, dn); \
        if (!save) {                            \
            loge("\n");                         \
            return -ENOMEM;                     \
        }                                       \
        save->basic_block = bb;                 \
        list_add_tail(h, &save->list);          \
    } while (0)

    f->nb_basic_blocks = 0;

    bb_auto_gc = NULL;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        bb->index = f->nb_basic_blocks++;

        for (i = 0; i < bb->dn_loads->size; i++) {
            dn = bb->dn_loads->data[i];

            if (vector_find(bb->dn_reloads, dn))
                continue;

            OPTIMIZER_LOAD(OP_3AC_LOAD, &bb->code_list_head);
        }

        for (i = 0; i < bb->dn_saves->size; i++) {
            dn = bb->dn_saves->data[i];

            if (vector_find(bb->dn_resaves, dn))
                continue;

            if (bb->loop_flag)
                OPTIMIZER_SAVE(OP_3AC_SAVE, &bb->save_list_head);
            else {
                OPTIMIZER_SAVE(OP_3AC_SAVE, &bb->code_list_head);

                if (bb->cmp_flag) {
                    list_del(&save->list);
                    list_add_front(&bb->code_list_head, &save->list);
                }
            }
        }

        for (i = 0; i < bb->dn_reloads->size; i++) {
            dn = bb->dn_reloads->data[i];
            OPTIMIZER_LOAD(OP_3AC_RELOAD, &bb->code_list_head);
        }

        for (i = 0; i < bb->dn_resaves->size; i++) {
            dn = bb->dn_resaves->data[i];
            OPTIMIZER_SAVE(OP_3AC_RESAVE, &bb->code_list_head);

            if (bb->cmp_flag && !vector_find(bb->dn_updateds, dn)) {
                list_del(&save->list);
                list_add_front(&bb->code_list_head, &save->list);
            }
        }
#if 1
        if (bb->auto_ref_flag || bb->auto_free_flag) {
            if (!bb_auto_gc) {
                c = _3ac_code_alloc();
                if (!c) {
                    loge("\n");
                    return -ENOMEM;
                }
                c->op = _3ac_find_operator(OP_3AC_PUSH_RETS);
                c->basic_block = bb;

                _list_add_front(&bb->code_list_head, &c->list);
            }

            bb_auto_gc = bb;

        } else {
            if (bb_auto_gc) {
                c = _3ac_code_alloc();
                if (!c) {
                    loge("\n");
                    return -ENOMEM;
                }
                c->op = _3ac_find_operator(OP_3AC_POP_RETS);
                c->basic_block = bb_auto_gc;

                list_add_tail(&bb_auto_gc->code_list_head, &c->list);
                bb_auto_gc = NULL;
            }
        }
#endif

#if 1
        logd("bb: %p, bb->index: %d\n", bb, bb->index);
        ret = basic_block_active_vars(bb);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
#endif
    }

    for (i = 0; i < f->bb_groups->size; i++) {
        bbg = f->bb_groups->data[i];

        for (j = 0; j < bbg->body->size; j++) {
            bb = bbg->body->data[j];

            for (k = 0; k < bb->exit_dn_actives->size; k++) {
                dn = bb->exit_dn_actives->data[k];

                for (l = list_tail(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_prev(l)) {
                    c = list_data(l, _3ac_code_t, list);

                    if (!c->active_vars)
                        continue;

                    ds = vector_find_cmp(c->active_vars, dn, dn_status_cmp);
                    if (ds)
                        break;

                    DN_STATUS_ALLOC(ds, c->active_vars, dn);
                    ds->active = 1;
                }
            }

            for (k = 0; k < bb->exit_dn_aliases->size; k++) {
                dn = bb->exit_dn_aliases->data[k];

                for (l = list_tail(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_prev(l)) {
                    c = list_data(l, _3ac_code_t, list);

                    if (!c->active_vars)
                        continue;

                    ds = vector_find_cmp(c->active_vars, dn, dn_status_cmp);
                    if (ds)
                        break;

                    DN_STATUS_ALLOC(ds, c->active_vars, dn);
                    ds->active = 1;
                }
            }
        }
    }

    for (i = 0; i < f->bb_loops->size; i++) {
        bbg = f->bb_loops->data[i];

        qsort(bbg->body->data, bbg->body->size, sizeof(void *), _bb_index_cmp);

        for (j = 0; j < bbg->body->size; j++) {
            bb = bbg->body->data[j];

            for (l = list_tail(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_prev(l)) {
                c = list_data(l, _3ac_code_t, list);

                if (!c->active_vars)
                    continue;

                int n;
                for (n = 0; n < bbg->posts->size; n++) {
                    post = bbg->posts->data[n];

                    for (k = 0; k < post->dn_saves->size; k++) {
                        dn = post->dn_saves->data[k];

                        DN_STATUS_GET(ds, c->active_vars, dn);
                        ds->active = 1;
                    }
                }

                for (k = 0; k < bbg->pre->dn_loads->size; k++) {
                    dn = bbg->pre->dn_loads->data[k];

                    DN_STATUS_GET(ds, c->active_vars, dn);
                    ds->active = 1;
                }
            }
        }
    }

    for (i = 0; i < f->jmps->size; i++) {
        c = f->jmps->data[i];

        _3ac_operand_t *dst = c->dsts->data[0];

        dst->bb->jmp_dst_flag = 1;
    }

    return 0;
}

optimizer_t optimizer_generate_loads_saves =
    {
        .name = "generate_loads_saves",

        .optimize = _optimize_generate_loads_saves,

        .flags = OPTIMIZER_LOCAL,
};
