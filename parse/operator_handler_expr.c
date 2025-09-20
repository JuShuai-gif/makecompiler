#include "ast.h"
#include "operator_handler_const.h"
#include "operator_handler_semantic.h"
#include "type_cast.h"
#include "calculate.h"

typedef struct {
    variable_t **pret;
} handler_data_t;

operator_handler_pt find_expr_operator_handler(const int type);

static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    if (!node)
        return 0;

    if (0 == node->nb_nodes) {
        assert(type_is_var(node->type));

        if (type_is_var(node->type) && node->var->w)
            logd("w: %s\n", node->var->w->text->data);

        return 0;
    }

    assert(type_is_operator(node->type));
    assert(node->nb_nodes > 0);

    if (!node->op) {
        node->op = find_base_operator_by_type(node->type);
        if (!node->op) {
            loge("node %p, type: %d, w: %p\n", node, node->type, node->w);
            return -1;
        }
    }

    handler_data_t *d = data;
    int i;

    if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {
        for (i = 0; i < node->nb_nodes; i++) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }

        operator_handler_pt h = find_expr_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            goto _error;
        }
    } else {
        for (i = node->nb_nodes - 1; i >= 0; i--) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }

        operator_handler_pt h = find_expr_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            goto _error;
        }
    }

    return 0;

_error:
    return -1;
}

static int _op_expr_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);
    return 0;
}

static int _op_expr_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    node_t *parent = nodes[0]->parent;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    if (!variable_const(v1)) {
        loge("\n");
        return -EINVAL;
    }

    if (!v0->const_literal_flag && !v0->member_flag) {
        loge("\n");
        return -EINVAL;
    }

    return 0;
}

static int _op_expr_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    node_t *n = nodes[0];
    node_t *parent = nodes[0]->parent;

    while (OP_EXPR == n->type)
        n = n->nodes[0];

    n->parent->nodes[0] = NULL;
    node_free(nodes[0]);
    nodes[0] = n;
    n->parent = parent;

    int ret = _expr_calculate_internal(ast, n, d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

static int _op_expr_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_expr_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    variable_t *dst = _operand_get(nodes[0]);
    variable_t *src = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    if (variable_const(src)) {
        int dst_type = dst->type;

        if (dst->nb_pointers + dst->nb_dimentions > 0)
            dst_type = VAR_UINTPTR;

        type_cast_t *cast = find_base_type_cast(src->type, dst_type);
        if (cast) {
            variable_t *r = NULL;

            int ret = cast->func(ast, &r, src);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
            r->const_flag = 1;
            r->type = dst->type;
            r->nb_pointers = variable_nb_pointers(dst);
            r->const_literal_flag = 1;

            if (parent->w)
                XCHG(r->w, parent->w);

            logd("parent: %p\n", parent);
            node_free_data(parent);
            parent->type = r->type;
            parent->var = r;
        }

        return 0;
    } else if (variable_const_string(src)) {
        assert(src == nodes[1]->var);

        variable_t *v = variable_ref(src);
        assert(v);

        node_free_data(parent);

        logd("parent->result: %p, parent: %p, v->type: %d\n", parent->result, parent, v->type);
        parent->type = v->type;
        parent->var = v;

        return 0;
    }

    return 0;
}

static int _op_expr_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return -1;
}

static int _op_expr_unary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);

    if (!v0->const_flag)
        return -1;

    if (type_is_number(v0->type)) {
        calculate_t *cal = find_base_calculate(parent->type, v0->type, v0->type);
        if (!cal) {
            loge("type %d not support\n", v0->type);
            return -EINVAL;
        }

        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, NULL);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
        r->const_flag = 1;

        XCHG(r->w, parent->w);

        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;

    } else {
        loge("type %d not support\n", v0->type);
        return -1;
    }

    return 0;
}

static int _op_expr_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data);
}

static int _op_expr_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data);
}

static int _op_expr_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_unary(ast, nodes, nb_nodes, data);
}

