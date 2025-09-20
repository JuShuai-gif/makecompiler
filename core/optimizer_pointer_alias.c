#include "optimizer.h"
#include "pointer_alias.h"

static int _filter_3ac_by_pointer_alias(_3ac_operand_t * pointer, list_t *prev, basic_block_t *bb, list_t *bb_list_head) {
    basic_block_t *bb2;
    basic_block_t *bb3;
    _3ac_code_t * c2;
    _3ac_code_t * c3;
    list_t *l2;
    list_t *l3;
    list_t h;

    list_init(&h);

    int ret = dag_expr_calculate(&h, pointer->dag_node);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    l3 = prev;

    for (l2 = list_tail(&h); l2 != list_sentinel(&h);) {
        c2 = list_data(l2, _3ac_code_t, list);

        for (; l3 != list_sentinel(&bb->code_list_head);) {
            c3 = list_data(l3, _3ac_code_t, list);

            if (_3ac_code_same(c2, c3)) {
                l3 = list_prev(l3);
                l2 = list_prev(l2);

                list_del(&c3->list);
                list_del(&c2->list);

                _3ac_code_free(c3);
                _3ac_code_free(c2);
                break;
            }

            l3 = list_prev(l3);
        }

        if (l3 == list_sentinel(&bb->code_list_head)) {
            if (list_prev(&bb->list) == list_sentinel(bb_list_head))
                break;

            bb2 = list_data(list_prev(&bb->list), basic_block_t, list);

            if (!bb2->nexts || bb2->nexts->size != 1)
                break;

            if (!bb->prevs || bb->prevs->size != 1)
                break;

            if (bb2->nexts->data[0] != bb || bb->prevs->data[0] != bb2)
                break;

            if (list_empty(&bb->code_list_head)) {
                XCHG(bb2->nexts, bb->nexts);

                int i;
                int j;

                for (i = 0; i < bb2->nexts->size; i++) {
                    bb3 = bb2->nexts->data[i];

                    for (j = 0; j < bb3->prevs->size; j++) {
                        if (bb3->prevs->data[j] == bb) {
                            bb3->prevs->data[j] = bb2;
                            break;
                        }
                    }
                }

                list_del(&bb->list);

                basic_block_free(bb);
                bb = NULL;
            }

            bb = bb2;
            l3 = list_tail(&bb->code_list_head);
        }
    }

    return 0;
}

static int _3ac_pointer_alias(dag_node_t *alias, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    _3ac_operand_t * pointer;

    int ret;

    assert(c->srcs && c->srcs->size >= 1);

    pointer = c->srcs->data[0];

    ret = vector_del(c->srcs, pointer);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

#if 1
    ret = _filter_3ac_by_pointer_alias(pointer, list_prev(&c->list), bb, bb_list_head);
    if (ret < 0)
        return ret;
#endif

    pointer->dag_node = alias;

    if (!c->dsts) {
        c->dsts = vector_alloc();
        if (!c->dsts)
            return -ENOMEM;

        if (vector_add(c->dsts, pointer) < 0)
            return -ENOMEM;
    } else {
        XCHG(c->dsts->data[0], pointer);
        _3ac_operand_free(pointer);
    }
    pointer = NULL;

    switch (c->op->type) {
    case OP_3AC_ASSIGN_DEREFERENCE:
        c->op = _3ac_find_operator(OP_ASSIGN);
        break;
    default:
        loge("\n");
        return -1;
        break;
    };
    assert(c->op);
    return 0;
}

static void __alias_filter_3ac_next2(dag_node_t *dn, dn_status_t *ds, list_t *start, basic_block_t *bb) {
    _3ac_operand_t * dst;
    _3ac_operand_t * src;
    _3ac_code_t * c;
    list_t *l;

    int i;

    for (l = start; l != list_sentinel(&bb->code_list_head); l = list_next(l)) {
        c = list_data(l, _3ac_code_t, list);

        if (c->srcs) {
            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                if (src->dag_node == dn)
                    src->dag_node = ds->alias;
            }
        }

        if (c->dsts) {
            for (i = 0; i < c->dsts->size; i++) {
                dst = c->dsts->data[i];

                if (dst->dag_node == dn)
                    dst->dag_node = ds->alias;
            }
        }
    }
}

