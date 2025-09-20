#include "calculate.h"

#define FLOAT_BINARY_OP(name, op)                                                         \
    int float_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_FLOAT);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                           \
        if (!r)                                                                           \
            return -ENOMEM;                                                               \
        r->data.f = src0->data.f op src1->data.f;                                         \
        if (pret)                                                                         \
            *pret = r;                                                                    \
        return 0;                                                                         \
    }

#define FLOAT_UNARY_OP(name, op)                                                          \
    int float_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_FLOAT);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                           \
        if (!r)                                                                           \
            return -ENOMEM;                                                               \
        r->data.f = op src0->data.f;                                                      \
        if (pret)                                                                         \
            *pret = r;                                                                    \
        return 0;                                                                         \
    }

FLOAT_BINARY_OP(add, +)
FLOAT_BINARY_OP(sub, -)
FLOAT_BINARY_OP(mul, *)
FLOAT_BINARY_OP(div, /)

FLOAT_UNARY_OP(neg, -)

FLOAT_BINARY_OP(gt, >)
FLOAT_BINARY_OP(lt, <)
