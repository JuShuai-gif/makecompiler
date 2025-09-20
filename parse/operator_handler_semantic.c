#include "ast.h"
#include "operator_handler_semantic.h"
#include "type_cast.h"

typedef struct {
    variable_t **pret;

} handler_data_t;

static int __op_semantic_call(ast_t *ast, function_t *f, void *data);

static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data);

static int _op_semantic_node(ast_t *ast, node_t *node, handler_data_t *d) {
    operator_t *op = node->op;

    if (!op) {
        op = find_base_operator_by_type(node->type);
        if (!op) {
            loge("\n");
            return -1;
        }
    }

    operator_handler_pt h = find_semantic_operator_handler(op->type);
    if (!h) {
        loge("\n");
        return -1;
    }

    variable_t **pret = d->pret;

    d->pret = &node->result;
    int ret = h(ast, node->nodes, node->nb_nodes, d);
    d->pret = pret;

    return ret;
}

static int _semantic_add_address_of(ast_t *ast, node_t **pp, node_t *src) {
    node_t *parent = src->parent;

    operator_t *op = find_base_operator_by_type(OP_ADDRESS_OF);
    if (!op)
        return -EINVAL;

    variable_t *v_src = _operand_get(src);

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v_src->type);
    if (ret < 0)
        return ret;
    assert(t);

    variable_t *v = VAR_ALLOC_BY_TYPE(v_src->w, t, v_src->const_flag, v_src->nb_pointers + 1, v_src->func_ptr);
    if (!v)
        return -ENOMEM;

    node_t *address_of = node_alloc(NULL, OP_ADDRESS_OF, NULL);
    if (!address_of) {
        variable_free(v);
        return -ENOMEM;
    }

    ret = node_add_child(address_of, src);
    if (ret < 0) {
        variable_free(v);
        node_free(address_of);
        return ret;
    }

    address_of->op = op;
    address_of->result = v;
    address_of->parent = parent;
    *pp = address_of;
    return 0;
}

static int _semantic_add_type_cast(ast_t *ast, node_t **pp, variable_t *v_dst, node_t *src) {
    node_t *parent = src->parent;

    operator_t *op = find_base_operator_by_type(OP_TYPE_CAST);
    if (!op)
        return -EINVAL;

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v_dst->type);
    if (ret < 0)
        return ret;
    assert(t);

    variable_t *v_src = _operand_get(src);
    variable_t *v = VAR_ALLOC_BY_TYPE(NULL, t, v_src->const_flag, v_dst->nb_pointers, v_dst->func_ptr);
    if (!v)
        return -ENOMEM;

    node_t *dst = node_alloc(NULL, v->type, v);
    variable_free(v);
    v = NULL;
    if (!dst)
        return -ENOMEM;

    node_t *cast = node_alloc(NULL, OP_TYPE_CAST, NULL);
    if (!cast) {
        node_free(dst);
        return -ENOMEM;
    }

    ret = node_add_child(cast, dst);
    if (ret < 0) {
        node_free(dst);
        node_free(cast);
        return ret;
    }

    ret = node_add_child(cast, src);
    if (ret < 0) {
        node_free(cast); // dst is cast's child, will be recursive freed
        return ret;
    }

    cast->op = op;
    cast->result = variable_ref(dst->var);
    cast->parent = parent;
    *pp = cast;
    return 0;
}

static int _semantic_do_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    int ret = find_updated_type(ast, v0, v1);
    if (ret < 0) {
        loge("var type update failed, type: %d, %d\n", v0->type, v1->type);
        return -EINVAL;
    }

    type_t *t = NULL;
    ret = ast_find_type_type(&t, ast, ret);
    if (ret < 0)
        return ret;
    assert(t);

    variable_t *v_std = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);
    if (!v_std)
        return -ENOMEM;

    if (t->type != v0->type) {
        ret = _semantic_add_type_cast(ast, &nodes[0], v_std, nodes[0]);
        if (ret < 0) {
            loge("add type cast failed\n");
            goto end;
        }
    }

    if (t->type != v1->type) {
        ret = _semantic_add_type_cast(ast, &nodes[1], v_std, nodes[1]);
        if (ret < 0) {
            loge("add type cast failed\n");
            goto end;
        }
    }

    ret = 0;
end:
    variable_free(v_std);
    return ret;
}

static void _semantic_check_var_size(ast_t *ast, node_t *node) {
    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, node->type);
    assert(0 == ret);
    assert(t);

    if (0 == node->var->size) {
        node->var->size = t->size;
        logd("node: %p var: %p, var->size: %d\n", node, node->var, node->var->size);
    }

    if (0 == node->var->data_size)
        node->var->data_size = t->size;
}

static int _semantic_add_call_rets(ast_t *ast, node_t *parent, handler_data_t *d, function_t *f) {
    variable_t *fret;
    variable_t *r;
    type_t *t;
    node_t *node;

    if (f->rets->size > 0) {
        if (!parent->result_nodes) {
            parent->result_nodes = vector_alloc();
            if (!parent->result_nodes)
                return -ENOMEM;
        } else
            vector_clear(parent->result_nodes, (void (*)(void *))node_free);
    }

    int i;
    for (i = 0; i < f->rets->size; i++) {
        fret = f->rets->data[i];

        t = NULL;
        int ret = ast_find_type_type(&t, ast, fret->type);
        if (ret < 0)
            return ret;
        assert(t);

        r = VAR_ALLOC_BY_TYPE(parent->w, t, fret->const_flag, fret->nb_pointers, fret->func_ptr);
        node = node_alloc(r->w, parent->type, NULL);
        //		node =   node_alloc(NULL, r->type, r);
        if (!node) {
            loge("\n");
            return -ENOMEM;
        }

        node->result = r;
        node->op = parent->op;
        node->split_parent = parent;
        node->split_flag = 1;

        if (vector_add(parent->result_nodes, node) < 0) {
            loge("\n");
            node_free(node);
            return -ENOMEM;
        }
    }

    if (d->pret && parent->result_nodes->size > 0) {
        r = _operand_get(parent->result_nodes->data[0]);

        *d->pret = variable_ref(r);
    }

    return 0;
}

static int _semantic_add_call(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d, function_t *f) {
    assert(nb_nodes >= 1);

    variable_t *var_pf = NULL;
    node_t *node_pf = NULL;
    node_t *node = NULL;
    node_t *parent = nodes[0]->parent;
    type_t *pt = block_find_type_type(ast->current_block, FUNCTION_PTR);

    var_pf = VAR_ALLOC_BY_TYPE(f->node.w, pt, 1, 1, f);
    if (!var_pf) {
        loge("var alloc error\n");
        return -ENOMEM;
    }
    var_pf->const_flag = 1;
    var_pf->const_literal_flag = 1;

    node_pf = node_alloc(NULL, var_pf->type, var_pf);
    if (!node_pf) {
        loge("node alloc failed\n");
        return -ENOMEM;
    }

    parent->type = OP_CALL;
    parent->op = find_base_operator_by_type(OP_CALL);

    node_add_child(parent, node_pf);

    int i;
    for (i = parent->nb_nodes - 2; i >= 0; i--)
        parent->nodes[i + 1] = parent->nodes[i];
    parent->nodes[0] = node_pf;

    return _semantic_add_call_rets(ast, parent, d, f);
}

static int _semantic_find_proper_function2(ast_t *ast, vector_t *fvec, vector_t *argv, function_t **pf) {
    function_t *f;
    variable_t *v0;
    variable_t *v1;

    int i;
    int j;

    for (i = 0; i < fvec->size; i++) {
        f = fvec->data[i];

        if (function_same_argv(f->argv, argv)) {
            *pf = f;
            return 0;
        }
    }

    for (i = 0; i < fvec->size; i++) {
        f = fvec->data[i];

        for (j = 0; j < argv->size; j++) {
            v0 = f->argv->data[j];
            v1 = argv->data[j];

            if (variable_is_struct_pointer(v0))
                continue;

            if (type_cast_check(ast, v0, v1) < 0)
                break;

            *pf = f;
            return 0;
        }
    }

    return -404;
}

static int _semantic_find_proper_function(ast_t *ast, type_t *t, const char *fname, vector_t *argv, function_t **pf) {
    vector_t *fvec = NULL;

    int ret = scope_find_like_functions(&fvec, t->scope, fname, argv);
    if (ret < 0)
        return ret;

    ret = _semantic_find_proper_function2(ast, fvec, argv, pf);

    vector_free(fvec);
    return ret;
}

