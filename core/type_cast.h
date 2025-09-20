#ifndef TYPE_CAST_H
#define TYPE_CAST_H

#include "ast.h"

typedef struct {
    const char *name;

    int src_type;
    int dst_type;

    int (*func)(ast_t *ast, variable_t **pret, variable_t *src);

} type_cast_t;

int type_cast_check(ast_t *ast, variable_t *dst, variable_t *src);

type_cast_t *find_base_type_cast(int src_type, int dst_type);

int find_updated_type(ast_t *ast, variable_t *v0, variable_t *v1);

int cast_to_i8(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_i16(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_i32(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_i64(ast_t *ast, variable_t **pret, variable_t *src);

int cast_to_u8(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_u16(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_u32(ast_t *ast, variable_t **pret, variable_t *src);
int cast_to_u64(ast_t *ast, variable_t **pret, variable_t *src);

int cast_to_float(ast_t *ast, variable_t **pret, variable_t *src);

int cast_to_double(ast_t *ast, variable_t **pret, variable_t *src);

#endif