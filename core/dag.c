#include "dag.h"
#include "3ac.h"

dn_index_t *dn_index_alloc() {
    dn_index_t *di = calloc(1, sizeof(dn_index_t));
    if (!di)
        return NULL;

    di->refs = 1;
    return di;
}

dn_index_t *dn_index_ref(dn_index_t *di) {
    if (di)
        di->refs++;
    return di;
}

dn_index_t *dn_index_clone(dn_index_t *di) {
    if (!di)
        return NULL;

    dn_index_t *di2 = dn_index_alloc();
    if (!di2)
        return NULL;

    di2->member = di->member;
    di2->index = di->index;
    di2->dn = di->dn;
    di2->dn_scale = di->dn_scale;

    return di2;
}

void dn_index_free(dn_index_t *di) {
    if (di && 0 == --di->refs)
        free(di);
}

int dn_index_same(const dn_index_t *di0, const dn_index_t *di1) {
    if (di0->member != di1->member)
        return 0;

    if (di0->member)
        return 1;

    if (-1 == di0->index || -1 == di1->index)
        return 0;

    if (di0->index == di1->index)
        return 1;
    return 0;
}

int dn_index_like(const dn_index_t *di0, const dn_index_t *di1) {
    if (di0->member != di1->member)
        return 0;

    if (di0->member)
        return 1;

    if (-1 == di0->index || -1 == di1->index)
        return 1;

    if (di0->index == di1->index)
        return 1;
    return 0;
}

int dn_status_is_like(const dn_status_t *ds) {
    if (!ds->dn_indexes)
        return 0;

    dn_index_t *di;
    int i;
    for (i = 0; i < ds->dn_indexes->size; i++) {
        di = ds->dn_indexes->data[i];

        if (di->member)
            continue;

        if (-1 == di->index)
            return 1;
    }
    return 0;
}

dn_status_t *dn_status_null() {
    dn_status_t *ds = calloc(1, sizeof(dn_status_t));
    if (!ds)
        return NULL;

    ds->refs = 1;
    return ds;
}

dn_status_t *dn_status_alloc(dag_node_t *dn) {
    dn_status_t *ds = calloc(1, sizeof(dn_status_t));
    if (!ds)
        return NULL;

    ds->refs = 1;
    ds->dag_node = dn;

    if (dn) {
        ds->active = dn->active;
        ds->inited = dn->inited;
        ds->updated = dn->updated;
    }

    return ds;
}

int ds_copy_dn(dn_status_t *dst, dn_status_t *src) {
    dn_index_t *di;
    int i;

    if (!dst || !src)
        return -1;

    dst->dag_node = src->dag_node;

    if (dst->dn_indexes) {
        vector_clear(dst->dn_indexes, (void (*)(void *))dn_index_free);
        vector_free(dst->dn_indexes);
        dst->dn_indexes = NULL;
    }

    if (src->dn_indexes) {
        dst->dn_indexes = vector_clone(src->dn_indexes);

        if (!dst->dn_indexes)
            return -ENOMEM;

        for (i = 0; i < dst->dn_indexes->size; i++) {
            di = dst->dn_indexes->data[i];
            di->refs++;
        }
    }
    return 0;
}

int ds_copy_alias(dn_status_t *dst, dn_status_t *src) {
    dn_index_t *di;
    int i;

    if (!dst || !src)
        return -1;

    dst->alias = src->alias;
    dst->alias_type = src->alias_type;

    if (dst->alias_indexes) {
        vector_clear(dst->alias_indexes, (void (*)(void *))dn_index_free);
        vector_free(dst->alias_indexes);
        dst->alias_indexes = NULL;
    }

    if (src->alias_indexes) {
        dst->alias_indexes = vector_clone(src->alias_indexes);

        if (!dst->alias_indexes)
            return -ENOMEM;

        for (i = 0; i < dst->alias_indexes->size; i++) {
            di = dst->alias_indexes->data[i];
            di->refs++;
        }
    }
    return 0;
}

