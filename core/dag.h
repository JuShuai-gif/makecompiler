#ifndef DAG_H
#define DAG_H

#include "utils_vector.h"
#include "variable.h"
#include "node.h"

typedef struct dag_node_s dag_node_t;
typedef struct dn_index_s dn_index_t;
typedef struct dn_status_s dn_status_t;

enum dn_alias_type {
    DN_ALIAS_NULL = 0,

    DN_ALIAS_VAR,
    DN_ALIAS_ARRAY,
    DN_ALIAS_MEMBER,
    DN_ALIAS_ALLOC,
};

struct dag_node_s {
    list_t list; // for dag node list in scope

    int type; // node type

    variable_t *var;
    dag_node_t *old;
    node_t *node;

    vector_t *parents;
    vector_t *childs;

    dag_node_t *direct;

    void *rabi;
    void *rabi2;

    Epin *pins[EDA_MAX_BITS];
    int n_pins;

    intptr_t color;

    uint32_t done : 1;

    uint32_t active : 1;
    uint32_t inited : 1;
    uint32_t updated : 1;

    uint32_t loaded : 1;
};

struct dn_index_s {
    variable_t *member;
    intptr_t index;

    dag_node_t *dn;
    dag_node_t *dn_scale;

    int refs;
};

struct dn_status_s {
    dag_node_t *dag_node;

    vector_t *dn_indexes;

    dag_node_t *alias;
    vector_t *alias_indexes;
    dag_node_t *dereference;
    int alias_type;

    int refs;

    intptr_t color;

    int ret_index;
    uint32_t ret_flag : 1;

    uint32_t active : 1;
    uint32_t inited : 1;
    uint32_t updated : 1;
    uint32_t loaded : 1;
};

dn_index_t *dn_index_alloc();
dn_index_t *dn_index_clone(dn_index_t *di);
dn_index_t *dn_index_ref(dn_index_t *di);
void dn_index_free(dn_index_t *di);

int dn_index_same(const dn_index_t *di0, const dn_index_t *di1);
int dn_index_like(const dn_index_t *di0, const dn_index_t *di1);

int dn_status_is_like(const dn_status_t *ds);

dn_status_t *dn_status_null();
dn_status_t *dn_status_alloc(dag_node_t *dn);
dn_status_t *dn_status_clone(dn_status_t *ds);
dn_status_t *dn_status_ref(dn_status_t *ds);

void dn_status_free(dn_status_t *ds);
void dn_status_print(dn_status_t *ds);

dag_node_t *dag_node_alloc(int type, variable_t *var, const node_t *node);

int dag_node_add_child(dag_node_t *parent, dag_node_t *child);
int dag_node_find_child(dag_node_t *parent, dag_node_t *child);

int dag_dn_same(dag_node_t *dn0, dag_node_t *dn1);
int dag_node_same(dag_node_t *dn, const node_t *node);
void dag_node_free(dag_node_t *dn);

dag_node_t *dag_find_node(list_t *h, const node_t *node);
int dag_get_node(list_t *h, const node_t *node, dag_node_t **pp);

int dag_find_roots(list_t *h, vector_t *roots);

int dag_expr_calculate(list_t *h, dag_node_t *node);
void dag_node_free_list(list_t *h);

int ds_copy_dn(dn_status_t *dst, dn_status_t *src);
int ds_copy_alias(dn_status_t *dst, dn_status_t *src);

int ds_cmp_alias(const void *p0, const void *p1);
int ds_cmp_same_indexes(const void *p0, const void *p1);
int ds_cmp_like_indexes(const void *p0, const void *p1);

int dn_status_index(dn_status_t *ds, dag_node_t *dn_index, int type);

int dn_status_alias_index(dn_status_t *ds, dag_node_t *dn_index, int type);

int ds_for_dn(dn_status_t **pds, dag_node_t *dn);
int ds_for_assign_member(dn_status_t **pds, dag_node_t *dn_base, dag_node_t *dn_member);
int ds_for_assign_dereference(dn_status_t **pds, dag_node_t *dn);
int ds_for_assign_array_member(dn_status_t **pds, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale);

void ds_vector_clear_by_ds(vector_t *vec, dn_status_t *ds);
void ds_vector_clear_by_dn(vector_t *vec, dag_node_t *dn);

static int dn_through_bb(dag_node_t *dn) {
    variable_t *v = dn->var;

    return (v->global_flag || v->local_flag || v->tmp_flag)
           && !(v->const_flag && 0 == v->nb_pointers + v->nb_dimentions);
}

static int dn_status_cmp(const void *p0, const void *p1) {
    const dag_node_t *dn0 = p0;
    const dn_status_t *ds1 = p1;

    if (ds1->dn_indexes)
        return -1;

    return dn0 != ds1->dag_node;
}

static int dn_status_cmp_dereference(const void *p0, const void *p1) {
    const dag_node_t *dn0 = p0;
    const dn_status_t *ds1 = p1;

    return dn0 != ds1->dereference;
}

static inline int ds_is_pointer(dn_status_t *ds) {
    if (!ds->dn_indexes)
        return ds->dag_node->var->nb_pointers > 0;

    dn_index_t *di;

    int n = variable_nb_pointers(ds->dag_node->var);
    int i;

    for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
        di = ds->dn_indexes->data[i];

        if (di->member)
            n = variable_nb_pointers(di->member);
        else
            n--;
    }

    assert(n >= 0);
    return n;
}

static inline int ds_nb_pointers(dn_status_t *ds) {
    if (!ds->dn_indexes)
        return ds->dag_node->var->nb_pointers;

    dn_index_t *di;

    int n = variable_nb_pointers(ds->dag_node->var);
    int i;

    for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
        di = ds->dn_indexes->data[i];

        if (di->member)
            n = variable_nb_pointers(di->member);
        else
            n--;
    }

    assert(n >= 0);
    return n;
}

#define DN_STATUS_GET(ds, vec, dn)                    \
    do {                                              \
        ds = vector_find_cmp(vec, dn, dn_status_cmp); \
        if (!ds) {                                    \
            ds = dn_status_alloc(dn);                 \
            if (!ds)                                  \
                return -ENOMEM;                       \
            int ret = vector_add(vec, ds);            \
            if (ret < 0) {                            \
                dn_status_free(ds);                   \
                return ret;                           \
            }                                         \
        }                                             \
    } while (0)

#define DN_STATUS_ALLOC(ds, vec, dn)   \
    do {                               \
        ds = dn_status_alloc(dn);      \
        if (!ds)                       \
            return -ENOMEM;            \
        int ret = vector_add(vec, ds); \
        if (ret < 0) {                 \
            dn_status_free(ds);        \
            return ret;                \
        }                              \
    } while (0)

#endif
