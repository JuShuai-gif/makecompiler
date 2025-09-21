#include "x64.h"

static instruction_t *_x64_make_OpCode(x64_OpCode_t *OpCode, int bytes,
                                       register_t *r,
                                       register_t *b,
                                       register_t *x) {
    instruction_t *inst = calloc(1, sizeof(instruction_t));
    if (!inst)
        return NULL;

    uint8_t prefix = 0;
    int i = 0;

    switch (OpCode->type) {
    case X64_MOVSD:
    case X64_ADDSD:
    case X64_SUBSD:
    case X64_MULSD:
    case X64_DIVSD:
    case X64_CVTSI2SD:
    case X64_CVTTSD2SI:
        inst->code[inst->len++] = OpCode->OpCodes[i++];
        break;
    default:
        break;
    };

    if (8 == bytes) {
        switch (OpCode->type) {
        case X64_MOVSD:
        case X64_ADDSD:
        case X64_SUBSD:
        case X64_MULSD:
        case X64_DIVSD:
        case X64_PXOR:
        case X64_UCOMISD:
        case X64_PUSH:
        case X64_POP:
        case X64_RET:
        case X64_CALL:
        case X64_CVTSS2SD:
            break;

        case X64_CVTSI2SD:
        case X64_CVTTSD2SI:
            if (!b || 4 != b->bytes)
                prefix |= X64_REX_INIT + X64_REX_W;
            break;
        default:
            prefix |= X64_REX_INIT + X64_REX_W;
            break;
        };
    } else if (2 == bytes) {
        inst->code[inst->len++] = 0x66;
    }

    if (r) {
        switch (X64_COLOR_ID(r->color)) {
        case X64_REG_ESI:
        case X64_REG_EDI:
            if (1 == r->bytes)
                prefix |= X64_REX_INIT;
            break;
        case X64_REG_R8:
        case X64_REG_R9:
        case X64_REG_R10:
        case X64_REG_R11:
        case X64_REG_R12:
        case X64_REG_R13:
        case X64_REG_R14:
        case X64_REG_R15:
            prefix |= X64_REX_INIT | X64_REX_R;
            break;
        default:
            break;
        };
    }

    if (b) {
        switch (X64_COLOR_ID(b->color)) {
        case X64_REG_ESI:
        case X64_REG_EDI:
            if (1 == b->bytes)
                prefix |= X64_REX_INIT;
            break;
        case X64_REG_R8:
        case X64_REG_R9:
        case X64_REG_R10:
        case X64_REG_R11:
        case X64_REG_R12:
        case X64_REG_R13:
        case X64_REG_R14:
        case X64_REG_R15:
            prefix |= X64_REX_INIT | X64_REX_B;
            break;
        default:
            break;
        };
    }

    if (x) {
        switch (X64_COLOR_ID(x->color)) {
        case X64_REG_R8:
        case X64_REG_R9:
        case X64_REG_R10:
        case X64_REG_R11:
        case X64_REG_R12:
        case X64_REG_R13:
        case X64_REG_R14:
        case X64_REG_R15:
            prefix |= X64_REX_INIT | X64_REX_X;
            break;
        default:
            break;
        };
    }

    if (prefix)
        inst->code[inst->len++] = prefix;

    for (; i < OpCode->nb_OpCodes; i++)
        inst->code[inst->len++] = OpCode->OpCodes[i];

    inst->OpCode = (OpCode_t *)OpCode;
    return inst;
}

static int _x64_make_disp(rela_t **prela, instruction_t *inst, uint32_t reg, uint32_t base, int32_t disp) {
    uint8_t ModRM = 0;
    ModRM_setReg(&ModRM, reg);

    // global var, use the offset to current instruction, such as 'mov 0(%rip), %rax'
    if (-1 == base) {
        ModRM_setRM(&ModRM, 0x5);
        ModRM_setMod(&ModRM, X64_MOD_BASE);
        inst->code[inst->len++] = ModRM;

        if (prela) {
            rela_t *rela = calloc(1, sizeof(rela_t));
            if (!rela)
                return -ENOMEM;

            rela->inst_offset = inst->len;
            *prela = rela;
        }

        uint8_t *p = (uint8_t *)&disp;
        int i;
        for (i = 0; i < 4; i++)
            inst->code[inst->len++] = p[i];
        return 0;
    }

    ModRM_setRM(&ModRM, base);

    if (X64_RM_EBP != base
        && X64_RM_ESP != base
        && X64_RM_R12 != base
        && X64_RM_R13 != base
        && 0 == disp) {
        ModRM_setMod(&ModRM, X64_MOD_BASE);
        inst->code[inst->len++] = ModRM;
        return 0;
    }

    if (disp <= 127 && disp >= -128) {
        ModRM_setMod(&ModRM, X64_MOD_BASE_DISP8);
        inst->code[inst->len++] = ModRM;
    } else {
        ModRM_setMod(&ModRM, X64_MOD_BASE_DISP32);
        inst->code[inst->len++] = ModRM;
    }

    if (X64_RM_ESP == base || X64_RM_R12 == base) {
        uint8_t SIB = 0;
        SIB_setBase(&SIB, base);
        SIB_setIndex(&SIB, base);
        SIB_setScale(&SIB, X64_SIB_SCALE1);
        inst->code[inst->len++] = SIB;
    }

    if (disp <= 127 && disp >= -128) {
        inst->code[inst->len++] = (int8_t)disp;
    } else {
        uint8_t *p = (uint8_t *)&disp;
        int i;
        for (i = 0; i < 4; i++)
            inst->code[inst->len++] = p[i];
    }
    return 0;
}

