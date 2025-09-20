#ifndef OPTIMIZER_H
#define OPTIMIZER_h

#include "ast.h"
#include "basic_block.h"
#include "3ac.h"

typedef struct optimizer_s optimizer_t;

#define OPTIMIZER_LOCAL 0
#define OPTIMIZER_GLOBAL 1

struct optimizer_s {
    const char *name;
    int (*optimize)(ast_t *ast, function_t *f, vector_t *functions);

    uint32_t flags;
};

int bbg_find_entry_exit(bb_group_t *bbg);
void loops_print(vector_t *loops);

int optimize(ast_t *ast, vector_t *functions);

#endif