static int _semantic_do_overloaded2(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d, vector_t *argv, function_t *f) {
    variable_t *v0;
    variable_t *v1;

    int i;
    for (i = 0; i < argv->size; i++) {
        v0 = f->argv->data[i];
        v1 = argv->data[i];

        if (variable_is_struct_pointer(v0))
            continue;

        if (variable_same_type(v0, v1))
            continue;

        int ret = _semantic_add_type_cast(ast, &nodes[i], v0, nodes[i]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    return _semantic_add_call(ast, nodes, nb_nodes, d, f);
}

static int _semantic_do_overloaded(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    function_t *f;
    variable_t *v;
    vector_t *argv;
    vector_t *fvec = NULL;
    node_t *parent = nodes[0]->parent;
    type_t *t = NULL;

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    int ret;
    int i;

    for (i = 0; i < nb_nodes; i++) {
        v = _operand_get(nodes[i]);

        if (!t && variable_is_struct_pointer(v)) {
            t = NULL;
            ret = ast_find_type_type(&t, ast, v->type);
            if (ret < 0)
                return ret;
            assert(t->scope);
        }

        ret = vector_add(argv, v);
        if (ret < 0) {
            vector_free(argv);
            return ret;
        }
    }

    ret = scope_find_overloaded_functions(&fvec, t->scope, parent->type, argv);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }

    ret = _semantic_find_proper_function2(ast, fvec, argv, &f);
    if (ret < 0)
        loge("\n");
    else
        ret = _semantic_do_overloaded2(ast, nodes, nb_nodes, d, argv, f);

    vector_free(fvec);
    vector_free(argv);
    return ret;
}

static int _semantic_do_overloaded_assign(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    function_t *f;
    variable_t *v;
    vector_t *argv;
    vector_t *fvec = NULL;
    node_t *parent = nodes[0]->parent;
    type_t *t = NULL;

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    int ret;
    int i;
    for (i = 0; i < nb_nodes; i++) {
        v = _operand_get(nodes[i]);

        if (variable_is_struct(v)) {
            if (!t) {
                t = NULL;
                ret = ast_find_type_type(&t, ast, v->type);
                if (ret < 0)
                    return ret;
                assert(t->scope);
            }

            ret = _semantic_add_address_of(ast, &nodes[i], nodes[i]);
            if (ret < 0) {
                loge("\n");
                return ret;
            }

            v = _operand_get(nodes[i]);
        }

        ret = vector_add(argv, v);
        if (ret < 0) {
            vector_free(argv);
            return ret;
        }
    }

    ret = scope_find_overloaded_functions(&fvec, t->scope, parent->type, argv);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }

    ret = _semantic_find_proper_function2(ast, fvec, argv, &f);
    if (ret < 0)
        loge("\n");
    else
        ret = _semantic_do_overloaded2(ast, nodes, nb_nodes, d, argv, f);

    vector_free(fvec);
    vector_free(argv);
    return ret;
}

static int _semantic_do_create(ast_t *ast, node_t **nodes, int nb_nodes, handler_data_t *d) {
    variable_t *v0;
    variable_t *v_pf;
    block_t *b;
    type_t *t;
    type_t *pt;
    node_t *parent = nodes[0]->parent;
    node_t *node0 = nodes[0];
    node_t *node1 = nodes[1];
    node_t *create = NULL;
    node_t *node_pf = NULL;

    v0 = _operand_get(nodes[0]);

    t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    pt = block_find_type_type(ast->current_block, FUNCTION_PTR);
    assert(t);
    assert(pt);

    create = node_alloc(parent->w, OP_CREATE, NULL);
    if (!create)
        return -ENOMEM;

    v_pf = VAR_ALLOC_BY_TYPE(t->w, pt, 1, 1, NULL);
    if (!v_pf)
        return -ENOMEM;
    v_pf->const_literal_flag = 1;

    node_pf = node_alloc(t->w, v_pf->type, v_pf);
    if (!node_pf)
        return -ENOMEM;

    ret = node_add_child(create, node_pf);
    if (ret < 0)
        return ret;

    ret = node_add_child(create, node1);
    if (ret < 0)
        return ret;
    create->parent = parent;
    parent->nodes[1] = create;

    b = block_alloc_cstr("multi_rets");
    if (!b)
        return -ENOMEM;

    ret = node_add_child((node_t *)b, node0);
    if (ret < 0)
        return ret;
    parent->nodes[0] = (node_t *)b;
    b->node.parent = parent;

    ret = _expr_calculate_internal(ast, parent, d);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    return 0;
}

static int _expr_calculate_internal(ast_t *ast, node_t *node, void *data) {
    if (!node)
        return 0;

    if (FUNCTION == node->type)
        return __op_semantic_call(ast, (function_t *)node, data);

    if (0 == node->nb_nodes) {
        if (type_is_var(node->type))
            _semantic_check_var_size(ast, node);

        logd("node->type: %d, %p, %p\n", node->type, _ operand_get(node), node);
        assert(type_is_var(node->type) || LABEL == node->type);
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

    if (node->result) {
        variable_free(node->result);
        node->result = NULL;
    }

    if (node->result_nodes) {
        vector_clear(node->result_nodes, (void (*)(void *))node_free);
        vector_free(node->result_nodes);
        node->result_nodes = NULL;
    }

    operator_handler_pt h;
    handler_data_t *d = data;
    variable_t **pret = d->pret; // save d->pret, and reload it before return

    int i;

    if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {
        // left associativity, from 0 to N -1

        for (i = 0; i < node->nb_nodes; i++) {
            d->pret = &(node->nodes[i]->result);

            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }

        h = find_semantic_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        d->pret = &node->result;

        if (h(ast, node->nodes, node->nb_nodes, d) < 0)
            goto _error;

    } else {
        // right associativity, from N - 1 to 0

        for (i = node->nb_nodes - 1; i >= 0; i--) {
            d->pret = &(node->nodes[i]->result);

            if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
                loge("\n");
                goto _error;
            }
        }

        h = find_semantic_operator_handler(node->op->type);
        if (!h) {
            loge("\n");
            goto _error;
        }

        d->pret = &node->result;

        if (h(ast, node->nodes, node->nb_nodes, d) < 0)
            goto _error;
    }

    d->pret = pret;
    return 0;

_error:
    d->pret = pret;
    return -1;
}

static int _expr_calculate(ast_t *ast, expr_t *e, variable_t **pret) {
    assert(e);
    assert(e->nodes);

    node_t *root = e->nodes[0];

    if (type_is_var(root->type)) {
        logd("root: %p var: %p\n", root, root->var);

        _semantic_check_var_size(ast, root);

        root->result = variable_ref(root->var);

        if (pret)
            *pret = variable_ref(root->var);
        return 0;
    }

    handler_data_t d = {0};
    d.pret = &root->result;

    if (_expr_calculate_internal(ast, root, &d) < 0) {
        loge("\n");
        return -1;
    }

    if (pret) {
        *pret = variable_ref(root->result);
    }
    return 0;
}

static int _semantic_add_var(node_t **pp, ast_t *ast, node_t *parent,
                             lex_word_t *w, int type, int const_, int nb_pointers_, function_t *func_ptr_) {
    node_t *node;
    type_t *t;
    variable_t *v;

    t = NULL;
    int ret = ast_find_type_type(&t, ast, type);
    if (ret < 0)
        return ret;
    if (!t)
        return -ENOMEM;

    v = VAR_ALLOC_BY_TYPE(w, t, const_, nb_pointers_, func_ptr_);
    if (!v)
        return -ENOMEM;

    node = node_alloc(v->w, v->type, v);
    if (!node) {
        variable_free(v);
        return -ENOMEM;
    }

    if (parent) {
        int ret = node_add_child(parent, node);
        if (ret < 0) {
            node_free(node);
            variable_free(v);
            return ret;
        }
    }

    *pp = node;
    return 0;
}