dn_status_t *dn_status_clone(dn_status_t *ds) {
    dn_status_t *ds2;
    dn_index_t *di;
    int i;

    if (!ds)
        return NULL;

    ds2 = calloc(1, sizeof(dn_status_t));
    if (!ds2)
        return NULL;

    ds2->refs = 1;

    if (ds->dn_indexes) {
        ds2->dn_indexes = vector_clone(ds->dn_indexes);

        if (!ds2->dn_indexes) {
            dn_status_free(ds2);
            return NULL;
        }

        for (i = 0; i < ds2->dn_indexes->size; i++) {
            di = ds2->dn_indexes->data[i];
            di->refs++;
        }
    }

    if (ds->alias_indexes) {
        ds2->alias_indexes = vector_clone(ds->alias_indexes);

        if (!ds2->alias_indexes) {
            dn_status_free(ds2);
            return NULL;
        }

        for (i = 0; i < ds2->alias_indexes->size; i++) {
            di = ds2->alias_indexes->data[i];
            di->refs++;
        }
    }

    ds2->dag_node = ds->dag_node;
    ds2->dereference = ds->dereference;

    ds2->alias = ds->alias;
    ds2->alias_type = ds->alias_type;

    ds2->active = ds->active;
    ds2->inited = ds->inited;
    ds2->updated = ds->updated;

    ds2->ret_flag = ds->ret_flag;
    ds2->ret_index = ds->ret_index;
    return ds2;
}

dn_status_t *dn_status_ref(dn_status_t *ds) {
    if (ds)
        ds->refs++;
    return ds;
}

void dn_status_free(dn_status_t *ds) {
    dn_index_t *di;
    int i;

    if (ds) {
        assert(ds->refs > 0);

        if (--ds->refs > 0)
            return;

        if (ds->dn_indexes) {
            vector_clear(ds->dn_indexes, (void (*)(void *))dn_index_free);
            vector_free(ds->dn_indexes);
        }

        if (ds->alias_indexes) {
            vector_clear(ds->alias_indexes, (void (*)(void *))dn_index_free);
            vector_free(ds->alias_indexes);
        }

        free(ds);
    }
}

void dn_status_print(dn_status_t *ds) {
    dn_index_t *di;
    variable_t *v;
    int i;

    if (ds->dag_node) {
        v = ds->dag_node->var;
        printf("dn: v_%d_%d/%s ", v->w->line, v->w->pos, v->w->text->data);

        if (ds->dn_indexes) {
            for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
                di = ds->dn_indexes->data[i];

                if (di->member)
                    printf("->%s ", di->member->w->text->data);
                else
                    printf("[%ld] ", di->index);
            }
        }
    }

    if (ds->alias) {
        v = ds->alias->var;
        printf(" alias: v_%d_%d/%s ", v->w->line, v->w->pos, v->w->text->data);

        if (ds->alias_indexes) {
            for (i = ds->alias_indexes->size - 1; i >= 0; i--) {
                di = ds->alias_indexes->data[i];

                if (di->member)
                    printf("->%s ", di->member->w->text->data);
                else
                    printf("[%ld] ", di->index);
            }
        }
    }

    printf(" alias_type: %d\n", ds->alias_type);
}

dag_node_t *dag_node_alloc(int type, variable_t *var, const node_t *node) {
    dag_node_t *dn = calloc(1, sizeof(dag_node_t));
    if (!dn)
        return NULL;

    dn->type = type;
    if (var)
        dn->var = variable_ref(var);
    else
        dn->var = NULL;

    dn->node = (node_t *)node;

#if 0
	if ( OP_CALL == type) {
		 logw("dn: %#lx, dn->type: %d", 0xffff & (uintptr_t)dn, dn->type);
		if (var) {
			printf(", var: %#lx, var->type: %d", 0xffff & (uintptr_t)var, var->type);
			if (var->w)
				printf(", v_%d_%d/%s", var->w->line, var->w->pos, var->w->text->data);
		}
		printf("\n");
	}
#endif
    return dn;
}

int dag_node_find_child(dag_node_t *parent, dag_node_t *child) {
    if (parent->childs
        && vector_find(parent->childs, child))
        return 1;
    return 0;
}

int dag_node_add_child(dag_node_t *parent, dag_node_t *child) {
    if (!parent || !child)
        return -EINVAL;

    if (!parent->childs) {
        parent->childs = vector_alloc();
        if (!parent->childs)
            return -ENOMEM;
    }

    if (!child->parents) {
        child->parents = vector_alloc();
        if (!child->parents)
            return -ENOMEM;
    }

    int ret = vector_add(parent->childs, child);
    if (ret < 0)
        return ret;

    ret = vector_add(child->parents, parent);
    if (ret < 0) {
        vector_del(parent->childs, child);
        return ret;
    }

    return 0;
}

void dag_node_free(dag_node_t *dn) {
    if (dn) {
        if (dn->var)
            variable_free(dn->var);

        if (dn->parents)
            vector_free(dn->parents);

        if (dn->childs)
            vector_free(dn->childs);

        free(dn);
        dn = NULL;
    }
}

