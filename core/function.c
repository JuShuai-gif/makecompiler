#include "function.h"
#include "scope.h"
#include "type.h"
#include "block.h"

function_t *function_alloc(lex_word_t *w) {
    assert(w);

    function_t *f = calloc(1, sizeof(function_t));
    if (!f)
        return NULL;

    f->node.type = FUNCTION;
    f->op_type = -1;

    list_init(&f->basic_block_list_head);
    list_init(&f->dag_list_head);

    f->scope = scope_alloc(w, "function");
    if (!f->scope)
        goto _scope_error;

    f->node.w = lex_word_clone(w);
    if (!f->node.w)
        goto _word_error;

    f->rets = vector_alloc();
    if (!f->rets)
        goto _ret_error;

    f->argv = vector_alloc();
    if (!f->argv)
        goto _argv_error;

    f->callee_functions = vector_alloc();
    if (!f->callee_functions)
        goto _callee_error;

    f->caller_functions = vector_alloc();
    if (!f->caller_functions)
        goto _caller_error;

    f->jmps = vector_alloc();
    if (!f->jmps)
        goto _jmps_error;

    f->bb_loops = vector_alloc();
    if (!f->bb_loops)
        goto _loop_error;

    f->bb_groups = vector_alloc();
    if (!f->bb_groups)
        goto _group_error;

    f->text_relas = vector_alloc();
    if (!f->text_relas)
        goto _text_rela_error;

    f->data_relas = vector_alloc();
    if (!f->data_relas)
        goto _data_rela_error;

    return f;

_data_rela_error:
    vector_free(f->text_relas);
_text_rela_error:
    vector_free(f->bb_groups);
_group_error:
    vector_free(f->bb_loops);
_loop_error:
    vector_free(f->jmps);
_jmps_error:
    vector_free(f->caller_functions);
_caller_error:
    vector_free(f->callee_functions);
_callee_error:
    vector_free(f->argv);
_argv_error:
    vector_free(f->rets);
_ret_error:
    lex_word_free(f->node.w);
_word_error:
    scope_free(f->scope);
_scope_error:
    free(f);
    return NULL;
}

void function_free(function_t *f) {
    if (f) {
        scope_free(f->scope);
        f->scope = NULL;

        if (f->signature) {
            string_free(f->signature);
            f->signature = NULL;
        }

        if (f->rets) {
            vector_clear(f->rets, (void (*)(void *))variable_free);
            vector_free(f->rets);
        }

        if (f->argv) {
            vector_clear(f->argv, (void (*)(void *))variable_free);
            vector_free(f->argv);
            f->argv = NULL;
        }

        if (f->callee_functions)
            vector_free(f->callee_functions);

        if (f->caller_functions)
            vector_free(f->caller_functions);

        if (f->jmps) {
            vector_free(f->jmps);
            f->jmps = NULL;
        }

        if (f->text_relas) {
            vector_free(f->text_relas);
            f->text_relas = NULL;
        }

        if (f->data_relas) {
            vector_free(f->data_relas);
            f->data_relas = NULL;
        }

        node_free((node_t *)f);
    }
}

int function_same(function_t *f0, function_t *f1) {
    if (strcmp(f0->node.w->text->data, f1->node.w->text->data))
        return 0;

    return function_same_type(f0, f1);
}

int function_same_argv(vector_t *argv0, vector_t *argv1) {
    if (argv0) {
        if (!argv1)
            return 0;

        if (argv0->size != argv1->size)
            return 0;

        int i;
        for (i = 0; i < argv0->size; i++) {
            variable_t *v0 = argv0->data[i];
            variable_t *v1 = argv1->data[i];

            if (!variable_type_like(v0, v1))
                return 0;
        }
    } else {
        if (argv1)
            return 0;
    }

    return 1;
}

int function_like_argv(vector_t *argv0, vector_t *argv1) {
    if (argv0) {
        if (!argv1)
            return 0;

        if (argv0->size != argv1->size)
            return 0;

        int i;
        for (i = 0; i < argv0->size; i++) {
            variable_t *v0 = argv0->data[i];
            variable_t *v1 = argv1->data[i];

            if (variable_type_like(v0, v1))
                continue;

            if (variable_is_struct_pointer(v0) || variable_is_struct_pointer(v1))
                return 0;
        }
    } else {
        if (argv1)
            return 0;
    }

    return 1;
}

int function_same_type(function_t *f0, function_t *f1) {
    if (f0->rets) {
        if (!f1->rets)
            return 0;

        if (f0->rets->size != f1->rets->size)
            return 0;

        int i;
        for (i = 0; i < f0->rets->size; i++) {
            if (!variable_type_like(f0->rets->data[i], f1->rets->data[i]))
                return 0;
        }
    } else if (f1->rets)
        return 0;

    return function_same_argv(f0->argv, f1->argv);
}