static int _op_semantic_create(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes >= 1);

    handler_data_t *d = data;
    variable_t **pret = NULL;

    int ret;
    int i;

    variable_t *v0;
    variable_t *v1;
    variable_t *v2;
    vector_t *argv;
    type_t *class;
    type_t *t;
    node_t *parent = nodes[0]->parent;
    node_t *ninit = nodes[0];

    function_t *fmalloc;
    function_t *finit;
    node_t *nmalloc;
    node_t *nsize;
    node_t *nthis;
    node_t *nerr;

    v0 = _operand_get(nodes[0]);
    assert(v0 && FUNCTION_PTR == v0->type);

    class = NULL;
    ret = ast_find_type(&class, ast, v0->w->text->data);
    if (ret < 0)
        return ret;
    assert(class);

    fmalloc = NULL;
    ret = ast_find_function(&fmalloc, ast, "  _auto_malloc");
    if (ret < 0)
        return ret;
    if (!fmalloc) {
        loge("\n");
        return -EINVAL;
    }

    argv = vector_alloc();
    if (!argv)
        return -ENOMEM;

    ret = _semantic_add_var(&nthis, ast, NULL, v0->w, class->type, 0, 1, NULL);
    if (ret < 0) {
        vector_free(argv);
        return ret;
    }

    ret = vector_add(argv, nthis->var);
    if (ret < 0) {
        vector_free(argv);
        node_free(nthis);
        return ret;
    }

    for (i = 1; i < nb_nodes; i++) {
        pret = d->pret;
        d->pret = &(nodes[i]->result);
        ret = _expr_calculate_internal(ast, nodes[i], d);
        d->pret = pret;

        if (ret < 0) {
            vector_free(argv);
            node_free(nthis);
            return ret;
        }

        ret = vector_add(argv, _operand_get(nodes[i]));
        if (ret < 0) {
            vector_free(argv);
            node_free(nthis);
            return ret;
        }
    }

    ret = _semantic_find_proper_function(ast, class, "__init", argv, &finit);
    vector_free(argv);

    if (ret < 0) {
        loge("init function of class '%s' not found\n", v0->w->text->data);
        node_free(nthis);
        return -1;
    }
    v0->func_ptr = finit;

    ret = _semantic_add_var(&nsize, ast, parent, v0->w, VAR_INT, 1, 0, NULL);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }
    nsize->var->const_literal_flag = 1;
    nsize->var->data.i64 = class->size;

    ret = _semantic_add_var(&nmalloc, ast, parent, fmalloc->node.w, FUNCTION_PTR, 1, 1, fmalloc);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }
    nmalloc->var->const_literal_flag = 1;

    ret = node_add_child(parent, nthis);
    if (ret < 0) {
        node_free(nthis);
        return ret;
    }

    for (i = parent->nb_nodes - 4; i >= 0; i--)
        parent->nodes[i + 3] = parent->nodes[i];
    parent->nodes[0] = nmalloc;
    parent->nodes[1] = nsize;
    parent->nodes[2] = ninit;
    parent->nodes[3] = nthis;

    assert(parent->nb_nodes - 3 == finit->argv->size);

    for (i = 0; i < finit->argv->size; i++) {
        v1 = finit->argv->data[i];

        v2 = _operand_get(parent->nodes[i + 3]);

        if (variable_is_struct_pointer(v1))
            continue;

        if (variable_same_type(v1, v2))
            continue;

        ret = _semantic_add_type_cast(ast, &parent->nodes[i + 3], v1, parent->nodes[i + 3]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    if (v0->w)
        lex_word_free(v0->w);
    v0->w = lex_word_clone(v0->func_ptr->node.w);

    if (!parent->result_nodes) {
        parent->result_nodes = vector_alloc();
        if (!parent->result_nodes) {
            node_free(nthis);
            return -ENOMEM;
        }
    } else
        vector_clear(parent->result_nodes, (void (*)(void *))node_free);

    if (vector_add(parent->result_nodes, nthis) < 0) {
        node_free(nthis);
        return ret;
    }

    ret = _semantic_add_var(&nerr, ast, NULL, parent->w, VAR_INT, 0, 0, NULL);
    if (ret < 0)
        return ret;

    if (vector_add(parent->result_nodes, nerr) < 0) {
        node_free(nerr);
        return ret;
    }

    nthis->op = parent->op;
    nthis->split_parent = parent;
    nthis->split_flag = 1;

    nerr->op = parent->op;
    nerr->split_parent = parent;
    nerr->split_flag = 1;

    *d->pret = variable_ref(nthis->var);
    return 0;
}

static int _op_semantic_pointer(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    assert(v0);
    assert(v1);
    assert(v0->type >= STRUCT);

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v1->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v1->const_flag, v1->nb_pointers, v1->func_ptr);
    if (!r)
        return -ENOMEM;

    r->member_flag = v1->member_flag;

    int i;
    for (i = 0; i < v1->nb_dimentions; i++)
        variable_add_array_dimention(r, v1->dimentions[i].num, NULL);

    *d->pret = r;
    return 0;
}

