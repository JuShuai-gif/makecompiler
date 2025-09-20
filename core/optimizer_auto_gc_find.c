#include "optimizer.h"
#include "pointer_alias.h"

#include "auto_gc_find.c"

static int _bb_add_ds(basic_block_t *bb, dn_status_t *ds_obj) {
    dn_status_t *ds;

    ds = vector_find_cmp(bb->ds_freed, ds_obj, ds_cmp_same_indexes);
    if (ds) {
        assert(0 == vector_del(bb->ds_freed, ds));

        dn_status_free(ds);
        ds = NULL;
    }

    ds = vector_find_cmp(bb->ds_malloced, ds_obj, ds_cmp_same_indexes);
    if (!ds) {
        ds = vector_find_cmp(bb->ds_malloced, ds_obj, ds_cmp_like_indexes);

        if (!ds) {
            ds = dn_status_clone(ds_obj);
            if (!ds)
                return -ENOMEM;

            int ret = vector_add(bb->ds_malloced, ds);
            if (ret < 0) {
                dn_status_free(ds);
                return ret;
            }
            return 0;
        }
    }

    ds->ret_flag |= ds_obj->ret_flag;
    ds->ret_index = ds_obj->ret_index;
    return 0;
}

static int _bb_del_ds(basic_block_t *bb, dn_status_t *ds_obj) {
    dn_status_t *ds;

    if (!vector_find_cmp(bb->ds_freed, ds_obj, ds_cmp_same_indexes)) {
        ds = vector_find_cmp(bb->ds_freed, ds_obj, ds_cmp_like_indexes);
        if (!ds) {
            ds = dn_status_clone(ds_obj);
            if (!ds)
                return -ENOMEM;

            int ret = vector_add(bb->ds_freed, ds);
            if (ret < 0) {
                dn_status_free(ds);
                return ret;
            }
        }
    }

    ds = vector_find_cmp(bb->ds_malloced, ds_obj, ds_cmp_same_indexes);
    if (ds) {
        assert(0 == vector_del(bb->ds_malloced, ds));

        dn_status_free(ds);
        ds = NULL;
    }
    return 0;
}

static int __ds_append_index(dn_status_t *dst, dn_status_t *src) {
    dn_index_t *di;
    dn_index_t *di2;
    int j;

    for (j = 0; j < src->dn_indexes->size; j++) {
        di2 = src->dn_indexes->data[j];

        di = dn_index_alloc();
        if (!di)
            return -ENOMEM;

        di->index = di2->index;
        di->member = di2->member;
        di->dn = di2->dn;
#if 0
		if (di2->dn) {
			di ->dn =   dag_node_alloc(di2->dn->type, di2->dn->var, di2->dn->node);
			if (!di->dn) {
				  dn_index_free (di);
				return -ENOMEM;
			}
		}
#endif
        int ret = vector_add_front(dst->dn_indexes, di);
        if (ret < 0) {
            dn_index_free(di);
            return ret;
        }
    }
    return 0;
}

static int _bb_add_ds_for_call(basic_block_t *bb, dn_status_t *ds_obj, function_t *f2, variable_t *arg) {
    list_t *l2 = list_tail(&f2->basic_block_list_head);
    basic_block_t *bb2 = list_data(l2, basic_block_t, list);

    dn_status_t *ds;
    dn_status_t *ds2;
    dn_index_t *di;
    variable_t *v;
    variable_t *v2;

    int i;
    int j;
    for (i = 0; i < bb2->ds_malloced->size; i++) {
        ds2 = bb2->ds_malloced->data[i];

        if (ds2->dag_node->var != arg)
            continue;

        if (!ds2->dn_indexes) {
            _bb_add_ds(bb, ds_obj);
            continue;
        }

        ds = dn_status_clone(ds_obj);
        if (!ds)
            return -ENOMEM;

        if (!ds->dn_indexes) {
            ds->dn_indexes = vector_alloc();

            if (!ds->dn_indexes) {
                dn_status_free(ds);
                return -ENOMEM;
            }
        }

        int ret = __ds_append_index(ds, ds2);
        if (ret < 0) {
            dn_status_free(ds);
            return ret;
        }

        v = ds->dag_node->var;
        v2 = ds2->dag_node->var;

        int m = v->nb_pointers + v->nb_dimentions + (v->type >= STRUCT);
        int n = v2->nb_pointers + v2->nb_dimentions + (v2->type >= STRUCT);

        for (j = 0; j + m < n; j++) {
            di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

            if (di->member || 0 == di->index) {
                assert(!di->dn);

                --ds->dn_indexes->size;
                dn_index_free(di);
                di = NULL;
            } else {
                dn_status_free(ds);
                return -ENOMEM;
            }
        }

        if (ds->dn_indexes->size <= 0) {
            vector_free(ds->dn_indexes);
            ds->dn_indexes = NULL;
        }

        _bb_add_ds(bb, ds);

        dn_status_free(ds);
        ds = NULL;
    }

    return 0;
}

