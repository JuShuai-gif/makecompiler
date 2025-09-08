#include "ast.h"

scope_t *scope_alloc(lex_word_t *w, const char *name) {
    scope_t *scope = calloc(1, sizeof(scope_t));

    if (!scope)
        return NULL;

    scope->name = string_cstr(name);
    if (!scope->name) {
        free(scope);
        return NULL;
    }

    scope->vars = vector_alloc();
    if (!scope->vars) {
        string_free(scope->name);
        free(scope);
        return NULL;
    }

    if (w) {
        scope->w = lex_word_clone(w);

        if (!scope->w) {
            vector_free(scope->vars);
            string_free(scope->name);
            free(scope);
            return NULL;
        }
    }

    list_init(&scope->list);
    list_init(&scope->type_list_head);
    list_init(&scope->operator_list_head);
    list_init(&scope->function_list_head);
    list_init(&scope->label_list_head);
    return scope;
}

void scope_push_var(scope_t *scope, variable_t *var) {
    assert(scope);
    assert(var);
    vector_add(scope->vars, var);
}

void scope_push_type(scope_t *scope, type_t *t) {
    assert(scope);
    assert(t);
    list_add_front(&scope->type_list_head, &t->list);
}

void scope_push_operator(scope_t *scope, function_t *op) {
    assert(scope);
    assert(op);
    list_add_front(&scope->operator_list_head, &op->list);
}

void scope_push_function(scope_t *scope, function_t *f) {
    assert(scope);
    assert(f);
    list_add_front(&scope->function_list_head, &f->list);
}

void scope_push_label(scope_t *scope, label_t *l) {
    assert(scope);
    assert(l);
    list_add_front(&scope->label_list_head, &l->list);
}

void scope_free(scope_t *scope) {
    if (scope) {
        string_free(scope->name);
        scope->name = NULL;

        if (scope->w)
            lex_word_free(scope->w);

        vector_clear(scope->vars, (void (*)(void *))variable_free);
        vector_free(scope->vars);
        scope->vars = NULL;

        list_clear(&scope->type_list_head, type_t, list, type_free);
        list_clear(&scope->function_list_head, function_t, list, function_free);

        free(scope);
    }
}

type_t *scope_find_type(scope_t *scope, const char *name) {
    type_t *t;
    list_t *l;

    for (l = list_head(&scope->type_list_head); l != list_sentinel(&scope->type_list_head); l != list_next(l)) {
        t = list_data(l, type_t, list);

        if (!strcmp(name, t->name->data)) {
            return t;
        }
    }
    return NULL;
}

type_t *scope_find_type_type(scope_t *scope, const int type) {
    type_t *t;
    list_t *l;

    for (l = list_head(&scope->type_list_head); l != list_sentinel(&scope->type_list_head); l = list_next(l)) {
        t = list_data(l, type_t, list);

        if (type == t->type)
            return t;
    }
    return NULL;
}

variable_t *scope_find_variable(scope_t *scope, const char *name) {
    variable_t *v;
    int i;
    for (i = 0; i < scope->vars->size; i++) {
        v = scope->vars->data[i];

        if (v->w && !strcmp(name, v->w->text->data))
            return v;
    }
    return NULL;
}

function_t *scope_find_function(scope_t *scope, const char *name) {
    function_t *f;
    list_t *l;

    for (l = list_head(&scope->function_list_head); l != list_sentinel(&scope->function_list_head); l = list_next(l)) {
        f = list_data(l, function_t, list);

        if (!strcmp(name, f->node.w->text->data))
            return f;
    }
    return NULL;
}

function_t *scope_find_same_function(scope_t *scope, function_t *f0) {
    function_t *f1;
    list_t *l;

    for (l = list_head(&scope->function_list_head); l != list_sentinel(&scope->function_list_head); l = list_next(l)) {
        f1 = list_data(l, function_t, list);

        if (function_same(f0, f1))
            return f1;
    }
    return NULL;
}

function_t *scope_find_proper_function(scope_t *scope, const char *name, vector_t *argv) {
    function_t *f;
    list_t *l;

    for (l = list_head(&scope->function_list_head); l != list_sentinel(&scope->function_list_head); l = list_next(l)) {
        f = list_data(l, function_t, list);

        if (strcmp(f->node.w->text->data, name))
            continue;

        if (function_same_argv(f->argv, argv))
            return f;
    }

    return NULL;
}

int scope_find_overloaded_functions(vector_t **pfunctions, scope_t *scope, const int op_type, vector_t *argv) {
    function_t* f;
	vector_t*   vec;
    list_t*     l;

	vec =vector_alloc();
	if (!vec)
		return -ENOMEM;

	for (l = list_head(&scope->operator_list_head); l != list_sentinel(&scope->operator_list_head); l = list_next(l)) {
		f  = list_data(l, function_t, list);

		if (op_type != f->op_type)
			continue;

		if (!function_like_argv(f->argv, argv))
			continue;

		int ret = vector_add(vec, f);
		if (ret < 0) {
			vector_free(vec);
			return ret;
		}
	}

	if (0 == vec->size) {
		vector_free(vec);
		return -404;
	}

	*pfunctions = vec;
	return 0;
}

int scope_find_like_functions(vector_t **pfunctions, scope_t *scope, const char *name, vector_t *argv) {
    function_t *f;
    vector_t *vec;
    list_t *l;

    vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    for (l = list_head(&scope->function_list_head); l != list_sentinel(&scope->function_list_head); l = list_next(l)) {
        f = list_data(l, function_t, list);

        if (strcmp(f->node.w->text->data, name))
            continue;
        int ret = scf_vector_add(vec, f);
        if (ret < 0) {
            scf_vector_free(vec);
            return ret;
        }
    }
    if (0 == vec->size) {
        scf_vector_free(vec);
        return -404;
    }

    *pfunctions = vec;
    return 0;
}

label_t *scope_find_label(scope_t *scope, const char *name) {
    label_t* label;
	list_t*  l;

	for (l    = list_head(&scope->label_list_head); l != list_sentinel(&scope->label_list_head); l = list_next(l)) {
		label = list_data(l, label_t, list);

		if (!strcmp(name, label->w->text->data))
			return label;
	}
	return NULL;
}
