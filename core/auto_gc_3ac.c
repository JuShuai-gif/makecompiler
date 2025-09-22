#include "optimizer.h"
#include "pointer_alias.h"

static _3ac_operand_t * _auto_gc_operand_alloc_pf(list_t *dag_list_head, ast_t *ast, function_t *f) {
    _3ac_operand_t * src;
    dag_node_t *dn;
    variable_t *v;
    type_t *t = NULL;

    int ret = ast_find_type_type(&t, ast, FUNCTION_PTR);
    assert(0 == ret);
    assert(t);

    if (f)
        v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
    else
        v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 1, NULL);

    if (!v)
        return NULL;
    v->const_literal_flag = 1;

    dn = dag_node_alloc(v->type, v, (node_t *)f);

    variable_free(v);
    v = NULL;
    if (!dn)
        return NULL;

    src = _3ac_operand_alloc();
    if (!src) {
        dag_node_free(dn);
        return NULL;
    }

    src->node = (node_t *)f;
    src->dag_node = dn;

    list_add_tail(dag_list_head, &dn->list);
    return src;
}

static _3ac_code_t * _auto_gc_code_ref(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    _3ac_operand_t * src;
    _3ac_code_t * c;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    int ret = ast_find_function(&f, ast, " _auto_ref");
    if (!f)
        return NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->op = _3ac_find_operator(OP_CALL);

#define AUTO_GC_CODE_ADD_FUNCPTR()                              \
    do {                                                        \
        src = _auto_gc_operand_alloc_pf(dag_list_head, ast, f); \
        if (!src) {                                             \
            _3ac_code_free(c);                                   \
            return NULL;                                        \
        }                                                       \
                                                                \
        if (vector_add(c->srcs, src) < 0) {                     \
            _3ac_operand_free(src);                              \
            _3ac_code_free(c);                                   \
            return NULL;                                        \
        }                                                       \
    } while (0)

    AUTO_GC_CODE_ADD_FUNCPTR();

#define AUTO_GC_CODE_ADD_DN()               \
    do {                                    \
        src = _3ac_operand_alloc();          \
        if (!src) {                         \
            _3ac_code_free(c);               \
            return NULL;                    \
        }                                   \
        src->node = dn->node;               \
        src->dag_node = dn;                 \
                                            \
        if (vector_add(c->srcs, src) < 0) { \
            _3ac_operand_free(src);          \
            _3ac_code_free(c);               \
            return NULL;                    \
        }                                   \
    } while (0)

    AUTO_GC_CODE_ADD_DN();

    return c;
}

static _3ac_code_t * _auto_gc_code_memset_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    _3ac_operand_t * src;
    _3ac_code_t * c;
    dag_node_t *dn;
    function_t *f;
    variable_t *v;
    type_t *t = NULL;

    int ret = ast_find_type_type(&t, ast, VAR_INTPTR);
    assert(0 == ret);
    assert(t);

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_3AC_MEMSET);

    dn = dn_array;
    AUTO_GC_CODE_ADD_DN();

#define AUTO_GC_CODE_ADD_VAR()                   \
    do {                                         \
        dn = dag_node_alloc(v->type, v, NULL);   \
                                                 \
        variable_free(v);                        \
        v = NULL;                                \
                                                 \
        if (!dn) {                               \
            _3ac_code_free(c);                    \
            return NULL;                         \
        }                                        \
        src = _3ac_operand_alloc();               \
        if (!src) {                              \
            dag_node_free(dn);                   \
            _3ac_code_free(c);                    \
            return NULL;                         \
        }                                        \
                                                 \
        if (vector_add(c->srcs, src) < 0) {      \
            _3ac_operand_free(src);               \
            dag_node_free(dn);                   \
            _3ac_code_free(c);                    \
            return NULL;                         \
        }                                        \
                                                 \
        src->node = dn->node;                    \
        src->dag_node = dn;                      \
        list_add_tail(dag_list_head, &dn->list); \
    } while (0)

    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = 0;
    v->const_literal_flag = 1;
    AUTO_GC_CODE_ADD_VAR();

    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = variable_size(dn_array->var);
    v->const_literal_flag = 1;
    AUTO_GC_CODE_ADD_VAR();

    return c;
}

