#include "optimizer.h"
#include "pointer_alias.h"

#include "auto_gc_3ac.c"

int __auto_gc_ds_for_assign(dn_status_t **ds, dag_node_t **dn, _3ac_code_t *c);

static int _find_ds_malloced(basic_block_t *bb, void *data) {
    dn_status_t *ds = data;

    if (!vector_find_cmp(bb->ds_malloced, ds, ds_cmp_same_indexes))
        return 0;

    if (vector_find_cmp(bb->ds_freed, ds, ds_cmp_same_indexes)) {
        loge("error free dn: \n");
        return -1;
    }

    if (vector_find(bb->dn_updateds, ds->dag_node))
        return 1;
    return 0;
}

static int __find_dn_active(vector_t *dn_vec, dag_node_t *dn) {
    dn_status_t *ds = NULL;
    dag_node_t *dn2;
    int i;

    for (i = 0; i < dn_vec->size; i++) {
        dn2 = dn_vec->data[i];

        if (dn2 == dn)
            return 1;

        int ret = ds_for_dn(&ds, dn2);
        if (ret < 0)
            return ret;

        if (ds->dag_node == dn) {
            dn_status_free(ds);
            return 1;
        }

        dn_status_free(ds);
        ds = NULL;
    }

    return 0;
}

static int _find_dn_active(basic_block_t *bb, void *data) {
    dag_node_t *dn = data;

    int ret = __find_dn_active(bb->dn_loads, dn);
    if (0 == ret) {
        ret = __find_dn_active(bb->dn_reloads, dn);

        if (0 == ret)
            ret = __find_dn_active(bb->entry_dn_actives, dn);
    }

    logd("bb: %p, dn: %s, 0\n", bb, dn->var->w->text->data);
    return ret;
}

static int _bb_find_ds_malloced(basic_block_t *root, list_t *bb_list_head, dn_status_t *ds, vector_t *results) {
    basic_block_visit_flag(bb_list_head, 0);

    return basic_block_search_dfs_prev(root, _find_ds_malloced, ds, results);
}

static int _bb_find_dn_active(basic_block_t *root, list_t *bb_list_head, dag_node_t *dn, vector_t *results) {
    basic_block_visit_flag(bb_list_head, 0);

    return basic_block_search_dfs_prev(root, _find_dn_active, dn, results);
}

static int _bb_prev_add_active(basic_block_t *bb, void *data, vector_t *queue) {
    basic_block_t *bb_prev;
    dag_node_t *dn = data;

    int count = 0;
    int ret;
    int j;

    for (j = 0; j < bb->prevs->size; j++) {
        bb_prev = bb->prevs->data[j];

        if (!vector_find(bb_prev->exit_dn_aliases, dn)) {
            ret = vector_add_unique(bb_prev->exit_dn_actives, dn);
            if (ret < 0)
                return ret;
        }

        if (vector_find(bb_prev->dn_updateds, dn)) {
            if (vector_find(bb_prev->exit_dn_aliases, dn)
                || type_is_operator(dn->type))

                ret = vector_add_unique(bb_prev->dn_resaves, dn);
            else
                ret = vector_add_unique(bb_prev->dn_saves, dn);

            if (ret < 0)
                return ret;
        }

        ++count;

        ret = vector_add(queue, bb_prev);
        if (ret < 0)
            return ret;
    }
    return count;
}

static int _bb_add_active(basic_block_t *bb, dag_node_t *dn) {
    int ret = vector_add_unique(bb->entry_dn_actives, dn);
    if (ret < 0)
        return ret;

    if (type_is_operator(dn->type))
        ret = vector_add(bb->dn_reloads, dn);
    //	else
    //		ret =   vector_add(bb->dn_loads, dn);

    return ret;
}