static int __bb_add_ds_append(basic_block_t *bb, dn_status_t *ds_obj, basic_block_t *__bb, dn_status_t *__ds) {
    dn_status_t *ds;
    dn_status_t *ds2;
    dn_index_t *di;
    dn_index_t *di2;
    int i;

    for (i = 0; i < __bb->ds_malloced->size; i++) {
        ds2 = __bb->ds_malloced->data[i];

        if (ds2->dag_node->var != __ds->dag_node->var)
            continue;

        if (!ds2->dn_indexes) {
            _bb_add_ds(bb, ds_obj);
            continue;
        }

        ds = dn_status_clone(ds_obj);
        if (!ds)
            return -ENOMEM;

        if (!ds->dn_indexes) {
            ds->dn_indexes = vector_alloc();

            if (!ds->dn_indexes) {
                dn_status_free(ds);
                return -ENOMEM;
            }
        }

        int ret = __ds_append_index(ds, ds2);
        if (ret < 0) {
            dn_status_free(ds);
            return ret;
        }

        _bb_add_ds(bb, ds);

        dn_status_free(ds);
        ds = NULL;
    }

    return 0;
}

static int _bb_add_ds_for_ret(basic_block_t *bb, dn_status_t *ds_obj, function_t *f2) {
    list_t *l2 = list_tail(&f2->basic_block_list_head);
    basic_block_t *bb2 = list_data(l2, basic_block_t, list);
    dn_status_t *ds2;

    int i;
    for (i = 0; i < bb2->ds_malloced->size; i++) {
        ds2 = bb2->ds_malloced->data[i];

        if (!ds2->ret_flag)
            continue;

        __bb_add_ds_append(bb, ds_obj, bb2, ds2);
    }

    return 0;
}

#define AUTO_GC_FIND_BB_SPLIT(parent, child)                            \
    do {                                                                \
        int ret = basic_block_split(parent, &child);                    \
        if (ret < 0)                                                    \
            return ret;                                                 \
                                                                        \
        child->call_flag = parent->call_flag;                           \
        child->dereference_flag = parent->dereference_flag;             \
                                                                        \
        vector_free(child->exit_dn_actives);                            \
        vector_free(child->exit_dn_aliases);                            \
        vector_free(child->dn_loads);                                   \
        vector_free(child->dn_reloads);                                 \
                                                                        \
        child->exit_dn_actives = vector_clone(parent->exit_dn_actives); \
        child->exit_dn_aliases = vector_clone(parent->exit_dn_aliases); \
        child->dn_loads = vector_clone(parent->dn_loads);               \
        child->dn_reloads = vector_clone(parent->dn_reloads);           \
                                                                        \
        list_add_front(&parent->list, &child->list);                    \
    } while (0)