static int _op_expr_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    if (type_is_number(v0->type) && type_is_number(v1->type)) {
        if (!variable_const(v0) || !variable_const(v1)) {
            return 0;
        }

        assert(v0->type == v1->type);

        calculate_t *cal = find_base_calculate(parent->type, v0->type, v1->type);
        if (!cal) {
            loge("type %d, %d not support\n", v0->type, v1->type);
            return -EINVAL;
        }

        variable_t *r = NULL;
        int ret = cal->func(ast, &r, v0, v1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
        r->const_flag = 1;

        XCHG(r->w, parent->w);

        node_free_data(parent);
        parent->type = r->type;
        parent->var = r;

    } else {
        loge("type %d, %d not support\n", v0->type, v1->type);
        return -1;
    }

    return 0;
}

static int _op_expr_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _shift_check_const(node_t **nodes, int nb_nodes) {
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    if (variable_const(v1)) {
        if (v1->data.i < 0) {
            loge("shift count %d < 0\n", v1->data.i);
            return -EINVAL;
        }

        if (v1->data.i >= v0->size << 3) {
            loge("shift count %d >= type bits: %d\n", v1->data.i, v0->size << 3);
            return -EINVAL;
        }
    }

    return 0;
}

static int _op_expr_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _expr_left(member_t **pm, node_t *left) {
    variable_t *idx;
    member_t *m;

    m = member_alloc(NULL);
    if (!m)
        return -ENOMEM;

    while (left) {
        if (OP_EXPR == left->type) {
        } else if (OP_ARRAY_INDEX == left->type) {
            idx = _operand_get(left->nodes[1]);

            if (member_add_index(m, NULL, idx->data.i) < 0)
                return -1;

        } else if (OP_POINTER == left->type) {
            idx = _operand_get(left->nodes[1]);

            if (member_add_index(m, idx, 0) < 0)
                return -1;
        } else
            break;

        left = left->nodes[0];
    }

    assert(type_is_var(left->type));

    m->base = _operand_get(left);

    *pm = m;
    return 0;
}

static int _expr_right(member_t **pm, node_t *right) {
    variable_t *idx;
    member_t *m;

    m = member_alloc(NULL);
    if (!m)
        return -ENOMEM;

    while (right) {
        if (OP_EXPR == right->type)
            right = right->nodes[0];

        else if (OP_ASSIGN == right->type)
            right = right->nodes[1];

        else if (OP_ADDRESS_OF == right->type) {
            if (member_add_index(m, NULL, -OP_ADDRESS_OF) < 0)
                return -1;

            right = right->nodes[0];

        } else if (OP_ARRAY_INDEX == right->type) {
            idx = _operand_get(right->nodes[1]);
            assert(idx->data.i >= 0);

            if (member_add_index(m, NULL, idx->data.i) < 0)
                return -1;

            right = right->nodes[0];

        } else if (OP_POINTER == right->type) {
            idx = _operand_get(right->nodes[1]);

            if (member_add_index(m, idx, 0) < 0)
                return -1;

            right = right->nodes[0];
        } else
            break;
    }

    assert(type_is_var(right->type));

    m->base = _operand_get(right);

    *pm = m;
    return 0;
}

static int _expr_init_const(member_t *m0, variable_t *v1) {
    variable_t *base = m0->base;

    if (!m0->indexes) {
        memcpy(&base->data, &v1->data, v1->size);
        return 0;
    }

    assert(variable_is_array(base)
           || variable_is_struct(base));

    int size = variable_size(base);
    int offset = member_offset(m0);

    assert(offset < size);

    if (!base->data.p) {
        base->data.p = calloc(1, size);
        if (!base->data.p)
            return -ENOMEM;
    }

    memcpy(base->data.p + offset, &v1->data, v1->size);
    return 0;
}

static int _expr_init_address(ast_t *ast, member_t *m0, member_t *m1) {
    int ret = vector_add_unique(ast->global_consts, m1->base);
    if (ret < 0)
        return ret;

    ast_rela_t *r = calloc(1, sizeof(ast_rela_t));
    if (!r)
        return -ENOMEM;
    r->ref = m0;
    r->obj = m1;

    if (vector_add(ast->global_relas, r) < 0) {
        free(r);
        return -ENOMEM;
    }

    return 0;
}