static int _ds_cmp_indexes(const void *p0, const void *p1,
                           int (*cmp)(const dn_index_t *, const dn_index_t *)) {
    const dn_status_t *ds0 = p0;
    const dn_status_t *ds1 = p1;

    if (ds0->dag_node != ds1->dag_node)
        return -1;

    if (ds0->dn_indexes) {
        if (!ds1->dn_indexes)
            return -1;

        if (ds0->dn_indexes->size != ds1->dn_indexes->size)
            return -1;

        dn_index_t *di0;
        dn_index_t *di1;
        int i;

        for (i = 0; i < ds0->dn_indexes->size; i++) {
            di0 = ds0->dn_indexes->data[i];
            di1 = ds1->dn_indexes->data[i];

            if (!cmp(di0, di1))
                return -1;
        }
        return 0;
    } else if (ds1->dn_indexes)
        return -1;
    return 0;
}

int ds_cmp_same_indexes(const void *p0, const void *p1) {
    return _ds_cmp_indexes(p0, p1, dn_index_same);
}

int ds_cmp_like_indexes(const void *p0, const void *p1) {
    return _ds_cmp_indexes(p0, p1, dn_index_like);
}

int ds_cmp_alias(const void *p0, const void *p1) {
    const dn_status_t *v0 = p0;
    const dn_status_t *v1 = p1;

    assert(DN_ALIAS_NULL != v0->alias_type);
    assert(DN_ALIAS_NULL != v1->alias_type);

    if (v0->alias_type != v1->alias_type)
        return -1;

    if (DN_ALIAS_ALLOC == v0->alias_type)
        return 0;

    if (v0->alias != v1->alias)
        return -1;

    switch (v0->alias_type) {
    case DN_ALIAS_VAR:
        return 0;
        break;

    case DN_ALIAS_ARRAY:
    case DN_ALIAS_MEMBER:
        if (v0->alias_indexes) {
            if (!v1->alias_indexes)
                return -1;

            if (v0->alias_indexes->size != v1->alias_indexes->size)
                return -1;

            dn_index_t *di0;
            dn_index_t *di1;
            int i;

            for (i = 0; i < v0->alias_indexes->size; i++) {
                di0 = v0->alias_indexes->data[i];
                di1 = v1->alias_indexes->data[i];

                if (!dn_index_like(di0, di1))
                    return -1;
            }
            return 0;
        } else if (v1->alias_indexes)
            return -1;
        return 0;
        break;

    default:
        break;
    };

    return -1;
}

void dag_node_free_list(list_t *dag_list_head) {
    dag_node_t *dn;
    list_t *l;

    for (l = list_head(dag_list_head); l != list_sentinel(dag_list_head);) {
        dn = list_data(l, dag_node_t, list);

        l = list_next(l);

        list_del(&dn->list);

        dag_node_free(dn);
        dn = NULL;
    }
}

static int __dn_same_call(dag_node_t *dn, const node_t *node, const node_t *split) {
    variable_t *v0 = _operand_get(node);
    variable_t *v1 = dn->var;

    if (split)
        v0 = _operand_get(split);

    if (v0 && v0->w && v1 && v1->w) {
        if (v0->type != v1->type) {
            logd("v0: %d/%s_%#lx, split_flag: %d\n", v0->w->line, v0->w->text->data, 0xffff & (uintptr_t)v0, node->split_flag);
            logd("v1: %d/%s_%#lx\n", v1->w->line, v1->w->text->data, 0xffff & (uintptr_t)v1);
        }
    }

    return v0 == v1;
}

