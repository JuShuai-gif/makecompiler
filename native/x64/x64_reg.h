#ifndef X64_REG_H
#define X64_REG_H

#include "native.h"
#include "x64_util.h"
#include "variable.h"

#define X64_COLOR(type, id, mask) ((type) << 24 | (id) << 16 | (mask))
#define X64_COLOR_TYPE(c) ((c) >> 24)
#define X64_COLOR_ID(c) (((c) >> 16) & 0xff)
#define X64_COLOR_MASK(c) ((c) & 0xffff)
#define X64_COLOR_CONFLICT(c0, c1) ((c0) >> 16 == (c1) >> 16 && (c0) & (c1) & 0xffff)

#define X64_COLOR_BYTES(c)             \
    ({                                 \
        int n = 0;                     \
        intptr_t minor = (c) & 0xffff; \
        while (minor) {                \
            minor &= minor - 1;        \
            n++;                       \
        }                              \
        n;                             \
    })

#define X64_SELECT_REG_CHECK(pr, dn, c, f, load_flag)      \
    do {                                                   \
        int ret = x64_select_reg(pr, dn, c, f, load_flag); \
        if (ret < 0) {                                     \
            loge("\n");                                    \
            return ret;                                    \
        }                                                  \
        assert(dn->color > 0);                             \
    } while (0)

// ABI:rdi rsi rdx rcx r8 r9
static uint32_t x64_abi_regs[] = {
    X64_REG_RDI,
    X64_REG_RSI,
    X64_REG_RDX,
    X64_REG_RCX,
    X64_REG_R8,
    X64_REG_R9,
};

#define X64_ABI_NB (sizeof(x64_abi_regs) / sizeof(x64_abi_regs[0]))

static uint32_t x64_abi_float_regs[] = {
    X64_REG_XMM0,
    X64_REG_XMM1,
    X64_REG_XMM2,
    X64_REG_XMM3,
    X64_REG_XMM4,
    X64_REG_XMM5,
    X64_REG_XMM6,
    X64_REG_XMM7,
};
#define X64_ABI_FLOAT_NB (sizeof(x64_abi_float_regs) / sizeof(x64_abi_float_regs[0]))

static uint32_t x64_abi_ret_regs[] = {
    X64_REG_RAX,
    X64_REG_RDI,
    X64_REG_RSI,
    X64_REG_RDX,
};
#define X64_ABI_RET_NB (sizeof(x64_abi_ret_regs) / sizeof(x64_abi_ret_regs[0]))

static const char *x64_abi_caller_saves[] = {
    "rax",
    "rcx",
    "rdx",
    "rsi",
    "rdi",
    "r8",
    "r9",
    "r10",
    "r11",

    "xmm0",
    "xmm1",
    "xmm2",
    "xmm3",
    "xmm4",
    "xmm5",
    "xmm6",
    "xmm7",
};
#define X64_ABI_CALLER_SAVES_NB (sizeof(x64_abi_caller_saves) / sizeof(x64_abi_caller_saves[0]))

static const char *x64_abi_callee_saves[] = {
    "rbx",
    "r12",
    "r13",
    "r14",
    "r15",
};
#define X64_ABI_CALLEE_SAVES_NB (sizeof(x64_abi_callee_saves) / sizeof(x64_abi_callee_saves[0]))

typedef struct
{
    register_t *base;
    register_t *index;

    int32_t scale;
    int32_t disp;
    int32_t size;
} x64_sib_t;

static inline int x64_variable_size(variable_t *v) {
    if (v->nb_dimentions > 0)
        return 8;

    if (v->type >= STRUCT && 0 == v->nb_pointers)
        return 8;

    return v->size;
}

typedef iint (*x64_sib_fill_pt)(x64_sib_t *sib, dag_node_t *base, dag_node_t *index, _3ac_code_t *c, function_t *f);

int x64_registers_init();
int x64_registers_reset();
int x64_registers_clear();
int x64_registers_print();
vector_t *x64_register_colors();

register_t *x64_find_register(const char *name);

register_t *x64_find_register_type_id_bytes(uint32_t type, uint32_t id, int bytes);

register_t *x64_find_register_color(intptr_t color);

register_t *x64_find_register_color_bytes(intptr_t color, int bytes);

register_t *x64_find_abi_register(int index, int bytes);

register_t *x64_select_overflowed_reg(dag_node_t *dn, _3ac_code_t *c);

int x64_reg_cached_vars(register_t *r);

int x64_save_var(dag_node_t *dn, _3ac_code_t *c, function_t *f);

int x64_save_var2(dag_node_t *dn, register_t *r, _3ac_code_t *c, function_t *f);

int x64_push_regs(vector_t *instructions, uint32_t *regs, int nb_regs);
int x64_pop_regs(vector_t *instructions, register_t **regs, int nb_regs, register_t **updated_regs, int nb_updated);

int x64_caller_save_regs(_3ac_code_t *c, const char *regs[], int nb_regs, int stack_size, register_t **saved_regs);

int x64_push_callee_regs(_3ac_code_t *c, function_t *f);
int x64_pop_callee_regs(_3ac_code_t *c, function_t *f);

int x64_save_reg(register_t *r, _3ac_code_t *c, function_t *f);

int x64_load_const(register_t *r, dag_node_t *dn, _3ac_code_t *c, function_t *f);
int x64_load_reg(register_t *r, dag_node_t *dn, _3ac_code_t *c, function_t *f);
int x64_reg_used(register_t *r, dag_node_t *dn);

int x64_overflow_reg(register_t *r, _3ac_code_t *c, function_t *f);
int x64_overflow_reg2(register_t *r, dag_node_t *dn, _3ac_code_t *c, function_t *f);

int x64_select_reg(register_t **preg, dag_node_t *dn, _3ac_code_t *c, function_t *f, int load_flag);

int x64_dereference_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *member, _3ac_code_t *c, function_t *f);

int x64_pointer_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *member, _3ac_code_t *c, function_t *f);

int x64_array_index_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *index, dag_node_t *scale, _3ac_code_t *c, function_t *f);

void x64_call_rabi(int *p_nints, int *p_nfloats, _3ac_code_t *c);

#endif