static int _op_semantic_array_index(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    variable_t *v0 = _operand_get(nodes[0]);
    assert(v0);

    handler_data_t *d = data;
    variable_t **pret = d->pret;

    d->pret = &(nodes[1]->result);
    int ret = _expr_calculate_internal(ast, nodes[1], d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    variable_t *v1 = _operand_get(nodes[1]);

    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (!variable_integer(v1)) {
        loge("array index should be an interger\n");
        return -1;
    }

    int nb_pointers = 0;

    if (v0->nb_dimentions > 0) {
        if (v0->dimentions[0].num < 0 && !v0->dimentions[0].vla) {
            loge("\n");
            return -1;
        }

        nb_pointers = v0->nb_pointers;

        if (variable_const(v1)) {
            if (v1->data.i < 0) {
                loge("array index '%s' < 0, real: %d, file: %s, line: %d\n",
                     v1->w->text->data, v1->data.i, v1->w->file->data, v1->w->line);
                return -1;
            }

            if (v1->data.i >= v0->dimentions[0].num && !v0->dimentions[0].vla) {
                if (!v0->member_flag) {
                    loge("array index '%s' >= size %d, real: %d, file: %s, line: %d\n",
                         v1->w->text->data, v0->dimentions[0].num, v1->data.i, v1->w->file->data, v1->w->line);
                    return -1;
                }

                logw("array index '%s' >= size %d, real: %d, confirm it for a zero-array end of a struct? file: %s, line: %d\n",
                     v1->w->text->data, v0->dimentions[0].num, v1->data.i, v1->w->file->data, v1->w->line);
            }
        }
    } else if (0 == v0->nb_dimentions && v0->nb_pointers > 0) {
        nb_pointers = v0->nb_pointers - 1;
    } else {
        loge("index out, v0: %s, v0->nb_dimentions: %d, v0->nb_pointers: %d, v0->arg_flag: %d\n",
             v0->w->text->data, v0->nb_dimentions, v0->nb_pointers, v0->arg_flag);
        return -1;
    }

    type_t *t = NULL;
    ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 0, nb_pointers, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    r->member_flag = v0->member_flag;

    int i;
    for (i = 1; i < v0->nb_dimentions; i++) {
        expr_t *vla = NULL;

        if (v0->dimentions[i].vla) {
            vla = expr_clone(v0->dimentions[i].vla);
            if (!vla) {
                variable_free(r);
                return -ENOMEM;
            }
        }

        variable_add_array_dimention(r, v0->dimentions[i].num, vla);
    }

    *d->pret = r;
    return 0;
}

static int _op_semantic_block(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (0 == nb_nodes)
        return 0;

    handler_data_t *d = data;
    block_t *up = ast->current_block;

    variable_t **pret;
    node_t *node;

    int ret;
    int i;

    ast->current_block = (block_t *)(nodes[0]->parent);

    for (i = 0; i < nb_nodes; i++) {
        node = nodes[i];

        if (type_is_var(node->type))
            continue;

        if (FUNCTION == node->type) {
            pret = d->pret;
            ret = __op_semantic_call(ast, (function_t *)node, data);
            d->pret = pret;
        } else
            ret = _op_semantic_node(ast, node, d);

        if (ret < 0) {
            ast->current_block = up;
            return -1;
        }
    }

    ast->current_block = up;
    return 0;
}

static int _op_semantic_return(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    function_t *f = (function_t *)ast->current_block;

    while (f && FUNCTION != f->node.type)
        f = (function_t *)f->node.parent;

    if (!f) {
        loge("\n");
        return -1;
    }

    if (nb_nodes > f->rets->size) {
        loge("\n");
        return -1;
    }

    int i;
    for (i = 0; i < nb_nodes; i++) {
        assert(nodes);

        variable_t *fret = f->rets->data[i];
        variable_t *r = NULL;
        expr_t *e = nodes[i];

        if (VAR_VOID == fret->type && 0 == fret->nb_pointers) {
            loge("void function needs no return value, file: %s, line: %d\n", e->parent->w->file->data, e->parent->w->line);
            return -1;
        }

        if (_expr_calculate(ast, e, &r) < 0) {
            loge("\n");
            return -1;
        }

        int same = variable_same_type(r, fret);

        variable_free(r);
        r = NULL;

        if (!same) {
            int ret = _semantic_add_type_cast(ast, &(e->nodes[0]), fret, e->nodes[0]);
            if (ret < 0) {
                loge("\n");
                return ret;
            }
        }
    }

    return 0;
}

static int _op_semantic_break(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    node_t *n = (node_t *)ast->current_block;

    while (n
           && OP_WHILE != n->type
           && OP_SWITCH != n->type
           && OP_DO != n->type
           && OP_FOR != n->type)
        n = n->parent;

    if (!n) {
        loge("\n");
        return -1;
    }

    if (!n->parent) {
        loge("\n");
        return -1;
    }

    return 0;
}

static int _op_semantic_continue(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    node_t *n = (node_t *)ast->current_block;

    while (n && (OP_WHILE != n->type && OP_FOR != n->type)) {
        n = n->parent;
    }

    if (!n) {
        loge("\n");
        return -1;
    }
    assert(OP_WHILE == n->type || OP_FOR == n->type);
    return 0;
}

static int _op_semantic_label(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_semantic_goto(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    node_t *nl = nodes[0];
    assert(LABEL == nl->type);

    label_t *l = nl->label;
    assert(l->w);

    label_t *l2 = block_find_label(ast->current_block, l->w->text->data);
    if (!l2) {
        loge("label '%s' not found\n", l->w->text->data);
        return -1;
    }

    return 0;
}

static int _op_semantic_if(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (nb_nodes < 2) {
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];

    assert(OP_EXPR == e->type);

    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }

    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }
    variable_free(r);
    r = NULL;

    int i;
    for (i = 1; i < nb_nodes; i++) {
        int ret = _op_semantic_node(ast, nodes[i], d);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

static int _op_semantic_do(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    node_t *node = nodes[0];
    expr_t *e = nodes[1];

    assert(OP_EXPR == e->type);

    int ret = _op_semantic_node(ast, node, d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }

    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }

    variable_free(r);
    r = NULL;

    return 0;
}

static int _op_semantic_while(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes || 1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *r = NULL;
    expr_t *e = nodes[0];

    assert(OP_EXPR == e->type);

    if (_expr_calculate(ast, e, &r) < 0) {
        loge("\n");
        return -1;
    }

    if (!r || !variable_integer(r)) {
        loge("\n");
        return -1;
    }
    variable_free(r);
    r = NULL;

    // while body
    if (2 == nb_nodes) {
        int ret = _op_semantic_node(ast, nodes[1], d);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

static int __switch_for_string(ast_t *ast, node_t *parent, node_t *child, expr_t *e, expr_t *e1, handler_data_t *d) {
    function_t *f = NULL;
    variable_t *v = NULL;
    expr_t *e2;
    expr_t *e3;
    expr_t *e4;

    int ret = ast_find_function(&f, ast, "strcmp");
    if (ret < 0)
        return ret;

    if (!f) {
        loge("can't find function 'strcmp()' for compare const string, file: %s, line: %d\n",
             parent->w->file->data, parent->w->line);
        return -1;
    }

    e2 = expr_clone(e);
    if (!e1)
        return -ENOMEM;

    if (_expr_calculate(ast, e2, &v) < 0) {
        expr_free(e2);
        return -1;
    }
    variable_free(v);
    v = NULL;

    e3 = expr_alloc();
    if (!e3) {
        expr_free(e2);
        return -ENOMEM;
    }

    ret = node_add_child(e3, e2);
    if (ret < 0) {
        expr_free(e2);
        expr_free(e3);
        return ret;
    }
    e2 = NULL;

    ret = node_add_child(e3, e1);
    if (ret < 0) {
        expr_free(e3);
        return ret;
    }
    child->nodes[0] = NULL;

    e4 = expr_alloc();
    if (!e4) {
        expr_free(e3);
        return -ENOMEM;
    }

    ret = node_add_child(e4, e3);
    if (ret < 0) {
        expr_free(e3);
        expr_free(e4);
        return ret;
    }

    child->nodes[0] = e4;
    e4->parent = child;

    d->pret = &e3->result;

    return _semantic_add_call(ast, e3->nodes, e3->nb_nodes, d, f);
}

static int _op_semantic_switch(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;
    variable_t **pret = d->pret;
    variable_t *v0 = NULL;
    variable_t *v1 = NULL;
    block_t *tmp = ast->current_block;
    expr_t *e = nodes[0];
    node_t *b = nodes[1];
    node_t *parent = nodes[0]->parent;
    node_t *child;
    expr_t *e1;

    assert(OP_EXPR == e->type);
    assert(OP_BLOCK == b->type);

    if (_expr_calculate(ast, e, &v0) < 0)
        return -1;

    if (!variable_integer(v0) && !variable_string(v0)) {
        loge("result of switch expr should be an integer or string, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
        variable_free(v0);
        return -1;
    }

    ast->current_block = (block_t *)b;

    int ret = -1;
    int i;

    for (i = 0; i < b->nb_nodes; i++) {
        child = b->nodes[i];

        if (OP_CASE == child->type) {
            assert(1 == child->nb_nodes);

            e1 = child->nodes[0];

            assert(OP_EXPR == e1->type);

            ret = _expr_calculate(ast, e1, &v1);
            if (ret < 0) {
                variable_free(v0);
                return ret;
            }

            if (!variable_const_integer(v1) && !variable_const_string(v1)) {
                ret = -1;
                loge("result of case expr should be const integer or const string, file: %s, line: %d\n", child->w->file->data, child->w->line);
                goto error;
            }

            if (!variable_type_like(v0, v1)) {
                if (type_cast_check(ast, v0, v1) < 0) {
                    ret = -1;
                    loge("type of switch's expr is NOT same to the case's, file: %s, line: %d\n", child->w->file->data, child->w->line);
                    goto error;
                }

                ret = _semantic_add_type_cast(ast, &(e1->nodes[0]), v0, e1->nodes[0]);
                if (ret < 0)
                    goto error;
            }

            if (variable_const_string(v1)) {
                ret = __switch_for_string(ast, parent, child, e, e1, d);
                if (ret < 0)
                    goto error;
            }

            variable_free(v1);
            v1 = NULL;

        } else {
            ret = _op_semantic_node(ast, child, d);
            if (ret < 0) {
                variable_free(v0);
                return -1;
            }
        }
    }

    ast->current_block = tmp;

    variable_free(v0);

    d->pret = pret;
    return 0;

error:
    variable_free(v0);
    variable_free(v1);
    d->pret = pret;
    return ret;
}

static int _op_semantic_case(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    loge("\n");
    return -1;
}

static int _op_semantic_default(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return 0;
}

static int _op_semantic_vla_alloc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes);

    logw("\n");
    return 0;
}

static int _op_semantic_for(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(4 == nb_nodes);

    handler_data_t *d = data;
    int ret = 0;

    if (nodes[0]) {
        ret = _op_semantic_node(ast, nodes[0], d);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    expr_t *e = nodes[1];
    if (e) {
        assert(OP_EXPR == e->type);

        variable_t *r = NULL;

        if (_expr_calculate(ast, e, &r) < 0) {
            loge("\n");
            return -1;
        }

        if (!r || !variable_integer(r)) {
            loge("\n");
            return -1;
        }
        variable_free(r);
        r = NULL;
    }

    int i;
    for (i = 2; i < nb_nodes; i++) {
        if (!nodes[i])
            continue;

        ret = _op_semantic_node(ast, nodes[i], d);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    return 0;
}

static int __op_semantic_call(ast_t *ast, function_t *f, void *data) {
    logd("f: %p, f->node->w: %s\n", f, f->node.w->text->data);

    handler_data_t *d = data;
    block_t *tmp = ast->current_block;

    // change the current block
    ast->current_block = (block_t *)f;

    if (_op_semantic_block(ast, f->node.nodes, f->node.nb_nodes, d) < 0)
        return -1;

    ast->current_block = tmp;
    return 0;
}

static int _op_semantic_call(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(nb_nodes > 0);

    handler_data_t *d = data;
    variable_t **pret = d->pret;
    variable_t *v0;
    variable_t *v1;
    function_t *f;
    node_t *parent = nodes[0]->parent;

    d->pret = &nodes[0]->result;
    int ret = _expr_calculate_internal(ast, nodes[0], d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    v0 = _operand_get(nodes[0]);

    if (FUNCTION_PTR != v0->type || !v0->func_ptr) {
        loge("\n");
        return -1;
    }

    f = v0->func_ptr;

    if (f->vargs_flag) {
        if (f->argv->size > nb_nodes - 1) {
            loge("number of args pass to '%s()' at least needs %d, real: %d, file: %s, line: %d\n",
                 f->node.w->text->data, f->argv->size, nb_nodes - 1, parent->w->file->data, parent->w->line);
            return -1;
        }
    } else if (f->argv->size != nb_nodes - 1) {
        loge("number of args pass to '%s()' needs %d, real: %d, file: %s, line: %d\n",
             f->node.w->text->data, f->argv->size, nb_nodes - 1, parent->w->file->data, parent->w->line);
        return -1;
    }

    int i;
    for (i = 0; i < f->argv->size; i++) {
        v0 = f->argv->data[i];

        v1 = _operand_get(nodes[i + 1]);

        if (VAR_VOID == v1->type && 0 == v1->nb_pointers) {
            loge("void var should be a pointer\n");
            return -1;
        }

        if (variable_type_like(v0, v1))
            continue;

        if (type_cast_check(ast, v0, v1) < 0) {
            loge("f: %s, arg var not same type, i: %d, line: %d\n",
                 f->node.w->text->data, i, parent->debug_w->line);
            return -1;
        }

        ret = _semantic_add_type_cast(ast, &nodes[i + 1], v0, nodes[i + 1]);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    return _semantic_add_call_rets(ast, parent, d, f);
}

static int _op_semantic_expr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    node_t *n = nodes[0];

    if (n->result) {
        variable_free(n->result);
        n->result = 0;
    }

    variable_t **pret = d->pret;

    d->pret = &n->result;
    int ret = _expr_calculate_internal(ast, n, d);
    d->pret = pret;

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    if (type_is_var(n->type)) {
        assert(n->var);
        if (d->pret)
            *d->pret = variable_ref(n->var);

    } else {
        if (n->result && d->pret)
            *d->pret = variable_ref(n->result);
    }

    return 0;
}

static int _op_semantic_neg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) || variable_float(v0)) {
        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;

        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

static int _op_semantic_inc(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);

    if (variable_const(v0) || variable_const_string(v0)) {
        loge("line: %d\n", parent->w->line);
        return -1;
    }

    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0)) {
        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;

        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

static int _op_semantic_inc_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

static int _op_semantic_dec(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

static int _op_semantic_dec_post(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_inc(ast, nodes, nb_nodes, data);
}

static int _op_semantic_positive(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    node_t *child = nodes[0];
    node_t *parent = nodes[0]->parent;

    nodes[0] = NULL;

    node_free_data(parent);
    node_move_data(parent, child);
    node_free(child);

    return 0;
}

static int _op_semantic_dereference(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (v0->nb_pointers <= 0) {
        loge("var is not a pointer\n");
        return -EINVAL;
    }

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 0, v0->nb_pointers - 1, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    *d->pret = r;
    return 0;
}

static int _op_semantic_address_of(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (v0->const_literal_flag) {
        loge("\n");
        return -EINVAL;
    }

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers + 1, v0->func_ptr);
    if (!r)
        return -ENOMEM;

    *d->pret = r;
    return 0;
}

static int _op_semantic_type_cast(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *dst = _operand_get(nodes[0]);
    variable_t *src = _operand_get(nodes[1]);

    assert(dst);
    assert(src);

    if (d->pret) {
        *d->pret = variable_ref(dst);
    }

    return 0;
}

static int _op_semantic_container(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(3 == nb_nodes);

    handler_data_t *d = data;

    node_t *e = nodes[0];
    node_t *node1 = nodes[1];
    node_t *node2 = nodes[2];
    node_t *parent = e->parent;

    variable_t *v0 = _operand_get(e);
    variable_t *v1 = _operand_get(node1);
    variable_t *v2 = _operand_get(node2);
    variable_t *u8;
    variable_t *offset;
    type_t *t;

    assert(v0);
    assert(OP_EXPR == e->type);
    assert(d->pret == &parent->result);

    assert(v0->nb_pointers > 0);
    assert(v1->nb_pointers > 0);
    assert(v1->type >= STRUCT);
    assert(v2->member_flag);

    parent->type = OP_TYPE_CAST;
    parent->result = variable_ref(v1);
    parent->nodes[0] = node1;
    parent->nodes[1] = e;
    parent->nodes[2] = NULL;
    parent->nb_nodes = 2;
    parent->op = find_base_operator_by_type(OP_TYPE_CAST);

    if (0 == v2->offset) {
        node_free(node2);
        return 0;
    }

    t = block_find_type_type(ast->current_block, VAR_U8);
    u8 = VAR_ALLOC_BY_TYPE(NULL, t, 0, 1, NULL);

    int ret = _semantic_add_type_cast(ast, &e->nodes[0], u8, e->nodes[0]);
    if (ret < 0)
        return ret;

    t = block_find_type_type(ast->current_block, VAR_UINTPTR);
    offset = VAR_ALLOC_BY_TYPE(v2->w, t, 1, 0, NULL);

    offset->data.u64 = v2->offset;

    assert(!node2->result);

    variable_free(node2->var);
    variable_free(e->result);

    node2->type = offset->type;
    node2->var = offset;

    node_add_child(e, node2);

    e->type = OP_SUB;
    e->result = u8;
    e->op = find_base_operator_by_type(OP_SUB);

    return 0;
}

static int _op_semantic_sizeof(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(OP_EXPR == nodes[0]->type);
    assert(d->pret == &parent->result);

    int size = variable_size(v0);
    if (size < 0)
        return size;

    type_t *t = block_find_type_type(ast->current_block, VAR_INTPTR);

    lex_word_t *w = parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, 1, 0, NULL);
    if (!r)
        return -ENOMEM;

    r->data.i64 = size;
    r->const_flag = 1;

    XCHG(r->w, parent->w);

    node_free_data(parent);
    parent->type = r->type;
    parent->var = r;

    return 0;
}

static int _op_semantic_logic_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0)) {
        int const_flag = v0->const_flag && 0 == v0->nb_pointers && 0 == v0->nb_dimentions;

        type_t *t = block_find_type_type(ast->current_block, VAR_INT);

        lex_word_t *w = nodes[0]->parent->w;
        variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, 0, NULL);
        if (!r)
            return -ENOMEM;

        *d->pret = r;
        return 0;
    }

    loge("v0: %d/%s\n", v0->w->line, v0->w->text->data);
    return -1;
}

