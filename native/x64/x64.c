#include "x64.h"
#include "elf.h"
#include "basic_block.h"
#include "3ac.h"

extern native_ops_t native_ops_x64;

int x64_open(native_t *ctx, const char *arch) {
    x64_context_t *x64 = calloc(1, sizeof(x64_context_t));
    if (!x64)
        return -ENOMEM;

    ctx->priv = x64;
    return 0;
}

int x64_close(native_t *ctx) {
    x64_context_t *x64 = ctx->priv;

    if (x64) {
        x64_registers_clear();

        free(x64);

        x64 = NULL;
    }
    return 0;
}

static void _x64_argv_rabi(function_t *f) {
    variable_t *v;

    f->args_int = 0;
    f->args_float = 0;

    int bp_int = -8;
    int bp_floats = -8 - (int)X64_ABI_NB * 8;
    int bp_others = 16;

    int i;
    for (i = 0; i < f->argv->size; i++) {
        v = f->argv->data[i];

        if (!v->arg_flag) {
            v->arg_flag = 1;
            assert(f->inline_flag);
        }

        int is_float = variable_float(v);
        int size = x64_variable_size(v);

        if (is_float) {
            if (f->args_float < X64_ABI_FLOAT_NB) {
                v->rabi = x64_find_register_type_id_bytes(is_float, x64_abi_float_regs(f->args_float), size);
                v->bp_offsets = bp_floats;
                bp_floats -= 8;
                f->args_float++;
                continue;
            }

        } else if (f->args_int < X64_ABI_NB) {
            v->rabi = x64_find_register_type_id_bytes(is_float, x64_abi_regs[f->args_int], size);
            v->bp_offsets = bp_int;
            bp_int -= 8;
            f->args_int++;
            continue;
        }
        v->rabi = NULL;
        v->bp_offsets = bp_others;
        bp_others += 8;
    }
}

static int _x64_function_init(function_t *f, vector_t *local_vars) {
    variable_t *v;

    int ret = x64_registers_init();
    if (ret < 0)
        return ret;

    int i;
    for (i = 0; i < local_vars->size; i++) {
        v = local_vars->data[i];
        v->bp_offsets = 0;
    }

    _x64_argv_rabi(f);

    int local_vars_size = 8 + (X64_ABI_NB + X64_ABI_FLOAT_NB) * 8;

    for (i = 0; i < local_vars->size; i++) {
        v = local_vars->data[i];

        if (v->arg_flag) {
            if (v->bp_offsets != 0)
                continue;
        }

        int size = variable_size(v);
        if (size < 0)
            return size;

        local_vars_size += size;

        if (local_vars_size & 0x7)
            local_vars_size = (local_vars_size + 7) >> 3 << 3;

        v->bp_offsets = -local_vars_size;
        v->local_flag = 1;
    }
    return local_vars_size;
}

static int _x64_save_rabi(function_t *f) {
    register_t *rbp;
    instruction_t *inst;
    x64_OpCode_t *mov;

    register_t *rdi;
    register_t *rsi;
    register_t *rdx;
    register_t *rcx;
    register_t *r8;
    register_t *r9;

    register_t *xmm0;
    register_t *xmm1;
    register_t *xmm2;
    register_t *xmm3;

    if (f->vargs_flag) {
        inst = NULL;
        mov = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);

        rbp = x64_find_register("rbp");

        rdi = x64_find_register("rdi");
        rsi = x64_find_register("rsi");
        rdx = x64_find_register("rdx");
        rcx = x64_find_register("rcx");
        r8 = x64_find_register("r8");
        r9 = x64_find_register("r9");

#define X64_SAVE_RABI(offset, rabi)                           \
    do {                                                      \
        inst = x64_make_inst_G2P(mov, rbp, offset, rabi);     \
        X64_INST_ADD_CHECK(f->init_code->instructions, inst); \
        f->init_code_bytes += inst->len;                      \
    } while (0)

        X64_SAVE_RABI(-8, rdi);
        X64_SAVE_RABI(-16, rsi);
        X64_SAVE_RABI(-24, rdx);
        X64_SAVE_RABI(-32, rcx);
        X64_SAVE_RABI(-40, r8);
        X64_SAVE_RABI(-48, r9);

        mov = x64_find_OpCode(X64_MOVSD, 8, 8, X64_G2E);

        xmm0 = x64_find_register("xmm0");
        xmm1 = x64_find_register("xmm1");
        xmm2 = x64_find_register("xmm2");
        xmm3 = x64_find_register("xmm3");

        X64_SAVE_RABI(-56, xmm0);
        X64_SAVE_RABI(-64, xmm1);
        X64_SAVE_RABI(-72, xmm2);
        X64_SAVE_RABI(-80, xmm3);
    }

    return 0;
}

