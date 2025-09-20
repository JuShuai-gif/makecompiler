#include "parse.h"

int _find_function(node_t *node, void *arg, vector_t *vec) {
    if (FUNCTION == node->type) {
        function_t *f = (function_t *)node;

        return vector_add(vec, f);
    }

    return 0;
}

int _find_global_var(node_t *node, void *arg, vector_t *vec) {
    if (OP_BLOCK == node->type
        || (node->type >= STRUCT && node->class_flag)) {
        block_t *b = (block_t *)node;

        if (!b->scope || !b->scope->vars)
            return 0;

        int i;
        for (i = 0; i < b->scope->vars->size; i++) {
            variable_t *v = b->scope->vars->data[i];

            if (v->global_flag || v->static_flag) {
                int ret = vector_add(vec, v);
                if (ret < 0)
                    return ret;
            }
        }
    }

    return 0;
}
