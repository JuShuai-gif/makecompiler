#include "block.h"
#include "scope.h"

block_t *block_alloc(lex_word_t *w) {
    block_t *b = calloc(1, sizeof(block_t));

    if (!b)
        return NULL;

    b->scope = scope_alloc(w, "block");

    if (!b->scope) {
        free(b);
        return NULL;
    }

    if (w) {
        b->node.w = lex_word_clone(w);

        if (!b->node.w) {
            scope_free(b->scope);
            free(b);
            return NULL;
        }
    }
    b->node.type = OP_BLOCK;
    return b;
}

block_t *block_alloc_cstr(const char *name) {
    block_t *b = calloc(1, sizeof(block_t));

    if (!b)
        return NULL;

    b->name = string_cstr(name);

    if (!b->name) {
        free(b);
        return NULL;
    }

    b->scope = scope_alloc(NULL, name);
    if (!b->scope) {
        string_free(b->name);
        free(b);
        return NULL;
    }

    b->node.type = OP_BLOCK;
    return b;
}

void block_free(block_t *b) {
    if (b) {
        scope_free(b->scope);
        b->scope = NULL;

        if (b->name) {
            string_free(b->name);
            b->name = NULL;
        }
        node_free((node_t *)b);
    }
}

type_t *block_find_type(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                type_t *t = scope_find_type(b->scope, name);
                if (t)
                    return t;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

type_t *block_find_type_type(block_t *b, const int type) {
    assert(b);

    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                type_t *t = scope_find_type_type(b->scope, type);
                if (t)
                    return t;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

variable_t *block_find_variable(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                variable_t *v = scope_find_variable(b->scope, name);
                if (v)
                    return v;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

function_t *block_find_function(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                function_t *f = scope_find_function(b->scope, name);
                if (f)
                    return f;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

label_t *block_find_label(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type) {
            assert(b->scope);

            label_t *l = scope_find_label(b->scope, name);

            if (l)
                return l;
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}
