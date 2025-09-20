#include "ast.h"
#include "operator_handler_const.h"
#include "type_cast.h"
#include "calculate.h"

typedef struct {
    variable_t **pret;

} handler_data_t;

static handler_data_t *gd = NULL;

static int __op_const_call(ast_t *ast, function_t *f, void *data);

static int _op_const_node(ast_t *ast, node_t *node, handler_data_t *d) {
    operator_t *op = node->op;

    if (!op) {
        op = find_base_operator_by_type(node->type);
        if (!op) {
            loge("\n");
            return -1;
        }
    }

    operator_handler_pt h = find_const_operator_handler(op->type);
    if (!h)
        return -1;

    return h(ast, node->nodes, node->nb_nodes, d);
}

static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    if (!node)
        return 0;

    if (FUNCTION == node->type)
        return __op_const_call(ast, (function_t *)node, data);

    if (0 == node->nb_nodes) {
        if (type_is_var(node->type) && node->var->w)
            logd("w: %s\n", node->var->w->text->data);

        assert(type_is_var(node->type)
               || LABEL == node->type
               || node->split_flag);
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
                return -1;
            }
        }

        operator_handler_pt h = find_const_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            return -1;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            return -1;
        }
    } else {
        for (i = node->nb_nodes - 1; i >= 0; i--) {
            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                return -1;
            }
        }

        operator_handler_pt h = find_const_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            return -1;
        }

        if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

static int _op_const_create(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 3);

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v2 = _operand_get(nodes[2]);
    node_t *parent = nodes[0]->parent;

    assert(FUNCTION_PTR == v0->type && v0->func_ptr);
    assert(FUNCTION_PTR == v2->type && v2->func_ptr);

    while (parent && FUNCTION != parent->type)
        parent = parent->parent;

    if (!parent) {
        loge("\n");
        return -1;
    }

    function_t *caller = (function_t *)parent;
    function_t *callee0 = v0->func_ptr;
    function_t *callee2 = v2->func_ptr;

    if (caller != callee0) {
        if (vector_add_unique(caller->callee_functions, callee0) < 0)
            return -1;

        if (vector_add_unique(callee0->caller_functions, caller) < 0)
            return -1;
    }

    if (caller != callee2) {
        if (vector_add_unique(caller->callee_functions, callee2) < 0)
            return -1;

        if (vector_add_unique(callee2->caller_functions, caller) < 0)
            return -1;
    }

    return 0;
}