static int _bb_add_free_arry(ast_t *ast, function_t *f, basic_block_t *bb, dag_node_t *dn_array) {
    basic_block_t *bb1 = NULL;

    int ret = basic_block_split(bb, &bb1);
    if (ret < 0)
        return ret;

    list_add_front(&bb->list, &bb1->list);

    if (bb->end_flag) {
        basic_block_mov_code(bb1, list_head(&bb->code_list_head), bb);

        bb1->ret_flag = bb->ret_flag;
        bb1->end_flag = 1;
        bb->end_flag = 0;
        bb->call_flag = 1;

        bb1 = bb;
    } else {
        bb1->call_flag = 1;
    }

    ret = _bb_add_gc_code_free_array(&f->dag_list_head, ast, bb1, dn_array);
    if (ret < 0)
        return ret;

    ret = _bb_add_active(bb1, dn_array);
    if (ret < 0)
        return ret;

    return basic_block_search_bfs(bb1, _bb_prev_add_active, dn_array);
}

static int _bb_add_memset_array(ast_t *ast, function_t *f, dag_node_t *dn_array) {
    basic_block_t *bb = NULL;
    basic_block_t *bb1 = NULL;
    list_t *l;

    l = list_head(&f->basic_block_list_head);
    bb = list_data(l, basic_block_t, list);

    int ret = basic_block_split(bb, &bb1);
    if (ret < 0)
        return ret;

    list_add_front(&bb->list, &bb1->list);

    basic_block_mov_code(bb1, list_head(&bb->code_list_head), bb);

    bb1->call_flag = 1;
    bb1->ret_flag = bb->ret_flag;
    bb1->end_flag = bb->end_flag;

    bb1 = bb;

    ret = _bb_add_gc_code_memset_array(&f->dag_list_head, ast, bb1, dn_array);
    if (ret < 0)
        return ret;

    ret = _bb_add_active(bb1, dn_array);
    if (ret < 0)
        return ret;

    return basic_block_search_bfs(bb1, _bb_prev_add_active, dn_array);
}

static int _bb_split_prev_add_free(ast_t *ast, function_t *f, basic_block_t *bb, dn_status_t *ds, vector_t *bb_split_prevs) {
    basic_block_t *bb1;
    basic_block_t *bb2;
    basic_block_t *bb3;
    dag_node_t *dn = ds->dag_node;
    list_t *bb_list_head = &f->basic_block_list_head;

    bb1 = basic_block_alloc();
    if (!bb1) {
        vector_free(bb_split_prevs);
        return -ENOMEM;
    }

    vector_free(bb1->prevs);
    bb1->prevs = bb_split_prevs;
    bb_split_prevs = NULL;

    bb1->ds_auto_gc = dn_status_clone(ds);
    if (!bb1->ds_auto_gc) {
        basic_block_free(bb1);
        return -ENOMEM;
    }

    int ret = vector_add(bb1->nexts, bb);
    if (ret < 0) {
        basic_block_free(bb1);
        return ret;
    }

    bb1->call_flag = 1;
    bb1->auto_free_flag = 1;

    list_add_tail(&bb->list, &bb1->list);

    _3ac_operand_t *dst;
    _3ac_code_t *c;
    list_t *l;
    int j;
    int k;

    for (j = 0; j < bb1->prevs->size; j++) {
        bb2 = bb1->prevs->data[j];

        assert(0 == vector_del(bb->prevs, bb2));

        for (k = 0; k < bb2->nexts->size; k++) {
            if (bb2->nexts->data[k] == bb)
                bb2->nexts->data[k] = bb1;
        }
    }

    for (j = 0; j < bb->prevs->size; j++) {
        bb2 = bb->prevs->data[j];

        for (l = list_next(&bb2->list); l != list_sentinel(bb_list_head);
             l = list_next(l)) {
            bb3 = list_data(l, basic_block_t, list);

            if (bb3->jcc_flag)
                continue;

            if (bb3->jmp_flag)
                break;

            if (bb3 == bb1) {
                bb3 = basic_block_jcc(bb, f, OP_GOTO);
                if (!bb3)
                    return -ENOMEM;

                list_add_tail(&bb1->list, &bb3->list);
            }
            break;
        }
    }
    vector_add(bb->prevs, bb1);

    for (j = 0; j < bb1->prevs->size; j++) {
        bb2 = bb1->prevs->data[j];

        dn_status_t *ds2;
        list_t *l2;

        for (l = list_next(&bb2->list); l != &bb1->list && l != list_sentinel(bb_list_head);
             l = list_next(l)) {
            bb3 = list_data(l, basic_block_t, list);

            if (!bb3->jmp_flag)
                break;

            l2 = list_head(&bb3->code_list_head);
            c = list_data(l2, _3ac_code_t, list);
            dst = c->dsts->data[0];

            if (dst->bb == bb)
                dst->bb = bb1;

            assert(list_next(l2) == list_sentinel(&bb3->code_list_head));
        }

        for (k = 0; k < bb2->ds_malloced->size; k++) {
            ds2 = bb2->ds_malloced->data[k];

            if (0 == ds_cmp_same_indexes(ds2, ds))
                continue;

            if (vector_find_cmp(bb2->ds_freed, ds2, ds_cmp_same_indexes))
                continue;

            if (vector_find_cmp(bb1->ds_malloced, ds2, ds_cmp_same_indexes))
                continue;

            dn_status_t *ds3 = dn_status_clone(ds2);
            if (!ds3)
                return -ENOMEM;

            ret = vector_add(bb1->ds_malloced, ds3);
            if (ret < 0)
                return ret;
        }
    }

    ret = _bb_add_gc_code_freep(&f->dag_list_head, ast, bb1, ds);
    if (ret < 0)
        return ret;

    ret = _bb_add_active(bb1, dn);
    if (ret < 0)
        return ret;

    return basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
}