static int _op_semantic_bit_not(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(1 == nb_nodes);

    handler_data_t *d = data;
    variable_t *v0 = _operand_get(nodes[0]);

    assert(v0);

    if (variable_is_struct_pointer(v0)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    type_t *t;

    if (v0->nb_pointers + v0->nb_dimentions > 0) {
        t = block_find_type_type(ast->current_block, VAR_UINTPTR);

    } else if (type_is_integer(v0->type)) {
        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
    } else {
        loge("\n");
        return -1;
    }

    lex_word_t *w = nodes[0]->parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, 0, NULL);
    if (!r)
        return -ENOMEM;

    *d->pret = r;
    return 0;
}

static int _semantic_pointer_add(ast_t *ast, node_t *parent, node_t *pointer, node_t *index) {
    variable_t *r;
    variable_t *v = _operand_get(pointer);
    type_t *t = NULL;
    node_t *add;
    node_t *neg;

    int ret = ast_find_type_type(&t, ast, v->type);
    if (ret < 0)
        return ret;

    add = node_alloc(parent->w, OP_ARRAY_INDEX, NULL);
    if (!add)
        return -ENOMEM;

    int nb_pointers = variable_nb_pointers(v);

    r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, nb_pointers - 1, v->func_ptr);
    if (!r) {
        node_free(add);
        return -ENOMEM;
    }
    r->local_flag = 1;
    r->tmp_flag = 1;

    add->result = r;
    r = NULL;

    ret = node_add_child(add, pointer);
    if (ret < 0) {
        node_free(add);
        return ret;
    }

    ret = node_add_child(add, index);
    if (ret < 0) {
        pointer->parent = parent;

        add->nb_nodes = 0;
        node_free(add);
        return ret;
    }

    add->parent = parent;

    parent->nodes[0] = add;
    parent->nodes[1] = NULL;
    parent->nb_nodes = 1;

    if (OP_SUB == parent->type) {
        neg = node_alloc(parent->w, OP_NEG, NULL);
        if (!neg) {
            ret = -ENOMEM;
            goto error;
        }

        v = _operand_get(index);

        ret = ast_find_type_type(&t, ast, v->type);
        if (ret < 0)
            goto error;

        r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, 0, NULL);
        if (!r) {
            node_free(neg);
            goto error;
        }
        r->local_flag = 1;
        r->tmp_flag = 1;

        neg->result = r;
        r = NULL;

        ret = node_add_child(neg, index);
        if (ret < 0) {
            node_free(neg);
            goto error;
        }

        add->nodes[1] = neg;
        neg->parent = add;
    }

    ret = 0;