static void __alias_filter_3ac_next(dag_node_t *dn, dn_status_t *ds, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    list_t *l;

    __alias_filter_3ac_next2(dn, ds, list_next(&c->list), bb);

    l = list_next(&bb->list);
    if (l != list_sentinel(bb_list_head)) {
        bb = list_data(l, basic_block_t, list);

        __alias_filter_3ac_next2(dn, ds, list_head(&bb->code_list_head), bb);
    }
}

static int _alias_dereference(vector_t **paliases, dag_node_t *dn_pointer, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    _3ac_operand_t * dst;
    dn_status_t *ds;
    variable_t *vd;
    variable_t *va;
    vector_t *aliases;

    int ret;

    aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;

    ret = __alias_dereference(aliases, dn_pointer, c, bb, bb_list_head);
    if (ret < 0) {
        loge("\n");
        vector_free(aliases);
        return ret;
    }

    if (1 == aliases->size) {
        ds = aliases->data[0];

        if (DN_ALIAS_VAR == ds->alias_type) {
            dst = c->dsts->data[0];

            vd = dst->dag_node->var;
            va = ds->alias->var;

            if (!variable_const(va)
                && variable_nb_pointers(vd) == variable_nb_pointers(va)) {
                __alias_filter_3ac_next(dst->dag_node, ds, c, bb, bb_list_head);

                vector_free(aliases);
                aliases = NULL;

                ret = basic_block_inited_vars(bb, bb_list_head);
                if (ret < 0)
                    return ret;
                return 1;
            }
        }
    }

    *paliases = aliases;
    return 0;
}

static int _alias_assign_dereference(vector_t **paliases, dag_node_t *dn_pointer, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    dn_status_t *status;
    vector_t *aliases;
    int ret;

    aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;

    ret = __alias_dereference(aliases, dn_pointer, c, bb, bb_list_head);
    if (ret < 0) {
        vector_free(aliases);
        return ret;
    }

    logd("aliases->size: %d\n", aliases->size);
    if (1 == aliases->size) {
        status = aliases->data[0];

        if (DN_ALIAS_VAR == status->alias_type && !variable_const_integer(status->alias->var)) {
            ret = _3ac_pointer_alias(status->alias, c, bb, bb_list_head);

            vector_free(aliases);
            aliases = NULL;

            if (ret < 0)
                return ret;
            return basic_block_inited_vars(bb, bb_list_head);
        }
    }

    *paliases = aliases;
    return 0;
}

static int __optimize_alias_dereference(_3ac_operand_t * pointer, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    basic_block_t *bb2;
    dn_status_t *ds;
    dag_node_t *dn_pointer;
    dag_node_t *dn_dereference;
    vector_t *aliases;
    list_t *l;

    dn_pointer = pointer->dag_node;
    dn_dereference = dn_pointer;
    aliases = NULL;

    assert(1 == dn_pointer->childs->size);
    dn_pointer = dn_pointer->childs->data[0];

    aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;

    int ret = __alias_dereference(aliases, dn_pointer, c, bb, bb_list_head);
    if (ret < 0) {
        vector_free(aliases);
        return ret;
    }

    if (1 == aliases->size) {
        ds = aliases->data[0];

        if (DN_ALIAS_VAR == ds->alias_type) {
            l = list_prev(&c->list);

            if (l == list_sentinel(&bb->code_list_head)) {
                l = list_prev(&bb->list);

                assert(l != list_sentinel(bb_list_head));

                bb2 = list_data(l, basic_block_t, list);
                l = list_tail(&bb2->code_list_head);
            } else {
                bb2 = bb;
                l = list_prev(&c->list);
            }

            vector_free(aliases);
            aliases = NULL;

            ret = _filter_3ac_by_pointer_alias(pointer, l, bb2, bb_list_head);
            if (ret < 0)
                return ret;

            pointer->dag_node = ds->alias;

            return basic_block_inited_vars(bb, bb_list_head);
        }
    }

    ret = _bb_copy_aliases(bb, dn_pointer, dn_dereference, aliases);

    vector_free(aliases);
    aliases = NULL;

    if (ret < 0)
        return ret;
    return 1;
}