static int _auto_gc_find_argv_out(basic_block_t *cur_bb, _3ac_code_t * c) {
    assert(c->srcs->size > 0);

    _3ac_operand_t *src = c->srcs->data[0];
    function_t *f2 = src->dag_node->var->func_ptr;
    dag_node_t *dn;
    variable_t *v0;
    variable_t *v1;
    dn_status_t *ds_obj;

    int count = 0;
    int ret;
    int i;

    for (i = 1; i < c->srcs->size; i++) {
        src = c->srcs->data[i];

        dn = src->dag_node;
        v0 = dn->var;

        while (dn) {
            if (OP_TYPE_CAST == dn->type)
                dn = dn->childs->data[0];

            else if (OP_EXPR == dn->type)
                dn = dn->childs->data[0];
            else
                break;
        }

        if (v0->nb_pointers + v0->nb_dimentions + (v0->type >= STRUCT) < 2)
            continue;

        if (i - 1 >= f2->argv->size)
            continue;

        v1 = f2->argv->data[i - 1];
        if (!v1->auto_gc_flag)
            continue;

        logd("i: %d, f2: %s, v0: %s, v1: %s\n", i, f2->node.w->text->data, v0->w->text->data, v1->w->text->data);

        ds_obj = NULL;
        if (OP_ADDRESS_OF == dn->type)

            ret = ds_for_dn(&ds_obj, dn->childs->data[0]);
        else
            ret = ds_for_dn(&ds_obj, dn);
        if (ret < 0)
            return ret;

        if (ds_obj->dag_node->var->arg_flag)
            ds_obj->ret_flag = 1;

        ret = vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
        if (ret < 0) {
            dn_status_free(ds_obj);
            return ret;
        }

        ret = _bb_add_ds_for_call(cur_bb, ds_obj, f2, v1);

        dn_status_free(ds_obj);
        ds_obj = NULL;
        if (ret < 0)
            return ret;

        count++;
    }

    return count;
}

static int _auto_gc_find_ret(basic_block_t *cur_bb, _3ac_code_t * c) {
    assert(c->srcs->size > 0);

    _3ac_operand_t * dst;
    _3ac_operand_t *src = c->srcs->data[0];
    function_t *f2 = src->dag_node->var->func_ptr;
    variable_t *fret;
    variable_t *v;
    dn_status_t *ds_obj;

    assert(c->dsts->size <= f2->rets->size);

    int i;
    for (i = 0; i < c->dsts->size; i++) {
        dst = c->dsts->data[i];

        fret = f2->rets->data[i];
        v = dst->dag_node->var;

        logd("--- f2: %s(), i: %d, auto_gc_flag: %d\n", f2->node.w->text->data, i, fret->auto_gc_flag);

        if (fret->auto_gc_flag) {
            if (!variable_may_malloced(v))
                return 0;

            ds_obj = dn_status_alloc(dst->dag_node);
            if (!ds_obj)
                return -ENOMEM;

            _bb_add_ds(cur_bb, ds_obj);

            dn_status_free(ds_obj);
            ds_obj = NULL;
        }
    }

    return 0;
}

int __auto_gc_ds_for_assign(dn_status_t **ds, dag_node_t **dn, _3ac_code_t * c) {
    _3ac_operand_t * base;
    _3ac_operand_t * member;
    _3ac_operand_t * index;
    _3ac_operand_t * scale;
    _3ac_operand_t * src;
    variable_t *v;

    switch (c->op->type) {
    case OP_ASSIGN:
        base = c->dsts->data[0];
        v = base->dag_node->var;

        if (!variable_may_malloced(v))
            return 0;

        *ds = dn_status_alloc(base->dag_node);
        if (!*ds)
            return -ENOMEM;

        src = c->srcs->data[0];
        *dn = src->dag_node;
        break;

    case OP_3AC_ASSIGN_ARRAY_INDEX:
        assert(4 == c->srcs->size);

        base = c->srcs->data[0];
        v = _operand_get(base->node->parent);

        if (!v || !variable_may_malloced(v))
            return 0;

        index = c->srcs->data[1];
        scale = c->srcs->data[2];
        src = c->srcs->data[3];
        *dn = src->dag_node;

        return ds_for_assign_array_member(ds, base->dag_node, index->dag_node, scale->dag_node);
        break;

    case OP_3AC_ASSIGN_POINTER:
        assert(3 == c->srcs->size);

        member = c->srcs->data[1];
        v = member->dag_node->var;

        if (!variable_may_malloced(v))
            return 0;

        base = c->srcs->data[0];
        src = c->srcs->data[2];
        *dn = src->dag_node;

        return ds_for_assign_member(ds, base->dag_node, member->dag_node);
        break;

    case OP_3AC_ASSIGN_DEREFERENCE:
        assert(2 == c->srcs->size);

        base = c->srcs->data[0];
        v = _operand_get(base->node->parent);

        if (!variable_may_malloced(v))
            return 0;

        src = c->srcs->data[1];
        *dn = src->dag_node;

        return ds_for_assign_dereference(ds, base->dag_node);
    default:
        break;
    };

    return 0;
}