static int _bb_split_prevs(basic_block_t *bb, dn_status_t *ds, vector_t *bb_split_prevs) {
    basic_block_t *bb_prev;
    int i;

    for (i = 0; i < bb->prevs->size; i++) {
        bb_prev = bb->prevs->data[i];

        if (!vector_find_cmp(bb_prev->ds_malloced, ds, ds_cmp_like_indexes))
            continue;

        if (vector_find_cmp(bb_prev->ds_freed, ds, ds_cmp_same_indexes))
            continue;

        int ret = vector_add(bb_split_prevs, bb_prev);
        if (ret < 0)
            return ret;
    }
    return 0;
}

static int _bb_add_last_free(dn_status_t *ds, basic_block_t *bb, ast_t *ast, function_t *f) {
    vector_t *vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    basic_block_t *bb2;
    int j;
    int dfo = 0;

    int ret = _bb_find_ds_malloced(bb, &f->basic_block_list_head, ds, vec);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

#define AUTO_GC_FIND_MAX_DFO()            \
    do {                                  \
        for (j = 0; j < vec->size; j++) { \
            bb2 = vec->data[j];           \
                                          \
            if (bb2->dfo > dfo)           \
                dfo = bb2->dfo;           \
        }                                 \
    } while (0)
    AUTO_GC_FIND_MAX_DFO();
    vec->size = 0;

    ret = _bb_find_dn_active(bb, &f->basic_block_list_head, ds->dag_node, vec);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }
    AUTO_GC_FIND_MAX_DFO();
    vec->size = 0;

    for (j = 0; j < bb->dominators->size; j++) {
        bb2 = bb->dominators->data[j];

        if (bb2->dfo > dfo)
            break;
    }

    ret = _bb_split_prevs(bb2, ds, vec);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

    return _bb_split_prev_add_free(ast, f, bb2, ds, vec);
}

static int _auto_gc_last_free(ast_t *ast, function_t *f) {
    list_t *l = list_tail(&f->basic_block_list_head);
    basic_block_t *bb;
    dn_status_t *ds;
    dag_node_t *dn;
    variable_t *v;
    vector_t *local_arrays;

    bb = list_data(l, basic_block_t, list);

    logd("bb: %p, bb->ds_malloced->size: %d\n", bb, bb->ds_malloced->size);

    local_arrays = vector_alloc();
    if (!local_arrays)
        return -ENOMEM;

    int ret;
    int i;

    for (i = 0; i < bb->ds_malloced->size; i++) {
        ds = bb->ds_malloced->data[i];

        dn = ds->dag_node;
        v = dn->var;

        logw("%s(), last free: v_%d_%d/%s, ds->ret_flag: %u\n",
             f->node.w->text->data, v->w->line, v->w->pos, v->w->text->data, ds->ret_flag);
        dn_status_print(ds);

        if (ds->ret_flag)
            continue;

        if (ds->dn_indexes) {
            int j;
            dn_index_t *di;

            for (j = ds->dn_indexes->size - 1; j >= 0; j--) {
                di = ds->dn_indexes->data[j];

                if (di->member) {
                    assert(di->member->member_flag && v->type >= STRUCT);
                    break;
                }

                if (-1 == di->index) {
                    if (v->nb_dimentions > 0 && v->local_flag) {
                        ret = vector_add_unique(local_arrays, dn);
                        if (ret < 0)
                            goto error;
                    }
                    break;
                }
            }

            if (j >= 0)
                continue;
        }

        ret = _bb_add_last_free(ds, bb, ast, f);
        if (ret < 0)
            goto error;
    }

    for (i = 0; i < local_arrays->size; i++) {
        dn = local_arrays->data[i];

        ret = _bb_add_memset_array(ast, f, dn);
        if (ret < 0)
            goto error;

        ret = _bb_add_free_arry(ast, f, bb, dn);
        if (ret < 0)
            goto error;
    }

    ret = 0;
error:
    vector_free(local_arrays);
    return ret;
}