static int __optimize_alias_bb(list_t **pend, list_t *start, basic_block_t *bb, list_t *bb_list_head) {
    list_t *l;
    _3ac_code_t * c;
    _3ac_operand_t * pointer;
    _3ac_operand_t * dst;
    dn_status_t *ds;
    dag_node_t *dn_pointer;
    dag_node_t *dn_dereference;
    vector_t *aliases;

    int ret = 0;

    for (l = start; l != *pend;) {
        c = list_data(l, _3ac_code_t, list);
        l = list_next(l);

        if (!type_is_assign_dereference(c->op->type)
            && OP_DEREFERENCE != c->op->type
            && OP_3AC_TEQ != c->op->type
            && OP_3AC_CMP != c->op->type)
            continue;

        assert(c->srcs && c->srcs->size >= 1);

        int flag = 0;
        int i;
        for (i = 0; i < c->srcs->size; i++) {
            pointer = c->srcs->data[i];
            dn_pointer = pointer->dag_node;
            aliases = NULL;
            dn_dereference = NULL;

            if (dn_pointer->var->arg_flag) {
                variable_t *v = dn_pointer->var;
                logd("arg: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
                continue;
            }

            if (dn_pointer->var->global_flag) {
                variable_t *v = dn_pointer->var;
                logd("global: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
                continue;
            }

            if (OP_3AC_TEQ == c->op->type || OP_3AC_CMP == c->op->type) {
                if (OP_DEREFERENCE != dn_pointer->type)
                    continue;

                ret = __optimize_alias_dereference(pointer, c, bb, bb_list_head);
                if (ret < 0)
                    return ret;

                flag += ret;

            } else if (OP_DEREFERENCE == c->op->type) {
#if 1
                assert(c->dsts && 1 == c->dsts->size);
                dst = c->dsts->data[0];
                dn_dereference = dst->dag_node;

                ret = _alias_dereference(&aliases, dn_pointer, c, bb, bb_list_head);
                if (1 == ret) {
                    list_del(&c->list);
                    _3ac_code_free(c);
                    c = NULL;
                    break;
                }
#endif
            } else {
                if (i > 0)
                    break;

                if (c->srcs->size > 1) {
                    pointer = c->srcs->data[1];
                    dn_pointer = pointer->dag_node;

                    if (OP_DEREFERENCE == dn_pointer->type) {
                        ret = __optimize_alias_dereference(pointer, c, bb, bb_list_head);
                        if (ret < 0)
                            return ret;

                        flag += ret;
                    }
                }

                pointer = c->srcs->data[0];
                dn_pointer = pointer->dag_node;
                dn_dereference = NULL;

                ret = _alias_assign_dereference(&aliases, dn_pointer, c, bb, bb_list_head);
            }

            if (ret < 0)
                return ret;

            if (aliases) {
                logd("bb: %p, bb->index: %d, aliases->size: %d\n", bb, bb->index, aliases->size);
                variable_t *v = dn_pointer->var;
                logd("v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

                ret = _bb_copy_aliases(bb, dn_pointer, dn_dereference, aliases);
                vector_free(aliases);
                aliases = NULL;
                if (ret < 0)
                    return ret;

                flag = 1;
            }
        }

        if (flag) {
            *pend = &c->list;
            break;
        }
    }

    return 0;
}

static int _optimize_alias_bb(basic_block_t *bb, list_t *bb_list_head) {
    list_t *start;
    list_t *end;

    int ret;

    do {
        start = list_head(&bb->code_list_head);
        end = list_sentinel(&bb->code_list_head);

        ret = __optimize_alias_bb(&end, start, bb, bb_list_head);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        if (end == list_sentinel(&bb->code_list_head))
            break;

        bb->dereference_flag = 1;

        if (list_next(end) == list_sentinel(&bb->code_list_head))
            break;

        basic_block_t *bb2 = NULL;

        int ret = basic_block_split(bb, &bb2);
        if (ret < 0)
            return ret;

        bb2->ret_flag = bb->ret_flag;
        bb->ret_flag = 0;

        bb2->dereference_flag = 0;
        bb2->array_index_flag = bb->array_index_flag;

        basic_block_mov_code(bb2, list_next(end), bb);

        list_add_front(&bb->list, &bb2->list);

        bb = bb2;
    } while (1);

    return 0;
}

static int _optimize_pointer_alias(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    if (list_empty(bb_list_head))
        return 0;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head);) {
        bb = list_data(l, basic_block_t, list);
        l = list_next(l);

        if (bb->jmp_flag || bb->end_flag)
            continue;

        if (!bb->dereference_flag)
            continue;

        int ret = _optimize_alias_bb(bb, bb_list_head);
        if (ret < 0)
            return ret;
    }

    //	 basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_pointer_alias =
    {
        .name = "pointer_alias",

        .optimize = _optimize_pointer_alias,

        .flags = OPTIMIZER_LOCAL,
};
