#include "x64.h"

register_t x64_registers[] = {
    {0, 1, "al", X64_COLOR(0, 0, 0x1), NULL, 0},
    {0, 2, "ax", X64_COLOR(0, 0, 0x3), NULL, 0},
    {0, 4, "eax", X64_COLOR(0, 0, 0xf), NULL, 0},
    {0, 8, "rax", X64_COLOR(0, 0, 0xff), NULL, 0},

    {1, 1, "cl", X64_COLOR(0, 1, 0x1), NULL, 0},
    {1, 2, "cx", X64_COLOR(0, 1, 0x3), NULL, 0},
    {1, 4, "ecx", X64_COLOR(0, 1, 0xf), NULL, 0},
    {1, 8, "rcx", X64_COLOR(0, 1, 0xff), NULL, 0},

    {2, 1, "dl", X64_COLOR(0, 2, 0x1), NULL, 0},
    {2, 2, "dx", X64_COLOR(0, 2, 0x3), NULL, 0},
    {2, 4, "edx", X64_COLOR(0, 2, 0xf), NULL, 0},
    {2, 8, "rdx", X64_COLOR(0, 2, 0xff), NULL, 0},

    {3, 1, "bl", X64_COLOR(0, 3, 0x1), NULL, 0},
    {3, 2, "bx", X64_COLOR(0, 3, 0x3), NULL, 0},
    {3, 4, "ebx", X64_COLOR(0, 3, 0xf), NULL, 0},
    {3, 8, "rbx", X64_COLOR(0, 3, 0xff), NULL, 0},

    {4, 2, "sp", X64_COLOR(0, 4, 0x3), NULL, 0},
    {4, 4, "esp", X64_COLOR(0, 4, 0xf), NULL, 0},
    {4, 8, "rsp", X64_COLOR(0, 4, 0xff), NULL, 0},

    {5, 2, "bp", X64_COLOR(0, 5, 0x3), NULL, 0},
    {5, 4, "ebp", X64_COLOR(0, 5, 0xf), NULL, 0},
    {5, 8, "rbp", X64_COLOR(0, 5, 0xff), NULL, 0},

    {6, 1, "sil", X64_COLOR(0, 6, 0x1), NULL, 0},
    {6, 2, "si", X64_COLOR(0, 6, 0x3), NULL, 0},
    {6, 4, "esi", X64_COLOR(0, 6, 0xf), NULL, 0},
    {6, 8, "rsi", X64_COLOR(0, 6, 0xff), NULL, 0},

    {7, 1, "dil", X64_COLOR(0, 7, 0x1), NULL, 0},
    {7, 2, "di", X64_COLOR(0, 7, 0x3), NULL, 0},
    {7, 4, "edi", X64_COLOR(0, 7, 0xf), NULL, 0},
    {7, 8, "rdi", X64_COLOR(0, 7, 0xff), NULL, 0},

    {8, 1, "r8b", X64_COLOR(0, 8, 0x1), NULL, 0},
    {8, 2, "r8w", X64_COLOR(0, 8, 0x3), NULL, 0},
    {8, 4, "r8d", X64_COLOR(0, 8, 0xf), NULL, 0},
    {8, 8, "r8", X64_COLOR(0, 8, 0xff), NULL, 0},

    {9, 1, "r9b", X64_COLOR(0, 9, 0x1), NULL, 0},
    {9, 2, "r9w", X64_COLOR(0, 9, 0x3), NULL, 0},
    {9, 4, "r9d", X64_COLOR(0, 9, 0xf), NULL, 0},
    {9, 8, "r9", X64_COLOR(0, 9, 0xff), NULL, 0},

    {10, 1, "r10b", X64_COLOR(0, 10, 0x1), NULL, 0},
    {10, 2, "r10w", X64_COLOR(0, 10, 0x3), NULL, 0},
    {10, 4, "r10d", X64_COLOR(0, 10, 0xf), NULL, 0},
    {10, 8, "r10", X64_COLOR(0, 10, 0xff), NULL, 0},

    {11, 1, "r11b", X64_COLOR(0, 11, 0x1), NULL, 0},
    {11, 2, "r11w", X64_COLOR(0, 11, 0x3), NULL, 0},
    {11, 4, "r11d", X64_COLOR(0, 11, 0xf), NULL, 0},
    {11, 8, "r11", X64_COLOR(0, 11, 0xff), NULL, 0},

    {12, 1, "r12b", X64_COLOR(0, 12, 0x1), NULL, 0},
    {12, 2, "r12w", X64_COLOR(0, 12, 0x3), NULL, 0},
    {12, 4, "r12d", X64_COLOR(0, 12, 0xf), NULL, 0},
    {12, 8, "r12", X64_COLOR(0, 12, 0xff), NULL, 0},

    {13, 1, "r13b", X64_COLOR(0, 13, 0x1), NULL, 0},
    {13, 2, "r13w", X64_COLOR(0, 13, 0x3), NULL, 0},
    {13, 4, "r13d", X64_COLOR(0, 13, 0xf), NULL, 0},
    {13, 8, "r13", X64_COLOR(0, 13, 0xff), NULL, 0},

    {14, 1, "r14b", X64_COLOR(0, 14, 0x1), NULL, 0},
    {14, 2, "r14w", X64_COLOR(0, 14, 0x3), NULL, 0},
    {14, 4, "r14d", X64_COLOR(0, 14, 0xf), NULL, 0},
    {14, 8, "r14", X64_COLOR(0, 14, 0xff), NULL, 0},

    {15, 1, "r15b", X64_COLOR(0, 15, 0x1), NULL, 0},
    {15, 2, "r15w", X64_COLOR(0, 15, 0x3), NULL, 0},
    {15, 4, "r15d", X64_COLOR(0, 15, 0xf), NULL, 0},
    {15, 8, "r15", X64_COLOR(0, 15, 0xff), NULL, 0},

    {4, 1, "ah", X64_COLOR(0, 0, 0x2), NULL, 0},
    {5, 1, "ch", X64_COLOR(0, 1, 0x2), NULL, 0},
    {6, 1, "dh", X64_COLOR(0, 2, 0x2), NULL, 0},
    {7, 1, "bh", X64_COLOR(0, 3, 0x2), NULL, 0},

    {0, 4, "mm0", X64_COLOR(1, 0, 0xf), NULL, 0},
    {0, 8, "xmm0", X64_COLOR(1, 0, 0xff), NULL, 0},

    {1, 4, "mm1", X64_COLOR(1, 1, 0xf), NULL, 0},
    {1, 8, "xmm1", X64_COLOR(1, 1, 0xff), NULL, 0},

    {2, 4, "mm2", X64_COLOR(1, 2, 0xf), NULL, 0},
    {2, 8, "xmm2", X64_COLOR(1, 2, 0xff), NULL, 0},

    {3, 4, "mm3", X64_COLOR(1, 3, 0xf), NULL, 0},
    {3, 8, "xmm3", X64_COLOR(1, 3, 0xff), NULL, 0},

    {4, 4, "mm4", X64_COLOR(1, 4, 0xf), NULL, 0},
    {4, 8, "xmm4", X64_COLOR(1, 4, 0xff), NULL, 0},

    {5, 4, "mm5", X64_COLOR(1, 5, 0xf), NULL, 0},
    {5, 8, "xmm5", X64_COLOR(1, 5, 0xff), NULL, 0},

    {6, 4, "mm6", X64_COLOR(1, 6, 0xf), NULL, 0},
    {6, 8, "xmm6", X64_COLOR(1, 6, 0xff), NULL, 0},

    {7, 4, "mm7", X64_COLOR(1, 7, 0xf), NULL, 0},
    {7, 8, "xmm7", X64_COLOR(1, 7, 0xff), NULL, 0},

    {0xf, 8, "rip", X64_COLOR(0, 7, 0xff), NULL, 0},
};