static _3ac_code_t * _auto_gc_code_free_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array, int capacity, int nb_pointers) {
    _3ac_operand_t * src;
    _3ac_code_t * c;
    dag_node_t *dn;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    int ret = ast_find_function(&f, ast, " _auto_free_array");
    if (!f)
        return NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_CALL);

    AUTO_GC_CODE_ADD_FUNCPTR();

    dn = dn_array;
    AUTO_GC_CODE_ADD_DN();

    t = block_find_type_type(ast->current_block, VAR_INTPTR);
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = capacity;
    v->const_literal_flag = 1;
    AUTO_GC_CODE_ADD_VAR();

    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = nb_pointers;
    v->const_literal_flag = 1;
    AUTO_GC_CODE_ADD_VAR();

    if (dn_array->var->type >= STRUCT) {
        t = NULL;
        ret = ast_find_type_type(&t, ast, dn_array->var->type);
        assert(0 == ret);
        assert(t);

        f = scope_find_function(t->scope, "__release");
    } else
        f = NULL;
    AUTO_GC_CODE_ADD_FUNCPTR();

    return c;
}

static _3ac_code_t * _auto_gc_code_freep_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array, int nb_pointers) {
    _3ac_operand_t * src;
    _3ac_code_t * c;
    dag_node_t *dn;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    int ret = ast_find_function(&f, ast, " _auto_freep_array");
    if (!f)
        return NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_CALL);

    AUTO_GC_CODE_ADD_FUNCPTR();

    dn = dn_array;
    AUTO_GC_CODE_ADD_DN();

    t = NULL;
    ret = ast_find_type_type(&t, ast, VAR_INTPTR);
    assert(0 == ret);
    assert(t);
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = nb_pointers;
    v->const_literal_flag = 1;

    char buf[128];
    snprintf(buf, sizeof(buf) - 1, "%d", nb_pointers);

    if (string_cat_cstr(v->w->text, buf) < 0) {
        variable_free(v);
        _3ac_code_free(c);
        return NULL;
    }
    AUTO_GC_CODE_ADD_VAR();

    if (dn_array->var->type >= STRUCT) {
        t = NULL;
        ret = ast_find_type_type(&t, ast, dn_array->var->type);
        assert(0 == ret);
        assert(t);

        f = scope_find_function(t->scope, "__release");
    } else
        f = NULL;
    AUTO_GC_CODE_ADD_FUNCPTR();

    return c;
}

static _3ac_code_t * _code_alloc_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    _3ac_operand_t * src;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    src = _3ac_operand_alloc();
    if (!src) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->srcs, src) < 0) {
        _3ac_operand_free(src);
        _3ac_code_free(c);
        return NULL;
    }
    src->node = dn->node;
    src->dag_node = dn;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    c->op = _3ac_find_operator(OP_ADDRESS_OF);

    w = lex_word_alloc(dn->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("&");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers + 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static _3ac_code_t * _code_alloc_dereference(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    _3ac_operand_t * src;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    src = _3ac_operand_alloc();
    if (!src) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->srcs, src) < 0) {
        _3ac_operand_free(src);
        _3ac_code_free(c);
        return NULL;
    }
    src->node = dn->node;
    src->dag_node = dn;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_DEREFERENCE);

    w = lex_word_alloc(dn->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("*");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers - 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static _3ac_code_t * _code_alloc_member(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_member) {
    _3ac_operand_t * base;
    _3ac_operand_t * member;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;

    member = _3ac_operand_alloc();
    if (!member) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, member) < 0) {
        _3ac_operand_free(member);
        _3ac_code_free(c);
        return NULL;
    }
    member->node = dn_member->node;
    member->dag_node = dn_member;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_POINTER);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("->");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn_member->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static _3ac_code_t * _code_alloc_member_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_member) {
    _3ac_operand_t * base;
    _3ac_operand_t * member;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;

    member = _3ac_operand_alloc();
    if (!member) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, member) < 0) {
        _3ac_operand_free(member);
        _3ac_code_free(c);
        return NULL;
    }
    member->node = dn_member->node;
    member->dag_node = dn_member;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_3AC_ADDRESS_OF_POINTER);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("&->");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn_member->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers + 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static _3ac_code_t * _code_alloc_array_member_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale) {
    _3ac_operand_t * base;
    _3ac_operand_t * index;
    _3ac_operand_t * scale;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;

    index = _3ac_operand_alloc();
    if (!index) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, index) < 0) {
        _3ac_operand_free(index);
        _3ac_code_free(c);
        return NULL;
    }
    index->node = dn_index->node;
    index->dag_node = dn_index;

    scale = _3ac_operand_alloc();
    if (!scale) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, scale) < 0) {
        _3ac_operand_free(scale);
        _3ac_code_free(c);
        return NULL;
    }
    scale->node = dn_scale->node;
    scale->dag_node = dn_scale;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_3AC_ADDRESS_OF_ARRAY_INDEX);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("&[]");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn_base->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static _3ac_code_t * _code_alloc_array_member(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale) {
    _3ac_operand_t * base;
    _3ac_operand_t * index;
    _3ac_operand_t * scale;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;

    index = _3ac_operand_alloc();
    if (!index) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, index) < 0) {
        _3ac_operand_free(index);
        _3ac_code_free(c);
        return NULL;
    }
    index->node = dn_index->node;
    index->dag_node = dn_index;

    scale = _3ac_operand_alloc();
    if (!scale) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, scale) < 0) {
        _3ac_operand_free(scale);
        _3ac_code_free(c);
        return NULL;
    }
    scale->node = dn_scale->node;
    scale->dag_node = dn_scale;

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_ARRAY_INDEX);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("[]");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }

    int ret = ast_find_type_type(&t, ast, dn_base->var->type);
    assert(0 == ret);
    assert(t);

    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }

    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

