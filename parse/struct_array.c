#include "dfa.h"
#include "parse.h"

static int __reshape_index(dfa_index_t **out, variable_t *array, dfa_index_t *index, int n) {
    assert(array->nb_dimentions > 0);
    assert(n > 0);

    dfa_index_t *p = calloc(array->nb_dimentions, sizeof(dfa_index_t));
    if (!p)
        return -ENOMEM;

    intptr_t i = index[n - 1].i;

    logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n",
         n, array->nb_dimentions, i);

    int j;
    for (j = array->nb_dimentions - 1; j >= 0; j--) {
        if (array->dimentions[j].num <= 0) {
            logw("array's %d-dimention size not set, file: %s, line: %d\n", j, array->w->file->data, array->w->line);

            free(p);
            return -1;
        }

        p[j].i = i % array->dimentions[j].num;
        i = i / array->dimentions[j].num;
    }

    for (j = 0; j < array->nb_dimentions; j++)
        logi("\033[32m dim: %d, size: %d, index: %ld\033[0m\n", j, array->dimentions[j].num, p[j].i);

    *out = p;
    return 0;
}

static int __array_member_init(ast_t *ast, lex_word_t *w, variable_t *array, dfa_index_t *index, int n, node_t **pnode) {
    if (!pnode)
        return -1;

    type_t *t = block_find_type_type(ast->current_block, VAR_INT);
    node_t *root = *pnode;

    if (!root)
        root = node_alloc(NULL, array->type, array);

    if (n < array->nb_dimentions) {
        if (n <= 0) {
            loge("number of indexes less than needed, array '%s', file: %s, line: %d\n",
                 array->w->text->data, w->file->data, w->line);
            return -1;
        }

        int ret = __reshape_index(&index, array, index, n);
        if (ret < 0)
            return ret;
    }

    int i;
    for (i = 0; i < array->nb_dimentions; i++) {
        intptr_t k = index[i].i;

        if (k >= array->dimentions[i].num) {
            loge("index [%ld] out of size [%d], in dim: %d, file: %s, line: %d\n",
                 k, array->dimentions[i].num, i, w->file->data, w->line);

            if (n < array->nb_dimentions) {
                free(index);
                index = NULL;
            }
            return -1;
        }

        variable_t *v_index = variable_alloc(NULL, t);
        v_index->const_flag = 1;
        v_index->const_literal_flag = 1;
        v_index->data.i64 = k;

        node_t *node_index = node_alloc(NULL, v_index->type, v_index);
        node_t *node_op = node_alloc(w, OP_ARRAY_INDEX, NULL);

        node_add_child(node_op, root);
        node_add_child(node_op, node_index);
        root = node_op;
    }

    if (n < array->nb_dimentions) {
        free(index);
        index = NULL;
    }

    *pnode = root;
    return array->nb_dimentions;
}

int struct_member_init(ast_t *ast, lex_word_t *w, variable_t *_struct, dfa_index_t *index, int n, node_t **pnode) {
    if (!pnode)
        return -1;

    variable_t *v = NULL;
    type_t *t = block_find_type_type(ast->current_block, _struct->type);
    node_t *root = *pnode;

    if (!root)
        root = node_alloc(NULL, _struct->type, _struct);

    int j = 0;
    while (j < n) {
        if (!t->scope) {
            loge("\n");
            return -1;
        }

        int k;

        if (index[j].w) {
            for (k = 0; k < t->scope->vars->size; k++) {
                v = t->scope->vars->data[k];

                if (v->w && !strcmp(index[j].w->text->data, v->w->text->data))
                    break;
            }
        } else
            k = index[j].i;

        if (k >= t->scope->vars->size) {
            loge("\n");
            return -1;
        }

        v = t->scope->vars->data[k];

        node_t *node_op = node_alloc(w, OP_POINTER, NULL);
        node_t *node_v = node_alloc(NULL, v->type, v);

        node_add_child(node_op, root);
        node_add_child(node_op, node_v);
        root = node_op;

        logi("j: %d, k: %d, v: '%s'\n", j, k, v->w->text->data);
        j++;

        if (v->nb_dimentions > 0) {
            int ret = __array_member_init(ast, w, v, index + j, n - j, &root);
            if (ret < 0)
                return -1;

            j += ret;
            logi("struct var member: %s->%s[]\n", _struct->w->text->data, v->w->text->data);
        }

        if (v->type < STRUCT || v->nb_pointers > 0) {
            // if 'v' is a base type var or a pointer, and of course 'v' isn't an array,
            // we can't get the member of v !!
            // the index must be the last one, and its expr is to init v !
            if (j < n - 1) {
                loge("number of indexes more than needed, struct member: %s->%s, file: %s, line: %d\n",
                     _struct->w->text->data, v->w->text->data, w->file->data, w->line);
                return -1;
            }

            logi("struct var member: %s->%s\n", _struct->w->text->data, v->w->text->data);

            *pnode = root;
            return n;
        }

        // 'v' is not a base type var or a pointer, it's a struct
        // first, find the type in this struct scope, then find in global
        type_t *type_v = NULL;

        while (t) {
            type_v = scope_find_type_type(t->scope, v->type);
            if (type_v)
                break;

            // only can use types in this scope, or parent scope
            // can't use types in children scope
            t = t->parent;
        }

        if (!type_v) {
            type_v = block_find_type_type(ast->current_block, v->type);

            if (!type_v) {
                loge("\n");
                return -1;
            }
        }

        t = type_v;
    }

    loge("number of indexes less than needed, struct member: %s->%s, file: %s, line: %d\n",
         _struct->w->text->data, v->w->text->data, w->file->data, w->line);
    return -1;
}