int x64_reg_cached_vars(register_t *r) {
    int nb_vars = 0;
    int i;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r2 = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r2->color) && (X64_REG_RSP == r2->id || X64_REG_RBP == r2->id))
            continue;

        if (!X64_COLOR_CONFLICT(r->color, r2->color))
            continue;

        nb_vars += r2->dag_nodes->size;
    }

    return nb_vars;
}

int x64_registers_init() {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        assert(!r->dag_nodes);

        r->dag_nodes = vector_alloc();
        if (!r->dag_nodes)
            return -ENOMEM;

        r->used = 0;
    }

    return 0;
}

void x64_registers_clear() {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        if (r->dag_nodes) {
            vector_free(r->dag_nodes);
            r->dag_nodes = NULL;
        }

        r->used = 0;
    }
}

void x64_registers_print() {
    register_t *r;
    dag_node_t *dn;
    variable_t *v;
    int i;
    int j;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        if (!r->dag_nodes)
            continue;

        for (j = 0; j < r->dag_nodes->size; j++) {
            dn = r->dag_nodes->data[j];

            v = dn->var;
            printf("%s, dn: %#lx, v_%#lx", r->name, 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)v);

            if (v->w)
                printf(", %s_%d_%d\n", v->w->text->data, v->w->line, v->w->pos);
            else
                printf("\n");
        }
    }
}

