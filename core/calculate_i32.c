#include "calculate.h"

#define I32_BINARY_OP(name, op)                                                         \
    int i32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_I32);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.i = src0->data.i op src1->data.i;                                       \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define I32_UNARY_OP(name, op)                                                          \
    int i32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_I32);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.i = op src0->data.i;                                                    \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define I32_UPDATE_OP(name, op)                                                         \
    int i32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        op src0->data.i;                                                                \
        if (pret)                                                                       \
            *pret = variable_ref(src0);                                                 \
        return 0;                                                                       \
    }

I32_BINARY_OP(add, +)
I32_BINARY_OP(sub, -)
I32_BINARY_OP(mul, *)
I32_BINARY_OP(div, /)
I32_BINARY_OP(mod, %)

I32_BINARY_OP(shl, <<)
I32_BINARY_OP(shr, >>)

I32_UPDATE_OP(inc, ++);
I32_UPDATE_OP(dec, --);

I32_UNARY_OP(neg, -)

I32_BINARY_OP(bit_and, &)
I32_BINARY_OP(bit_or, |)
I32_UNARY_OP(bit_not, ~)

I32_BINARY_OP(logic_and, &&)
I32_BINARY_OP(logic_or, ||)
I32_UNARY_OP(logic_not, !)

I32_BINARY_OP(gt, >)
I32_BINARY_OP(lt, <)
I32_BINARY_OP(eq, ==)
I32_BINARY_OP(ne, !=)
I32_BINARY_OP(ge, >=)
I32_BINARY_OP(le, <=)
