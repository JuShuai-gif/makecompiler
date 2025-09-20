#include "calculate.h"

#define U32_BINARY_OP(name, op)                                                         \
    int u32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_U32);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.u32 = src0->data.u32 op src1->data.u32;                                 \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define U32_UNARY_OP(name, op)                                                          \
    int u32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        type_t *t = block_find_type_type(ast->current_block, VAR_U32);                  \
        variable_t *r = VAR_ALLOC_BY_TYPE(NULL, t, 0, 0, NULL);                         \
        if (!r)                                                                         \
            return -ENOMEM;                                                             \
        r->data.u32 = op src0->data.u32;                                                \
        if (pret)                                                                       \
            *pret = r;                                                                  \
        return 0;                                                                       \
    }

#define U32_UPDATE_OP(name, op)                                                         \
    int u32_##name(ast_t *ast, variable_t **pret, variable_t *src0, variable_t *src1) { \
        op src0->data.u32;                                                              \
        if (pret)                                                                       \
            *pret = variable_ref(src0);                                                 \
        return 0;                                                                       \
    }

U32_BINARY_OP(add, +)
U32_BINARY_OP(sub, -)
U32_BINARY_OP(mul, *)
U32_BINARY_OP(div, /)
U32_BINARY_OP(mod, %)

U32_BINARY_OP(shl, <<)
U32_BINARY_OP(shr, >>)

U32_UPDATE_OP(inc, ++);
U32_UPDATE_OP(dec, --);

U32_UNARY_OP(neg, -)

U32_BINARY_OP(bit_and, &)
U32_BINARY_OP(bit_or, |)
U32_UNARY_OP(bit_not, ~)

U32_BINARY_OP(logic_and, &&)
U32_BINARY_OP(logic_or, ||)
U32_UNARY_OP(logic_not, !)

U32_BINARY_OP(gt, >)
U32_BINARY_OP(lt, <)
U32_BINARY_OP(eq, ==)
U32_BINARY_OP(ne, !=)
U32_BINARY_OP(ge, >=)
U32_BINARY_OP(le, <=)