error:
    parent->op = find_base_operator_by_type(OP_ADDRESS_OF);
    parent->type = OP_ADDRESS_OF;
    return ret;
}

static int _semantic_pointer_add_assign(ast_t *ast, node_t *parent, node_t *pointer, node_t *index) {
    variable_t *v = _operand_get(pointer);
    variable_t *r = NULL;
    type_t *t = NULL;
    node_t *p2;
    node_t *add;

    int ret = ast_find_type_type(&t, ast, v->type);
    if (ret < 0)
        return ret;

    if (OP_ADD_ASSIGN == parent->type)
        add = node_alloc(parent->w, OP_ADD, NULL);
    else
        add = node_alloc(parent->w, OP_SUB, NULL);
    if (!add)
        return -ENOMEM;

    r = VAR_ALLOC_BY_TYPE(parent->w, t, v->const_flag, variable_nb_pointers(v), v->func_ptr);
    if (!r) {
        node_free(add);
        return -ENOMEM;
    }
    r->local_flag = 1;
    r->tmp_flag = 1;

    add->result = r;
    r = NULL;

    p2 = expr_clone(pointer);
    if (!p2) {
        node_free(add);
        return -ENOMEM;
    }

    if (type_is_var(pointer->type))
        p2->var = variable_ref(pointer->var);

    else if (type_is_operator(pointer->type))
        p2->result = variable_ref(pointer->result);

    ret = node_add_child(add, p2);
    if (ret < 0) {
        node_free(p2);
        node_free(add);
        return ret;
    }

    ret = node_add_child(add, index);
    if (ret < 0) {
        node_free(add);
        return ret;
    }

    parent->nodes[1] = NULL;

    ret = _semantic_pointer_add(ast, add, p2, index);
    if (ret < 0) {
        node_free(add);
        return ret;
    }

    parent->nodes[1] = add;

    add->parent = parent;
    parent->op = find_base_operator_by_type(OP_ASSIGN);
    parent->type = OP_ASSIGN;
    return 0;
}

static int _op_semantic_binary_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    variable_t *v2 = NULL;
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    if (v0->const_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;
    }

    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            type_t *t = NULL;

            int nb_pointers0 = variable_nb_pointers(v0);
            int nb_pointers1 = variable_nb_pointers(v1);

            if (nb_pointers0 > 0) {
                if (nb_pointers1 > 0) {
                    if (!variable_same_type(v0, v1)) {
                        loge("different type pointer, type: %d,%d, nb_pointers: %d,%d\n",
                             v0->type, v1->type, nb_pointers0, nb_pointers1);
                        return -EINVAL;
                    }

                } else if (!variable_integer(v1)) {
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;
                } else {
                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v1->w, t, v1->const_flag, 0, NULL);

                    int ret = _semantic_add_type_cast(ast, &nodes[1], v2, nodes[1]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }

                    if (OP_ADD_ASSIGN == parent->type || OP_SUB_ASSIGN == parent->type) {
                        ret = _semantic_pointer_add_assign(ast, parent, nodes[0], nodes[1]);
                        if (ret < 0)
                            return ret;
                    }
                }
            } else if (nb_pointers1 > 0) {
                loge("assign a pointer to an integer NOT with a type cast, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                return -1;

            } else if (v0->type != v1->type) {
                if (type_cast_check(ast, v0, v1) < 0) {
                    loge("type cast failed, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                    return -1;
                }

                int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
                if (ret < 0) {
                    loge("add type cast failed\n");
                    return ret;
                }
            }

            int ret = ast_find_type_type(&t, ast, v0->type);
            if (ret < 0)
                return ret;
            assert(t);

            lex_word_t *w = nodes[0]->parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
            if (!r)
                return -ENOMEM;

            *d->pret = r;
            return 0;
        }
    }

    loge("type %d, %d not support\n", v0->type, v1->type);
    return -1;
}

static int _op_semantic_binary(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;

    assert(v0);
    assert(v1);

    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            function_t *func_ptr = NULL;
            variable_t *v2 = NULL;
            type_t *t = NULL;

            int const_flag = 0;
            int nb_pointers = 0;
            int nb_pointers0 = variable_nb_pointers(v0);
            int nb_pointers1 = variable_nb_pointers(v1);
            int add_flag = 0;

            if (nb_pointers0 > 0) {
                if (nb_pointers1 > 0) {
                    if (!variable_same_type(v0, v1)) {
                        loge("different type pointer, type: %d,%d, nb_pointers: %d,%d\n",
                             v0->type, v1->type, nb_pointers0, nb_pointers1);
                        return -EINVAL;
                    }

                } else if (!variable_integer(v1)) {
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;
                } else {
                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v1->w, t, v1->const_flag, 0, NULL);

                    int ret = _semantic_add_type_cast(ast, &nodes[1], v2, nodes[1]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }

                    if (OP_ADD == parent->type || OP_SUB == parent->type) {
                        ret = _semantic_pointer_add(ast, parent, nodes[0], nodes[1]);
                        if (ret < 0)
                            return ret;
                        add_flag = 1;
                    }
                }

                t = NULL;
                int ret = ast_find_type_type(&t, ast, v0->type);
                if (ret < 0)
                    return ret;

                const_flag = v0->const_flag;
                nb_pointers = nb_pointers0;
                func_ptr = v0->func_ptr;

            } else if (nb_pointers1 > 0) {
                if (!variable_integer(v0)) {
                    loge("var calculated with a pointer should be a interger\n");
                    return -EINVAL;

                } else {
                    if (OP_SUB == parent->type) {
                        loge("only a pointer sub an integer, NOT reverse, file: %s, line: %d\n", parent->w->file->data, parent->w->line);
                        return -1;
                    }

                    t = block_find_type_type(ast->current_block, VAR_INTPTR);

                    v2 = VAR_ALLOC_BY_TYPE(v0->w, t, v0->const_flag, 0, NULL);

                    int ret = _semantic_add_type_cast(ast, &nodes[0], v2, nodes[0]);

                    variable_free(v2);
                    v2 = NULL;
                    if (ret < 0) {
                        loge("add type cast failed\n");
                        return ret;
                    }

                    if (OP_ADD == parent->type) {
                        ret = _semantic_pointer_add(ast, parent, nodes[1], nodes[0]);
                        if (ret < 0)
                            return ret;

                        add_flag = 1;
                    }
                }

                t = NULL;
                int ret = ast_find_type_type(&t, ast, v1->type);
                if (ret < 0)
                    return ret;

                const_flag = v1->const_flag;
                nb_pointers = nb_pointers1;
                func_ptr = v1->func_ptr;

            } else if (v0->type == v1->type) { // from here v0 & v1 are normal var
                t = NULL;
                int ret = ast_find_type_type(&t, ast, v0->type);
                if (ret < 0)
                    return ret;

                const_flag = v0->const_flag && v1->const_flag;
                nb_pointers = 0;
                func_ptr = NULL;

            } else {
                int ret = find_updated_type(ast, v0, v1);
                if (ret < 0) {
                    loge("var type update failed, type: %d, %d\n", v0->type, v1->type);
                    return -EINVAL;
                }

                t = NULL;
                ret = ast_find_type_type(&t, ast, ret);
                if (ret < 0)
                    return ret;

                if (t->type != v0->type)
                    ret = _semantic_add_type_cast(ast, &nodes[0], v1, nodes[0]);
                else
                    ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);

                if (ret < 0) {
                    loge("add type cast failed\n");
                    return ret;
                }

                const_flag = v0->const_flag && v1->const_flag;
                nb_pointers = 0;
                func_ptr = NULL;
            }

            lex_word_t *w = parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, nb_pointers, func_ptr);
            if (!r)
                return -ENOMEM;

            r->tmp_flag = add_flag;

            *d->pret = r;
            return 0;
        }
    }

    loge("type %d, %d not support\n", v0->type, v1->type);
    return -1;
}