void x64_make_inst_I2(instruction_t *inst, x64_OpCode_t *OpCode, uint8_t *imm, int size) {
    inst->OpCode = (OpCode_t *)OpCode;
    inst->len = 0;

    int i;
    for (i = 0; i < OpCode->nb_OpCodes; i++)
        inst->code[inst->len++] = OpCode->OpCodes[i];

    for (i = 0; i < size; i++)
        inst->code[inst->len++] = imm[i];
}

instruction_t *x64_make_inst_I(x64_OpCode_t *OpCode, uint8_t *imm, int size) {
    instruction_t *inst = calloc(1, sizeof(instruction_t));
    if (!inst)
        return NULL;

    x64_make_inst_I2(inst, OpCode, imm, size);
    return inst;
}

instruction_t *x64_make_inst(x64_OpCode_t *OpCode, int size) {
    return _x64_make_OpCode(OpCode, size, NULL, NULL, NULL);
}

instruction_t *x64_make_inst_G(x64_OpCode_t *OpCode, register_t *r) {
    instruction_t *inst = _x64_make_OpCode(OpCode, r->bytes, NULL, r, NULL);
    if (!inst)
        return NULL;

    inst->OpCode = (OpCode_t *)OpCode;
    assert(1 == OpCode->nb_OpCodes);

    inst->code[inst->len - 1] += r->id & 0x7;

    if (X64_PUSH == OpCode->type)
        inst->src.base = r;
    else if (X64_POP == OpCode->type)
        inst->dst.base = r;

    return inst;
}

instruction_t *x64_make_inst_I2G(x64_OpCode_t *OpCode, register_t *r_dst, uint8_t *imm, int size) {
    instruction_t *inst = _x64_make_OpCode(OpCode, r_dst->bytes, NULL, r_dst, NULL);
    if (!inst)
        return NULL;

    assert(1 == OpCode->nb_OpCodes);
    inst->code[inst->len - 1] += r_dst->id & 0x7;

    uint8_t *p = (uint8_t *)&inst->src.imm;
    int i;

    inst->src.imm = 0;

    for (i = 0; i < size; i++) {
        inst->code[inst->len++] = imm[i];
        p[i] = imm[i];
    }

    inst->dst.base = r_dst;
    inst->src.imm_size = size;
    return inst;
}

instruction_t *x64_make_inst_E(x64_OpCode_t *OpCode, register_t *r) {
    instruction_t *inst = _x64_make_OpCode(OpCode, r->bytes, NULL, r, NULL);
    if (!inst)
        return NULL;

    uint8_t ModRM = 0;
    if (OpCode->ModRM_OpCode_used)
        ModRM_setReg(&ModRM, OpCode->ModRM_OpCode);

    ModRM_setRM(&ModRM, r->id);
    ModRM_setMod(&ModRM, X64_MOD_REGISTER);

    inst->code[inst->len++] = ModRM;

    if (X64_INC == OpCode->type || X64_INC == OpCode->type) {
        inst->src.base = r;
        inst->dst.base = r;

    } else if (X64_MUL == OpCode->type
               || X64_DIV == OpCode->type
               || X64_IMUL == OpCode->type
               || X64_IDIV == OpCode->type
               || X64_CALL == OpCode->type)
        inst->src.base = r;

    return inst;
}

instruction_t *x64_make_inst_I2E(x64_OpCode_t *OpCode, register_t *r_dst, uint8_t *imm, int size) {
    instruction_t *inst = x64_make_inst_E(OpCode, r_dst);
    if (!inst)
        return NULL;

    size = size > r_dst->bytes ? r_dst->bytes : size;

    uint8_t *p = (uint8_t *)&inst->src.imm;
    int i;

    inst->src.imm = 0;

    for (i = 0; i < size; i++) {
        inst->code[inst->len++] = imm[i];
        p[i] = imm[i];
    }

    inst->dst.base = r_dst;
    inst->src.imm_size = size;
    return inst;
}

