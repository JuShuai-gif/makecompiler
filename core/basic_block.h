#ifndef BASIC_BLOCK_H
#define BASIC_BLOCK_H

#include "core_types.h"
#include "utils_list.h"
#include "utils_vector.h"

typedef struct basic_block_s basic_block_t;

typedef struct bb_edge_s bb_edge_t;
typedef struct bb_group_s bb_group_t;

struct bb_edge_s {
    basic_block_t *start;
    basic_block_t *end;
};

struct bb_group_s {
    basic_block_t *entry;

    basic_block_t *pre;

    vector_t *posts;

    vector_t *entries;
    vector_t *exits;

    vector_t *body;

    bb_group_t *loop_parent;
    vector_t *loop_childs;
    int loop_layers;
};

struct basic_block_s {
    list_t list; // for function's basic block list

    list_t dag_list_head;

    list_t code_list_head;
    list_t save_list_head;

    vector_t *var_dag_nodes;

    vector_t *prevs; // prev basic blocks
    vector_t *nexts; // next basic blocks

    vector_t *dominators;
    int dfo;

    vector_t *entry_dn_delivery;
    vector_t *entry_dn_inactives;
    vector_t *entry_dn_actives;
    vector_t *exit_dn_actives;

    vector_t *dn_updateds;
    vector_t *dn_loads;
    vector_t *dn_saves;

    vector_t *dn_colors_entry;
    vector_t *dn_colors_exit;

    vector_t *dn_status_initeds;

    vector_t *dn_pointer_aliases;
    vector_t *entry_dn_aliases;
    vector_t *exit_dn_aliases;

    vector_t *dn_reloads;
    vector_t *dn_resaves;

    vector_t *ds_malloced;
    vector_t *ds_freed;

    void *ds_auto_gc;

#define EDA_FLAG_BITS 4
#define EDA_FLAG_ZERO 1
#define EDA_FLAG_SIGN 2
    Epin *flag_pins[EDA_FLAG_BITS];
    Epin *mask_pin;

    int code_bytes;
    int index;

    uint32_t call_flag : 1;
    uint32_t cmp_flag : 1;
    uint32_t jmp_flag : 1;
    uint32_t jcc_flag : 1;
    uint32_t ret_flag : 1;
    uint32_t vla_flag : 1;
    uint32_t end_flag : 1;
    uint32_t varg_flag : 1;
    uint32_t dump_flag : 1;
    uint32_t jmp_dst_flag : 1;

    uint32_t dereference_flag : 1;
    uint32_t array_index_flag : 1;

    uint32_t auto_ref_flag : 1;
    uint32_t auto_free_flag : 1;

    uint32_t back_flag : 1;
    uint32_t loop_flag : 1;
    uint32_t group_flag : 1;
    uint32_t visit_flag : 1;
    uint32_t native_flag : 1;

    basic_block_t *vla_free;
};

typedef int (*basic_block_bfs_pt)(basic_block_t *bb, void *data, vector_t *queue);
typedef int (*basic_block_dfs_pt)(basic_block_t *bb, void *data);

int basic_block_search_bfs(basic_block_t *root, basic_block_bfs_pt find, void *data);
int basic_block_search_dfs_prev(basic_block_t *root, basic_block_dfs_pt find, void *data, vector_t *results);

basic_block_t *basic_block_alloc();
basic_block_t *basic_block_jcc(basic_block_t *to, function_t *f, int jcc);
void basic_block_free(basic_block_t *bb);

bb_group_t *bb_group_alloc();
void bb_group_free(bb_group_t *bbg);
void bb_group_print(bb_group_t *bbg);
void bb_loop_print(bb_group_t *loop);

void basic_block_print(basic_block_t *bb, list_t *sentinel);
void basic_block_print_list(list_t *h);

int basic_block_vars(basic_block_t *bb, list_t *bb_list_head);
int basic_block_dag(basic_block_t *bb, list_t *dag_list_head);

int basic_block_active_vars(basic_block_t *bb);
int basic_block_inited_vars(basic_block_t *bb, list_t *bb_list_head);
int basic_block_loads_saves(basic_block_t *bb, list_t *bb_list_head);

int basic_block_connect(basic_block_t *prev_bb, basic_block_t *next_bb);

int basic_block_split(basic_block_t *bb_parent, basic_block_t **pbb_child);

int basic_block_inited_by3ac(basic_block_t *bb);

void basic_block_mov_code(basic_block_t *to, list_t *start, basic_block_t *from);
void basic_block_add_code(basic_block_t *bb, list_t *h);

bb_group_t *basic_block_find_min_loop(basic_block_t *bb, vector_t *loops);

static inline void basic_block_visit_flag(list_t *h, int visit_flag) {
    basic_block_t *bb;
    list_t *l;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        bb->visit_flag = visit_flag;
    }
}

#endif