int x64_caller_save_regs(_3ac_code_t * c, const char *regs[], int nb_regs, int stack_size, register_t **saved_regs) {
    basic_block_t *bb = c->basic_block;
    dag_node_t *dn;

    instruction_t *inst;
    x64_OpCode_t *push = x64_find_OpCode(X64_PUSH, 8, 8, X64_G);
    x64_OpCode_t *movsd = x64_find_OpCode(X64_MOVSD, 8, 8, X64_G2E);
    x64_OpCode_t *mov = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);
    x64_OpCode_t *sub = x64_find_OpCode(X64_SUB, 4, 4, X64_I2E);
    register_t *rsp = x64_find_register("rsp");
    register_t *r;
    register_t *r2;

    uint32_t imm = 8;
    int i;
    int j;
    int k;
    int size = 0;
    int n = 0;

    for (j = 0; j < nb_regs; j++) {
        r2 = x64_find_register(regs[j]);

        for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
            r = &(x64_registers[i]);

            if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
                continue;

            if (0 == r->dag_nodes->size)
                continue;

            if (X64_COLOR_CONFLICT(r2->color, r->color)) {
                for (k = 0; k < r->dag_nodes->size; k++) {
                    dn = r->dag_nodes->data[k];

                    if (vector_find(bb->exit_dn_actives, dn)
                        || vector_find(bb->dn_saves, dn)
                        || vector_find(bb->dn_resaves, dn)) {
                        variable_t *v = dn->var;
                        if (v && v->w)
                            logw("dn: %#lx, v_%d/%s/%#lx\n", 0xffff & (uintptr_t)dn, v->w->line, v->w->text->data, 0xffff & (uintptr_t)v);
                        else
                            logw("dn: %#lx, v_%#lx\n", 0xffff & (uintptr_t)dn, 0xffff & (uintptr_t)v);
                        break;
                    }
                }

                if (k < r->dag_nodes->size)
                    break;
            }
        }

        if (i == sizeof(x64_registers) / sizeof(x64_registers[0]))
            continue;

        if (X64_COLOR_TYPE(r2->color)) {
            if (stack_size > 0)
                inst = x64_make_inst_G2P(movsd, rsp, size + stack_size, r2);
            else {
                inst = x64_make_inst_I2E(sub, rsp, (uint8_t *)&imm, 4);
                X64_INST_ADD_CHECK(c->instructions, inst);

                inst = x64_make_inst_G2P(movsd, rsp, 0, r2);
            }
        } else {
            if (stack_size > 0)
                inst = x64_make_inst_G2P(mov, rsp, size + stack_size, r2);
            else
                inst = x64_make_inst_G(push, r2);
        }
        X64_INST_ADD_CHECK(c->instructions, inst);

        saved_regs[n++] = r2;
        size += 8;
    }

    if (size & 0xf) {
        r2 = saved_regs[n - 1];

        if (X64_COLOR_TYPE(r2->color)) {
            if (stack_size > 0)
                inst = x64_make_inst_G2P(movsd, rsp, size + stack_size, r2);
            else {
                inst = x64_make_inst_I2E(sub, rsp, (uint8_t *)&imm, 4);
                X64_INST_ADD_CHECK(c->instructions, inst);

                inst = x64_make_inst_G2P(movsd, rsp, 0, r2);
            }
        } else {
            if (stack_size > 0)
                inst = x64_make_inst_G2P(mov, rsp, size + stack_size, r2);
            else
                inst = x64_make_inst_G(push, r2);
        }
        X64_INST_ADD_CHECK(c->instructions, inst);

        saved_regs[n++] = r2;
        size += 8;
    }

    if (stack_size > 0) {
        for (j = 0; j < n / 2; j++) {
            i = n - 1 - j;
            XCHG(saved_regs[i], saved_regs[j]);
        }
    }

    return size;
}

int x64_pop_regs(vector_t *instructions, register_t **regs, int nb_regs, register_t **updated_regs, int nb_updated) {
    register_t *rsp = x64_find_register("rsp");
    register_t *r;
    register_t *r2;
    instruction_t *inst;
    x64_OpCode_t *movsd = x64_find_OpCode(X64_MOVSD, 8, 8, X64_E2G);
    x64_OpCode_t *pop = x64_find_OpCode(X64_POP, 8, 8, X64_G);
    x64_OpCode_t *add = x64_find_OpCode(X64_ADD, 4, 4, X64_I2E);

    uint32_t imm = 8;
    int i;
    int j;

    for (j = nb_regs - 1; j >= 0; j--) {
        r2 = regs[j];

        for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
            r = &(x64_registers[i]);

            if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
                continue;

            if (0 == r->dag_nodes->size)
                continue;

            if (X64_COLOR_CONFLICT(r2->color, r->color))
                break;
        }

        if (i == sizeof(x64_registers) / sizeof(x64_registers[0]))
            continue;

        for (i = 0; i < nb_updated; i++) {
            r = updated_regs[i];

            if (X64_COLOR_CONFLICT(r2->color, r->color))
                break;
        }

        if (i == nb_updated) {
            if (X64_COLOR_TYPE(r2->color)) {
                inst = x64_make_inst_P2G(movsd, r2, rsp, 0);
                X64_INST_ADD_CHECK(instructions, inst);

                inst = x64_make_inst_I2E(add, rsp, (uint8_t *)&imm, 4);
            } else
                inst = x64_make_inst_G(pop, r2);
            X64_INST_ADD_CHECK(instructions, inst);
        } else {
            inst = x64_make_inst_I2E(add, rsp, (uint8_t *)&imm, 4);
            X64_INST_ADD_CHECK(instructions, inst);
        }
    }
    return 0;
}

int x64_registers_reset() {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        if (!r->dag_nodes)
            continue;

        int j = 0;
        while (j < r->dag_nodes->size) {
            dag_node_t *dn = r->dag_nodes->data[j];

            if (dn->var->w)
                logd("drop: v_%d_%d/%s\n", dn->var->w->line, dn->var->w->pos, dn->var->w->text->data);
            else
                logd("drop: v_%#lx\n", 0xffff & (uintptr_t)dn->var);

            int ret = vector_del(r->dag_nodes, dn);
            if (ret < 0) {
                loge("\n");
                return ret;
            }

            dn->loaded = 0;
            dn->color = 0;
        }
    }

    return 0;
}

