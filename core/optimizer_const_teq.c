#include "optimizer.h"

static void __bb_dfs_del(basic_block_t *bb, function_t *f) {
    _3ac_operand_t * dst;
    basic_block_t *bb2;
    basic_block_t *bb_prev;
    basic_block_t *bb_next;
    _3ac_code_t * c;
    list_t *l;
    list_t *l2;
    list_t *sentinel = list_sentinel(&f->basic_block_list_head);

    for (l = list_next(&bb->list); l != sentinel;) {
        bb2 = list_data(l, basic_block_t, list);
        l = list_next(l);

        if (!bb2->jmp_flag)
            break;

        l2 = list_head(&bb2->code_list_head);
        c = list_data(l2, _3ac_code_t, list);

        assert(0 == vector_del(f->jmps, c));

        list_del(&bb2->list);
        basic_block_free(bb2);
        bb2 = NULL;
    }

    int i;
    for (i = 0; i < bb->nexts->size;) {
        bb2 = bb->nexts->data[i];

        assert(0 == vector_del(bb->nexts, bb2));

        if (bb2->prevs->size > 1) {
            assert(0 == vector_del(bb2->prevs, bb));
            continue;
        }

        assert(&bb2->list != list_head(&f->basic_block_list_head));

        __bb_dfs_del(bb2, f);

        assert(0 == vector_del(bb2->prevs, bb));

        logd("bb2: %#lx, bb2->index: %d, prevs->size: %d\n", 0xffff & (uintptr_t)bb2, bb2->index, bb2->prevs->size);

        list_del(&bb2->list);
        basic_block_free(bb2);
        bb2 = NULL;
    }

    if (list_prev(&bb->list) != sentinel && list_next(&bb->list) != sentinel) {
        bb_prev = list_data(list_prev(&bb->list), basic_block_t, list);
        bb_next = list_data(list_next(&bb->list), basic_block_t, list);

        if (bb_prev->jmp_flag) {
            l2 = list_head(&bb_prev->code_list_head);
            c = list_data(l2, _3ac_code_t, list);

            assert(c->dsts && 1 == c->dsts->size);
            dst = c->dsts->data[0];

            if (dst->bb == bb_next) {
                assert(0 == vector_del(f->jmps, c));

                list_del(&bb_prev->list);
                basic_block_free(bb_prev);
                bb_prev = NULL;
            }
        }
    }
}

static int __optimize_const_teq(basic_block_t *bb, function_t *f) {
    basic_block_t *bb2;

    _3ac_operand_t * src;
    _3ac_operand_t * dst;
    dag_node_t *dn;
    variable_t *v;
    _3ac_code_t * c;
    list_t *l;

    int flag = -1;

    for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head);) {
        c = list_data(l, _3ac_code_t, list);
        l = list_next(l);

        if (OP_3AC_TEQ == c->op->type) {
            assert(c->srcs && 1 == c->srcs->size);

            src = c->srcs->data[0];
            dn = src->dag_node;
            v = dn->var;

            if (v->const_flag && 0 == v->nb_pointers && 0 == v->nb_dimentions) {
                flag = !!v->data.i;

                list_del(&c->list);
                _3ac_code_free(c);
                c = NULL;

            } else if (v->const_literal_flag) {
                flag = 1;

                list_del(&c->list);
                _3ac_code_free(c);
                c = NULL;
            }

        } else if (flag >= 0) {
            int flag2;

            if (OP_3AC_SETZ == c->op->type)
                flag2 = !flag;
            else if (OP_3AC_SETNZ == c->op->type)
                flag2 = flag;
            else {
                loge("\n");
                return -EINVAL;
            }

            assert(c->dsts && 1 == c->dsts->size);

            dst = c->dsts->data[0];
            dn = dst->dag_node;
            v = dn->var;

            v->const_flag = 1;
            v->data.i = flag2;

            list_del(&c->list);
            _3ac_code_free(c);
            c = NULL;
        }
    }

    if (flag < 0)
        return 0;

    int jmp_flag = 0;
    bb2 = NULL;

    for (l = list_next(&bb->list); l != list_sentinel(&f->basic_block_list_head);) {
        bb2 = list_data(l, basic_block_t, list);
        l = list_next(l);

        if (!bb2->jmp_flag)
            break;

        list_t *l2;

        l2 = list_head(&bb2->code_list_head);
        c = list_data(l2, _3ac_code_t, list);

        if (!jmp_flag) {
            if (OP_3AC_JZ == c->op->type) {
                if (0 == flag) {
                    c->op = _3ac_find_operator(OP_GOTO);
                    jmp_flag = 1;
                    continue;
                }

            } else if (OP_3AC_JNZ == c->op->type) {
                if (1 == flag) {
                    c->op = _3ac_find_operator(OP_GOTO);
                    jmp_flag = 1;
                    continue;
                }

            } else if (OP_GOTO == c->op->type) {
                jmp_flag = 1;
                continue;
            } else {
                loge("\n");
                return -EINVAL;
            }
        }

        assert(c->dsts && 1 == c->dsts->size);
        dst = c->dsts->data[0];

        assert(0 == vector_del(dst->bb->prevs, bb));
        assert(0 == vector_del(f->jmps, c));

        list_del(&bb2->list);
        basic_block_free(bb2);
        bb2 = NULL;
    }

    if (jmp_flag && bb2 && !bb2->jmp_flag) {
        vector_del(bb2->prevs, bb);
    }

    int ret;
    int i;

    for (i = 0; i < bb->nexts->size;) {
        bb2 = bb->nexts->data[i];

        if (vector_find(bb2->prevs, bb)) {
            i++;
            continue;
        }

        assert(0 == vector_del(bb->nexts, bb2));

        if (0 == bb2->prevs->size && &bb2->list != list_head(&f->basic_block_list_head)) {
            assert(0 == vector_add(bb2->prevs, bb));

            __bb_dfs_del(bb2, f);

            assert(0 == vector_del(bb2->prevs, bb));

            list_del(&bb2->list);
            basic_block_free(bb2);
            bb2 = NULL;
        }
    }

    return 0;
}

static int _optimize_const_teq(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;
    basic_block_t *bb2;

    if (list_empty(bb_list_head))
        return 0;

    logd("------- %s() ------\n", f->node.w->text->data);

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        if (!bb->cmp_flag)
            continue;

        int ret = __optimize_const_teq(bb, f);
        if (ret < 0)
            return ret;
    }

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head);) {
        bb = list_data(l, basic_block_t, list);

        l = list_next(l);

        if (!bb->cmp_flag)
            continue;

        if (0 == bb->prevs->size && list_empty(&bb->code_list_head)) {
            int i;
            for (i = 0; i < bb->nexts->size;) {
                bb2 = bb->nexts->data[i];

                assert(0 == vector_del(bb->nexts, bb2));
                assert(0 == vector_del(bb2->prevs, bb));
            }

            list_del(&bb->list);
            basic_block_free(bb);
            bb = NULL;
        }
    }

    return 0;
}

optimizer_t optimizer_const_teq =
    {
        .name = "const_teq",

        .optimize = _optimize_const_teq,

        .flags = OPTIMIZER_LOCAL,
};