static int _auto_gc_find_ref(dn_status_t *ds_obj, dag_node_t *dn, _3ac_code_t * c,
                             basic_block_t *bb,
                             basic_block_t *cur_bb,
                             function_t *f) {
    dn_status_t *ds;
    dag_node_t *pf;
    function_t *f2;
    variable_t *fret;
    node_t *parent;
    node_t *result;

    if (OP_CALL == dn->type || dn->node->split_flag) {
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

        logd("f2: %s, rets[%d], auto_gc_flag: %d\n", f2->node.w->text->data, i, fret->auto_gc_flag);

        if (!strcmp(f2->node.w->text->data, "  _auto_malloc")) {
            _bb_add_ds(cur_bb, ds_obj);
            return 1;
        }

        if (fret->auto_gc_flag) {
            ds = dn_status_alloc(dn);
            if (!ds)
                return -ENOMEM;
            _bb_del_ds(cur_bb, ds);

            dn_status_free(ds);
            ds = NULL;

            _bb_add_ds(cur_bb, ds_obj);
            _bb_add_ds_for_ret(cur_bb, ds_obj, f2);
            return 2;
        }
    } else {
        ds = NULL;
        int ret = ds_for_dn(&ds, dn);
        if (ret < 0)
            return ret;

        ret = _bb_find_ds_alias(ds, c, bb, &f->basic_block_list_head);

        dn_status_free(ds);
        ds = NULL;
        if (ret < 0)
            return ret;

        if (1 == ret) {
            _bb_add_ds(cur_bb, ds_obj);
            return 2;
        }
    }

    return 0;
}

static int _auto_gc_find_return(vector_t *objs, _3ac_code_t * c, basic_block_t *bb, basic_block_t *cur_bb, function_t *f) {
    _3ac_operand_t * src;
    dn_status_t *ds_obj;
    dag_node_t *dn;
    variable_t *v;

    int count = 0;
    int i;

    for (i = 0; i < c->srcs->size; i++) {
        src = c->srcs->data[i];

        dn = src->dag_node;
        v = dn->var;

        if (!variable_may_malloced(dn->var))
            continue;

        if (v->w)
            logd("v: %s, line: %d\n", v->w->text->data, v->w->line);

        while (dn) {
            if (OP_TYPE_CAST == dn->type)
                dn = dn->childs->data[0];

            else if (OP_EXPR == dn->type)
                dn = dn->childs->data[0];
            else
                break;
        }

        ds_obj = dn_status_alloc(dn);
        if (!ds_obj)
            return -ENOMEM;

        ds_obj->ret_flag = 1;
        ds_obj->ret_index = i;

        logd("i: %d, ds: %#lx, ret_index: %d\n", i, 0xffffff & (uintptr_t)ds_obj, ds_obj->ret_index);
        //		  dn_status_print(ds_obj);

        int ret = _auto_gc_find_ref(ds_obj, dn, c, bb, cur_bb, f);
        if (ret < 0) {
            dn_status_free(ds_obj);
            return ret;
        }

        count += ret > 0;

        if (ret > 1) {
            ret = vector_add(objs, ds_obj);
            if (ret < 0) {
                dn_status_free(ds_obj);
                return ret;
            }
        } else
            dn_status_free(ds_obj);
        ds_obj = NULL;
    }

    return count;
}

