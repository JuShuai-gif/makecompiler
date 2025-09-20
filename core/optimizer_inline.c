#include "optimizer.h"

static int _arg_cmp(const void *p0, const void *p1) {
    node_t *n0 = (node_t *)p0;
    node_t *n1 = (node_t *)p1;

    variable_t *v0 = _operand_get(n0);
    variable_t *v1 = _operand_get(n1);

    return v0 != v1;
}

static int _find_argv(vector_t *argv, vector_t *operands) {
    _3ac_operand_t *operand;
    variable_t *v;

    int i;
    for (i = 0; i < operands->size; i++) {
        operand = operands->data[i];
        v = _operand_get(operand->node);

        if (!v->arg_flag)
            continue;

        if (vector_find_cmp(argv, operand->node, _arg_cmp))
            continue;

        if (vector_add(argv, operand->node) < 0)
            return -ENOMEM;
    }

    return 0;
}

static int _copy_codes(list_t *hbb, vector_t *argv, _3ac_code_t *c, function_t *f, function_t *f2) {
    basic_block_t *bb;
    basic_block_t *bb2;
    basic_block_t *bb3;
    _3ac_operand_t *src;
    _3ac_operand_t *dst;
    _3ac_code_t *c2;
    _3ac_code_t *c3;
    variable_t *v;
    list_t *l;
    list_t *l2;
    list_t *l3;

    for (l = list_head(&f2->basic_block_list_head); l != list_sentinel(&f2->basic_block_list_head);) {
        bb = list_data(l, basic_block_t, list);
        l = list_next(l);

        bb2 = basic_block_alloc();
        if (!bb2)
            return -ENOMEM;
        bb2->index = bb->index;

        vector_free(bb2->prevs);
        vector_free(bb2->nexts);

        bb2->prevs = vector_clone(bb->prevs);
        bb2->nexts = vector_clone(bb->nexts);

        bb2->call_flag = bb->call_flag;
        bb2->cmp_flag = bb->cmp_flag;
        bb2->jmp_flag = bb->jmp_flag;
        bb2->jcc_flag = bb->jcc_flag;
        bb2->ret_flag = 0;
        bb2->end_flag = 0;

        bb2->varg_flag = bb->varg_flag;
        bb2->jmp_dst_flag = bb->jmp_dst_flag;

        bb2->dereference_flag = bb->dereference_flag;
        bb2->array_index_flag = bb->array_index_flag;

        list_add_tail(hbb, &bb2->list);

        for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);) {
            c2 = list_data(l2, _3ac_code_t, list);
            l2 = list_next(l2);

            if (OP_3AC_END == c2->op->type)
                continue;

            if (OP_RETURN == c2->op->type && c2->srcs && c->dsts) {
                int i;
                int min = c2->srcs->size;

                if (c2->srcs->size > c->dsts->size)
                    min = c->dsts->size;

                for (i = 0; i < min; i++) {
                    src = c2->srcs->data[i];
                    dst = c->dsts->data[i];
                    v = _operand_get(dst->node);

                    dst->node->type = v->type;
                    dst->node->var = v;
                    dst->node->result = NULL;
                    dst->node->split_flag = 0;

                    c3 = _3ac_code_NN(OP_ASSIGN, &dst->node, 1, &src->node, 1);
                    list_add_tail(&bb2->code_list_head, &c3->list);
                }
            } else {
                c3 = _3ac_code_clone(c2);
                if (!c3)
                    return -ENOMEM;
                list_add_tail(&bb2->code_list_head, &c3->list);
            }
            c3->basic_block = bb2;

            if (type_is_jmp(c3->op->type))
                continue;

            if (c3->srcs) {
                int ret = _find_argv(argv, c3->srcs);
                if (ret < 0)
                    return ret;
            }

            if (c3->dsts) {
                int ret = _find_argv(argv, c3->dsts);
                if (ret < 0)
                    return ret;
            }
        }
    }

    for (l = list_head(hbb); l != list_sentinel(hbb); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        int i;
        for (i = 0; i < bb->prevs->size; i++) {
            bb2 = bb->prevs->data[i];

            for (l3 = list_head(hbb); l3 != list_sentinel(hbb); l3 = list_next(l3)) {
                bb3 = list_data(l3, basic_block_t, list);

                if (bb2->index == bb3->index) {
                    bb->prevs->data[i] = bb3;
                    break;
                }
            }
        }

        for (i = 0; i < bb->nexts->size; i++) {
            bb2 = bb->nexts->data[i];

            for (l3 = list_head(hbb); l3 != list_sentinel(hbb); l3 = list_next(l3)) {
                bb3 = list_data(l3, basic_block_t, list);

                if (bb2->index == bb3->index) {
                    bb->nexts->data[i] = bb3;
                    break;
                }
            }
        }

        for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);) {
            c2 = list_data(l2, _3ac_code_t, list);
            l2 = list_next(l2);

            if (!type_is_jmp(c2->op->type))
                continue;

            dst = c2->dsts->data[0];

            for (l3 = list_head(hbb); l3 != list_sentinel(hbb); l3 = list_next(l3)) {
                bb3 = list_data(l3, basic_block_t, list);

                if (dst->bb->index == bb3->index) {
                    dst->bb = bb3;
                    break;
                }
            }

            if (vector_add(f->jmps, c2) < 0)
                return -ENOMEM;
        }
    }

    return 0;
}