register_t *x64_find_register(const char *name) {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!strcmp(r->name, name))
            return r;
    }
    return NULL;
}

register_t *x64_find_register_type_id_bytes(uint32_t type, uint32_t id, int bytes) {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (X64_COLOR_TYPE(r->color) == type && r->id == id && r->bytes == bytes)
            return r;
    }
    return NULL;
}

register_t *x64_find_register_color(intptr_t color) {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (r->color == color)
            return r;
    }
    return NULL;
}

register_t *x64_find_register_color_bytes(intptr_t color, int bytes) {
    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (X64_COLOR_CONFLICT(r->color, color) && r->bytes == bytes)
            return r;
    }
    return NULL;
}

vector_t *x64_register_colors() {
    vector_t *colors = vector_alloc();
    if (!colors)
        return NULL;

    int i;
    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        // ah bh ch dh can't use
        if (X64_COLOR(0, 0, 0x2) == r->color
            || X64_COLOR(0, 1, 0x2) == r->color
            || X64_COLOR(0, 2, 0x2) == r->color
            || X64_COLOR(0, 3, 0x2) == r->color)
            continue;

        int ret = vector_add(colors, (void *)r->color);
        if (ret < 0) {
            vector_free(colors);
            return NULL;
        }
    }
#if 0
	srand(time(NULL));
	for (i = 0; i < colors->size; i++) {
		int j = rand() % colors->size;

		void* t = colors->data[i];
		colors->data[i] = colors->data[j];
		colors->data[j] = t;
	}
#endif
    return colors;
}

int x64_save_var2(dag_node_t *dn, register_t *r, _3ac_code_t * c, function_t *f) {
    variable_t *v = dn->var;
    rela_t *rela = NULL;
    x64_OpCode_t *mov;
    x64_OpCode_t *and;
    x64_OpCode_t *shl;
    instruction_t *inst;

    int var_size = x64_variable_size(v);
    int is_float = variable_float(v);

    assert(var_size == r->bytes);

    if (variable_const(v)) {
        if (v->w)
            logd("const literal var: v_%s_%d_%d not save\n", v->w->text->data, v->w->line, v->w->pos);
        else
            logd("const literal var: v_%#lx not save\n", 0xffff & (uintptr_t)v);
        goto end;
    }

    // if temp var in register, alloc it in stack
    if (0 == v->bp_offset && !v->global_flag && !v->local_flag) {
        int local_vars_size = f->local_vars_size;
        local_vars_size += var_size;

        if (local_vars_size & 0x7)
            local_vars_size = (local_vars_size + 7) >> 3 << 3;

        v->bp_offset = -local_vars_size;
        v->tmp_flag = 1;

        f->local_vars_size = local_vars_size;
#if 0
		logw("r: %s, temp var, ", r->name);
		if (v->w)
			printf("v_%d_%d/%s, bp_offset: %d\n", v->w->line, v->w->pos, v->w->text->data, v->bp_offset);
		else
			printf("v_%#lx, bp_offset: %d\n", 0xffff & (uintptr_t)v, v->bp_offset);
#endif
    } else {
#if 0
		if (v->w)
			logw("save var: v_%d_%d/%s, ", v->w->line, v->w->pos, v->w->text->data);
		else
			logw("save var: v_%#lx, ", 0xffff & (uintptr_t)v);
		printf("size: %d, bp_offset: %d, r: %s\n", var_size, v->bp_offset, r->name);
#endif
    }

    if (is_float) {
        if (VAR_FLOAT == dn->var->type)
            mov = x64_find_OpCode(X64_MOVSS, r->bytes, r->bytes, X64_G2E);
        else if (VAR_DOUBLE == dn->var->type)
            mov = x64_find_OpCode(X64_MOVSD, r->bytes, r->bytes, X64_G2E);
    } else {
        logd("v->size: %d\n", v->size);

        mov = x64_find_OpCode(X64_MOV, r->bytes, r->bytes, X64_G2E);
    }

    inst = x64_make_inst_G2M(&rela, mov, v, NULL, r);
    X64_INST_ADD_CHECK(c->instructions, inst);
    X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);

end:
    // if this var is function argment, it become a normal local var
    v->arg_flag = 0;
    dn->color = -1;
    dn->loaded = 0;

    vector_del(r->dag_nodes, dn);
    return 0;
}

int x64_save_var(dag_node_t *dn, _3ac_code_t * c, function_t *f) {
    if (dn->color <= 0)
        return -EINVAL;

    register_t *r = x64_find_register_color(dn->color);

    return x64_save_var2(dn, r, c, f);
}

int x64_save_reg(register_t *r, _3ac_code_t * c, function_t *f) {
    int i = 0;
    while (i < r->dag_nodes->size) {
        dag_node_t *dn = r->dag_nodes->data[i];

        int ret = x64_save_var(dn, c, f);
        if (ret < 0) {
            loge("i: %d, size: %d, r: %s, dn->var: %s\n", i, r->dag_nodes->size, r->name, dn->var->w->text->data);
            return ret;
        }
    }

    return 0;
}