static int _op_const_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    variable_t *v0 = _operand_get(nodes[0]);
    assert(v0);

    if (variable_nb_pointers(v0) <= 0) {
        loge("index out\n");
        return -1;
    }

    handler_data_t *d = data;

    int ret = _expr_calculate_internal(ast, nodes[1], d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

static int _op_const_block(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (0 == nb_nodes)
        return 0;

    handler_data_t *d = data;
    block_t *up = ast->current_block;

    ast->current_block = (block_t *)(nodes[0]->parent);

    int ret;
    int i;

    for (i = 0; i < nb_nodes; i++) {
        node_t *node = nodes[i];

        if (FUNCTION == node->type)
            ret = __op_const_call(ast, (function_t *)node, data);
        else
            ret = _op_const_node(ast, node, d);

        if (ret < 0) {
            loge("\n");
            ast->current_block = up;
            return -1;
        }
    }

    ast->current_block = up;
    return 0;
}

static int _op_const_return(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    int i;
    for (i = 0; i < nb_nodes; i++) {
        expr_t *e = nodes[i];
        variable_t *r = NULL;

        if (_expr_calculate_internal(ast, e, &r) < 0)
            return -1;
    }

    return 0;
}

static int _op_const_break(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_continue(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_label(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_goto(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_if(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (nb_nodes < 2) {
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];

    assert(OP_EXPR == e->type);

    if (_expr_calculate_internal(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }

    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _op_const_node(ast, nodes[i], d);
        if (ret < 0)
            return -1;
    }

    return 0;
}

static int _op_const_do(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[1];

    assert(OP_EXPR == e->type);

    if (_op_const_node(ast, nodes[1], d) < 0)
        return -1;

    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;

    return 0;
}

static int _op_const_while(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes || 1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];

    assert(OP_EXPR == e->type);

    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;

    if (2 == nb_nodes) {
        if (_op_const_node(ast, nodes[1], d) < 0)
            return -1;
    }

    return 0;
}

static int _op_const_switch(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];
    expr_t *e2;
    node_t *b = nodes[1];
    node_t *child;

    assert(OP_EXPR == e->type);

    if (_expr_calculate_internal(ast, e, &r) < 0)
        return -1;

    int i;
    for (i = 0; i < b->nb_nodes; i++) {
        child = b->nodes[i];

        if (OP_CASE == child->type) {
            assert(1 == child->nb_nodes);

            e = child->nodes[0];

            assert(OP_EXPR == e->type);

            if (_expr_calculate_internal(ast, e, &r) < 0)
                return -1;

        } else {
            if (_op_const_node(ast, child, d) < 0)
                return -1;
        }
    }

    return 0;
}

static int _op_const_case(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_default(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_vla_alloc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes);

    return 0;
}

static int _op_const_for(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes);

    handler_data_t *d = data;

    if (nodes[0]) {
        if (_op_const_node(ast, nodes[0], d) < 0) {
            loge("\n");
            return -1;
        }
    }

    expr_t *e = nodes[1];
    if (e) {
        assert(OP_EXPR == e->type);

        variable_t *r = NULL;

        if (_expr_calculate_internal(ast, e, &r) < 0) {
            loge("\n");
            return -1;
        }
    }

    int i;
    for (i = 2; i < nb_nodes; i++) {
        if (!nodes[i])
            continue;

        if (_op_const_node(ast, nodes[i], d) < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

static int __op_const_call(ast_t *ast, function_t *f, void *data) {
    logd("f: %p, f->node->w: %s\n", f, f->node.w->text->data);

    handler_data_t *d = data;
    block_t *tmp = ast->current_block;

    // change the current block
    ast->current_block = (block_t *)f;

    if (_op_const_block(ast, f->node.nodes, f->node.nb_nodes, d) < 0) {
        loge("\n");
        return -1;
    }

    ast->current_block = tmp;
    return 0;
}

static int _op_const_call(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 0);

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(FUNCTION_PTR == v0->type && v0->func_ptr);

    while (parent && FUNCTION != parent->type)
        parent = parent->parent;

    if (!parent) {
        loge("\n");
        return -1;
    }

    function_t *caller = (function_t *)parent;
    function_t *callee = v0->func_ptr;

    if (caller != callee) {
        if (vector_add_unique(caller->callee_functions, callee) < 0)
            return -1;

        if (vector_add_unique(callee->caller_functions, caller) < 0)
            return -1;
    }

    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _expr_calculate_internal(ast, nodes[i], data);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

static int _op_const_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    node_t *parent = nodes[0]->parent;
    node_t *node;

    expr_simplify(&nodes[0]);

    node = nodes[0];

    int ret = _expr_calculate_internal(ast, node, data);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    if (parent->result)
        variable_free(parent->result);

    variable_t *v = _operand_get(node);
    if (v)
        parent->result = variable_ref(v);
    else
        parent->result = NULL;

    return 0;
}

static int _op_const_inc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}
static int _op_const_inc_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_dec(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}
static int _op_const_dec_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_positive(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_dereference(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    variable_t *v = _operand_get(nodes[0]->parent);

    v->const_flag = 0;
    return 0;
}

static int _op_const_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    node_t *child = nodes[1];
    node_t *parent = nodes[0]->parent;

    variable_t *dst = _operand_get(nodes[0]);
    variable_t *src = _operand_get(nodes[1]);
    variable_t *result = _operand_get(parent);
    variable_t *r = NULL;

    if (variable_const(src)) {
        if (FUNCTION_PTR == src->type || src->nb_dimentions > 0) {
            r = variable_ref(src);

            node_free_data(parent);

            parent->type = r->type;
            parent->var = r;
            return 0;
        }

        int dst_type = dst->type;

        if (dst->nb_pointers + dst->nb_dimentions > 0)
            dst_type = VAR_UINTPTR;

        type_cast_t *cast = find_base_type_cast(src->type, dst_type);
        if (cast) {
            int ret = cast->func(ast, &r, src);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
            r->const_flag = 1;

            if (parent->w)
                XCHG(r->w, parent->w);

            node_free_data(parent);
            parent->type = r->type;
            parent->var = r;
        }

        return 0;
    } else
        result->const_flag = 0;

    if (variable_integer(src) && variable_integer(dst)) {
        int size;
        if (src->nb_dimentions > 0)
            size = sizeof(void *);
        else
            size = src->size;

        assert(0 == dst->nb_dimentions);

        if (variable_size(dst) <= size) {
            node_t *child = nodes[1];

            logd("child: %d/%s, size: %d, dst size: %d\n", src->w->line, src->w->text->data,
                 size, variable_size(dst));

            nodes[1] = NULL;
            node_free_data(parent);
            node_move_data(parent, child);
        }
    }

    return 0;
}

static int _op_const_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return -1;
}

static int _op_const_unary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);

    int const_flag = v0->const_flag && 0 == v0->nb_pointers && 0 == v0->nb_dimentions;
    if (!const_flag)
        return 0;

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

static int _op_const_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

static int _op_const_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_unary(ast, nodes, nb_nodes, data);
}