instruction_t *x64_make_inst_M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v, register_t *r_base) {
    register_t *rbp = x64_find_register("rbp");

    uint32_t base;
    int32_t offset;
    uint8_t reg = 0;

    if (OpCode->ModRM_OpCode_used)
        reg = OpCode->ModRM_OpCode;

    if (!r_base) {
        if (v->local_flag || v->tmp_flag) {
            base = X64_REG_RBP;
            offset = v->bp_offset;
            r_base = rbp;

        } else if (v->global_flag) {
            base = -1;
            offset = 0;
        } else {
            loge("temp var should give a register\n");
            return NULL;
        }
    } else {
        base = r_base->id;

        if (v->local_flag || v->tmp_flag)
            offset = v->bp_offset;
        else
            offset = v->offset;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, v->size, NULL, r_base, NULL);
    if (!inst)
        return NULL;

    if (_x64_make_disp(prela, inst, reg, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    if (X64_INC == OpCode->type || X64_INC == OpCode->type) {
        inst->src.base = r_base;
        inst->src.disp = offset;
        inst->src.flag = 1;

        inst->dst.base = r_base;
        inst->dst.disp = offset;
        inst->dst.flag = 1;

    } else if (X64_MUL == OpCode->type
               || X64_DIV == OpCode->type
               || X64_IMUL == OpCode->type
               || X64_IDIV == OpCode->type
               || X64_CALL == OpCode->type) {
        inst->src.base = r_base;
        inst->src.disp = offset;
        inst->src.flag = 1;
    }

    return inst;
}

instruction_t *x64_make_inst_I2M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v_dst, register_t *r_base, uint8_t *imm, int32_t size) {
    register_t *rbp = x64_find_register("rbp");

    uint32_t base;
    int32_t offset;
    uint8_t reg = 0;

    if (OpCode->ModRM_OpCode_used)
        reg = OpCode->ModRM_OpCode;

    if (!r_base) {
        if (v_dst->local_flag || v_dst->tmp_flag) {
            base = X64_REG_RBP;
            offset = v_dst->bp_offset;
            r_base = rbp;

        } else if (v_dst->global_flag) {
            base = -1;
            offset = 0;
        } else {
            loge("temp var should give a register\n");
            return NULL;
        }
    } else {
        base = r_base->id;

        if (v_dst->local_flag || v_dst->tmp_flag)
            offset = v_dst->bp_offset;
        else
            offset = v_dst->offset;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, v_dst->size, NULL, r_base, NULL);
    if (!inst)
        return NULL;

    if (_x64_make_disp(prela, inst, reg, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    size = size > v_dst->size ? v_dst->size : size;

    uint8_t *p = (uint8_t *)&inst->src.imm;
    int i;

    inst->src.imm = 0;

    for (i = 0; i < size; i++) {
        inst->code[inst->len++] = imm[i];
        p[i] = imm[i];
    }

    inst->dst.base = r_base;
    inst->dst.disp = offset;
    inst->dst.flag = 1;

    inst->src.imm_size = size;
    return inst;
}

instruction_t *x64_make_inst_G2M(rela_t **prela, x64_OpCode_t *OpCode, variable_t *v_dst, register_t *r_base, register_t *r_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    register_t *rbp = x64_find_register("rbp");
    instruction_t *inst = NULL;

    uint32_t base;
    int32_t offset;

    if (!r_base) {
        if (v_dst->local_flag || v_dst->tmp_flag) {
            base = X64_REG_RBP;
            offset = v_dst->bp_offset;

            r_base = rbp;

        } else if (v_dst->global_flag) {
            base = -1;
            offset = 0;
        } else {
            loge("temp var should give a register\n");
            return NULL;
        }
    } else {
        base = r_base->id;

        if (v_dst->local_flag || v_dst->tmp_flag)
            offset = v_dst->bp_offset;
        else
            offset = v_dst->offset;
    }

    inst = _x64_make_OpCode(OpCode, x64_variable_size(v_dst), r_src, r_base, NULL);
    if (!inst)
        return NULL;

    if (_x64_make_disp(prela, inst, r_src->id, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    inst->src.base = r_src;
    inst->dst.base = r_base;
    inst->dst.disp = offset;
    inst->dst.flag = 1;

    return inst;
}

instruction_t *x64_make_inst_M2G(rela_t **prela, x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, variable_t *v_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    register_t *rbp = x64_find_register("rbp");
    instruction_t *inst = NULL;

    uint32_t base;
    int32_t offset;

    if (!r_base) {
        if (v_src->local_flag || v_src->tmp_flag) {
            base = X64_REG_RBP;
            offset = v_src->bp_offset;
            r_base = rbp;

        } else if (v_src->global_flag) {
            base = -1;
            offset = 0;
        } else {
            loge("temp var should give a register\n");
            return NULL;
        }
    } else {
        base = r_base->id;

        if (v_src->local_flag || v_src->tmp_flag)
            offset = v_src->bp_offset;
        else
            offset = v_src->offset;
    }

    inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_dst, r_base, NULL);
    if (!inst)
        return NULL;

    if (_x64_make_disp(prela, inst, r_dst->id, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    inst->dst.base = r_dst;
    inst->src.base = r_base;
    inst->src.disp = offset;
    inst->src.flag = 1;

    return inst;
}

instruction_t *x64_make_inst_P2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, int32_t offset) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = NULL;

    uint32_t base;

    if (r_base) {
        base = r_base->id;
        inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_dst, r_base, NULL);
    } else {
        base = -1;
        inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_dst, NULL, NULL);
    }

    if (!inst)
        return NULL;

    if (_x64_make_disp(NULL, inst, r_dst->id, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    inst->dst.base = r_dst;
    inst->src.base = r_base;
    inst->src.disp = offset;
    inst->src.flag = 1;

    return inst;
}

instruction_t *x64_make_inst_G2P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, register_t *r_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = NULL;

    uint32_t base;

    if (r_base) {
        base = r_base->id;
        inst = _x64_make_OpCode(OpCode, r_src->bytes, r_src, r_base, NULL);
    } else {
        base = -1;
        inst = _x64_make_OpCode(OpCode, r_src->bytes, r_src, NULL, NULL);
    }

    if (!inst)
        return NULL;

    if (_x64_make_disp(NULL, inst, r_src->id, base, offset) < 0) {
        free(inst);
        return NULL;
    }

    inst->src.base = r_src;
    inst->dst.base = r_base;
    inst->dst.disp = offset;
    inst->dst.flag = 1;

    return inst;
}

instruction_t *x64_make_inst_P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, int size) {
    uint8_t reg = 0;

    if (OpCode->ModRM_OpCode_used)
        reg = OpCode->ModRM_OpCode;

    instruction_t *inst = _x64_make_OpCode(OpCode, size, NULL, r_base, NULL);
    if (!inst)
        return NULL;

    if (_x64_make_disp(NULL, inst, reg, r_base->id, offset) < 0) {
        free(inst);
        return NULL;
    }

    return inst;
}

instruction_t *x64_make_inst_I2P(x64_OpCode_t *OpCode, register_t *r_base, int32_t offset, uint8_t *imm, int size) {
    instruction_t *inst = x64_make_inst_P(OpCode, r_base, offset, size);
    if (!inst)
        return NULL;

    uint8_t *p = (uint8_t *)&inst->src.imm;
    int i;

    inst->src.imm = 0;

    for (i = 0; i < size; i++) {
        inst->code[inst->len++] = imm[i];
        p[i] = imm[i];
    }

    inst->src.imm_size = size;

    inst->dst.base = r_base;
    inst->dst.disp = offset;
    inst->dst.flag = 1;

    return inst;
}

instruction_t *x64_make_inst_G2E(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_src, r_dst, NULL);
    if (!inst)
        return NULL;

    uint8_t ModRM = 0;
    ModRM_setReg(&ModRM, r_src->id);
    ModRM_setRM(&ModRM, r_dst->id);
    ModRM_setMod(&ModRM, X64_MOD_REGISTER);

    inst->code[inst->len++] = ModRM;

    inst->src.base = r_src;
    inst->dst.base = r_dst;

    return inst;
}

instruction_t *x64_make_inst_E2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_dst, r_src, NULL);
    if (!inst)
        return NULL;

    uint8_t ModRM = 0;
    ModRM_setReg(&ModRM, r_dst->id);
    ModRM_setRM(&ModRM, r_src->id);
    ModRM_setMod(&ModRM, X64_MOD_REGISTER);

    inst->code[inst->len++] = ModRM;

    inst->src.base = r_src;
    inst->dst.base = r_dst;

    return inst;
}