static int _x64_function_finish(function_t *f) {
    assert(!f->init_code);

    f->init_code = _3ac_code_alloc();
    if (!f->init_code)
        return -ENOMEM;

    f->init_code->instructions = vector_alloc();

    if (!f->init_code->instructions) {
        _3ac_code_free(f->init_code);
        return -ENOMEM;
    }

    x64_OpCode_t *push = x64_find_OpCode(X64_PUSH, 8, 8, X64_G);
    x64_OpCode_t *pop = x64_find_OpCode(X64_POP, 8, 8, X64_G);
    x64_OpCode_t *mov = x64_find_OpCode(X64_MOV, 4, 4, X64_G2E);
    x64_OpCode_t *sub = x64_find_OpCode(X64_SUB, 4, 8, X64_I2E);
    x64_OpCode_t *ret = x64_find_OpCode(X64_RET, 8, 8, X64_G);

    register_t *rsp = x64_find_register("rsp");
    register_t *rdp = x64_find_register("rbp");

    register_t *r;
    instruction_t *inst = NULL;

    basic_block_t *bb;
    _3ac_code_t *end;
    list_t *l;

    l = list_tail(&f->basic_block_list_head);
    bb = list_data(l, basic_block_t, list);

    l = list_tail(&bb->code_list_head);
    end = list_data(l, 3ac_code_t, list);

    if (f->bp_used_flag || f->vla_flag || f->call_flag) {
        inst = x64_make_inst_G2E(mov, rsp, rbp);
        X64_INST_ADD_CHECK(end->instructions, inst);
        end->inst_bytes += inst->len;
        bb->code_bytes += inst->len;

        inst = x64_make_inst_G(pop, rbp);
        X64_INST_ADD_CHECK(end->instructions, inst);
        end->inst_bytes += inst->len;
        bb->code_bytes += inst->len;
    }

    int err = x64_pop_callee_regs(end, f);
    if (err < 0)
        return err;

    f->init_code_bytes = 0;

    err = x64_push_callee_regs(f->init_code, f);
    if (err < 0)
        return err;

    uint32_t local = f->bp_used_flag ? f->local_vars_size : 0;

    if (f->bp_used_flag || f->vla_flag || f->call_flag) {
        inst = x64_make_inst_G(push, rbp);
        X64_INST_ADD_CHECK(f->init_code->instructions, inst);
        f->init_code_bytes += inst->len;

        inst = x64_make_inst_G2E(mov, rbp, rsp);
        X64_INST_ADD_CHECK(f->init_code->instructions, inst);
        f->init_code_bytes += inst->len;

        if (f->callee_saved_size & 0xf) {
            if (!(local & 0xf))
                local += 8;
        } else {
            if ((local & 0xf))
                local += 8;
        }

        logd("### local: %#x, local_vars_size: %#x, callee_saved_size: %#x\n",
             local, f->local_vars_size, f->callee_saved_size);

        inst = x64_make_inst_I2E(sub, rsp, (uint8_t *)&local, 4);
        X64_INST_ADD_CHECK(f->init_code->instructions, inst);
        f->init_code_bytes += inst->len;

        int err = _x64_save_rabi(f);
        if (err < 0)
            return err;
    }

    inst = x64_make_inst(ret, 8);
    X64_INST_ADD_CHECK(end->instructions, inst);
    end->inst_bytes += inst->len;
    bb->code_bytes += inst->len;

    x64_registers_clear();
    return 0;
}