int x64_overflow_reg(register_t *r, _3ac_code_t * c, function_t *f) {
    int i;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r2 = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r2->color) && (X64_REG_RSP == r2->id || X64_REG_RBP == r2->id))
            continue;

        if (!X64_COLOR_CONFLICT(r->color, r2->color))
            continue;

        int ret = x64_save_reg(r2, c, f);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    r->used = 1;
    return 0;
}

int x64_overflow_reg2(register_t *r, dag_node_t *dn, _3ac_code_t * c, function_t *f) {
    register_t *r2;
    dag_node_t *dn2;

    int i;
    int j;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        r2 = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r2->color) && (X64_REG_RSP == r2->id || X64_REG_RBP == r2->id))
            continue;

        if (!X64_COLOR_CONFLICT(r->color, r2->color))
            continue;

        for (j = 0; j < r2->dag_nodes->size;) {
            dn2 = r2->dag_nodes->data[j];

            if (dn2 == dn) {
                j++;
                continue;
            }

            int ret = x64_save_var(dn2, c, f);
            if (ret < 0)
                return ret;
        }
    }

    r->used = 1;
    return 0;
}

int x64_reg_used(register_t *r, dag_node_t *dn) {
    register_t *r2;
    dag_node_t *dn2;

    int i;
    int j;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        r2 = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r2->color) && (X64_REG_RSP == r2->id || X64_REG_RBP == r2->id))
            continue;

        if (!X64_COLOR_CONFLICT(r->color, r2->color))
            continue;

        for (j = 0; j < r2->dag_nodes->size; j++) {
            dn2 = r2->dag_nodes->data[j];

            if (dn2 != dn)
                return 1;
        }
    }
    return 0;
}

static register_t *_x64_reg_cached_min_vars(register_t **regs, int nb_regs) {
    register_t *r_min = NULL;

    int min = 0;
    int i;

    for (i = 0; i < nb_regs; i++) {
        register_t *r = regs[i];

        int nb_vars = x64_reg_cached_vars(r);

        if (!r_min) {
            r_min = r;
            min = nb_vars;
            continue;
        }

        if (min > nb_vars) {
            r_min = r;
            min = nb_vars;
        }
    }

    return r_min;
}

register_t *x64_select_overflowed_reg(dag_node_t *dn, _3ac_code_t * c) {
    vector_t *neighbors = NULL;
    graph_node_t *gn = NULL;

    register_t *free_regs[sizeof(x64_registers) / sizeof(x64_registers[0])];

    int nb_free_regs = 0;
    int is_float = variable_float(dn->var);
    int bytes = x64_variable_size(dn->var);
    int ret;
    int i;
    int j;

    logd("bytes: %d\n", bytes);

    assert(c->rcg);

    ret = x64_rcg_find_node(&gn, c->rcg, dn, NULL);
    if (ret < 0)
        neighbors = c->rcg->nodes;
    else
        neighbors = gn->neighbors;

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        if (1 == bytes) {
            // ah bh ch dh can't use
            if (X64_COLOR(0, 0, 0x2) == r->color
                || X64_COLOR(0, 1, 0x2) == r->color
                || X64_COLOR(0, 2, 0x2) == r->color
                || X64_COLOR(0, 3, 0x2) == r->color)
                continue;
        }

        if (r->bytes < bytes || X64_COLOR_TYPE(r->color) != is_float)
            continue;

        for (j = 0; j < neighbors->size; j++) {
            graph_node_t *neighbor = neighbors->data[j];
            x64_rcg_node_t *rn = neighbor->data;

            if (rn->dag_node) {
                if (rn->dag_node->color <= 0)
                    continue;

                if (X64_COLOR_CONFLICT(r->color, rn->dag_node->color))
                    break;
            } else {
                assert(rn->reg);

                if (X64_COLOR_CONFLICT(r->color, rn->reg->color))
                    break;
            }
        }

        if (j == neighbors->size)
            free_regs[nb_free_regs++] = r;
    }

    if (nb_free_regs > 0)
        return _x64_reg_cached_min_vars(free_regs, nb_free_regs);

    for (i = 0; i < sizeof(x64_registers) / sizeof(x64_registers[0]); i++) {
        register_t *r = &(x64_registers[i]);

        if (!X64_COLOR_TYPE(r->color) && (X64_REG_RSP == r->id || X64_REG_RBP == r->id))
            continue;

        if (1 == bytes) {
            // ah bh ch dh can't use
            if (X64_COLOR(0, 0, 0x2) == r->color
                || X64_COLOR(0, 1, 0x2) == r->color
                || X64_COLOR(0, 2, 0x2) == r->color
                || X64_COLOR(0, 3, 0x2) == r->color)
                continue;
        }

        if (r->bytes < bytes || X64_COLOR_TYPE(r->color) != is_float)
            continue;

        if (c->dsts) {
            _3ac_operand_t * dst;

            for (j = 0; j < c->dsts->size; j++) {
                dst = c->dsts->data[j];

                if (dst->dag_node && dst->dag_node->color > 0
                    && X64_COLOR_CONFLICT(r->color, dst->dag_node->color))
                    break;
            }

            if (j < c->dsts->size)
                continue;
        }

        if (c->srcs) {
            _3ac_operand_t * src;

            for (j = 0; j < c->srcs->size; j++) {
                src = c->srcs->data[j];

                if (src->dag_node && src->dag_node->color > 0
                    && X64_COLOR_CONFLICT(r->color, src->dag_node->color))
                    break;
            }

            if (j < c->srcs->size)
                continue;
        }

        return r;
    }

    return NULL;
}