static int _auto_gc_bb_find(basic_block_t *bb, function_t *f) {
    basic_block_t *cur_bb = bb;
    basic_block_t *bb2 = NULL;
    _3ac_code_t * c;
    list_t *l;

    int count = 0;
    int ret;
    int i;

    for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head);) {
        c = list_data(l, _3ac_code_t, list);
        l = list_next(l);

        _3ac_operand_t * src;
        dn_status_t *ds_obj = NULL;
        dn_status_t *ds = NULL;
        dag_node_t *dn = NULL;

        if (OP_ASSIGN == c->op->type
            || OP_3AC_ASSIGN_ARRAY_INDEX == c->op->type
            || OP_3AC_ASSIGN_POINTER == c->op->type
            || OP_3AC_ASSIGN_DEREFERENCE == c->op->type) {
            ret = __auto_gc_ds_for_assign(&ds_obj, &dn, c);
            if (ret < 0)
                return ret;

            if (!ds_obj)
                goto end;

            if (OP_ASSIGN != c->op->type) {
                if (ds_obj->dag_node->var->arg_flag)
                    ds_obj->ret_flag = 1;
            }

        } else if (OP_RETURN == c->op->type) {
            vector_t *objs = vector_alloc();
            if (!objs)
                return -ENOMEM;

            ret = _auto_gc_find_return(objs, c, bb, cur_bb, f);
            if (ret < 0) {
                vector_clear(objs, (void (*)(void *))dn_status_free);
                vector_free(objs);
                return ret;
            }
            count += ret;

            if (cur_bb != bb) {
                list_del(&c->list);
                list_add_tail(&cur_bb->code_list_head, &c->list);
            }

            if (objs->size > 0 && l != list_sentinel(&bb->code_list_head)) {
                AUTO_GC_FIND_BB_SPLIT(cur_bb, bb2);
                cur_bb = bb2;

                for (i = 0; i < objs->size; i++) {
                    ds_obj = objs->data[i];

                    ret = vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
                    if (ret < 0) {
                        vector_clear(objs, (void (*)(void *))dn_status_free);
                        vector_free(objs);
                        return ret;
                    }
                }
            }

            vector_clear(objs, (void (*)(void *))dn_status_free);
            vector_free(objs);
            continue;

        } else if (OP_CALL == c->op->type) {
            assert(c->srcs->size > 0);

            ret = _auto_gc_find_argv_out(cur_bb, c);
            if (ret < 0)
                return ret;
            count += ret;

            ret = _auto_gc_find_argv_in(cur_bb, c);
            if (ret < 0)
                return ret;

            ret = _auto_gc_find_ret(cur_bb, c);
            if (ret < 0)
                return ret;

            goto end;
        } else
            goto end;

        _bb_del_ds(cur_bb, ds_obj);
        count++;
    ref:
        while (dn) {
            if (OP_TYPE_CAST == dn->type)
                dn = dn->childs->data[0];

            else if (OP_EXPR == dn->type)
                dn = dn->childs->data[0];
            else
                break;
        }

        ret = _auto_gc_find_ref(ds_obj, dn, c, bb, cur_bb, f);
        if (ret < 0) {
            dn_status_free(ds_obj);
            return ret;
        }

        count += ret > 0;

        if (cur_bb != bb) {
            list_del(&c->list);
            list_add_tail(&cur_bb->code_list_head, &c->list);
        }

        if (ret > 1 && l != list_sentinel(&bb->code_list_head)) {
            AUTO_GC_FIND_BB_SPLIT(cur_bb, bb2);
            cur_bb = bb2;

            ret = vector_add_unique(cur_bb->entry_dn_actives, ds_obj->dag_node);
            if (ret < 0) {
                dn_status_free(ds_obj);
                return ret;
            }
        }

        dn_status_free(ds_obj);
        ds_obj = NULL;
        continue;
    end:
        if (cur_bb != bb) {
            list_del(&c->list);
            list_add_tail(&cur_bb->code_list_head, &c->list);
        }
    }

    return count;
}