static void _x64_rcg_node_printf(x64_rcg_node_t *rn) {
    if (rn->dag_node) {
        variable_t *v = rn->dag_node->var;

        if (v->w) {
            logw("v_%d_%d/%s, ",
                 v->w->line, v->w->pos, v->w->text->data);

            if (v->bp_offset < 0)
                printf("bp_offset: -%#x, ", -v->bp_offset);
            else
                printf("bp_offset:  %#x, ", v->bp_offset);

            printf("color: %ld, type: %ld, id: %ld, mask: %ld",
                   rn->dag_node->color,
                   X64_COLOR_TYPE(rn->dag_node->color),
                   X64_COLOR_ID(rn->dag_node->color),
                   X64_COLOR_MASK(rn->dag_node->color));

        } else {
            logw("v_%#lx, %p,%p,  color: %ld, type: %ld, id: %ld, mask: %ld",
                 (uintptr_t)v & 0xffff, v, rn->dag_node,
                 rn->dag_node->color,
                 X64_COLOR_TYPE(rn->dag_node->color),
                 X64_COLOR_ID(rn->dag_node->color),
                 X64_COLOR_MASK(rn->dag_node->color));
        }

        if (rn->dag_node->color > 0) {
            register_t *r = x64_find_register_color(rn->dag_node->color);
            printf(", reg: %s\n", r->name);
        } else {
            printf("\n");
        }
    } else if (rn->reg) {
        logw("r/%s, color: %ld, type: %ld, major: %ld, minor: %ld\n",
             rn->reg->name, rn->reg->color,
             X64_COLOR_TYPE(rn->reg->color),
             X64_COLOR_ID(rn->reg->color),
             X64_COLOR_MASK(rn->reg->color));
    }
}

static void _x64_inst_printf(3ac_code_t * c) {
    if (!c->instructions)
        return;

    int i;
    for (i = 0; i < c->instructions->size; i++) {
        instruction_t *inst = c->instructions->data[i];
        int j;
        for (j = 0; j < inst->len; j++)
            printf("%02x ", inst->code[j]);
        printf("\n");
    }
    printf("\n");
}

static int _x64_argv_prepare(graph_t *g, basic_block_t *bb, function_t *f) {
    graph_node_t *gn;
    dag_node_t *dn;
    variable_t *v;
    list_t *l;

    int i;
    for (i = 0; i < f->argv->size; i++) {
        v = f->argv->data[i];

        assert(v->arg_flag);

        for (l = list_head(&f->dag_list_head); l != list_sentinel(&f->dag_list_head);
             l = list_next(l)) {
            dn = list_data(l, dag_node_t, list);
            if (dn->var == v)
                break;
        }

        // if arg is not found in dag, it's not used in function, ignore.
        if (l == list_sentinel(&f->dag_list_head))
            continue;

        int ret = _x64_rcg_make_node(&gn, g, dn, v->rabi, NULL);
        if (ret < 0)
            return ret;

        dn->rabi = v->rabi;

        if (dn->rabi)
            dn->loaded = 1;
        else
            dn->color = -1;
    }

    return 0;
}