int x64_load_const(register_t *r, dag_node_t *dn, _3ac_code_t * c, function_t *f) {
    instruction_t *inst;
    x64_OpCode_t *lea;
    x64_OpCode_t *mov;
    variable_t *v;

    v = dn->var;
    r->used = 1;

    int size = x64_variable_size(v);
    int is_float = variable_float(v);

    if (FUNCTION_PTR == v->type) {
        if (v->func_ptr) {
            assert(v->const_literal_flag);

            v->global_flag = 1;
            v->local_flag = 0;
            v->tmp_flag = 0;

            rela_t *rela = NULL;

            lea = x64_find_OpCode(X64_LEA, size, size, X64_E2G);
            inst = x64_make_inst_M2G(&rela, lea, r, NULL, v);
            X64_INST_ADD_CHECK(c->instructions, inst);
            X64_RELA_ADD_CHECK(f->text_relas, rela, c, NULL, v->func_ptr);

        } else {
            x64_OpCode_t * xor ;

            xor= x64_find_OpCode(X64_XOR, size, size, X64_G2E);
            inst = x64_make_inst_G2E(xor, r, r);
            X64_INST_ADD_CHECK(c->instructions, inst);
        }

    } else if (variable_const_string(v)) {
        rela_t *rela = NULL;

        v->global_flag = 1;
        v->local_flag = 0;
        v->tmp_flag = 0;

        lea = x64_find_OpCode(X64_LEA, size, size, X64_E2G);

        inst = x64_make_inst_M2G(&rela, lea, r, NULL, v);
        X64_INST_ADD_CHECK(c->instructions, inst);
        X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);

    } else if (v->nb_dimentions > 0) {
        assert(v->const_literal_flag);

        rela_t *rela = NULL;

        lea = x64_find_OpCode(X64_LEA, size, size, X64_E2G);

        inst = x64_make_inst_M2G(&rela, lea, r, NULL, v);
        X64_INST_ADD_CHECK(c->instructions, inst);
        X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);

    } else {
        mov = x64_find_OpCode(X64_MOV, size, size, X64_I2G);
        inst = x64_make_inst_I2G(mov, r, (uint8_t *)&v->data, size);
        X64_INST_ADD_CHECK(c->instructions, inst);
    }

    return 0;
}

int x64_load_reg(register_t *r, dag_node_t *dn, _3ac_code_t * c, function_t *f) {
    if (dn->loaded)
        return 0;

    x64_OpCode_t *mov;
    x64_OpCode_t *shr;
    x64_OpCode_t *and;
    instruction_t *inst;
    rela_t *rela = NULL;

    int is_float = variable_float(dn->var);
    int var_size = x64_variable_size(dn->var);

    r->used = 1;

    if (!is_float) {
        if (variable_const(dn->var)) {
            int ret = x64_load_const(r, dn, c, f);
            if (ret < 0)
                return ret;

            dn->loaded = 1;
            return 0;
        }

        if (!dn->var->global_flag && !dn->var->local_flag && !dn->var->tmp_flag)
            return 0;

        if (variable_const_string(dn->var)) {
            dn->var->global_flag = 1;
            dn->var->local_flag = 0;
            dn->var->tmp_flag = 0;

            mov = x64_find_OpCode(X64_LEA, var_size, var_size, X64_E2G);

            variable_t *v = dn->var;
            loge("v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
        } else {
            if ((dn->var->nb_dimentions > 0 && dn->var->const_literal_flag)
                || (dn->var->type >= STRUCT && 0 == dn->var->nb_pointers))
                mov = x64_find_OpCode(X64_LEA, var_size, var_size, X64_E2G);
            else
                mov = x64_find_OpCode(X64_MOV, var_size, var_size, X64_E2G);
        }
    } else {
        if (!dn->var->global_flag && !dn->var->local_flag && !dn->var->tmp_flag)
            return 0;

        if (VAR_FLOAT == dn->var->type)
            mov = x64_find_OpCode(X64_MOVSS, var_size, var_size, X64_E2G);
        else
            mov = x64_find_OpCode(X64_MOVSD, var_size, var_size, X64_E2G);
    }

    if (!mov) {
        loge("\n");
        return -EINVAL;
    }

    if (0 == dn->var->bp_offset && !dn->var->global_flag) {
        loge("\n");
        return -EINVAL;
    }

    inst = x64_make_inst_M2G(&rela, mov, r, NULL, dn->var);
    X64_INST_ADD_CHECK(c->instructions, inst);
    X64_RELA_ADD_CHECK(f->data_relas, rela, c, dn->var, NULL);

    dn->loaded = 1;
    return 0;
}

int x64_select_reg(register_t **preg, dag_node_t *dn, _3ac_code_t * c, function_t *f, int load_flag) {
    if (0 == dn->color)
        return -EINVAL;

    register_t *r;

    int ret;
    int is_float = variable_float(dn->var);
    int var_size = x64_variable_size(dn->var);

    if (dn->color > 0) {
        r = x64_find_register_color(dn->color);
#if 1
        ret = x64_overflow_reg2(r, dn, c, f);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
#endif
    } else {
        r = x64_select_overflowed_reg(dn, c);
        if (!r) {
            loge("\n");
            return -1;
        }

        ret = x64_overflow_reg(r, c, f);
        if (ret < 0) {
            loge("overflow reg failed\n");
            return ret;
        }
        assert(0 == r->dag_nodes->size);

        r = x64_find_register_type_id_bytes(is_float, r->id, var_size);
        assert(0 == r->dag_nodes->size);

        dn->color = r->color;
    }

    ret = vector_add_unique(r->dag_nodes, dn);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    if (load_flag) {
        ret = x64_load_reg(r, dn, c, f);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    } else
        dn->loaded = 1;

    r->used = 1;
    *preg = r;
    return 0;
}

int x64_dereference_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *member, _3ac_code_t * c, function_t *f) {
    register_t *rb = NULL;
    variable_t *vb = base->var;

    logd("base->color: %ld\n", base->color);

    int ret = x64_select_reg(&rb, base, c, f, 1);
    if (ret < 0) {
        loge("\n");
        return ret;
    }
    logd("base->color: %ld\n", base->color);

    if (vb->nb_pointers + vb->nb_dimentions > 1 || vb->type >= STRUCT)
        sib->size = 8;
    else {
        sib->size = vb->data_size;
        assert(8 >= vb->data_size);
    }

    sib->base = rb;
    sib->index = NULL;
    sib->scale = 0;
    sib->disp = 0;
    return 0;
}