instruction_t *_x64_make_inst_SIB(instruction_t *inst, x64_OpCode_t *OpCode, uint32_t reg, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, int size) {
    uint8_t ModRM = 0;
    ModRM_setReg(&ModRM, reg);
    ModRM_setRM(&ModRM, X64_RM_SIB);

    if (X64_RM_EBP != r_base->id
        && X64_RM_ESP != r_base->id
        && X64_RM_R12 != r_base->id
        && X64_RM_R13 != r_base->id
        && 0 == disp)
        ModRM_setMod(&ModRM, X64_MOD_BASE);
    else {
        if (disp <= 127 && disp >= -128)
            ModRM_setMod(&ModRM, X64_MOD_BASE_DISP8);
        else
            ModRM_setMod(&ModRM, X64_MOD_BASE_DISP32);
    }
    inst->code[inst->len++] = ModRM;

    uint8_t SIB = 0;
    SIB_setBase(&SIB, r_base->id);
    SIB_setIndex(&SIB, r_index->id);
    switch (scale) {
    case 1:
        SIB_setScale(&SIB, X64_SIB_SCALE1);
        break;
    case 2:
        SIB_setScale(&SIB, X64_SIB_SCALE2);
        break;
    case 4:
        SIB_setScale(&SIB, X64_SIB_SCALE4);
        break;
    case 8:
        SIB_setScale(&SIB, X64_SIB_SCALE8);
        break;
    default:
        free(inst);
        return NULL;
        break;
    };
    inst->code[inst->len++] = SIB;

    if (X64_RM_EBP == r_base->id
        || X64_RM_ESP == r_base->id
        || X64_RM_R12 == r_base->id
        || X64_RM_R13 == r_base->id
        || 0 != disp) {
        if (disp <= 127 && disp >= -128)
            inst->code[inst->len++] = (int8_t)disp;
        else {
            uint8_t *p = (uint8_t *)&disp;
            int i;
            for (i = 0; i < 4; i++)
                inst->code[inst->len++] = p[i];
        }
    }
    return inst;
}