static int _op_semantic_add(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

static int _op_semantic_sub(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

static int _op_semantic_mul(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

static int _op_semantic_div(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary(ast, nodes, nb_nodes, data);
}

static int _op_semantic_binary_interger(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;
    lex_word_t *w = parent->w;

    type_t *t;
    variable_t *r;

    assert(v0);
    assert(v1);

    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) && variable_integer(v1)) {
        int const_flag = v0->const_flag && v1->const_flag;

        if (!variable_same_type(v0, v1)) {
            int ret = _semantic_do_type_cast(ast, nodes, nb_nodes, data);
            if (ret < 0) {
                loge("semantic do type cast failed, line: %d\n", parent->w->line);
                return ret;
            }
        }

        v0 = _operand_get(nodes[0]);

        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;

        r = VAR_ALLOC_BY_TYPE(w, t, const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r) {
            loge("var alloc failed\n");
            return -ENOMEM;
        }

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

static int _op_semantic_mod(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shl(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shr(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_bit_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_bit_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _semantic_multi_rets_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    node_t *parent = nodes[0]->parent;
    node_t *gp = parent->parent;

    assert(OP_BLOCK == nodes[0]->type);

    while (OP_EXPR == gp->type)
        gp = gp->parent;
#if 1
    if (gp->type != OP_BLOCK && gp->type != FUNCTION) {
        loge("a call to multi-return-values function MUST be in block, line: %d, gp->type: %d, b: %d, f: %d\n",
             parent->w->line, gp->type, OP_BLOCK, FUNCTION);
        return -1;
    }
#endif

    node_t *rets = nodes[0];
    node_t *call = nodes[1];
    node_t *ret;

    while (call) {
        if (OP_EXPR == call->type)
            call = call->nodes[0];
        else
            break;
    }

    if (OP_CALL != call->type && OP_CREATE != call->type) {
        loge("\n");
        return -1;
    }

    assert(call->nb_nodes > 0);

    logd("rets->nb_nodes: %d, call->result_nodes: %p\n", rets->nb_nodes, call->result_nodes);
    logd("rets->nb_nodes: %d, call->result_nodes->size: %d\n", rets->nb_nodes, call->result_nodes->size);

    assert(rets->nb_nodes <= call->result_nodes->size);

    int i;
    for (i = 0; i < rets->nb_nodes; i++) {
        variable_t *v0 = _operand_get(rets->nodes[i]);
        variable_t *v1 = _operand_get(call->result_nodes->data[i]);

        if (!variable_same_type(v0, v1)) {
            loge("\n");
            return -1;
        }

        if (v0->const_flag) {
            loge("\n");
            return -1;
        }

        node_t *assign = node_alloc(parent->w, OP_ASSIGN, NULL);
        if (!assign)
            return -ENOMEM;

        node_add_child(assign, rets->nodes[i]);
        node_add_child(assign, call->result_nodes->data[i]);

        rets->nodes[i] = assign;
    }

    node_add_child(rets, nodes[1]);

    for (i = rets->nb_nodes - 2; i >= 0; i--)
        rets->nodes[i + 1] = rets->nodes[i];
    rets->nodes[0] = nodes[1];

    parent->type = OP_EXPR;
    parent->nb_nodes = 1;
    parent->nodes[0] = rets;

    return 0;
}

static int _op_semantic_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    node_t *parent = nodes[0]->parent;
    node_t *call = nodes[1];

    while (call) {
        if (OP_EXPR == call->type)
            call = call->nodes[0];
        else
            break;
    }

    if (OP_CALL == call->type
        && call->result_nodes->size > 1) {
        if (OP_BLOCK != nodes[0]->type) {
            block_t *b = block_alloc_cstr("multi_rets");
            if (!b)
                return -ENOMEM;

            int ret = node_add_child((node_t *)b, nodes[0]);
            if (ret < 0) {
                block_free(b);
                return ret;
            }
            parent->nodes[0] = (node_t *)b;
            b->node.parent = parent;
        }
    }

    if (OP_BLOCK == nodes[0]->type) {
        return _semantic_multi_rets_assign(ast, nodes, nb_nodes, data);
    }

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    assert(v0);
    assert(v1);

    if (VAR_VOID == v0->type && 0 == v0->nb_pointers) {
        loge("var '%s' can't be 'void' type\n", v0->w->text->data);
        return -1;
    }

    if (VAR_VOID == v1->type && 0 == v1->nb_pointers) {
        loge("var '%s' can't be 'void' type\n", v1->w->text->data);
        return -1;
    }

    if (v0->const_literal_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;

    } else if (v0->const_flag) {
        logw("const var '%s' can't be assigned\n", v0->w->text->data);
    }

    if (variable_is_struct(v0) || variable_is_struct(v1)) {
        int size = variable_size(v0);

        int ret = _semantic_do_overloaded_assign(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error, ret: %d\n", ret);
            return -1;
        }

        if (variable_same_type(v0, v1)) {
            function_t *f = NULL;
            ret = ast_find_function(&f, ast, "  _memcpy");
            if (ret < 0)
                return ret;

            if (!f) {
                loge("semantic do overloaded error: default '  _memcpy' NOT found\n");
                return -1;
            }

            type_t *t = block_find_type_type(ast->current_block, VAR_INTPTR);
            variable_t *v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 0, NULL);
            if (!v) {
                loge("var alloc failed\n");
                return -ENOMEM;
            }
            v->data.i64 = size;

            node_t *node_size = node_alloc(NULL, v->type, v);
            if (!node_size) {
                loge("node alloc failed\n");
                return -ENOMEM;
            }

            node_add_child(parent, node_size);

            return _semantic_add_call(ast, parent->nodes, parent->nb_nodes, d, f);
        }

        loge("semantic do overloaded error\n");
        return -1;
    }

    if (!variable_same_type(v0, v1)) {
        if (variable_is_struct_pointer(v0) && v1->w && strcmp(v1->w->text->data, "NULL")) {
            type_t *t = NULL;
            int ret = ast_find_type_type(&t, ast, v0->type);
            if (ret < 0)
                return ret;
            assert(t);

            if (scope_find_function(t->scope, "__init")) {
                int ret = _semantic_do_create(ast, nodes, nb_nodes, d);
                if (0 == ret)
                    return 0;

                if (-404 != ret) {
                    loge("semantic do overloaded error, ret: %d\n", ret);
                    return -1;
                }
            }
        }

        logd("v0: v_%d_%d/%s\n", v0->w->line, v0->w->pos, v0->w->text->data);

        if (type_cast_check(ast, v0, v1) < 0) {
            loge("\n");
            return -1;
        }

        int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
        if (ret < 0) {
            loge("add type cast failed\n");
            return ret;
        }
    }

    type_t *t = NULL;
    int ret = ast_find_type_type(&t, ast, v0->type);
    if (ret < 0)
        return ret;

    lex_word_t *w = parent->w;
    variable_t *r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
    if (!r) {
        loge("var alloc failed\n");
        return -1;
    }

    *d->pret = r;
    return 0;
}

static int _op_semantic_binary_interger_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);
    node_t *parent = nodes[0]->parent;
    lex_word_t *w = parent->w;

    type_t *t;
    variable_t *r;

    assert(v0);
    assert(v1);

    if (v0->const_flag || v0->nb_dimentions > 0) {
        loge("const var '%s' can't be assigned\n", v0->w->text->data);
        return -1;
    }

    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) && variable_integer(v1)) {
        if (!variable_same_type(v0, v1)) {
            if (type_cast_check(ast, v0, v1) < 0) {
                loge("\n");
                return -1;
            }

            int ret = _semantic_add_type_cast(ast, &nodes[1], v0, nodes[1]);
            if (ret < 0) {
                loge("add type cast failed\n");
                return ret;
            }
        }

        t = NULL;
        int ret = ast_find_type_type(&t, ast, v0->type);
        if (ret < 0)
            return ret;
        assert(t);

        r = VAR_ALLOC_BY_TYPE(w, t, v0->const_flag, v0->nb_pointers, v0->func_ptr);
        if (!r) {
            loge("var alloc failed\n");
            return -ENOMEM;
        }

        *d->pret = r;
        return 0;
    }

    loge("\n");
    return -1;
}