static int _auto_gc_code_list_ref(list_t *h, list_t *dag_list_head, ast_t *ast, dn_status_t *ds) {
    dag_node_t *dn = ds->dag_node;
    _3ac_code_t * c;
    _3ac_operand_t * dst;

    if (ds->dn_indexes) {
        int i;
        for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
            dn_index_t *di = ds->dn_indexes->data[i];

            if (di->member) {
                assert(di->dn);

                c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

            } else {
                assert(di->index >= 0 || -1 == di->index);
                assert(di->dn_scale);

                c = _code_alloc_array_member(dag_list_head, ast, dn, di->dn, di->dn_scale);
            }

            list_add_tail(h, &c->list);

            dst = c->dsts->data[0];
            dn = dst->dag_node;
        }
    }

    c = _auto_gc_code_ref(dag_list_head, ast, dn);

    list_add_tail(h, &c->list);
    return 0;
}

static int _auto_gc_code_list_freep(list_t *h, list_t *dag_list_head, ast_t *ast, dn_status_t *ds) {
    dag_node_t *dn = ds->dag_node;
    _3ac_code_t * c;
    _3ac_operand_t * dst;

    if (ds->dn_indexes) {
        dn_index_t *di;
        int i;

        for (i = ds->dn_indexes->size - 1; i >= 1; i--) {
            di = ds->dn_indexes->data[i];

            if (di->member) {
                assert(di->dn);

                c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

            } else {
                assert(di->index >= 0);

                assert(0 == di->index);

                c = _code_alloc_dereference(dag_list_head, ast, dn);
            }

            list_add_tail(h, &c->list);

            dst = c->dsts->data[0];
            dn = dst->dag_node;
        }

        di = ds->dn_indexes->data[0];

        if (di->member) {
            assert(di->dn);

            c = _code_alloc_member_address(dag_list_head, ast, dn, di->dn);

        } else {
            assert(di->index >= 0 || -1 == di->index);
            assert(di->dn_scale);

            c = _code_alloc_array_member_address(dag_list_head, ast, dn, di->dn, di->dn_scale);
        }

        list_add_tail(h, &c->list);

        dst = c->dsts->data[0];
        dn = dst->dag_node;

    } else {
        c = _code_alloc_address(dag_list_head, ast, dn);

        list_add_tail(h, &c->list);

        dst = c->dsts->data[0];
        dn = dst->dag_node;
    }

    int nb_pointers = variable_nb_pointers(dn->var);

    assert(nb_pointers >= 2);

    c = _auto_gc_code_freep_array(dag_list_head, ast, dn, nb_pointers - 1);

    list_add_tail(h, &c->list);
    return 0;
}

static int _auto_gc_code_list_free_array(list_t *h, list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    _3ac_code_t * c;

    assert(dn_array->var->nb_dimentions > 0);
    assert(dn_array->var->capacity > 0);

    c = _auto_gc_code_free_array(dag_list_head, ast, dn_array, dn_array->var->capacity, dn_array->var->nb_pointers);

    list_add_tail(h, &c->list);
    return 0;
}

static int _auto_gc_code_list_memset_array(list_t *h, list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    _3ac_code_t * c;

    assert(dn_array->var->capacity > 0);

    c = _auto_gc_code_memset_array(dag_list_head, ast, dn_array);

    list_add_tail(h, &c->list);
    return 0;
}

static int _bb_add_gc_code_ref(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dn_status_t *ds) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_ref(&h, dag_list_head, ast, ds);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}

static int _bb_add_gc_code_freep(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dn_status_t *ds) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_freep(&h, dag_list_head, ast, ds);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}

static int _bb_add_gc_code_memset_array(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dag_node_t *dn_array) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_memset_array(&h, dag_list_head, ast, dn_array);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}

static int _bb_add_gc_code_free_array(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dag_node_t *dn_array) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_free_array(&h, dag_list_head, ast, dn_array);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}
