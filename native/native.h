#ifndef NAIVE_H
#define NAIVE_H

#include "3ac.h"
#include "parse.h"

typedef struct native_ops_s native_ops_t;

struct register_s {
    uint32_t id;
    int bytes;
    char *name;

    intptr_t color;

    vector_t *dag_nodes;

    uint32_t updated;
    uint32_t used;
};

#define COLOR_CONFLICT(c0, c1) ((c0) >> 16 == (c1) >> 16 && (c0) & (c1) & 0xffff)

struct OpCode_s {
    int type;
    char *name;
};

typedef struct
{
    register_t *base;
    register_t *index;

    int32_t scale;
    int32_t disp;
    int32_t size;
} sib_t;

typedef struct
{
    register_t *base;
    register_t *index;
    int scale;
    int disp;

    uint64_t imm;
    int imm_size;

    uint8_t flag;
} inst_data_t;

typedef struct
{
    _3ac_code_t *c;

    OpCode_t *OpCode;

    inst_data_t src;
    inst_data_t dst;

    uint8_t code[32];
    int len;
    int nb_used;
} instruction_t;

typedef struct
{
    _3ac_code_t *code;
    function_t *func;
    variable_t *var;
    string_t *name;

    instruction_t *inst;
    int inst_offset;
    int64_t text_offset;
    uint64_t type;
    int addend;
} rela_t;

typedef struct
{
    native_ops_t *ops;

    inst_ops_t *iops;
    regs_ops_t *rops;

    void *priv;

} native_t;

struct native_ops_s {
    const char *name;
    int (*open)(native_t *ctx, const char *arch);
    int (*close)(native_t *ctx);

    int (*select_inst)(native_t *ctx, function_t *f);
};

struct regs_ops_s {
    const char *name;

    uint32_t *abi_regs;
    uint32_t *abi_float_regs;
    uint32_t *abi_double_regs;
    uint32_t *abi_ret_regs;
    uint32_t *abi_caller_saves;
    uint32_t *abi_callee_saves;

    const int ABI_NB;
    const int ABI_FLOAT_NB;
    const int ABI_DOUBLE_NB;
    const int ABI_RET_NB;
    const int ABI_CALLER_SAVES_NB;
    const int ABI_CALLEE_SAVES_NB;

    const int MAX_BYTES :

        int (*register_init)();
    int (*register_reset)();
    void (*register_clear)();
    vector_t *(*register_colors)();

    int (*color_conflict)(intptr_t color0, intptr_t color1);

    void (*argv_rabi)(function_t *f);
    void (*call_rabi)(_3ac_code_t *c, function_t *f, int *p_nints, int *p_nfloats, int *p_ndoubles);
    void (*call_rabi_varg)(_3ac_code_t *c, function_t *f);

    int (*reg_used)(_register_t *r, dag_node_t *dn);
    int (*reg_cached_vars)(_register_t *r);

    int (*variable_size)(_variable_t *v);

    int (*caller_save_regs)(_3ac_code_t *c, function_t *f, uint32_t *regs, int nb_regs, int stack_size, register_t **saved_regs);
    int (*pop_regs)(_3ac_code_t *c, function_t *f, register_t **regs, int nb_regs, register_t **updated_regs, int nb_updated);

    register_t *(*find_register)(const char *name);
    register_t *(*find_register_color)(intptr_t color);
    register_t *(*find_register_color_bytes)(intptr_t color, int bytes);
    register_t *(*find_register_type_id_bytes)(uint32_t type, uint32_t id, int bytes);

    register_t *(*select_overflowed_reg)(dag_node_t *dn, _3ac_code_t *c, int is_float);

    int (*overflow_reg)(register_t *r, _3ac_code_t *c, function_t *f);
    int (*overflow_reg2)(register_t *r, _dag_node_t *dn, _3ac_code_t *c, function_t *f);
    int (*overflow_reg3)(register_t *r, _dag_node_t *dn, _3ac_code_t *c, function_t *f);

    int (*push_callee_regs)(_3ac_code_t *c, function_t *f);
    int (*pop_callee_regs)(_3ac_code_t *c, function_t *f);
};

struct inst_ops_s {
    const char *name;

    int (*BL)(_3ac_code_t *c, function_t *f, function_t *pf);
    instruction_t *(*BLR)(_3ac_code_t *c, register_t *r);
    instruction_t *(*PUSH)(_3ac_code_t *c, register_t *r);
    instruction_t *(*POP)(_3ac_code_t *c, register_t *r);
    instruction_t *(*TEQ)(_3ac_code_t *c, register_t *rs);
    instruction_t *(*NEG)(_3ac_code_t *c, register_t *rd, register_t *rs);