static int _x64_argv_save(basic_block_t *bb, function_t *f) {
    list_t *l;
    3ac_code_t * c;

    if (!list_empty(&bb->code_list_head)) {
        l = list_head(&bb->code_list_head);
        c = list_data(l, 3ac_code_t, list);
    } else {
        c = 3ac_code_alloc();
        if (!c)
            return -ENOMEM;

        c->op = 3ac_find_operator(OP_3AC_NOP);
        c->basic_block = bb;

        list_add_tail(&bb->code_list_head, &c->list);
    }

    if (!c->instructions) {
        c->instructions = vector_alloc();
        if (!c->instructions)
            return -ENOMEM;
    }

    int i;
    for (i = 0; i < f->argv->size; i++) {
        variable_t *v = f->argv->data[i];

        assert(v->arg_flag);

        dag_node_t *dn;
        dag_node_t *dn2;
        dn_status_t *active;
        register_t *rabi;

        for (l = list_head(&f->dag_list_head); l != list_sentinel(&f->dag_list_head);
             l = list_next(l)) {
            dn = list_data(l, dag_node_t, list);
            if (dn->var == v)
                break;
        }

        if (l == list_sentinel(&f->dag_list_head))
            continue;

        if (!dn->rabi)
            continue;

        rabi = dn->rabi;

        int save_flag = 0;
        int j;

        if (dn->color > 0) {
            if (dn->color != rabi->color)
                save_flag = 1;
        } else
            save_flag = 1;

        for (j = 0; j < bb->dn_colors_entry->size; j++) {
            active = bb->dn_colors_entry->data[j];
            dn2 = active->dag_node;

            if (dn2 != dn && dn2->color > 0
                && X64_COLOR_CONFLICT(dn2->color, rabi->color)) {
                save_flag = 1;
                break;
            }
        }

        if (save_flag) {
            int ret = x64_save_var2(dn, rabi, c, f);
            if (ret < 0)
                return ret;
        } else {
            assert(0 == rabi->dag_nodes->size);

            int ret = vector_add(rabi->dag_nodes, dn);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

static int _x64_make_bb_rcg(graph_t *g, basic_block_t *bb, native_t *ctx) {
    list_t *l;
    3ac_code_t * c;
    x64_rcg_handler_pt h;

    for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_next(l)) {
        c = list_data(l, 3ac_code_t, list);

        h = x64_find_rcg_handler(c->op->type);
        if (!h) {
            loge("3ac operator '%s' not supported\n", c->op->name);
            return -EINVAL;
        }

        int ret = h(ctx, c, g);
        if (ret < 0)
            return ret;
    }

    logd("g->nodes->size: %d\n", g->nodes->size);
    return 0;
}

static int _x64_bb_regs_from_graph(basic_block_t *bb, graph_t *g) {
    int i;
    for (i = 0; i < g->nodes->size; i++) {
        graph_node_t *gn = g->nodes->data[i];
        x64_rcg_node_t *rn = gn->data;
        dag_node_t *dn = rn->dag_node;
        dn_status_t *ds;

        if (!dn) {
            //_x64_rcg_node_printf(rn);
            continue;
        }

        ds = dn_status_alloc(dn);
        if (!ds)
            return -ENOMEM;

        int ret = vector_add(bb->dn_colors_entry, ds);
        if (ret < 0) {
            dn_status_free(ds);
            return ret;
        }
        ds->color = gn->color;
        dn->color = gn->color;

        //_x64_rcg_node_printf(rn);
    }
    // printf("\n");

    return 0;
}

static int _x64_select_bb_regs(basic_block_t *bb, native_t *ctx) {
    x64_context_t *x64 = ctx->priv;
    function_t *f = x64->f;

    graph_t *g = graph_alloc();
    if (!g)
        return -ENOMEM;

    vector_t *colors = NULL;

    int ret = 0;
    int i;

    if (0 == bb->index) {
        ret = _x64_argv_prepare(g, bb, f);
        if (ret < 0)
            goto error;
    }

    ret = _x64_make_bb_rcg(g, bb, ctx);
    if (ret < 0)
        goto error;

    colors = x64_register_colors();
    if (!colors) {
        ret = -ENOMEM;
        goto error;
    }

    ret = x64_graph_kcolor(g, 16, colors);
    if (ret < 0)
        goto error;

    ret = _x64_bb_regs_from_graph(bb, g);

error:
    if (colors)
        vector_free(colors);

    graph_free(g);
    g = NULL;
    return ret;
}

static int _x64_select_bb_group_regs(bb_group_t *bbg, native_t *ctx) {
    x64_context_t *x64 = ctx->priv;
    function_t *f = x64->f;

    graph_t *g = graph_alloc();
    if (!g)
        return -ENOMEM;

    vector_t *colors = NULL;
    basic_block_t *bb;

    int ret = 0;
    int i;

    if (0 == bbg->pre->index) {
        ret = _x64_argv_prepare(g, bb, f);
        if (ret < 0)
            goto error;
    }

    for (i = 0; i < bbg->body->size; i++) {
        bb = bbg->body->data[i];

        ret = _x64_make_bb_rcg(g, bb, ctx);
        if (ret < 0)
            goto error;
    }

    colors = x64_register_colors();
    if (!colors) {
        ret = -ENOMEM;
        goto error;
    }

    ret = x64_graph_kcolor(g, 16, colors);
    if (ret < 0)
        goto error;

    ret = _x64_bb_regs_from_graph(bbg->pre, g);
    if (ret < 0)
        goto error;

error:
    if (colors)
        vector_free(colors);

    graph_free(g);
    g = NULL;
    return ret;
}

static int _x64_make_insts_for_list(native_t *ctx, list_t *h, int bb_offset) {
    list_t *l;
    int ret;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        3ac_code_t *c = list_data(l, 3ac_code_t, list);

        x64_inst_handler_pt h = x64_find_inst_handler(c->op->type);
        if (!h) {
            loge("3ac operator '%s' not supported\n", c->op->name);
            return -EINVAL;
        }

        ret = h(ctx, c);
        if (ret < 0) {
            3ac_code_print(c, NULL);
            loge("3ac op '%s' make inst failed\n", c->op->name);
            return ret;
        }

        if (!c->instructions)
            continue;

        //		3ac_code_print(c, NULL);
        //		_x64_inst_printf(c);
    }

    return bb_offset;
}

static void _x64_set_offset_for_jmps(native_t *ctx, function_t *f) {
    while (1) {
        int drop_bytes = 0;
        int i;

        for (i = 0; i < f->jmps->size; i++) {
            3ac_code_t *c = f->jmps->data[i];

            3ac_operand_t *dst = c->dsts->data[0];
            basic_block_t *cur_bb = c->basic_block;
            basic_block_t *dst_bb = dst->bb;

            basic_block_t *bb = NULL;
            list_t *l = NULL;
            int32_t bytes = 0;

            if (cur_bb->index < dst_bb->index) {
                for (l = list_next(&cur_bb->list); l != &dst_bb->list; l = list_next(l)) {
                    bb = list_data(l, basic_block_t, list);

                    bytes += bb->code_bytes;
                }
            } else {
                for (l = &cur_bb->list; l != list_prev(&dst_bb->list); l = list_prev(l)) {
                    bb = list_data(l, basic_block_t, list);

                    bytes -= bb->code_bytes;
                }
            }

            assert(c->instructions && 1 == c->instructions->size);

            int nb_bytes;
            if (-128 <= bytes && bytes <= 127)
                nb_bytes = 1;
            else
                nb_bytes = 4;

            instruction_t *inst = c->instructions->data[0];
            x64_OpCode_t *jcc = x64_find_OpCode(inst->OpCode->type, nb_bytes, nb_bytes, X64_I);

            int old_len = inst->len;
            x64_make_inst_I2(inst, jcc, (uint8_t *)&bytes, nb_bytes);
            int diff = old_len - inst->len;
            assert(diff >= 0);

            cur_bb->code_bytes -= diff;
            c->inst_bytes -= diff;
            drop_bytes += diff;
        }

        if (0 == drop_bytes)
            break;
    }
}

static void _x64_set_offset_for_relas(native_t *ctx, function_t *f, vector_t *relas) {
    int i;
    for (i = 0; i < relas->size; i++) {
        rela_t *rela = relas->data[i];
        3ac_code_t *c = rela->code;
        instruction_t *inst = rela->inst;
        basic_block_t *cur_bb = c->basic_block;

        instruction_t *inst2;
        basic_block_t *bb;
        list_t *l;

        int bytes = f->init_code_bytes;

        for (l = list_head(&f->basic_block_list_head); l != &cur_bb->list;
             l = list_next(l)) {
            bb = list_data(l, basic_block_t, list);
            bytes += bb->code_bytes;
        }

        bytes += c->bb_offset;

        int j;
        for (j = 0; j < c->instructions->size; j++) {
            inst2 = c->instructions->data[j];

            if (inst2 == inst)
                break;

            bytes += inst2->len;
        }

        rela->inst_offset += bytes;
    }
}

static int _x64_bbg_fix_saves(bb_group_t *bbg, function_t *f) {
    basic_block_t *pre;
    basic_block_t *post;
    basic_block_t *bb;
    3ac_operand_t * src;
    dag_node_t *dn;
    dag_node_t *dn2;
    3ac_code_t * c;
    3ac_code_t * c2;
    list_t *l;
    list_t *l2;

    int i;
    //	int j;

    if (0 == bbg->posts->size)
        return 0;

    pre = bbg->pre;
    post = bbg->posts->data[0];

    for (l = list_head(&post->save_list_head); l != list_sentinel(&post->save_list_head);) {
        c = list_data(l, 3ac_code_t, list);
        l = list_next(l);

        assert(c->srcs && 1 == c->srcs->size);
        src = c->srcs->data[0];
        dn = src->dag_node;

        variable_t *v = dn->var;

        intptr_t color = x64_bb_find_color(pre->dn_colors_exit, dn);
        int updated = 0;

        for (i = 0; i < bbg->body->size; i++) {
            bb = bbg->body->data[i];

            intptr_t color2 = x64_bb_find_color(bb->dn_colors_exit, dn);

            if (color2 != color)
                updated++;
        }

        if (0 == updated) {
            if (color <= 0)
                continue;

            int ret = x64_bb_save_dn(color, dn, c, post, f);
            if (ret < 0)
                return ret;

            list_del(&c->list);
            list_add_tail(&post->code_list_head, &c->list);

            for (i = 1; i < bbg->posts->size; i++) {
                bb = bbg->posts->data[i];

                l2 = list_head(&bb->save_list_head);
                c2 = list_data(l2, 3ac_code_t, list);

                assert(c2->srcs && 1 == c2->srcs->size);
                src = c2->srcs->data[0];
                dn2 = src->dag_node;

                ret = x64_bb_save_dn(color, dn2, c2, bb, f);
                if (ret < 0)
                    return ret;

                list_del(&c2->list);
                list_add_tail(&bb->code_list_head, &c2->list);
            }
        } else {
            list_del(&c->list);
            3ac_code_free(c);
            c = NULL;

            for (i = 1; i < bbg->posts->size; i++) {
                bb = bbg->posts->data[i];

                l2 = list_head(&bb->save_list_head);
                c2 = list_data(l2, 3ac_code_t, list);

                list_del(&c2->list);

                3ac_code_free(c2);
                c2 = NULL;
            }
#if 0
			variable_t* v = dn->var;
			loge("save: v_%d_%d/%s, dn->color: %ld\n",
					v->w->line, v->w->pos, v->w->text->data, dn->color);
#endif
            for (i = 0; i < bbg->body->size; i++) {
                bb = bbg->body->data[i];

                for (l2 = list_head(&bb->save_list_head); l2 != list_sentinel(&bb->save_list_head); l2 = list_next(l2)) {
                    c = list_data(l2, 3ac_code_t, list);

                    assert(c->srcs && 1 == c->srcs->size);
                    src = c->srcs->data[0];

                    if (dn == src->dag_node)
                        break;
                }
                if (l2 == list_sentinel(&bb->save_list_head))
                    continue;

                intptr_t color = x64_bb_find_color(bb->dn_colors_exit, dn);
                if (color <= 0)
                    continue;

                int ret = x64_bb_save_dn(color, dn, c, bb, f);
                if (ret < 0)
                    return ret;

                list_del(&c->list);
                list_add_tail(&bb->code_list_head, &c->list);
            }
        }
    }

    return 0;
}

static void _x64_bbg_fix_loads(bb_group_t *bbg) {
    if (0 == bbg->body->size)
        return;

    basic_block_t *pre;
    basic_block_t *bb;
    3ac_operand_t * dst;
    dn_status_t *ds;
    dag_node_t *dn;
    3ac_code_t * c;
    list_t *l;

    int i;
    int j;

    pre = bbg->pre;
    bb = bbg->body->data[0];

    for (i = 0; i < pre->dn_colors_exit->size; i++) {
        ds = pre->dn_colors_exit->data[i];

        dn = ds->dag_node;

        if (ds->color <= 0)
            continue;

        if (ds->color == dn->color)
            continue;
#if 0
		variable_t* v = dn->var;
		if (v->w)
			logw("v_%d_%d/%s, ", v->w->line, v->w->pos, v->w->text->data);
		else
			logw("v_%#lx, ", 0xffff & (uintptr_t)v);
		printf("ds->color: %ld, dn->color: %ld\n", ds->color, dn->color);
#endif
        for (l = list_head(&pre->code_list_head); l != list_sentinel(&pre->code_list_head);) {
            c = list_data(l, 3ac_code_t, list);
            l = list_next(l);

            dst = c->dsts->data[0];

            if (dst->dag_node == dn) {
                list_del(&c->list);
                list_add_front(&bb->code_list_head, &c->list);
                c->basic_block = bb;
                break;
            }
        }
    }
}

static void _x64_set_offsets(function_t *f) {
    instruction_t *inst;
    basic_block_t *bb;
    3ac_code_t * c;
    list_t *l;
    list_t *l2;

    int i;

    for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
         l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        bb->code_bytes = 0;

        for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);
             l2 = list_next(l2)) {
            c = list_data(l2, 3ac_code_t, list);

            c->inst_bytes = 0;
            c->bb_offset = bb->code_bytes;

            if (!c->instructions)
                continue;

            for (i = 0; i < c->instructions->size; i++) {
                inst = c->instructions->data[i];

                c->inst_bytes += inst->len;
            }

            bb->code_bytes += c->inst_bytes;
        }
    }
}

