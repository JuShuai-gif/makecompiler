#include "type_cast.h"

int cast_to_i64(ast_t *ast, variable_t **pret, variable_t *src) {
    if (!pret || !src)
        return -EINVAL;

    type_t *t = block_find_type_type(ast->current_block, VAR_I64);

    variable_t *r = VAR_ALLOC_BY_TYPE(src->w, t, 0, 0, NULL);

    switch (src->type) {
    case VAR_FLOAT:
        r->data.i64 = (int64_t)src->data.f;
        break;

    case VAR_DOUBLE:
        r->data.i64 = (int64_t)src->data.d;
        break;

    case VAR_CHAR:
    case VAR_I8:
    case VAR_I16:
    case VAR_I32:
    case VAR_U8:
    case VAR_U16:
        r->data.i64 = src->data.i;
        break;

    case VAR_U32:
        r->data.i64 = src->data.u32;
        break;

    case VAR_I64:
        r->data.i64 = src->data.i64;
        break;

    case VAR_U64:
        r->data.i64 = src->data.u64;
        break;

    default:
        if (src->nb_pointers > 0)
            r->data.i64 = src->data.i64;
        else {
            variable_free(r);
            return -EINVAL;
        }
        break;
    };

    *pret = r;
    return 0;
}