#define AUTO_GC_BB_SPLIT(parent, child)                                 \
    do {                                                                \
        int ret = basic_block_split(parent, &child);                    \
        if (ret < 0)                                                    \
            return ret;                                                 \
                                                                        \
        child->call_flag = parent->call_flag;                           \
        child->dereference_flag = parent->dereference_flag;             \
                                                                        \
        XCHG(parent->ds_malloced, child->ds_malloced);                  \
                                                                        \
        vector_free(child->exit_dn_actives);                            \
        vector_free(child->exit_dn_aliases);                            \
                                                                        \
        child->exit_dn_actives = vector_clone(parent->exit_dn_actives); \
        child->exit_dn_aliases = vector_clone(parent->exit_dn_aliases); \
                                                                        \
        list_add_front(&parent->list, &child->list);                    \
    } while (0)

static int _auto_gc_bb_split(basic_block_t *parent, basic_block_t **pchild) {
    basic_block_t *child = NULL;

    AUTO_GC_BB_SPLIT(parent, child);

    *pchild = child;
    return 0;
}

static int _auto_gc_bb_ref(dn_status_t *ds_obj, vector_t *ds_malloced, ast_t *ast, function_t *f, basic_block_t **pbb) {
    basic_block_t *cur_bb = *pbb;
    basic_block_t *bb1 = NULL;
    dag_node_t *dn = ds_obj->dag_node;

    AUTO_GC_BB_SPLIT(cur_bb, bb1);

    bb1->ds_auto_gc = dn_status_clone(ds_obj);
    if (!bb1->ds_auto_gc)
        return -ENOMEM;

    int ret = _bb_add_gc_code_ref(&f->dag_list_head, ast, bb1, ds_obj);
    if (ret < 0)
        return ret;

    ret = _bb_add_active(bb1, dn);
    if (ret < 0)
        return ret;

    ret = basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
    if (ret < 0)
        return ret;

    if (!vector_find_cmp(ds_malloced, ds_obj, ds_cmp_like_indexes)) {
        ret = vector_add(ds_malloced, ds_obj);
        if (ret < 0)
            return ret;

        dn_status_ref(ds_obj);
    }

    bb1->call_flag = 1;
    bb1->dereference_flag = 0;
    bb1->auto_ref_flag = 1;

    *pbb = bb1;
    return 0;
}

static int _bb_split_add_free(ast_t *ast, function_t *f, basic_block_t **pbb, dn_status_t *ds) {
    basic_block_t *cur_bb = *pbb;
    basic_block_t *bb1 = NULL;
    basic_block_t *bb2 = NULL;
    dag_node_t *dn = ds->dag_node;
    list_t *bb_list_head = &f->basic_block_list_head;

    AUTO_GC_BB_SPLIT(cur_bb, bb1);
    AUTO_GC_BB_SPLIT(bb1, bb2);

    bb1->ds_auto_gc = dn_status_clone(ds);
    if (!bb1->ds_auto_gc)
        return -ENOMEM;

    bb1->call_flag = 1;
    bb1->dereference_flag = 0;
    bb1->auto_free_flag = 1;

    int ret = _bb_add_gc_code_freep(&f->dag_list_head, ast, bb1, ds);
    if (ret < 0)
        return ret;

    ret = _bb_add_active(bb1, dn);
    if (ret < 0)
        return ret;

    ret = basic_block_search_bfs(bb1, _bb_prev_add_active, dn);
    if (ret < 0)
        return ret;

    *pbb = bb2;
    return 0;
}