static int _op_expr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    member_t *m0 = NULL;
    member_t *m1 = NULL;

    logd("v0->type: %d, v1->type: %d\n", v0->type, v1->type);

    if (!variable_const_string(v1))
        assert(v0->type == v1->type);

    assert(v0->size == v1->size);

    if (variable_is_array(v1) || variable_is_struct(v1)) {
        loge("\n");
        return -1;
    }

    if (_expr_left(&m0, nodes[0]) < 0) {
        loge("\n");
        return -1;
    }

    if (_expr_right(&m1, nodes[1]) < 0) {
        loge("\n");
        member_free(m0);
        return -1;
    }

    int ret = -1;

    if (!m1->indexes) {
        if (variable_const_string(m1->base)) {
            ret = _expr_init_address(ast, m0, m1);

        } else if (variable_const(m1->base)) {
            if (FUNCTION_PTR == m1->base->type) {
                ret = _expr_init_address(ast, m0, m1);
            } else {
                ret = _expr_init_const(m0, m1->base);

                member_free(m0);
                member_free(m1);
                return ret;
            }
        } else {
            variable_t *v = m1->base;
            loge("v: %d/%s\n", v->w->line, v->w->text->data);
            return -1;
        }
    } else {
        index_t *idx;

        assert(m1->indexes->size >= 1);

        idx = m1->indexes->data[0];

        assert(-OP_ADDRESS_OF == idx->index);

        assert(0 == vector_del(m1->indexes, idx));

        free(idx);
        idx = NULL;

        ret = _expr_init_address(ast, m0, m1);
    }

    if (ret < 0) {
        loge("\n");
        member_free(m0);
        member_free(m1);
    }
    return ret;
}

static int _op_expr_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_cmp(ast, nodes, nb_nodes, data);
}

static int _op_expr_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

static int _op_expr_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_expr_binary(ast, nodes, nb_nodes, data);
}

operator_handler_pt expr_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_expr_expr,

        [OP_ARRAY_INDEX] = _op_expr_array_index,
        [OP_POINTER] = _op_expr_pointer,

        [OP_SIZEOF] = _op_expr_sizeof,
        [OP_TYPE_CAST] = _op_expr_type_cast,
        [OP_LOGIC_NOT] = _op_expr_logic_not,
        [OP_BIT_NOT] = _op_expr_bit_not,
        [OP_NEG] = _op_expr_neg,

        [OP_ADDRESS_OF] = _op_expr_address_of,

        [OP_MUL] = _op_expr_mul,
        [OP_DIV] = _op_expr_div,
        [OP_MOD] = _op_expr_mod,

        [OP_ADD] = _op_expr_add,
        [OP_SUB] = _op_expr_sub,

        [OP_SHL] = _op_expr_shl,
        [OP_SHR] = _op_expr_shr,

        [OP_BIT_AND] = _op_expr_bit_and,
        [OP_BIT_OR] = _op_expr_bit_or,

        [OP_EQ] = _op_expr_eq,
        [OP_NE] = _op_expr_ne,
        [OP_GT] = _op_expr_gt,
        [OP_LT] = _op_expr_lt,
        [OP_GE] = _op_expr_ge,
        [OP_LE] = _op_expr_le,

        [OP_LOGIC_AND] = _op_expr_logic_and,
        [OP_LOGIC_OR] = _op_expr_logic_or,

        [OP_ASSIGN] = _op_expr_assign,
};

operator_handler_pt find_expr_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return expr_operator_handlers[type];
}

int expr_calculate(ast_t *ast, expr_t *e, variable_t **pret) {
    if (!e || !e->nodes || e->nb_nodes <= 0)
        return -1;

    if (expr_semantic_analysis(ast, e) < 0)
        return -1;

    handler_data_t d = {0};
    variable_t *v;

    if (!type_is_var(e->nodes[0]->type)) {
        if (_expr_calculate_internal(ast, e->nodes[0], &d) < 0) {
            loge("\n");
            return -1;
        }
    }

    v = _operand_get(e->nodes[0]);

    if (pret)
        *pret = variable_ref(v);

    return 0;
}
