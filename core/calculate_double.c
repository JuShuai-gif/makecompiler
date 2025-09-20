#include "calculate.h"

#define DOUBLE_BINARY_OP(name, op)                                                         \
    int double_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_DOUBLE);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                            \
        if (!r)                                                                            \
            return -ENOMEM;                                                                \
        r->data.d = src0->data.d op src1->data.d;                                          \
        if (pret)                                                                          \
            *pret = r;                                                                     \
        return 0;                                                                          \
    }

#define DOUBLE_UNARY_OP(name, op)                                                          \
    int double_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_DOUBLE);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                            \
        if (!r)                                                                            \
            return -ENOMEM;                                                                \
        r->data.d = op src0->data.d;                                                       \
        if (pret)                                                                          \
            *pret = r;                                                                     \
        return 0;                                                                          \
    }

DOUBLE_BINARY_OP(add, +)
DOUBLE_BINARY_OP(sub, -)
DOUBLE_BINARY_OP(mul, *)
DOUBLE_BINARY_OP(div, /)

DOUBLE_UNARY_OP(neg, -)

DOUBLE_BINARY_OP(gt, >)
DOUBLE_BINARY_OP(lt, <)