static int _bb_prevs_malloced(basic_block_t *bb, vector_t *ds_malloced) {
    basic_block_t *bb_prev;
    dn_status_t *ds;
    dn_status_t *ds2;
    int i;
    int j;

    for (i = 0; i < bb->prevs->size; i++) {
        bb_prev = bb->prevs->data[i];

        for (j = 0; j < bb_prev->ds_malloced->size; j++) {
            ds = bb_prev->ds_malloced->data[j];

            if (vector_find_cmp(bb_prev->ds_freed, ds, ds_cmp_same_indexes))
                continue;

            if (vector_find_cmp(ds_malloced, ds, ds_cmp_like_indexes))
                continue;

            ds2 = dn_status_clone(ds);
            if (!ds2)
                return -ENOMEM;

            int ret = vector_add(ds_malloced, ds2);
            if (ret < 0) {
                dn_status_free(ds2);
                return ret;
            }
        }
    }
    return 0;
}

int __bb_find_ds_alias(vector_t *aliases, dn_status_t *ds_obj, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head);

static int _bb_need_ref(dag_node_t *dn, vector_t *ds_malloced, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    dn_status_t *ds = NULL;
    vector_t *aliases;
    int i;

    int ret = ds_for_dn(&ds, dn);
    if (ret < 0)
        return ret;

    if (vector_find_cmp(ds_malloced, ds, ds_cmp_like_indexes)) {
        dn_status_free(ds);
        return 1;
    }

    aliases = vector_alloc();
    if (!aliases) {
        dn_status_free(ds);
        return -ENOMEM;
    }

    ret = __bb_find_ds_alias(aliases, ds, c, bb, bb_list_head);

    dn_status_free(ds);
    ds = NULL;
    if (ret < 0)
        goto error;

    ret = 0;
    for (i = 0; i < aliases->size; i++) {
        ds = aliases->data[i];

        if (!ds->dag_node)
            continue;

        if (vector_find_cmp(ds_malloced, ds, ds_cmp_like_indexes)) {
            ret = 1;
            break;
        }
    }

error:
    vector_clear(aliases, (void (*)(void *))dn_status_free);
    vector_free(aliases);
    return ret;
}

static int _bb_split_prevs_need_free(dn_status_t *ds_obj, vector_t *ds_malloced, vector_t *bb_split_prevs, basic_block_t *bb) {
    if (vector_find_cmp(ds_malloced, ds_obj, ds_cmp_like_indexes)) {
        int ret = _bb_split_prevs(bb, ds_obj, bb_split_prevs);
        if (ret < 0)
            return ret;

        return 1;
    }

    return 0;
}

static int _auto_gc_bb_free(dn_status_t *ds_obj, vector_t *ds_malloced, vector_t *ds_assigned, ast_t *ast,
                            function_t *f,
                            basic_block_t *bb,
                            basic_block_t **cur_bb) {
    vector_t *bb_split_prevs = vector_alloc();

    if (!bb_split_prevs)
        return -ENOMEM;

    int ret = _bb_split_prevs_need_free(ds_obj, ds_malloced, bb_split_prevs, bb);
    if (ret < 0) {
        vector_free(bb_split_prevs);
        return ret;
    }

    if (ret) {
        if (!vector_find_cmp(ds_assigned, ds_obj, ds_cmp_like_indexes)
            && bb_split_prevs->size > 0
            && bb_split_prevs->size < bb->prevs->size) {
            return _bb_split_prev_add_free(ast, f, bb, ds_obj, bb_split_prevs);
        }

        ret = _bb_split_add_free(ast, f, cur_bb, ds_obj);
    }

    vector_free(bb_split_prevs);
    bb_split_prevs = NULL;
    return ret;
}