    instruction_t *(*MOVZX)(_3ac_code_t *c, register_t *rd, register_t *rs, int size);
    instruction_t *(*MOVSX)(_3ac_code_t *c, register_t *rd, register_t *rs, int size);
    instruction_t *(*MVN)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*MOV_G)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*MOV_SP)(_3ac_code_t *c, register_t *rd, register_t *rs);

    instruction_t *(*ADD_G)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*SUB_G)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*CMP_G)(_3ac_code_t *c, register_t *rs0, register_t *rs1);
    instruction_t *(*AND_G)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*OR_G)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);

    instruction_t *(*ADD_IMM)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rs, uint64_t imm);
    instruction_t *(*SUB_IMM)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rs, uint64_t imm);
    instruction_t *(*CMP_IMM)(_3ac_code_t *c, function_t *f, register_t *rs, uint64_t imm);

    instruction_t *(*MUL)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*DIV)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*SDIV)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*MSUB)(_3ac_code_t *c, register_t *rd, register_t *rm, register_t *rn, register_t *ra);

    instruction_t *(*SHL)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*SHR)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*ASR)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);

    instruction_t *(*CVTSS2SD)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*CVTSD2SS)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*CVTF2SI)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*CVTF2UI)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*CVTSI2F)(_3ac_code_t *c, register_t *rd, register_t *rs);
    instruction_t *(*CVTUI2F)(_3ac_code_t *c, register_t *rd, register_t *rs);

    instruction_t *(*FCMP)(_3ac_code_t *c, register_t *rs0, register_t *rs1);
    instruction_t *(*FADD)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*FSUB)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*FMUL)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*FDIV)(_3ac_code_t *c, register_t *rd, register_t *rs0, register_t *rs1);
    instruction_t *(*FMOV_G)(_3ac_code_t *c, register_t *rd, register_t *rs);

    instruction_t *(*JA)(_3ac_code_t *c);
    instruction_t *(*JB)(_3ac_code_t *c);
    instruction_t *(*JZ)(_3ac_code_t *c);
    instruction_t *(*JNZ)(_3ac_code_t *c);
    instruction_t *(*JGT)(_3ac_code_t *c);
    instruction_t *(*JGE)(_3ac_code_t *c);
    instruction_t *(*JLT)(_3ac_code_t *c);
    instruction_t *(*JLE)(_3ac_code_t *c);
    instruction_t *(*JMP)(_3ac_code_t *c);
    instruction_t *(*JAE)(_3ac_code_t *c);
    instruction_t *(*JBE)(_3ac_code_t *c);
    instruction_t *(*RET)(_3ac_code_t *c);

    instruction_t *(*SETZ)(_3ac_code_t *c, register_t *rd);
    instruction_t *(*SETNZ)(_3ac_code_t *c, register_t *rd);
    instruction_t *(*SETGT)(_3ac_code_t *c, register_t *rd);
    instruction_t *(*SETGE)(_3ac_code_t *c, register_t *rd);
    instruction_t *(*SETLT)(_3ac_code_t *c, register_t *rd);
    instruction_t *(*SETLE)(_3ac_code_t *c, register_t *rd);

    int (*I2G)(_3ac_code_t *c, register_t *rd, uint64_t imm, int bytes);
    int (*M2G)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rb, variable_t *vs);
    int (*M2GF)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rb, variable_t *vs);
    int (*G2M)(_3ac_code_t *c, function_t *f, register_t *rs, register_t *rb, variable_t *vs);
    int (*G2P)(_3ac_code_t *c, function_t *f, register_t *rs, register_t *rb, int32_t offset, int size);
    int (*P2G)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rb, int32_t offset, int size);
    int (*ISTR2G)(_3ac_code_t *c, function_t *f, register_t *rd, variable_t *vs);
    int (*SIB2G)(_3ac_code_t *c, function_t *f, register_t *rd, sib_t *sib);
    int (*G2SIB)(_3ac_code_t *c, function_t *f, register_t *rd, sib_t *sib);
    int (*ADR2G)(_3ac_code_t *c, function_t *f, register_t *rd, variable_t *vs);
    int (*ADRP2G)(_3ac_code_t *c, function_t *f, register_t *rd, register_t *rb, int32_t offset);
    int (*ADRSIB2G)(_3ac_code_t *c, function_t *f, register_t *rd, sib_t *sib);

    int (*cmp_update)(_3ac_code_t *c, function_t *f, instruction_t *inst);
    int (*set_rel_veneer)(function_t *f);

    void (*set_jmp_offset)(instruction_t *inst, int32_t bytes);
};

static inline int inst_data_same(inst_data_t *id0, inst_data_t *id1) {
    if ((id0->flag && !id0->base) || (id1->flag && !id1->base))
        return 0;

    if (id0->scale == id1->scale
        && id0->disp == id1->disp
        && id0->flag == id1->flag
        && id0->imm == id1->imm
        && id0->imm_size == id1->imm_size) {
        if (id0->base == id1->base
            || (id0->base && id1->base && COLOR_CONFLICT(id0->base->color, id1->base->color))) {
            if (id0->index == id1->index
                || (id0->index && id1->index && COLOR_CONFLICT(id0->index->color, id1->index->color)))
                return 1;
        }
    }
    return 0;
}

void instruction_print(instruction_t *inst);

int native_open(native_t **pctx, const char *name);
int native_close(native_t *ctx);

int native_select_inst(native_t *ctx, function_t *f);

#endif