#ifndef X64_H
#define X64_H

#include "native.h"
#include "x64_util.h"
#include "x64_reg.h"
#include "x64_opcode.h"
#include "graph.h"
#include "elf.h"

#define X64_INST_ADD_CHECK(vec, inst)        \
    do {                                     \
        if (!(inst)) {                       \
            loge("\n");                      \
            return -ENOMEM;                  \
        }                                    \
        int ret = vector_add((vec), (inst)); \
        if (ret < 0) {                       \
            loge("\n");                      \
            free(inst);                      \
            return ret;                      \
        }                                    \
    } while (0)

#define X64_RELA_ADD_CHECK(vec, rela, c, v, f)                                   \
    do {                                                                         \
        if (rela) {                                                              \
            (rela)->code = (c);                                                  \
            (rela)->var = (v);                                                   \
            (rela)->func = (f);                                                  \
            (rela)->inst = (c)->instructions->data[(c)->instructions->size - 1]; \
            (rela)->addend = -4;                                                 \
            (rela)->type = R_X86_64_PC32;                                        \
            int ret = vector_add((vec), (rela));                                 \
            if (ret < 0) {                                                       \
                loge("\n");                                                      \
                free(rela);                                                      \
                return ret;                                                      \
            }                                                                    \
        }                                                                        \
    } while (0)

#define X64_PEEPHOLE_DEL 1
#define X64_PEEPHOLE_OK 0

typedef struct {
    function_t *f;

} x64_context_t;

typedef struct {
    dag_node_t *dag_node;

    register_t *reg;

    x64_OpCode_t *OpCode;

} x64_rcg_node_t;

typedef int (*x64_rcg_handler_pt)(native_t *ctx, _3ac_code_t * c, graph_t *g);
typedef int (*x64_inst_handler_pt)(native_t *ctx, _3ac_code_t * c);

x64_rcg_handler_pt x64_find_rcg_handler(const int op_type);
x64_inst_handler_pt x64_find_inst_handler(const int op_type);

int x64_rcg_find_node(graph_node_t **pp, graph_t *g, dag_node_t *dn, register_t *reg);
int _x64_rcg_make_node(graph_node_t **pp, graph_t *g, dag_node_t *dn, register_t *reg, x64_OpCode_t *OpCode);

int x64_open(native_t *ctx, const char *arch);
int x64_close(native_t *ctx);
int x64_select(native_t *ctx);

int x64_optimize_peephole(native_t *ctx, function_t *f);

int x64_graph_kcolor(graph_t *graph, int k, vector_t *colors);

intptr_t x64_bb_find_color(vector_t *dn_colors, dag_node_t *dn);
int x64_save_bb_colors(vector_t *dn_colors, bb_group_t *bbg, basic_block_t *bb);

int x64_bb_load_dn(intptr_t color, dag_node_t *dn, _3ac_code_t * c, basic_block_t *bb, function_t *f);
int x64_bb_save_dn(intptr_t color, dag_node_t *dn, _3ac_code_t * c, basic_block_t *bb, function_t *f);
int x64_bb_load_dn2(intptr_t color, dag_node_t *dn, basic_block_t *bb, function_t *f);
int x64_bb_save_dn2(intptr_t color, dag_node_t *dn, basic_block_t *bb, function_t *f);

int x64_fix_bb_colors(basic_block_t *bb, bb_group_t *bbg, function_t *f);
int x64_load_bb_colors(basic_block_t *bb, bb_group_t *bbg, function_t *f);
int x64_load_bb_colors2(basic_block_t *bb, bb_group_t *bbg, function_t *f);
int x64_init_bb_colors(basic_block_t *bb);

instruction_t *x64_make_inst(x64_OpCode_t *OpCode, int size);
instruction_t *x64_make_inst_G(x64_OpCode_t *OpCode, register_t *r);
instruction_t *x64_make_inst_E(x64_OpCode_t *OpCode, register_t *r);
instruction_t *x64_make_inst_I(x64_OpCode_t *OpCode, uint8_t *imm, int size);
void x64_make_inst_I2(instruction_t *inst, x64_OpCode_t *OpCode, uint8_t *imm, int size);

instruction_t *x64_make_inst_I2G(x64_OpCode_t *OpCode, register_t *r_dst, uint8_t *imm, int size);
instruction_t *x64_make_inst_I2E(x64_OpCode_t *OpCode, register_t *r_dst, uint8_t *imm, int size);

instruction_t *x64_make_inst_M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v, register_t *r_base);
instruction_t *x64_make_inst_I2M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v_dst, register_t *r_base, uint8_t *imm, int32_t size);
instruction_t *x64_make_inst_G2M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v_dst, register_t *r_base, register_t *r_src);
instruction_t *x64_make_inst_M2G(rela_t **prela, x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, variable_t *v_src);

instruction_t *x64_make_inst_G2E(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_src);
instruction_t *x64_make_inst_E2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_src);

instruction_t *x64_make_inst_P2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, int32_t offset);
instruction_t *x64_make_inst_G2P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, register_t *r_src);
instruction_t *x64_make_inst_I2P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, uint8_t *imm, int size);

instruction_t *x64_make_inst_SIB2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp);
instruction_t *x64_make_inst_G2SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, register_t *r_src);
instruction_t *x64_make_inst_I2SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, uint8_t *imm, int32_t size);

instruction_t *x64_make_inst_SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, int size);
instruction_t *x64_make_inst_P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, int size);

int x64_float_OpCode_type(int OpCode_type, int var_type);

int x64_shift(native_t *ctx, _3ac_code_t * c, int OpCode_type);

int x64_shift_assign(native_t *ctx, _3ac_code_t * c, int OpCode_type);

int x64_binary_assign(native_t *ctx, _3ac_code_t * c, int OpCode_type);

int x64_assign_dereference(native_t *ctx, _3ac_code_t * c);
int x64_assign_pointer(native_t *ctx, _3ac_code_t * c);
int x64_assign_array_index(native_t *ctx, _3ac_code_t * c);

int x64_inst_int_mul(dag_node_t *dst, dag_node_t *src, _3ac_code_t * c, function_t *f);
int x64_inst_int_div(dag_node_t *dst, dag_node_t *src, _3ac_code_t * c, function_t *f, int mod_flag);

int x64_inst_pointer(native_t *ctx, _3ac_code_t * c, int lea_flag);
int x64_inst_dereference(native_t *ctx, _3ac_code_t * c);

int x64_inst_float_cast(dag_node_t *dst, dag_node_t *src, _3ac_code_t * c, function_t *f);

int x64_inst_movx(dag_node_t *dst, dag_node_t *src, _3ac_code_t * c, function_t *f);

int x64_inst_op2(int OpCode_type, dag_node_t *dst, dag_node_t *src, _3ac_code_t * c, function_t *f);

int x64_inst_jmp(native_t *ctx, _3ac_code_t * c, int OpCode_type);

int x64_inst_teq(native_t *ctx, _3ac_code_t * c);
int x64_inst_cmp(native_t *ctx, _3ac_code_t * c);
int x64_inst_set(native_t *ctx, _3ac_code_t * c, int setcc_type);

int x64_inst_cmp_set(native_t *ctx, _3ac_code_t * c, int setcc_type);

#endif