int dag_node_same(dag_node_t *dn, const node_t *node) {
    int i;

    const node_t *split = NULL;

    if (node->split_flag) {
        if (dn->var != _operand_get(node))
            return 0;

        split = node;
        node = node->split_parent;

        logd("split type: %d, node: %#lx, var: %#lx\n", split->type, 0xffff & (uintptr_t)split, 0xffff & (uintptr_t)split->var);
        logd("node  type: %d, node: %#lx, var: %#lx\n", node->type, 0xffff & (uintptr_t)node, 0xffff & (uintptr_t)node->var);
        logd("dag   type: %d, node: %#lx, var: %#lx\n", dn->type, 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)dn->var);
    }

    if (OP_CREATE == node->type)
        node = node->result_nodes->data[0];

    if (dn->type != node->type)
        return 0;

    if (OP_ADDRESS_OF == node->type)
        logd("type: %d, %d, node: %#lx, %#lx, ", dn->type, node->type, 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)node);

    if (type_is_var(node->type)) {
        if (dn->var == node->var)
            return 1;
        else
            return 0;
    }

    if (OP_LOGIC_AND == node->type
        || OP_LOGIC_OR == node->type
        || OP_INC == node->type
        || OP_DEC == node->type
        || OP_INC_POST == node->type
        || OP_DEC_POST == node->type
        || OP_ADDRESS_OF == node->type) {
        if (dn->var == _operand_get((node_t *)node))
            return 1;
        return 0;
    }

    if (!dn->childs) {
        if (OP_CALL == node->type && 1 == node->nb_nodes)
            return __dn_same_call(dn, node, split);
        return 0;
    }

    if (OP_TYPE_CAST == node->type) {
        dag_node_t *dn0 = dn->childs->data[0];
        variable_t *vn1 = _operand_get(node->nodes[1]);
        node_t *n1 = node->nodes[1];

        while (OP_EXPR == n1->type)
            n1 = n1->nodes[0];

        if (dag_node_same(dn0, n1)
            && variable_same_type(dn->var, node->result))
            return 1;
        else {
            logd("var: %#lx, %#lx, type: %d, %d, node: %#lx, %#lx, same: %d\n",
                 0xffff & (uintptr_t)dn0->var,
                 0xffff & (uintptr_t)vn1,
                 dn->var->type, node->result->type,
                 0xffff & (uintptr_t)dn,
                 0xffff & (uintptr_t)node,
                 variable_same_type(dn->var, node->result));
            return 0;
        }
    } else if (OP_ARRAY_INDEX == node->type) {
        assert(3 == dn->childs->size);
        assert(2 == node->nb_nodes);
        goto cmp_childs;

    } else if (OP_ADDRESS_OF == node->type) {
        assert(1 == node->nb_nodes);

        if (OP_ARRAY_INDEX == node->nodes[0]->type) {
            assert(2 == node->nodes[0]->nb_nodes);

            if (!dn->childs || 3 != dn->childs->size)
                return 0;

            node = node->nodes[0];
            goto cmp_childs;

        } else if (OP_POINTER == node->nodes[0]->type) {
            assert(2 == node->nodes[0]->nb_nodes);

            if (!dn->childs || 2 != dn->childs->size)
                return 0;

            node = node->nodes[0];
            goto cmp_childs;
        }
    }

    if (dn->childs->size != node->nb_nodes)
        return 0;

cmp_childs:
    for (i = 0; i < node->nb_nodes; i++) {
        node_t *child = node->nodes[i];

        while (OP_EXPR == child->type)
            child = child->nodes[0];

        if (0 == dag_node_same(dn->childs->data[i], child))
            return 0;
    }

    for (i = 0; i < dn->childs->size; i++) {
        dag_node_t *child = dn->childs->data[i];
        dag_node_t *parent = NULL;

        assert(child->parents);

        int j;
        for (j = 0; j < child->parents->size; j++) {
            if (dn == child->parents->data[j])
                break;
        }
        assert(j < child->parents->size);

        for (++j; j < child->parents->size; j++) {
            parent = child->parents->data[j];

            if (OP_INC == parent->type
                || OP_DEC == parent->type
                || OP_3AC_SETZ == parent->type
                || OP_3AC_SETNZ == parent->type
                || OP_3AC_SETLT == parent->type
                || OP_3AC_SETLE == parent->type
                || OP_3AC_SETGT == parent->type
                || OP_3AC_SETGE == parent->type)
                return 0;

            if (type_is_assign(parent->type)) {
                if (child == parent->childs->data[0])
                    return 0;
            }
        }
    }

    if (OP_CALL == dn->type)
        return __dn_same_call(dn, node, split);

    return 1;
}

dag_node_t *dag_find_node(list_t *h, const node_t *node) {
    dag_node_t *dn;
    list_t *l;
    node_t *origin = (node_t *)node;

    while (OP_EXPR == origin->type)
        origin = origin->nodes[0];

    for (l = list_tail(h); l != list_sentinel(h); l = list_prev(l)) {
        dn = list_data(l, dag_node_t, list);

        if (dag_node_same(dn, origin))
            return dn;
    }

    return NULL;
}