int _x64_select_inst(native_t *ctx) {
    x64_context_t *x64 = ctx->priv;
    function_t *f = x64->f;
    basic_block_t *bb;
    bb_group_t *bbg;

    int i;
    int ret = 0;
    list_t *l;

    for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
         l = list_next(l)) {
        bb = list_data(l, basic_block_t, list);

        if (bb->group_flag || bb->loop_flag)
            continue;

        ret = _x64_select_bb_regs(bb, ctx);
        if (ret < 0)
            return ret;

        x64_init_bb_colors(bb);

        ret = _x64_make_insts_for_list(ctx, &bb->code_list_head, 0);
        if (ret < 0)
            return ret;
    }

    for (i = 0; i < f->bb_groups->size; i++) {
        bbg = f->bb_groups->data[i];

        ret = _x64_select_bb_group_regs(bbg, ctx);
        if (ret < 0)
            return ret;

        x64_init_bb_colors(bbg->pre);

        int j;
        for (j = 0; j < bbg->body->size; j++) {
            bb = bbg->body->data[j];

            assert(!bb->native_flag);

            if (0 != j) {
                ret = x64_load_bb_colors2(bb, bbg, f);
                if (ret < 0)
                    return ret;
            }

            logd("************ bb: %d\n", bb->index);
            ret = _x64_make_insts_for_list(ctx, &bb->code_list_head, 0);
            if (ret < 0)
                return ret;
            bb->native_flag = 1;
            logd("************ bb: %d\n", bb->index);

            ret = x64_save_bb_colors(bb->dn_colors_exit, bbg, bb);
            if (ret < 0)
                return ret;
        }
    }

    for (i = 0; i < f->bb_loops->size; i++) {
        bbg = f->bb_loops->data[i];

        ret = _x64_select_bb_group_regs(bbg, ctx);
        if (ret < 0)
            return ret;

        x64_init_bb_colors(bbg->pre);

        ret = _x64_make_insts_for_list(ctx, &bbg->pre->code_list_head, 0);
        if (ret < 0)
            return ret;

        ret = x64_save_bb_colors(bbg->pre->dn_colors_exit, bbg, bbg->pre);
        if (ret < 0)
            return ret;

        int j;
        for (j = 0; j < bbg->body->size; j++) {
            bb = bbg->body->data[j];

            assert(!bb->native_flag);

            ret = x64_load_bb_colors(bb, bbg, f);
            if (ret < 0)
                return ret;

            ret = _x64_make_insts_for_list(ctx, &bb->code_list_head, 0);
            if (ret < 0)
                return ret;
            bb->native_flag = 1;

            ret = x64_save_bb_colors(bb->dn_colors_exit, bbg, bb);
            if (ret < 0)
                return ret;
        }

        _x64_bbg_fix_loads(bbg);

        ret = _x64_bbg_fix_saves(bbg, f);
        if (ret < 0)
            return ret;

        for (j = 0; j < bbg->body->size; j++) {
            bb = bbg->body->data[j];

            ret = x64_fix_bb_colors(bb, bbg, f);
            if (ret < 0)
                return ret;
        }
    }