int x64_pointer_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *member, _3ac_code_t * c, function_t *f) {
    variable_t *vb = base->var;
    variable_t *vm = member->var;
    register_t *rb = NULL;

    x64_OpCode_t *lea;
    x64_OpCode_t *mov;
    instruction_t *inst;

    int ret;
    int32_t disp = 0;

    if (vb->nb_pointers > 0 && 0 == vb->nb_dimentions) {
        ret = x64_select_reg(&rb, base, c, f, 1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    } else if (vb->local_flag) {
        rb = x64_find_register("rbp");
        disp = vb->bp_offset;

    } else if (vb->global_flag) {
        rela_t *rela = NULL;

        ret = x64_select_reg(&rb, base, c, f, 1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

    } else {
        ret = x64_select_reg(&rb, base, c, f, 0);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    disp += vm->offset;

    sib->base = rb;
    sib->index = NULL;
    sib->scale = 0;
    sib->disp = disp;
    sib->size = x64_variable_size(vm);
    return 0;
}

int x64_array_index_reg(x64_sib_t *sib, dag_node_t *base, dag_node_t *index, dag_node_t *scale, _3ac_code_t * c, function_t *f) {
    variable_t *vb = base->var;
    variable_t *vi = index->var;

    register_t *rb = NULL;
    register_t *ri = NULL;

    x64_OpCode_t * xor ;
    x64_OpCode_t *add;
    x64_OpCode_t *shl;
    x64_OpCode_t *lea;
    x64_OpCode_t *mov;
    instruction_t *inst;

    int ret;
    int i;

    int32_t disp = 0;

    if ((vb->nb_pointers > 0 && 0 == vb->nb_dimentions) || vb->vla_flag) {
        ret = x64_select_reg(&rb, base, c, f, 1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    } else if (vb->local_flag) {
        rb = x64_find_register("rbp");
        disp = vb->bp_offset;

    } else if (vb->global_flag) {
        rela_t *rela = NULL;

        if (0 == base->color)
            base->color = -1;

        ret = x64_select_reg(&rb, base, c, f, 1);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

    } else {
        ret = x64_select_reg(&rb, base, c, f, 0);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    int32_t s = scale->var->data.i;
    assert(s > 0);

    if (vb->nb_pointers + vb->nb_dimentions > 1 || vb->type >= STRUCT)
        sib->size = 8;
    else {
        sib->size = vb->data_size;
        assert(8 >= vb->data_size);
    }

    if (0 == index->color) {
        disp += vi->data.i * s;

        sib->base = rb;
        sib->index = NULL;
        sib->scale = 0;
        sib->disp = disp;
        return 0;
    }

    ret = x64_select_reg(&ri, index, c, f, 1);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    register_t *ri2 = x64_find_register_color_bytes(ri->color, 8);

    if (ri->bytes < ri2->bytes) {
        if (variable_signed(index->var)) {
            mov = x64_find_OpCode(X64_MOVSX, ri->bytes, ri2->bytes, X64_E2G);

        } else if (ri->bytes <= 2) {
            mov = x64_find_OpCode(X64_MOVZX, ri->bytes, ri2->bytes, X64_E2G);

        } else {
            assert(4 == ri->bytes);

            xor= x64_find_OpCode(X64_XOR, 8, 8, X64_G2E);
            inst = x64_make_inst_G2E(xor, ri2, ri2);
            X64_INST_ADD_CHECK(c->instructions, inst);

            mov = x64_find_OpCode(X64_MOV, 4, 4, X64_E2G);
        }

        inst = x64_make_inst_E2G(mov, ri2, ri);
        X64_INST_ADD_CHECK(c->instructions, inst);

        ri = ri2;
    }

    if (1 != s
        && 2 != s
        && 4 != s
        && 8 != s) {
        assert(8 == scale->var->size);

        register_t *rs = NULL;

        ret = x64_select_reg(&rs, scale, c, f, 0);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        mov = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);
        inst = x64_make_inst_G2E(mov, rs, ri2);
        X64_INST_ADD_CHECK(c->instructions, inst);

        shl = x64_find_OpCode(X64_SHL, 1, 8, X64_I2E);
        add = x64_find_OpCode(X64_ADD, 8, 8, X64_G2E);

        logw("s: %d\n", s);

        int32_t count = 0;
        int i;
        for (i = 30; i >= 0; i--) {
            if (0 == (s & 1 << i))
                continue;
            logw("s: %d, i: %d\n", s, i);

            if (0 == count) {
                logw("s: %d, i: %d\n", s, i);
                count = i;
                continue;
            }

            count -= i;

            inst = x64_make_inst_I2E(shl, rs, (uint8_t *)&count, 1);
            X64_INST_ADD_CHECK(c->instructions, inst);

            inst = x64_make_inst_G2E(add, rs, ri2);
            X64_INST_ADD_CHECK(c->instructions, inst);

            count = i;
        }

        if (count > 0) {
            inst = x64_make_inst_I2E(shl, rs, (uint8_t *)&count, 1);
            X64_INST_ADD_CHECK(c->instructions, inst);
        }

        ri = rs;
        s = 1;
    }

    sib->base = rb;
    sib->index = ri;
    sib->scale = s;
    sib->disp = disp;
    return 0;
}

void x64_call_rabi(int *p_nints, int *p_nfloats, _3ac_code_t * c) {
    _3ac_operand_t *src = NULL;
    dag_node_t *dn = NULL;

    int nfloats = 0;
    int nints = 0;
    int i;

    for (i = 1; i < c->srcs->size; i++) {
        src = c->srcs->data[i];
        dn = src->dag_node;

        int is_float = variable_float(dn->var);
        int size = x64_variable_size(dn->var);

        if (is_float) {
            if (nfloats < X64_ABI_FLOAT_NB)
                dn->rabi2 = x64_find_register_type_id_bytes(is_float, x64_abi_float_regs[nfloats++], size);
            else
                dn->rabi2 = NULL;
        } else {
            if (nints < X64_ABI_NB)
                dn->rabi2 = x64_find_register_type_id_bytes(is_float, x64_abi_regs[nints++], size);
            else
                dn->rabi2 = NULL;
        }

        src->rabi = dn->rabi2;
    }

    if (p_nints)
        *p_nints = nints;

    if (p_nfloats)
        *p_nfloats = nfloats;
}

int x64_push_callee_regs(_3ac_code_t * c, function_t *f) {
    x64_OpCode_t *push = x64_find_OpCode(X64_PUSH, 8, 8, X64_G);

    instruction_t *inst;
    register_t *r2;
    register_t *r;

    int N = sizeof(x64_registers) / sizeof(x64_registers[0]);
    int i;
    int j;

    for (i = 0; i < X64_ABI_CALLEE_SAVES_NB; i++) {
        r = x64_find_register(x64_abi_callee_saves[i]);

        for (j = 0; j < N; j++) {
            r2 = &(x64_registers[j]);

            if (r2->used && X64_COLOR_CONFLICT(r2->color, r->color))
                break;
        }

        if (j < N) {
            inst = x64_make_inst_G(push, r);
            X64_INST_ADD_CHECK(f->init_code->instructions, inst);

            f->init_code_bytes += inst->len;
        }
    }

    return 0;
}

int x64_pop_callee_regs(_3ac_code_t * c, function_t *f) {
    x64_OpCode_t *pop = x64_find_OpCode(X64_POP, 8, 8, X64_G);

    basic_block_t *bb = c->basic_block;

    instruction_t *inst;
    register_t *r2;
    register_t *r;

    int N = sizeof(x64_registers) / sizeof(x64_registers[0]);
    int i;
    int j;

    f->callee_saved_size = 0;

    for (i = X64_ABI_CALLEE_SAVES_NB - 1; i >= 0; i--) {
        r = x64_find_register(x64_abi_callee_saves[i]);

        for (j = 0; j < N; j++) {
            r2 = &(x64_registers[j]);

            if (r2->used && X64_COLOR_CONFLICT(r2->color, r->color))
                break;
        }

        if (j < N) {
            inst = x64_make_inst_G(pop, r);
            X64_INST_ADD_CHECK(c->instructions, inst);

            bb->code_bytes += inst->len;
            f->callee_saved_size += 8;
        }
    }

    return 0;
}