static int _auto_gc_retval(dn_status_t *ds_obj, dag_node_t *dn, vector_t *ds_malloced, _3ac_code_t * c, function_t *f) {
    dag_node_t *pf;
    function_t *f2;
    variable_t *fret;
    node_t *parent;
    node_t *result;

    int i = 0;

    if (dn->node->split_flag) {
        parent = dn->node->split_parent;

        assert(OP_CALL == parent->type || OP_CREATE == parent->type);

        for (i = 0; i < parent->result_nodes->size; i++) {
            result = parent->result_nodes->data[i];

            if (dn->node == result)
                break;
        }
    }

    pf = dn->childs->data[0];
    f2 = pf->var->func_ptr;
    fret = f2->rets->data[i];

    if (!strcmp(f2->node.w->text->data, "  _auto_malloc") || fret->auto_gc_flag) {
        assert(OP_RETURN != c->op->type);
        //		fret = f->rets->data[i];
        //		fret->auto_gc_flag = 1;

        if (!vector_find_cmp(ds_malloced, ds_obj, ds_cmp_like_indexes)) {
            int ret = vector_add(ds_malloced, ds_obj);
            if (ret < 0)
                return ret;

            dn_status_ref(ds_obj);
        }
    }

    return 0;
}

static int _optimize_auto_gc_bb(ast_t *ast, function_t *f, basic_block_t *bb, list_t *bb_list_head) {
    basic_block_t *cur_bb = bb;
    basic_block_t *bb2;
    _3ac_code_t * c;
    vector_t *ds_malloced;
    vector_t *ds_assigned;
    list_t *l;

    ds_malloced = vector_alloc();
    if (!ds_malloced)
        return -ENOMEM;

    ds_assigned = vector_alloc();
    if (!ds_assigned) {
        vector_free(ds_malloced);
        return -ENOMEM;
    }

    // at first, the malloced vars, are ones malloced in previous blocks

    int ret = _bb_prevs_malloced(bb, ds_malloced);
    if (ret < 0)
        goto error;

    for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head);) {
        c = list_data(l, _3ac_code_t, list);
        l = list_next(l);

        dn_status_t *ds_obj = NULL;
        dn_status_t *ds = NULL;
        dag_node_t *dn = NULL;

        if (OP_ASSIGN == c->op->type
            || OP_3AC_ASSIGN_ARRAY_INDEX == c->op->type
            || OP_3AC_ASSIGN_POINTER == c->op->type
            || OP_3AC_ASSIGN_DEREFERENCE == c->op->type) {
            ret = __auto_gc_ds_for_assign(&ds_obj, &dn, c);
            if (ret < 0)
                goto error;

            if (!ds_obj)
                goto end;

            if (OP_ASSIGN != c->op->type) {
                if (ds_obj->dag_node->var->arg_flag)
                    ds_obj->ret_flag = 1;
            }
        } else
            goto end;

        ret = _auto_gc_bb_free(ds_obj, ds_malloced, ds_assigned, ast, f, bb, &cur_bb);
        if (ret < 0) {
            dn_status_free(ds_obj);
            goto error;
        }

        ret = vector_add(ds_assigned, ds_obj);
        if (ret < 0) {
            dn_status_free(ds_obj);
            goto error;
        }
    ref:
        if (!dn->var->local_flag && cur_bb != bb)
            dn->var->tmp_flag = 1;

        while (dn) {
            if (OP_TYPE_CAST == dn->type)
                dn = dn->childs->data[0];

            else if (OP_EXPR == dn->type)
                dn = dn->childs->data[0];
            else
                break;
        }

        if (OP_CALL == dn->type || dn->node->split_flag) {
            ret = _auto_gc_retval(ds_obj, dn, ds_malloced, c, f);
            if (ret < 0)
                goto error;

        } else {
            ret = _bb_need_ref(dn, ds_malloced, c, bb, bb_list_head);
            if (ret < 0)
                goto error;

            if (ret > 0) {
                assert(OP_RETURN != c->op->type);
                //				  variable_t* fret = f->rets->data[0];
                //				fret->auto_gc_flag = 1;

                if (cur_bb != bb) {
                    list_del(&c->list);
                    list_add_tail(&cur_bb->code_list_head, &c->list);
                }

                ret = _auto_gc_bb_ref(ds_obj, ds_malloced, ast, f, &cur_bb);
                if (ret < 0)
                    goto error;

                if (l != list_sentinel(&bb->code_list_head)) {
                    bb2 = NULL;
                    ret = _auto_gc_bb_split(cur_bb, &bb2);
                    if (ret < 0)
                        goto error;

                    cur_bb = bb2;
                }
                continue;
            }
        }
    end:
        if (cur_bb != bb) {
            list_del(&c->list);
            list_add_tail(&cur_bb->code_list_head, &c->list);
        }
    }

    ret = 0;
