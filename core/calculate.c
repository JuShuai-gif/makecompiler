#include "calculate.h"

#include "calculate_i32.c"
#include "calculate_u32.c"
#include "calculate_i64.c"
#include "calculate_float.c"
#include "calculate_double.c"

calculate_t base_calculates[] =
    {
        // i32
        {"i32", OP_ADD, VAR_I32, VAR_I32, VAR_I32, i32_add},
        {"i32", OP_SUB, VAR_I32, VAR_I32, VAR_I32, i32_sub},
        {"i32", OP_MUL, VAR_I32, VAR_I32, VAR_I32, i32_mul},
        {"i32", OP_DIV, VAR_I32, VAR_I32, VAR_I32, i32_div},
        {"i32", OP_MOD, VAR_I32, VAR_I32, VAR_I32, i32_mod},

        {"i32", OP_SHL, VAR_I32, VAR_I32, VAR_I32, i32_shl},
        {"i32", OP_SHR, VAR_I32, VAR_I32, VAR_I32, i32_shr},

        {"i32", OP_INC, VAR_I32, VAR_I32, VAR_I32, i32_inc},
        {"i32", OP_DEC, VAR_I32, VAR_I32, VAR_I32, i32_dec},

        {"i32", OP_NEG, VAR_I32, VAR_I32, VAR_I32, i32_neg},

        {"i32", OP_BIT_AND, VAR_I32, VAR_I32, VAR_I32, i32_bit_and},
        {"i32", OP_BIT_OR, VAR_I32, VAR_I32, VAR_I32, i32_bit_or},
        {"i32", OP_BIT_NOT, VAR_I32, VAR_I32, VAR_I32, i32_bit_not},

        {"i32", OP_LOGIC_AND, VAR_I32, VAR_I32, VAR_I32, i32_logic_and},
        {"i32", OP_LOGIC_OR, VAR_I32, VAR_I32, VAR_I32, i32_logic_or},
        {"i32", OP_LOGIC_NOT, VAR_I32, VAR_I32, VAR_I32, i32_logic_not},

        {"i32", OP_GT, VAR_I32, VAR_I32, VAR_I32, i32_gt},
        {"i32", OP_LT, VAR_I32, VAR_I32, VAR_I32, i32_lt},
        {"i32", OP_EQ, VAR_I32, VAR_I32, VAR_I32, i32_eq},
        {"i32", OP_NE, VAR_I32, VAR_I32, VAR_I32, i32_ne},
        {"i32", OP_GE, VAR_I32, VAR_I32, VAR_I32, i32_ge},
        {"i32", OP_LE, VAR_I32, VAR_I32, VAR_I32, i32_le},

        // u32
        {"u32", OP_ADD, VAR_U32, VAR_U32, VAR_U32, u32_add},
        {"u32", OP_SUB, VAR_U32, VAR_U32, VAR_U32, u32_sub},
        {"u32", OP_MUL, VAR_U32, VAR_U32, VAR_U32, u32_mul},
        {"u32", OP_DIV, VAR_U32, VAR_U32, VAR_U32, u32_div},
        {"u32", OP_MOD, VAR_U32, VAR_U32, VAR_U32, u32_mod},

        {"u32", OP_SHL, VAR_U32, VAR_U32, VAR_U32, u32_shl},
        {"u32", OP_SHR, VAR_U32, VAR_U32, VAR_U32, u32_shr},

        {"u32", OP_INC, VAR_U32, VAR_U32, VAR_U32, u32_inc},
        {"u32", OP_DEC, VAR_U32, VAR_U32, VAR_U32, u32_dec},

        {"u32", OP_NEG, VAR_U32, VAR_U32, VAR_U32, u32_neg},

        {"u32", OP_BIT_AND, VAR_U32, VAR_U32, VAR_U32, u32_bit_and},
        {"u32", OP_BIT_OR, VAR_U32, VAR_U32, VAR_U32, u32_bit_or},
        {"u32", OP_BIT_NOT, VAR_U32, VAR_U32, VAR_U32, u32_bit_not},

        {"u32", OP_LOGIC_AND, VAR_U32, VAR_U32, VAR_U32, u32_logic_and},
        {"u32", OP_LOGIC_OR, VAR_U32, VAR_U32, VAR_U32, u32_logic_or},
        {"u32", OP_LOGIC_NOT, VAR_U32, VAR_U32, VAR_U32, u32_logic_not},

        {"u32", OP_GT, VAR_U32, VAR_U32, VAR_U32, u32_gt},
        {"u32", OP_LT, VAR_U32, VAR_U32, VAR_U32, u32_lt},
        {"u32", OP_EQ, VAR_U32, VAR_U32, VAR_U32, u32_eq},
        {"u32", OP_NE, VAR_U32, VAR_U32, VAR_U32, u32_ne},
        {"u32", OP_GE, VAR_U32, VAR_U32, VAR_U32, u32_ge},
        {"u32", OP_LE, VAR_U32, VAR_U32, VAR_U32, u32_le},

        // i64
        {"i64", OP_ADD, VAR_I64, VAR_I64, VAR_I64, i64_add},
        {"i64", OP_SUB, VAR_I64, VAR_I64, VAR_I64, i64_sub},
        {"i64", OP_MUL, VAR_I64, VAR_I64, VAR_I64, i64_mul},
        {"i64", OP_DIV, VAR_I64, VAR_I64, VAR_I64, i64_div},
        {"i64", OP_MOD, VAR_I64, VAR_I64, VAR_I64, i64_mod},

        {"i64", OP_SHL, VAR_I64, VAR_I64, VAR_I64, i64_shl},
        {"i64", OP_SHR, VAR_I64, VAR_I64, VAR_I64, i64_shr},

        {"i64", OP_INC, VAR_I64, VAR_I64, VAR_I64, i64_inc},
        {"i64", OP_DEC, VAR_I64, VAR_I64, VAR_I64, i64_dec},

        {"i64", OP_NEG, VAR_I64, VAR_I64, VAR_I64, i64_neg},

        {"i64", OP_BIT_AND, VAR_I64, VAR_I64, VAR_I64, i64_bit_and},
        {"i64", OP_BIT_OR, VAR_I64, VAR_I64, VAR_I64, i64_bit_or},
        {"i64", OP_BIT_NOT, VAR_I64, VAR_I64, VAR_I64, i64_bit_not},

        {"i64", OP_LOGIC_AND, VAR_I64, VAR_I64, VAR_I64, i64_logic_and},
        {"i64", OP_LOGIC_OR, VAR_I64, VAR_I64, VAR_I64, i64_logic_or},
        {"i64", OP_LOGIC_NOT, VAR_I64, VAR_I64, VAR_I64, i64_logic_not},

        {"i64", OP_GT, VAR_I64, VAR_I64, VAR_I64, i64_gt},
        {"i64", OP_LT, VAR_I64, VAR_I64, VAR_I64, i64_lt},

        {"i64", OP_EQ, VAR_I64, VAR_I64, VAR_I64, i64_eq},
        {"i64", OP_NE, VAR_I64, VAR_I64, VAR_I64, i64_ne},
        {"i64", OP_GE, VAR_I64, VAR_I64, VAR_I64, i64_ge},
        {"i64", OP_LE, VAR_I64, VAR_I64, VAR_I64, i64_le},

        {"float", OP_ADD, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_add},
        {"float", OP_SUB, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_sub},
        {"float", OP_MUL, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_mul},
        {"float", OP_DIV, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_div},

        {"float", OP_NEG, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_neg},

        {"float", OP_GT, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_gt},
        {"float", OP_LT, VAR_FLOAT, VAR_FLOAT, VAR_FLOAT, float_lt},

        {"double", OP_ADD, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_add},
        {"double", OP_SUB, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_sub},
        {"double", OP_MUL, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_mul},
        {"double", OP_DIV, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_div},

        {"double", OP_NEG, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_neg},

        {"double", OP_GT, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_gt},
        {"double", OP_LT, VAR_DOUBLE, VAR_DOUBLE, VAR_DOUBLE, double_lt},
};

calculate_t *find_base_calculate(int op_type, int src0_type, int src1_type) {
    int i;
    for (i = 0; i < sizeof(base_calculates) / sizeof(base_calculates[0]); i++) {
        calculate_t *cal = &base_calculates[i];

        if (op_type == cal->op_type
            && src0_type == cal->src0_type
            && src1_type == cal->src1_type) {
            return cal;
        }
    }

    return NULL;
}