static int _bb_find_ds_alias_leak(dn_status_t *ds_obj, _3ac_code_t * c, basic_block_t *bb, list_t *bb_list_head) {
    dn_status_t *ds;
    vector_t *aliases;
    int i;

    aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;

    int ret = pointer_alias_ds_leak(aliases, ds_obj, c, bb, bb_list_head);
    if (ret < 0)
        goto error;

    for (i = 0; i < aliases->size; i++) {
        ds = aliases->data[i];

        XCHG(ds->dn_indexes, ds->alias_indexes);
        XCHG(ds->dag_node, ds->alias);

        if (!ds->dag_node)
            continue;

        ret = __bb_add_ds_append(bb, ds_obj, bb, ds);
        if (ret < 0) {
            loge("\n");
            goto error;
        }
    }

    ret = 0;
error:
    vector_clear(aliases, (void (*)(void *))dn_status_free);
    vector_free(aliases);
    return ret;
}

static int _auto_gc_function_find(ast_t *ast, function_t *f, list_t *bb_list_head) {
    if (!f || !bb_list_head)
        return -EINVAL;

    if (list_empty(bb_list_head))
        return 0;

    list_t *l;
    basic_block_t *bb;
    dn_status_t *ds;
    _3ac_code_t * c;
    variable_t *fret;

    int total = 0;
    int count = 0;
    int ret;

    logw("--- %s() ---\n", f->node.w->text->data);

    do {
        for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head);) {
            bb = list_data(l, basic_block_t, list);
            l = list_next(l);

            ret = _auto_gc_bb_find(bb, f);
            if (ret < 0) {
                loge("\n");
                return ret;
            }

            total += ret;
        }

        l = list_head(bb_list_head);
        bb = list_data(l, basic_block_t, list);

        ret = basic_block_search_bfs(bb, _auto_gc_bb_next_find, NULL);
        if (ret < 0)
            return ret;

        total += ret;
        count = ret;
    } while (count > 0);

    l = list_tail(bb_list_head);
    bb = list_data(l, basic_block_t, list);

    l = list_tail(&bb->code_list_head);
    c = list_data(l, _3ac_code_t, list);

    int i;
    for (i = 0; i < bb->ds_malloced->size; i++) {
        ds = bb->ds_malloced->data[i];

        if (!ds->ret_flag)
            continue;
#if 1
        logi("ds: %#lx, ds->ret_flag: %u, ds->ret_index: %d, ds->dag_node->var->arg_flag: %u\n",
             0xffff & (uintptr_t)ds, ds->ret_flag, ds->ret_index, ds->dag_node->var->arg_flag);
        dn_status_print(ds);
#endif
        if (ds->dag_node->var->arg_flag)
            ds->dag_node->var->auto_gc_flag = 1;
        else {
            assert(ds->ret_index < f->rets->size);

            fret = f->rets->data[ds->ret_index];
            fret->auto_gc_flag = 1;

            _bb_find_ds_alias_leak(ds, c, bb, bb_list_head);
        }
    }
    logi("--- %s() ---\n\n", f->node.w->text->data);

    return total;
}

static int _optimize_auto_gc_find(ast_t *ast, function_t *f, vector_t *functions) {
    if (!ast || !functions)
        return -EINVAL;

    if (functions->size <= 0)
        return 0;

    vector_t *fqueue = vector_alloc();
    if (!fqueue)
        return -ENOMEM;

    int ret = _bfs_sort_function(fqueue, functions);
    if (ret < 0) {
        vector_free(fqueue);
        return ret;
    }

    int total0 = 0;
    int total1 = 0;

    do {
        total0 = total1;
        total1 = 0;

        int i;
        for (i = 0; i < fqueue->size; i++) {
            f = fqueue->data[i];

            if (!f->node.define_flag)
                continue;

            if (!strcmp(f->node.w->text->data, "  _auto_malloc"))
                continue;

            ret = _auto_gc_function_find(ast, f, &f->basic_block_list_head);
            if (ret < 0) {
                vector_free(fqueue);
                return ret;
            }

            total1 += ret;
        }

        logi("total0: %d, total1: %d\n", total0, total1);

    } while (total0 != total1);

    vector_free(fqueue);
    return 0;
}

optimizer_t optimizer_auto_gc_find =
    {
        .name = "auto_gc_find",

        .optimize = _optimize_auto_gc_find,

        .flags = OPTIMIZER_GLOBAL,
};
