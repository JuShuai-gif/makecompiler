#include "parse.h"

expr_t *expr_alloc() {
    expr_t *e = calloc(1, sizeof(expr_t));
    if (!e)
        return NULL;

    e->nodes = calloc(1, sizeof(node_t *));
    if (!e->nodes) {
        free(e);
        return NULL;
    }

    e->type = OP_EXPR;
    return e;
}

int expr_copy(node_t *e2, node_t *e) {
    node_t *node;
    int i;

    for (i = 0; i < e->nb_nodes; i++) {
        node = node_clone(e->nodes[i]);
        if (!node)
            return -ENOMEM;

        node_add_child(e2, node);

        int ret = expr_copy(e2->nodes[i], e->nodes[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

expr_t *expr_clone(node_t *e) {
    expr_t *e2 = node_clone(e);
    if (!e2)
        return NULL;

    if (expr_copy(e2, e) < 0) {
        expr_free(e2);
        return NULL;
    }

    return e2;
}

void expr_free(expr_t *e) {
    if (e) {
        node_free(e);
        e = NULL;
    }
}

static int __expr_node_add_node(node_t **pparent, node_t *child) {
    node_t *parent = *pparent;
    if (!parent) {
        *pparent = child;
        return 0;
    }

    if (parent->priority > child->priority) {
        assert(parent->op);

        if (parent->op->nb_operands > parent->nb_nodes)
            return node_add_child(parent, child);

        assert(parent->nb_nodes >= 1);
        return __expr_node_add_node(&(parent->nodes[parent->nb_nodes - 1]), child);

    } else if (parent->priority < child->priority) {
        assert(child->op);

        if (child->op->nb_operands > 0)
            assert(child->op->nb_operands > child->nb_nodes);

        child->parent = parent->parent;
        if (node_add_child(child, parent) < 0)
            return -1;

        *pparent = child;
        return 0;
    }

    // parent->priority == child->priority
    assert(parent->op);
    assert(child->op);

    if (OP_ASSOCIATIVITY_LEFT == child->op->associativity) {
        if (child->op->nb_operands > 0)
            assert(child->op->nb_operands > child->nb_nodes);

        child->parent = parent->parent;

        node_add_child(child, parent); // add parent to child's child node

        *pparent = child; // child is the new parent node
        return 0;
    }

    if (parent->op->nb_operands > parent->nb_nodes)
        return node_add_child(parent, child);

    assert(parent->nb_nodes >= 1);
    return __expr_node_add_node(&(parent->nodes[parent->nb_nodes - 1]), child);
}

int expr_add_node(expr_t *e, node_t *node) {
    assert(e);
    assert(node);

    if (type_is_var(node->type))
        node->priority = -1;

    else if (type_is_operator(node->type)) {
        node->op = find_base_operator_by_type(node->type);
        if (!node->op) {
            loge("\n");
            return -1;
        }
        node->priority = node->op->priority;
    } else {
        loge("\n");
        return -1;
    }

    if (__expr_node_add_node(&(e->nodes[0]), node) < 0)
        return -1;

    e->nodes[0]->parent = e;
    e->nb_nodes = 1;
    return 0;
}

void expr_simplify(expr_t **pe) {
    expr_t **pp = pe;
    expr_t *e;

    while (OP_EXPR == (*pp)->type) {
        e = *pp;
        pp = &(e->nodes[0]);
    }

    if (pp != pe) {
        e = *pp;
        *pp = NULL;

        e->parent = (*pe)->parent;

        expr_free(*pe);
        *pe = e;
    }
}