int array_member_init(ast_t *ast, lex_word_t *w, variable_t *array, dfa_index_t *index, int n, node_t **pnode) {
    node_t *root = NULL;

    int ret = __array_member_init(ast, w, array, index, n, &root);
    if (ret < 0)
        return ret;

    if (array->type < STRUCT || array->nb_pointers > 0) {
        if (ret < n - 1) {
            loge("\n");
            return -1;
        }

        *pnode = root;
        return n;
    }

    ret = struct_member_init(ast, w, array, index + ret, n - ret, &root);
    if (ret < 0)
        return ret;

    *pnode = root;
    return n;
}

int array_init(ast_t *ast, lex_word_t *w, variable_t *v, vector_t *init_exprs) {
    dfa_init_expr_t *ie;

    int unset = 0;
    int unset_dim = -1;
    int capacity = 1;
    int i;
    int j;

    for (i = 0; i < v->nb_dimentions; i++) {
        assert(v->dimentions);

        logi("dim[%d]: %d\n", i, v->dimentions[i].num);

        if (v->dimentions[i].num < 0) {
            if (unset > 0) {
                loge("array '%s' should only unset 1-dimention size, file: %s, line: %d\n",
                     v->w->text->data, w->file->data, w->line);
                return -1;
            }

            unset++;
            unset_dim = i;
        } else
            capacity *= v->dimentions[i].num;
    }

    if (unset) {
        int unset_max = -1;

        for (i = 0; i < init_exprs->size; i++) {
            ie = init_exprs->data[i];

            if (unset_dim < ie->n) {
                if (unset_max < ie->index[unset_dim].i)
                    unset_max = ie->index[unset_dim].i;
            }
        }

        if (-1 == unset_max) {
            unset_max = init_exprs->size / capacity;

            v->dimentions[unset_dim].num = unset_max;

            logw("don't set %d-dimention size of array '%s', use '%d' as calculated, file: %s, line: %d\n",
                 unset_dim, v->w->text->data, unset_max, w->file->data, w->line);
        } else
            v->dimentions[unset_dim].num = unset_max + 1;
    }

    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        if (ie->n < v->nb_dimentions) {
            int n = ie->n;

            void *p = realloc(ie, sizeof(dfa_init_expr_t) + sizeof(dfa_index_t) * v->nb_dimentions);
            if (!p)
                return -ENOMEM;
            init_exprs->data[i] = p;

            ie = p;
            ie->n = v->nb_dimentions;

            intptr_t index = ie->index[n - 1].i;

            logw("reshape 'init exprs' from %d-dimention to %d-dimention, origin last index: %ld\n", n, v->nb_dimentions, index);

            for (j = v->nb_dimentions - 1; j >= 0; j--) {
                ie->index[j].i = index % v->dimentions[j].num;
                index = index / v->dimentions[j].num;
            }
        }

        for (j = 0; j < v->nb_dimentions; j++)
            logi("\033[32mi: %d, dim: %d, size: %d, index: %ld\033[0m\n", i, j, v->dimentions[j].num, ie->index[j].i);
    }

    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        logi("#### data init, i: %d, init expr: %p\n", i, ie->expr);

        expr_t *e;
        node_t *assign;
        node_t *node = NULL;

        if (array_member_init(ast, w, v, ie->index, ie->n, &node) < 0) {
            loge("\n");
            return -1;
        }

        e = expr_alloc();
        assign = node_alloc(w, OP_ASSIGN, NULL);

        node_add_child(assign, node);
        node_add_child(assign, ie->expr);
        expr_add_node(e, assign);

        ie->expr = e;
        printf("\n");
    }

    return 0;
}

int struct_init(ast_t *ast, lex_word_t *w, variable_t *var, vector_t *init_exprs) {
    dfa_init_expr_t *ie;

    int i;
    for (i = 0; i < init_exprs->size; i++) {
        ie = init_exprs->data[i];

        logi("#### struct init, i: %d, init_expr->expr: %p\n", i, ie->expr);

        expr_t *e;
        node_t *assign;
        node_t *node = NULL;

        if (struct_member_init(ast, w, var, ie->index, ie->n, &node) < 0)
            return -1;

        e = expr_alloc();
        assign = node_alloc(w, OP_ASSIGN, NULL);

        node_add_child(assign, node);
        node_add_child(assign, ie->expr);
        expr_add_node(e, assign);

        ie->expr = e;
        printf("\n");
    }

    return 0;
}
