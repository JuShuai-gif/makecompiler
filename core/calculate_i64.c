#include "calculate.h"

#define I64_BINARY_OP(name, op)                                                         \
    int i64_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_I64);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.i64 = src0->data.i64 op src1->data.i64;                                 \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define I64_UNARY_OP(name, op)                                                          \
    int i64_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_I64);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.i64 = op src0->data.i64;                                                \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define I64_UPDATE_OP(name, op)                                                         \
    int i64_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        op src0->data.i64;                                                              \
        if (pret)                                                                       \
            *pret = variable_ref(src0);                                                 \
        return 0;                                                                       \
    }

I64_BINARY_OP(add, +)
I64_BINARY_OP(sub, -)
I64_BINARY_OP(mul, *)
I64_BINARY_OP(div, /)
I64_BINARY_OP(mod, %)

I64_BINARY_OP(shl, <<)
I64_BINARY_OP(shr, >>)

I64_UPDATE_OP(inc, ++);
I64_UPDATE_OP(dec, --);

I64_UNARY_OP(neg, -)

I64_BINARY_OP(bit_and, &)
I64_BINARY_OP(bit_or, |)
I64_UNARY_OP(bit_not, ~)

I64_BINARY_OP(logic_and, &&)
I64_BINARY_OP(logic_or, ||)
I64_UNARY_OP(logic_not, !)

I64_BINARY_OP(gt, >)
I64_BINARY_OP(lt, <)
I64_BINARY_OP(eq, ==)
I64_BINARY_OP(ne, !=)
I64_BINARY_OP(ge, >=)
I64_BINARY_OP(le, <=)