static int _op_const_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    if (type_is_number(v0->type) && type_is_number(v1->type)) {
        if (!variable_const(v0) || !variable_const(v1))
            return 0;

        assert(v0->type == v1->type);

        calculate_t *cal = find_base_calculate(parent->type, v0->type, v1->type);
        if (!cal) {
            loge("type %d, %d not support, line: %d\n", v0->type, v1->type, parent->w->line);
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
    }

    return 0;
}

static int _op_const_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
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

static int _op_const_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_add_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_sub_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_mul_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_div_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_mod_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_shl_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_shr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    int ret = _shift_check_const(nodes, nb_nodes);
    if (ret < 0)
        return ret;

    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_and_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_or_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_cmp(ast, nodes, nb_nodes, data);
}

static int _op_const_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_const_binary(ast, nodes, nb_nodes, data);
}

static int _op_const_va_start(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_va_arg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_const_va_end(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

operator_handler_pt const_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_const_expr,
        [OP_CALL] = _op_const_call,

        [OP_ARRAY_INDEX] = _op_const_array_index,
        [OP_POINTER] = _op_const_pointer,
        [OP_CREATE] = _op_const_create,

        [OP_VA_START] = _op_const_va_start,
        [OP_VA_ARG] = _op_const_va_arg,
        [OP_VA_END] = _op_const_va_end,

        [OP_SIZEOF] = _op_const_sizeof,
        [OP_TYPE_CAST] = _op_const_type_cast,
        [OP_LOGIC_NOT] = _op_const_logic_not,
        [OP_BIT_NOT] = _op_const_bit_not,
        [OP_NEG] = _op_const_neg,
        [OP_POSITIVE] = _op_const_positive,

        [OP_INC] = _op_const_inc,
        [OP_DEC] = _op_const_dec,

        [OP_INC_POST] = _op_const_inc_post,
        [OP_DEC_POST] = _op_const_dec_post,

        [OP_DEREFERENCE] = _op_const_dereference,
        [OP_ADDRESS_OF] = _op_const_address_of,

        [OP_MUL] = _op_const_mul,
        [OP_DIV] = _op_const_div,
        [OP_MOD] = _op_const_mod,

        [OP_ADD] = _op_const_add,
        [OP_SUB] = _op_const_sub,

        [OP_SHL] = _op_const_shl,
        [OP_SHR] = _op_const_shr,

        [OP_BIT_AND] = _op_const_bit_and,
        [OP_BIT_OR] = _op_const_bit_or,

        [OP_EQ] = _op_const_eq,
        [OP_NE] = _op_const_ne,
        [OP_GT] = _op_const_gt,
        [OP_LT] = _op_const_lt,
        [OP_GE] = _op_const_ge,
        [OP_LE] = _op_const_le,

        [OP_LOGIC_AND] = _op_const_logic_and,
        [OP_LOGIC_OR] = _op_const_logic_or,

        [OP_ASSIGN] = _op_const_assign,
        [OP_ADD_ASSIGN] = _op_const_add_assign,
        [OP_SUB_ASSIGN] = _op_const_sub_assign,
        [OP_MUL_ASSIGN] = _op_const_mul_assign,
        [OP_DIV_ASSIGN] = _op_const_div_assign,
        [OP_MOD_ASSIGN] = _op_const_mod_assign,
        [OP_SHL_ASSIGN] = _op_const_shl_assign,
        [OP_SHR_ASSIGN] = _op_const_shr_assign,
        [OP_AND_ASSIGN] = _op_const_and_assign,
        [OP_OR_ASSIGN] = _op_const_or_assign,

        [OP_BLOCK] = _op_const_block,
        [OP_RETURN] = _op_const_return,
        [OP_BREAK] = _op_const_break,
        [OP_CONTINUE] = _op_const_continue,
        [OP_GOTO] = _op_const_goto,
        [LABEL] = _op_const_label,

        [OP_IF] = _op_const_if,
        [OP_WHILE] = _op_const_while,
        [OP_DO] = _op_const_do,
        [OP_FOR] = _op_const_for,

        [OP_SWITCH] = _op_const_switch,
        [OP_CASE] = _op_const_case,
        [OP_DEFAULT] = _op_const_default,

        [OP_VLA_ALLOC] = _op_const_vla_alloc,
};

operator_handler_pt find_const_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return const_operator_handlers[type];
}

int function_const_opt(ast_t *ast, function_t *f) {
    handler_data_t d = {0};

    int ret = __op_const_call(ast, f, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}