int dag_get_node(list_t *h, const node_t *node, dag_node_t **pp) {
    const node_t *node2;
    variable_t *v;
    dag_node_t *dn;

    if (*pp)
        node2 = (*pp)->node;
    else
        node2 = node;

    v = _operand_get((node_t *)node2);

    dn = dag_find_node(h, node2);

    if (!dn) {
        dn = dag_node_alloc(node2->type, v, node2);
        if (!dn)
            return -ENOMEM;

        list_add_tail(h, &dn->list);

        dn->old = *pp;
    } else {
        dn->var->local_flag |= v->local_flag;
        dn->var->tmp_flag |= v->tmp_flag;
    }

    *pp = dn;
    return 0;
}

int dag_dn_same(dag_node_t *dn0, dag_node_t *dn1) {
    int i;

    if (dn0->type != dn1->type)
        return 0;

    if (type_is_var(dn0->type)) {
        if (dn0->var == dn1->var)
            return 1;
        else
            return 0;
    }

    if (!dn0->childs) {
        if (dn1->childs)
            return 0;

        if (dn0->var == dn1->var)
            return 1;
        logd("dn0: %p, %p, dn1: %p, %p, var: %p, %p\n", dn0, dn0->childs, dn1, dn1->childs, dn0->var, dn1->var);
        return 0;
    } else if (!dn1->childs)
        return 0;

    if (dn0->childs->size != dn1->childs->size) {
        logd("dn0: %p, %d, dn1: %p, %d\n", dn0, dn0->childs->size, dn1, dn1->childs->size);
        return 0;
    }

    for (i = 0; i < dn0->childs->size; i++) {
        dag_node_t *child0 = dn0->childs->data[i];
        dag_node_t *child1 = dn1->childs->data[i];

        if (2 == i && type_is_assign_array_index(dn0->type))
            continue;
        if (2 == i && OP_ARRAY_INDEX == dn0->type)
            continue;

        if (0 == dag_dn_same(child0, child1))
            return 0;
    }

    return 1;
}