static int _op_semantic_add_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_sub_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_mul_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_div_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_mod_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shl_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_shr_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_and_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_or_assign(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger_assign(ast, nodes, nb_nodes, data);
}

static int _op_semantic_cmp(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    assert(2 == nb_nodes);

    handler_data_t *d = data;

    variable_t *v0 = _operand_get(nodes[0]);
    variable_t *v1 = _operand_get(nodes[1]);

    assert(v0);
    assert(v1);

    if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1)) {
        int ret = _semantic_do_overloaded(ast, nodes, nb_nodes, d);
        if (0 == ret)
            return 0;

        if (-404 != ret) {
            loge("semantic do overloaded error\n");
            return -1;
        }
    }

    if (variable_integer(v0) || variable_float(v0)) {
        if (variable_integer(v1) || variable_float(v1)) {
            int const_flag = v0->const_flag && v1->const_flag;

            if (!variable_same_type(v0, v1)) {
                int ret = _semantic_do_type_cast(ast, nodes, nb_nodes, data);
                if (ret < 0) {
                    loge("semantic do type cast failed\n");
                    return ret;
                }
            }

            v0 = _operand_get(nodes[0]);

            type_t *t = block_find_type_type(ast->current_block, VAR_INT);

            lex_word_t *w = nodes[0]->parent->w;
            variable_t *r = VAR_ALLOC_BY_TYPE(w, t, const_flag, 0, NULL);
            if (!r) {
                loge("var alloc failed\n");
                return -ENOMEM;
            }

            *d->pret = r;
            return 0;
        }
    }

    loge("\n");
    return -1;
}

static int _op_semantic_eq(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
#define CMPEQ_CHECK_FLOAT()                             \
    do {                                                \
        assert(2 == nb_nodes);                          \
        variable_t *v0 = _operand_get(nodes[0]);        \
        variable_t *v1 = _operand_get(nodes[1]);        \
                                                        \
        if (variable_float(v0) || variable_float(v1)) { \
            loge("float type can't cmp equal\n");       \
            return -EINVAL;                             \
        }                                               \
    } while (0)

    CMPEQ_CHECK_FLOAT();

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_ne(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT();

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_gt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_ge(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT();

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_lt(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_le(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    CMPEQ_CHECK_FLOAT();

    return _op_semantic_cmp(ast, nodes, nb_nodes, data);
}

static int _op_semantic_logic_and(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_logic_or(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    return _op_semantic_binary_interger(ast, nodes, nb_nodes, data);
}

static int _op_semantic_va_start(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (2 != nb_nodes) {
        loge("\n");
        return -1;
    }
    return 0;
}

static int _op_semantic_va_arg(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (2 != nb_nodes) {
        loge("\n");
        return -1;
    }

    handler_data_t *d = data;

    if (d->pret) {
        variable_t *v = _operand_get(nodes[1]);

        type_t *t = NULL;
        int ret = ast_find_type_type(&t, ast, v->type);
        if (ret < 0)
            return ret;
        assert(t);

        variable_t *r = VAR_ALLOC_BY_TYPE(nodes[0]->parent->w, t, 0, v->nb_pointers, v->func_ptr);

        if (!r)
            return -ENOMEM;

        *d->pret = r;
    }
    return 0;
}

static int _op_semantic_va_end(ast_t *ast, node_t **nodes, int nb_nodes, void *data) {
    if (1 != nb_nodes) {
        loge("\n");
        return -1;
    }
    return 0;
}

operator_handler_pt semantic_operator_handlers[N_OPS] =
    {
        [OP_EXPR] = _op_semantic_expr,
        [OP_CALL] = _op_semantic_call,

        [OP_ARRAY_INDEX] = _op_semantic_array_index,
        [OP_POINTER] = _op_semantic_pointer,
        [OP_CREATE] = _op_semantic_create,

        [OP_VA_START] = _op_semantic_va_start,
        [OP_VA_ARG] = _op_semantic_va_arg,
        [OP_VA_END] = _op_semantic_va_end,

        [OP_CONTAINER] = _op_semantic_container,

        [OP_SIZEOF] = _op_semantic_sizeof,
        [OP_TYPE_CAST] = _op_semantic_type_cast,
        [OP_LOGIC_NOT] = _op_semantic_logic_not,
        [OP_BIT_NOT] = _op_semantic_bit_not,
        [OP_NEG] = _op_semantic_neg,
        [OP_POSITIVE] = _op_semantic_positive,

        [OP_INC] = _op_semantic_inc,
        [OP_DEC] = _op_semantic_dec,

        [OP_INC_POST] = _op_semantic_inc_post,
        [OP_DEC_POST] = _op_semantic_dec_post,

        [OP_DEREFERENCE] = _op_semantic_dereference,
        [OP_ADDRESS_OF] = _op_semantic_address_of,

        [OP_MUL] = _op_semantic_mul,
        [OP_DIV] = _op_semantic_div,
        [OP_MOD] = _op_semantic_mod,

        [OP_ADD] = _op_semantic_add,
        [OP_SUB] = _op_semantic_sub,

        [OP_SHL] = _op_semantic_shl,
        [OP_SHR] = _op_semantic_shr,

        [OP_BIT_AND] = _op_semantic_bit_and,
        [OP_BIT_OR] = _op_semantic_bit_or,

        [OP_EQ] = _op_semantic_eq,
        [OP_NE] = _op_semantic_ne,
        [OP_GT] = _op_semantic_gt,
        [OP_LT] = _op_semantic_lt,
        [OP_GE] = _op_semantic_ge,
        [OP_LE] = _op_semantic_le,

        [OP_LOGIC_AND] = _op_semantic_logic_and,
        [OP_LOGIC_OR] = _op_semantic_logic_or,

        [OP_ASSIGN] = _op_semantic_assign,
        [OP_ADD_ASSIGN] = _op_semantic_add_assign,
        [OP_SUB_ASSIGN] = _op_semantic_sub_assign,
        [OP_MUL_ASSIGN] = _op_semantic_mul_assign,
        [OP_DIV_ASSIGN] = _op_semantic_div_assign,
        [OP_MOD_ASSIGN] = _op_semantic_mod_assign,
        [OP_SHL_ASSIGN] = _op_semantic_shl_assign,
        [OP_SHR_ASSIGN] = _op_semantic_shr_assign,
        [OP_AND_ASSIGN] = _op_semantic_and_assign,
        [OP_OR_ASSIGN] = _op_semantic_or_assign,

        [OP_BLOCK] = _op_semantic_block,
        [OP_RETURN] = _op_semantic_return,
        [OP_BREAK] = _op_semantic_break,
        [OP_CONTINUE] = _op_semantic_continue,
        [OP_GOTO] = _op_semantic_goto,
        [LABEL] = _op_semantic_label,

        [OP_IF] = _op_semantic_if,
        [OP_WHILE] = _op_semantic_while,
        [OP_DO] = _op_semantic_do,
        [OP_FOR] = _op_semantic_for,

        [OP_SWITCH] = _op_semantic_switch,
        [OP_CASE] = _op_semantic_case,
        [OP_DEFAULT] = _op_semantic_default,

        [OP_VLA_ALLOC] = _op_semantic_vla_alloc,
};

operator_handler_pt find_semantic_operator_handler(const int type) {
    if (type < 0 || type >= N_OPS)
        return NULL;

    return semantic_operator_handlers[type];
}

int function_semantic_analysis(ast_t *ast, function_t *f) {
    handler_data_t d = {0};

    int ret = __op_semantic_call(ast, f, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

int expr_semantic_analysis(ast_t *ast, expr_t *e) {
    handler_data_t d = {0};

    if (!e->nodes || e->nb_nodes != 1) {
        loge("\n");
        return -1;
    }

    int ret = _expr_calculate_internal(ast, e->nodes[0], &d);
    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}

int semantic_analysis(ast_t *ast) {
    handler_data_t d = {0};

    int ret = _expr_calculate_internal(ast, (node_t *)ast->root_block, &d);

    if (ret < 0) {
        loge("\n");
        return -1;
    }

    return 0;
}