instruction_t *x64_make_inst_SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, int size) {
    instruction_t *inst = _x64_make_OpCode(OpCode, size, NULL, r_base, r_index);
    if (!inst)
        return NULL;

    uint32_t reg = 0;
    if (OpCode->ModRM_OpCode_used)
        reg = OpCode->ModRM_OpCode;

    return _x64_make_inst_SIB(inst, OpCode, reg, r_base, r_index, scale, disp, size);
}

instruction_t *x64_make_inst_SIB2G(x64_OpCode_t *OpCode, register_t *r_dst, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, r_dst->bytes, r_dst, r_base, r_index);
    if (!inst)
        return NULL;

    inst->dst.base = r_dst;

    inst->src.base = r_base;
    inst->src.index = r_index;
    inst->src.scale = scale;
    inst->src.disp = disp;
    inst->src.flag = 1;

    return _x64_make_inst_SIB(inst, OpCode, r_dst->id, r_base, r_index, scale, disp, r_dst->bytes);
}

instruction_t *x64_make_inst_G2SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, register_t *r_src) {
    if (OpCode->ModRM_OpCode_used) {
        loge("ModRM opcode invalid\n");
        return NULL;
    }

    instruction_t *inst = _x64_make_OpCode(OpCode, r_src->bytes, r_src, r_base, r_index);
    if (!inst)
        return NULL;

    inst->src.base = r_src;

    inst->dst.base = r_base;
    inst->dst.index = r_index;
    inst->dst.scale = scale;
    inst->dst.disp = disp;
    inst->dst.flag = 1;

    return _x64_make_inst_SIB(inst, OpCode, r_src->id, r_base, r_index, scale, disp, r_src->bytes);
}

instruction_t *x64_make_inst_I2SIB(x64_OpCode_t *OpCode, register_t *r_base, register_t *r_index, int32_t scale, int32_t disp, uint8_t *imm, int32_t size) {
    uint32_t reg = 0;
    if (OpCode->ModRM_OpCode_used)
        reg = OpCode->ModRM_OpCode;

    instruction_t *inst = _x64_make_OpCode(OpCode, size, NULL, r_base, r_index);
    if (!inst)
        return NULL;

    inst = _x64_make_inst_SIB(inst, OpCode, reg, r_base, r_index, scale, disp, size);
    if (!inst)
        return NULL;

    uint8_t *p = (uint8_t *)&inst->src.imm;
    int i;

    inst->src.imm = 0;

    for (i = 0; i < size; i++) {
        inst->code[inst->len++] = imm[i];
        p[i] = imm[i];
    }

    inst->src.imm_size = size;

    inst->dst.base = r_base;
    inst->dst.index = r_index;
    inst->dst.scale = scale;
    inst->dst.disp = disp;
    inst->dst.flag = 1;

    return inst;
}
