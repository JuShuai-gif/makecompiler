#include "type_cast.h"

static int type_update[] = {
    VAR_I8,
    VAR_CHAR,
    VAR_BIT,
    VAR_U8,

    VAR_I16,
    VAR_U16,

    VAR_INT,
    VAR_I32,
    VAR_U32,

    VAR_I64,
    VAR_INTPTR,

    VAR_U64,
    VAR_UINTPTR,

    VAR_FLOAT,
    VAR_DOUBLE,
};

static type_cast_t base_type_casts[] =
    {
        {"char", -1, VAR_CHAR, cast_to_u8},
        {"bit", -1, VAR_BIT, cast_to_u8},
        {"u8", -1, VAR_U8, cast_to_u8},
        {"u16", -1, VAR_U16, cast_to_u16},
        {"u32", -1, VAR_U32, cast_to_u32},
        {"u64", -1, VAR_U64, cast_to_u64},
        {"uintptr", -1, VAR_UINTPTR, cast_to_u64},

        {"i8", -1, VAR_I8, cast_to_i8},
        {"i16", -1, VAR_I16, cast_to_i16},
        {"i32", -1, VAR_I32, cast_to_i32},
        {"i64", -1, VAR_I64, cast_to_i64},
        //	{"intptr",  -1,   VAR_INTPTR,    cast_to_i64},

        {"float", -1, VAR_FLOAT, cast_to_float},
        //	{"double",  -1,   VAR_DOUBLE,    cast_to_double},

};

int find_updated_type(ast_t *ast, variable_t *v0, variable_t *v1) {
    if (v0->nb_pointers > 0 || v1->nb_pointers > 0)
        return VAR_UINTPTR;

    int n = sizeof(type_update) / sizeof(type_update[0]);

    int index0 = -1;
    int index1 = -1;

    int i;
    for (i = 0; i < n; i++) {
        if (v0->type == type_update[i])
            index0 = i;

        if (v1->type == type_update[i])
            index1 = i;

        if (index0 >= 0 && index1 >= 0)
            break;
    }

    if (index0 < 0 || index1 < 0) {
        loge("type update not found for type: %d, %d\n",
             v0->type, v1->type);
        return -1;
    }

    if (index0 > index1)
        return type_update[index0];

    return type_update[index1];
}

type_cast_t *find_base_type_cast(int src_type, int dst_type) {
    int i;
    for (i = 0; i < sizeof(base_type_casts) / sizeof(base_type_casts[0]); i++) {
        type_cast_t *cast = &base_type_casts[i];

        if (dst_type == cast->dst_type)
            return cast;
    }

    return NULL;
}

int type_cast_check(ast_t *ast, variable_t *dst, variable_t *src) {
    if (!dst->const_flag && src->const_flag && !src->const_literal_flag) {
        logw("type cast %s -> %s discard 'const'\n", src->w->text->data, dst->w->text->data);
    }

    string_t *dst_type = NULL;
    string_t *src_type = NULL;

    int dst_nb_pointers = dst->nb_pointers + dst->nb_dimentions;
    int src_nb_pointers = src->nb_pointers + src->nb_dimentions;

    if (dst_nb_pointers > 0) {
        if (0 == src_nb_pointers) {
            if (VAR_INTPTR == src->type || VAR_UINTPTR == src->type
                || VAR_U64 == src->type)
                return 0;
            goto failed;
        }

        if (VAR_VOID == src->type || VAR_VOID == dst->type)
            return 0;

        if (dst_nb_pointers != src_nb_pointers)
            goto failed;

        if (type_is_integer(src->type) && type_is_integer(dst->type)) {
            type_t *t0 = NULL;
            type_t *t1 = NULL;

            int ret = ast_find_type_type(&t0, ast, src->type);
            if (ret < 0)
                goto failed;

            ret = ast_find_type_type(&t1, ast, dst->type);
            if (ret < 0)
                goto failed;

            if (t0->size != t1->size)
                goto failed;

        } else if (src->type != dst->type)
            goto failed;

        return 0;
    }

    if (src_nb_pointers > 0) {
        if (VAR_INTPTR == dst->type || VAR_UINTPTR == dst->type)
            return 0;

        goto failed;
    }

    if (type_is_integer(src->type)) {
        if (VAR_FLOAT <= dst->type && VAR_DOUBLE >= dst->type)
            return 0;

        if (type_is_integer(dst->type)) {
            if (dst->size < src->size)
                logw("type cast %s -> %s discard bits, file: %s, line: %d\n",
                     src->w->text->data, dst->w->text->data, src->w->file->data, src->w->line);
            return 0;
        }
    }

    if (VAR_FLOAT <= src->type && VAR_DOUBLE >= src->type) {
        if (VAR_FLOAT <= dst->type && VAR_DOUBLE >= dst->type)
            return 0;

        if (type_is_integer(dst->type))
            return 0;
    }

failed:
    dst_type = variable_type_name(ast, dst);
    src_type = variable_type_name(ast, src);

    loge("type cast '%s -> %s' with different type: from '%s' to '%s', file: %s, line: %d\n",
         src->w->text->data, dst->w->text->data,
         src_type->data, dst_type->data, src->w->file->data, src->w->line);

    string_free(dst_type);
    string_free(src_type);
    return -1;
}