int dag_find_roots(list_t *h, vector_t *roots) {
    dag_node_t *dn;
    list_t *l;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        dn = list_data(l, dag_node_t, list);

        if (!dn->parents) {
            int ret = vector_add(roots, dn);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

static int _dn_status_index(vector_t *indexes, dag_node_t *dn_index, int type) {
    dn_index_t *di;

    di = dn_index_alloc();
    if (!di)
        return -ENOMEM;

    if (OP_ARRAY_INDEX == type) {
        if (variable_const(dn_index->var))
            di->index = dn_index->var->data.i;
        else
            di->index = -1;
        di->dn = dn_index;

    } else if (OP_POINTER == type) {
        di->member = dn_index->var;
        di->dn = dn_index;
    } else {
        dn_index_free(di);
        return -1;
    }

    int ret = vector_add(indexes, di);
    if (ret < 0) {
        dn_index_free(di);
        return ret;
    }
    return 0;
}

int dn_status_index(dn_status_t *ds, dag_node_t *dn_index, int type) {
    return _dn_status_index(ds->dn_indexes, dn_index, type);
}

int dn_status_alias_index(dn_status_t *ds, dag_node_t *dn_index, int type) {
    return _dn_status_index(ds->alias_indexes, dn_index, type);
}

void ds_vector_clear_by_ds(vector_t *vec, dn_status_t *ds) {
    dn_status_t *ds2;

    while (1) {
        ds2 = vector_find_cmp(vec, ds, ds_cmp_same_indexes);
        if (!ds2)
            break;

        assert(0 == vector_del(vec, ds2));

        dn_status_free(ds2);
        ds2 = NULL;
    }
}

void ds_vector_clear_by_dn(vector_t *vec, dag_node_t *dn) {
    dn_status_t *ds;

    while (1) {
        ds = vector_find_cmp(vec, dn, dn_status_cmp);
        if (!ds)
            break;

        assert(0 == vector_del(vec, ds));

        dn_status_free(ds);
        ds = NULL;
    }
}

static int __ds_for_dn(dn_status_t *ds, dag_node_t *dn_base) {
    dag_node_t *dn_index;
    dag_node_t *dn_scale;
    dn_index_t *di;

    int ret;

    if (!ds || !dn_base || !ds->dn_indexes)
        return -EINVAL;

    while (OP_DEREFERENCE == dn_base->type) {
        di = dn_index_alloc();
        if (!di)
            return -ENOMEM;
        di->index = 0;

        ret = vector_add(ds->dn_indexes, di);
        if (ret < 0) {
            dn_index_free(di);
            return -ENOMEM;
        }

        dn_base = dn_base->childs->data[0];
    }

    while (OP_ARRAY_INDEX == dn_base->type
           || OP_POINTER == dn_base->type) {
        dn_index = dn_base->childs->data[1];

        ret = dn_status_index(ds, dn_index, dn_base->type);
        if (ret < 0)
            return ret;

        if (OP_ARRAY_INDEX == dn_base->type) {
            di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

            di->dn_scale = dn_base->childs->data[2];
            assert(di->dn_scale);
        }

        dn_base = dn_base->childs->data[0];
    }

    /*	 loge("dn_base->type: %d\n", dn_base->type);
        assert( type_is_var(dn_base->type)
                ||  OP_INC == dn_base->type ||  OP_INC_POST == dn_base->type
                ||  OP_DEC == dn_base->type ||  OP_DEC_POST == dn_base->type
                ||  OP_CALL == dn_base->type
                ||  OP_ADDRESS_OF == dn_base->type);
    */
    ds->dag_node = dn_base;
    return 0;
}

int ds_for_dn(dn_status_t **pds, dag_node_t *dn) {
    dn_status_t *ds;

    ds = dn_status_null();
    if (!ds)
        return -ENOMEM;

    ds->dn_indexes = vector_alloc();
    if (!ds->dn_indexes) {
        free(ds);
        return -ENOMEM;
    }

    int ret = __ds_for_dn(ds, dn);
    if (ret < 0) {
        dn_status_free(ds);
        return ret;
    }

    if (0 == ds->dn_indexes->size) {
        vector_free(ds->dn_indexes);
        ds->dn_indexes = NULL;
    }

    *pds = ds;
    return 0;
}

int ds_for_assign_member(dn_status_t **pds, dag_node_t *dn_base, dag_node_t *dn_member) {
    dn_status_t *ds;
    dn_index_t *di;
    int i;

    ds = dn_status_null();
    if (!ds)
        return -ENOMEM;

    ds->dn_indexes = vector_alloc();
    if (!ds->dn_indexes) {
        free(ds);
        return -ENOMEM;
    }

    int ret = __ds_for_dn(ds, dn_base);
    if (ret < 0) {
        dn_status_free(ds);
        return ret;
    }

    ret = dn_status_index(ds, dn_member, OP_POINTER);
    if (ret < 0)
        return ret;

    di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

    for (i = ds->dn_indexes->size - 2; i >= 0; i--)
        ds->dn_indexes->data[i + 1] = ds->dn_indexes->data[i];

    ds->dn_indexes->data[0] = di;

    *pds = ds;
    return 0;
}

int ds_for_assign_array_member(dn_status_t **pds, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale) {
    dn_status_t *ds;
    dn_index_t *di;
    int i;

    ds = dn_status_null();
    if (!ds)
        return -ENOMEM;

    ds->dn_indexes = vector_alloc();
    if (!ds->dn_indexes) {
        free(ds);
        return -ENOMEM;
    }

    int ret = __ds_for_dn(ds, dn_base);
    if (ret < 0) {
        dn_status_free(ds);
        return ret;
    }

    ret = dn_status_index(ds, dn_index, OP_ARRAY_INDEX);
    if (ret < 0)
        return ret;

    di = ds->dn_indexes->data[ds->dn_indexes->size - 1];

    di->dn_scale = dn_scale;

    for (i = ds->dn_indexes->size - 2; i >= 0; i--)
        ds->dn_indexes->data[i + 1] = ds->dn_indexes->data[i];

    ds->dn_indexes->data[0] = di;

    *pds = ds;
    return 0;
}

int ds_for_assign_dereference(dn_status_t **pds, dag_node_t *dn) {
    dn_status_t *ds;
    dag_node_t *dn_index;
    dn_index_t *di;

    ds = dn_status_null();
    if (!ds)
        return -ENOMEM;

    ds->dn_indexes = vector_alloc();
    if (!ds->dn_indexes) {
        free(ds);
        return -ENOMEM;
    }

    di = dn_index_alloc();
    if (!di) {
        dn_status_free(ds);
        return -ENOMEM;
    }
    di->index = 0;

    int ret = vector_add(ds->dn_indexes, di);
    if (ret < 0) {
        dn_index_free(di);
        dn_status_free(ds);
        return ret;
    }

    ret = __ds_for_dn(ds, dn);
    if (ret < 0) {
        dn_status_free(ds);
        return ret;
    }

    *pds = ds;
    return 0;
}