#if 1
    if (x64_optimize_peephole(ctx, f) < 0) {
        loge("\n");
        return -1;
    }
#endif
    _x64_set_offsets(f);

    _x64_set_offset_for_jmps(ctx, f);
    return 0;
}

static int _find_local_vars(node_t *node, void *arg, vector_t *results) {
    block_t *b = (block_t *)node;

    if ((OP_BLOCK == b->node.type || FUNCTION == b->node.type) && b->scope) {
        int i;
        for (i = 0; i < b->scope->vars->size; i++) {
            variable_t *var = b->scope->vars->data[i];

            int ret = vector_add(results, var);
            if (ret < 0)
                return ret;
        }
    }
    return 0;
}

int x64_select_inst(native_t *ctx, function_t *f) {
    x64_context_t *x64 = ctx->priv;

    x64->f = f;

    vector_t *local_vars = vector_alloc();
    if (!local_vars)
        return -ENOMEM;

    int ret = node_search_bfs((node_t *)f, NULL, local_vars, -1, _find_local_vars);
    if (ret < 0)
        return ret;

    int local_vars_size = _x64_function_init(f, local_vars);
    if (local_vars_size < 0)
        return -1;

    logi("---------- %s() ------------\n", f->node.w->text->data);

    int i;
    for (i = 0; i < local_vars->size; i++) {
        variable_t *v = local_vars->data[i];
        assert(v->w);

        logd("v: %p, name: %s_%d_%d, size: %d, bp_offset: %d, arg_flag: %d\n",
             v, v->w->text->data, v->w->line, v->w->pos,
             variable_size(v), v->bp_offset, v->arg_flag);
    }

    f->local_vars_size = local_vars_size;
    f->bp_used_flag = 1;
    f->call_flag = 0;

    ret = _x64_select_inst(ctx);
    if (ret < 0)
        return ret;

    ret = _x64_function_finish(f);
    if (ret < 0)
        return ret;

    _x64_set_offset_for_relas(ctx, f, f->text_relas);
    _x64_set_offset_for_relas(ctx, f, f->data_relas);
    return 0;
}

native_ops_t native_ops_x64 = {
    .name = "x64",

    .open = x64_open,
    .close = x64_close,

    .select_inst = x64_select_inst,
};