error:
    vector_clear(ds_assigned, (void (*)(void *))dn_status_free);
    vector_clear(ds_malloced, (void (*)(void *))dn_status_free);
    vector_free(ds_assigned);
    vector_free(ds_malloced);
    return ret;
}

static int _auto_gc_prev_dn_actives(list_t *current, list_t *sentinel) {
    basic_block_t *bb;
    dag_node_t *dn;
    vector_t *dn_actives;
    list_t *l;

    int ret;
    int i;

    dn_actives = vector_alloc();
    if (!dn_actives)
        return -ENOMEM;

    for (l = current; l != sentinel; l = list_prev(l)) {
        bb = list_data(l, basic_block_t, list);

        ret = basic_block_active_vars(bb);
        if (ret < 0)
            goto error;

        for (i = 0; i < dn_actives->size; i++) {
            dn = dn_actives->data[i];

            ret = vector_add_unique(bb->exit_dn_actives, dn);
            if (ret < 0)
                goto error;
        }

        for (i = 0; i < bb->entry_dn_actives->size; i++) {
            dn = bb->entry_dn_actives->data[i];

            ret = vector_add_unique(dn_actives, dn);
            if (ret < 0)
                goto error;
        }
    }

    ret = 0;
error:
    vector_free(dn_actives);
    return ret;
}

static void _auto_gc_delete(basic_block_t *bb, basic_block_t *bb2) {
    basic_block_t *bb3;
    basic_block_t *bb4;

    assert(1 == bb2->prevs->size);
    assert(1 == bb->nexts->size);

    bb3 = bb2->prevs->data[0];
    bb4 = bb->nexts->data[0];

    assert(1 == bb3->nexts->size);
    assert(1 == bb4->prevs->size);

    bb3->nexts->data[0] = bb4;
    bb4->prevs->data[0] = bb3;

    list_del(&bb->list);
    list_del(&bb2->list);

    basic_block_free(bb);
    basic_block_free(bb2);
}

static int _optimize_auto_gc(ast_t *ast, function_t *f, vector_t *functions) {
    if (!ast || !f)
        return -EINVAL;

    if (!strcmp(f->node.w->text->data, "  _auto_malloc"))
        return 0;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;
    basic_block_t *bb2;

    int ret = _auto_gc_last_free(ast, f);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head);) {
        list_t *start = l;
        list_t *l2;

        bb = list_data(l, basic_block_t, list);

        for (l = list_next(l); l != list_sentinel(bb_list_head); l = list_next(l)) {
            bb2 = list_data(l, basic_block_t, list);

            if (!bb2->auto_ref_flag && !bb2->auto_free_flag)
                break;
        }

        ret = _optimize_auto_gc_bb(ast, f, bb, bb_list_head);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        ret = _auto_gc_prev_dn_actives(list_prev(l), list_prev(start));
        if (ret < 0)
            return ret;

        for (l2 = list_prev(l); l2 != list_prev(start);) {
            bb = list_data(l2, basic_block_t, list);
            l2 = list_prev(l2);

            if (l2 != start && list_prev(l) != &bb->list) {
                bb2 = list_data(l2, basic_block_t, list);

                if (bb->auto_free_flag
                    && bb2->auto_ref_flag
                    && 0 == ds_cmp_same_indexes(bb->ds_auto_gc, bb2->ds_auto_gc)) {
                    l2 = list_prev(l2);

                    _auto_gc_delete(bb, bb2);
                    continue;
                }
            }

            ret = basic_block_loads_saves(bb, bb_list_head);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

optimizer_t optimizer_auto_gc =
    {
        .name = "auto_gc",

        .optimize = _optimize_auto_gc,

        .flags = OPTIMIZER_LOCAL,
};