static int _find_local_vars(node_t *node, void *arg, vector_t *results) {
    block_t *b = (block_t *)node;

    if ((OP_BLOCK == b->node.type || FUNCTION == b->node.type) && b->scope) {
        int i;
        for (i = 0; i < b->scope->vars->size; i++) {
            variable_t *var = b->scope->vars->data[i];

            int ret = vector_add(results, var);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int _do_inline(ast_t *ast, _3ac_code_t *c, basic_block_t **pbb, function_t *f, function_t *f2) {
    basic_block_t *bb = *pbb;
    basic_block_t *bb2;
    basic_block_t *bb_next;
    _3ac_operand_t *src;
    _3ac_code_t *c2;
    variable_t *v2;
    vector_t *argv;
    node_t *node;
    list_t *l;

    list_t hbb;

    assert(c->srcs->size - 1 == f2->argv->size);

    list_init(&hbb);

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    _copy_codes(&hbb, argv, c, f, f2);

    int i;
    int j;
    for (i = 0; i < argv->size; i++) {
        node = argv->data[i];

        for (j = 0; j < f2->argv->size; j++) {
            v2 = f2->argv->data[j];

            if (_operand_get(node) == v2)
                break;
        }

        assert(j < f2->argv->size);

        src = c->srcs->data[j + 1];

        c2 = _3ac_code_NN(OP_ASSIGN, &node, 1, &src->node, 1);

        list_add_tail(&c->list, &c2->list);

        c2->basic_block = bb;
    }

    vector_clear(argv, NULL);

    int ret = node_search_bfs((node_t *)f2, NULL, argv, -1, _find_local_vars);
    if (ret < 0) {
        vector_free(argv);
        return -ENOMEM;
    }

    for (i = 0; i < argv->size; i++) {
        v2 = argv->data[i];

        ret = vector_add_unique(f->scope->vars, v2);
        if (ret < 0) {
            vector_free(argv);
            return -ENOMEM;
        }
    }

    vector_free(argv);
    argv = NULL;

    l = list_tail(&hbb);
    bb2 = list_data(l, basic_block_t, list);
    *pbb = bb2;

    XCHG(bb->nexts, bb2->nexts);
    bb2->end_flag = 0;

    for (i = 0; i < bb2->nexts->size; i++) {
        bb_next = bb2->nexts->data[i];

        int j;
        for (j = 0; j < bb_next->prevs->size; j++) {
            if (bb_next->prevs->data[j] == bb) {
                bb_next->prevs->data[j] = bb2;
                break;
            }
        }
    }

    int nblocks = 0;

    while (l != list_sentinel(&hbb)) {
        bb2 = list_data(l, basic_block_t, list);
        l = list_prev(l);

        list_del(&bb2->list);
        list_add_front(&bb->list, &bb2->list);

        nblocks++;
    }

    if (bb2->jmp_flag || bb2->jmp_dst_flag) {
        if (vector_add(bb->nexts, bb2) < 0)
            return -ENOMEM;

        if (vector_add(bb2->prevs, bb) < 0)
            return -ENOMEM;

    } else {
        for (l = list_head(&bb2->code_list_head); l != list_sentinel(&bb2->code_list_head);) {
            c2 = list_data(l, _3ac_code_t, list);
            l = list_next(l);

            list_del(&c2->list);
            list_add_tail(&c->list, &c2->list);

            c2->basic_block = bb;

            if (OP_CALL == c2->op->type)
                bb->call_flag = 1;
        }

        XCHG(bb->nexts, bb2->nexts);

        for (i = 0; i < bb->nexts->size; i++) {
            bb_next = bb->nexts->data[i];

            int j;
            for (j = 0; j < bb_next->prevs->size; j++) {
                if (bb_next->prevs->data[j] == bb2) {
                    bb_next->prevs->data[j] = bb;
                    break;
                }
            }
        }

        list_del(&bb2->list);
        basic_block_free(bb2);
        bb2 = NULL;

        if (1 == nblocks)
            *pbb = bb;
    }
    return 0;
}

static int _optimize_inline2(ast_t *ast, function_t *f) {
    basic_block_t *bb;
    basic_block_t *bb_cur;
    basic_block_t *bb2;
    _3ac_operand_t *src;
    _3ac_code_t *c;
    variable_t *v;
    function_t *f2;
    list_t *l;
    list_t *l2;

    bb_cur = NULL;

    for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);) {
        bb = list_data(l, basic_block_t, list);
        l = list_next(l);

        if (!bb->call_flag)
            continue;

        int n_calls = 0;

        bb_cur = bb;

        for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);) {
            c = list_data(l2, _3ac_code_t, list);
            l2 = list_next(l2);

            if (bb_cur != bb) {
                list_del(&c->list);
                list_add_tail(&bb_cur->code_list_head, &c->list);

                c->basic_block = bb_cur;
            }

            if (OP_CALL != c->op->type)
                continue;

            n_calls++;

            src = c->srcs->data[0];
            v = _operand_get(src->node);

            if (!v->const_literal_flag)
                continue;

            f2 = v->func_ptr;

            if (!f2->node.define_flag)
                continue;

            if (!f2->inline_flag)
                continue;

            if (f2->vargs_flag)
                continue;

            if (f2->nb_basic_blocks > 10)
                continue;
#if 1
            bb2 = bb_cur;
            bb_cur->call_flag = 0;

            n_calls--;

            int ret = _do_inline(ast, c, &bb_cur, f, f2);
            if (ret < 0)
                return ret;

            list_del(&c->list);
            _3ac_code_free(c);
            c = NULL;

            bb2->call_flag |= n_calls > 0;

            if (bb2->ret_flag) {
                bb2->ret_flag = 0;
                bb_cur->ret_flag = 1;
            }
#endif
        }

        bb_cur->call_flag |= n_calls > 0;
    }

#if 0
	if (bb_cur)
		 basic_block_print_list(&f->basic_block_list_head);
#endif
    return 0;
}

static int _optimize_inline(ast_t *ast, function_t *f, vector_t *functions) {
    if (!ast || !functions)
        return -EINVAL;

    int i;
    for (i = 0; i < functions->size; i++) {
        int ret = _optimize_inline2(ast, functions->data[i]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    return 0;
}

optimizer_t optimizer_inline =
    {
        .name = "inline",

        .optimize = _optimize_inline,

        .flags = OPTIMIZER_GLOBAL,
};
