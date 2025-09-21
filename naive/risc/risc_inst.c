#include"risc.h"

#define RISC_INST_OP3_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 2) \
		return -EINVAL; \
	\
	risc_context_t* risc = ctx->priv; \
	function_t*      f     = risc->f; \
	\
	_3ac_operand_t*   dst   = c->dsts->data[0]; \
	_3ac_operand_t*   src0  = c->srcs->data[0]; \
	_3ac_operand_t*   src1  = c->srcs->data[1]; \
	\
	if (!src0 || !src0->dag_node) \
		return -EINVAL; \
	\
	if (!src1 || !src1->dag_node) \
		return -EINVAL; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (src0->dag_node->var->size != src1->dag_node->var->size) {\
		loge("size: %d, %d\n", src0->dag_node->var->size, src1->dag_node->var->size); \
		return -EINVAL; \
	}


static int _risc_inst_call_stack_size(_3ac_code_t* c, function_t* f)
{
	int stack_size = 0;

	int i;
	for (i = 1; i < c->srcs->size; i++) {
		_3ac_operand_t*  src = c->srcs->data[i];
		variable_t*     v   = src->dag_node->var;

		if (src->dag_node->rabi2)
			continue;

		int size = f->rops->variable_size(v);
		if (size & 0x7)
			size = (size + 7) >> 3 << 3;

		v->sp_offset  = stack_size;
		stack_size   += size;
	}
	assert(0 == (stack_size & 0x7));

	if (stack_size & 0xf)
		stack_size += 8;

	return stack_size;
}

static int _risc_inst_call_argv(native_t* ctx, _3ac_code_t* c, function_t* f)
{
	register_t* sp  = f->rops->find_register("sp");

	risc_OpCode_t*   lea;
	risc_OpCode_t*   mov;
	risc_OpCode_t*   movx;
	instruction_t*   inst;

	uint32_t opcode;

	int nb_floats = 0;
	int ret;
	int i;
	for (i = c->srcs->size - 1; i >= 1; i--) {

		_3ac_operand_t* src  = c->srcs->data[i];
		variable_t*    v    = src->dag_node->var;
		register_t*    rd   = src->rabi;
		register_t*    rabi = src->dag_node->rabi2;
		register_t*    rs   = NULL;

		int size     = f->rops->variable_size (v);
		int is_float =      variable_float(v);

		if (!rabi || is_float != RISC_COLOR_TYPE(rabi->color)) {

			rabi = f->rops->find_register_type_id_bytes(is_float, RISC_REG_X0,  size);

			ret  = f->rops->overflow_reg(rabi, c, f);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
		}

		logd("i: %d, size: %d, v: %s, rabi: %s\n", i, size, v->w->text->data, rabi->name);

		movx = NULL;

		if (!is_float) {
			mov  = risc_find_OpCode(RISC_MOV, f->rops->MAX_BYTES, f->rops->MAX_BYTES, RISC_G2E);

			if (size < f->rops->MAX_BYTES) {

				if (variable_signed(v))
					movx = risc_find_OpCode(RISC_MOVSX, size, f->rops->MAX_BYTES, RISC_E2G);
				else if (size < 4)
					movx = risc_find_OpCode(RISC_MOVZX, size, f->rops->MAX_BYTES, RISC_E2G);
			}

			if (0 == src->dag_node->color) {

				ret = f->rops->overflow_reg(rabi, c, f);
				if (ret < 0) {
					loge("\n");
					return ret;
				}

				ret = risc_load_const(rabi, src->dag_node, c, f);
				if (ret < 0) {
					loge("\n");
					return ret;
				}

				rabi = f->rops->find_register_color_bytes(rabi->color, f->rops->MAX_BYTES);
				rs   = rabi;
			} else {
				if (src->dag_node->color < 0)
					src->dag_node->color = rabi->color;
			}
		} else {
			nb_floats++;

			if (0 == src->dag_node->color) {
				src->dag_node->color = -1;
				v->global_flag       =  1;
			}

			if (VAR_FLOAT == v->type) {
				mov  = risc_find_OpCode(RISC_MOVSS,    4, 4, RISC_G2E);
				movx = risc_find_OpCode(RISC_CVTSS2SD, 4, 8, RISC_E2G);
			} else
				mov  = risc_find_OpCode(RISC_MOVSD, size, size, RISC_G2E);

			if (src->dag_node->color < 0)
				src->dag_node->color = rabi->color;
		}

		if (!rs) {
			assert(src->dag_node->color > 0);

			if (rd && f->rops->color_conflict(rd->color, src->dag_node->color)) {

				ret = f->rops->overflow_reg2(rd, src->dag_node, c, f);
				if (ret < 0) {
					loge("\n");
					return ret;
				}
			}

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

			rs = f->rops->find_register_color_bytes(rs->color, is_float ? 8 : f->rops->MAX_BYTES);
		}

		if (movx) {
			if (RISC_MOVSX == movx->type) {

				inst = ctx->iops->MOVSX(c, rs, rs, size);

			} else if (RISC_MOVZX == movx->type) {

				inst = ctx->iops->MOVZX(c, rs, rs, size);

			} else {
				assert(RISC_CVTSS2SD == movx->type);

				inst = ctx->iops->CVTSS2SD(c, rs, rs);
			}

			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		if (!rd) {
			ret = ctx->iops->G2P(c, f, rs, sp, v->sp_offset, size);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
			continue;
		}

		ret = f->rops->overflow_reg2(rd, src->dag_node, c, f);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		if (!f->rops->color_conflict(rd->color, rs->color)) {
			rd     = f->rops->find_register_color_bytes(rd->color, rs->bytes);

			loge("i: %d, rd: %s, rs: %s\n", i, rd->name, rs->name);

			if (!is_float)
				inst   = ctx->iops->MOV_G(c, rd, rs);
			else
				inst   = ctx->iops->FMOV_G(c, rd, rs);

			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		ret = risc_rcg_make(c, c->rcg, NULL, rd);
		if (ret < 0)
			return ret;
	}

	return nb_floats;
}

static int _risc_call_save_ret_regs(_3ac_code_t* c, function_t* f, function_t* pf)
{
	register_t* r;
	variable_t*     v;

	int i;
	for (i = 0; i < pf->rets->size; i++) {
		v  =        pf->rets->data[i];

		int is_float = variable_float(v);

		if (is_float) {

			if (i > 0) {
				loge("\n");
				return -1;
			}

			r = f->rops->find_register_type_id_bytes(is_float, 0, 8);
		} else
			r = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_ret_regs[i], f->rops->MAX_BYTES);

		int ret = f->rops->overflow_reg(r, c, f);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}
	return 0;
}

static int _risc_dst_reg_valid(function_t* f, register_t* rd, register_t** updated_regs, int nb_updated, int abi_idx, int abi_total)
{
	register_t* r;

	int i;
	for (i = 0; i < nb_updated; i++) {

		r  = updated_regs[i];

		if (f->rops->color_conflict(r->color, rd->color))
			return 0;
	}

	for (i = abi_idx; i < abi_total; i++) {

		r  = f->rops->find_register_type_id_bytes(RISC_COLOR_TYPE(rd->color), f->rops->abi_ret_regs[i], rd->bytes);

		if (f->rops->color_conflict(r->color, rd->color))
			return 0;
	}

	return 1;
}

static int _risc_call_update_dsts(native_t* ctx, _3ac_code_t* c, function_t* f, register_t** updated_regs, int max_updated)
{
	_3ac_operand_t*  dst;
	dag_node_t*     dn;
	variable_t*     v;

	register_t* rd;
	register_t* rs;
	risc_OpCode_t*   mov;

	int nb_float   = 0;
	int nb_int     = 0;

	int i;
	for (i  = 0; i < c->dsts->size; i++) {
		dst =        c->dsts->data[i];
		dn  =        dst->dag_node;
		v   =        dn->var;

		if (VAR_VOID == v->type && 0 == v->nb_pointers)
			continue;

		assert(0 != dn->color);

		int is_float = variable_float(v);

		if (is_float)
			nb_float++;
		else
			nb_int++;
	}

	int nb_updated = 0;
	int idx_float  = 0;
	int idx_int    = 0;

	for (i  = 0; i < c->dsts->size; i++) {
		dst =        c->dsts->data[i];
		dn  =        dst->dag_node;
		v   =        dn->var;

		if (VAR_VOID == v->type && 0 == v->nb_pointers)
			continue;

		int is_float = variable_float(v);
		int dst_size = f->rops->variable_size (v);

		if (is_float) {
			if (i > 0) {
				loge("\n");
				return -1;
			}

			loge("\n");
			return -EINVAL;

			rs = f->rops->find_register_type_id_bytes(is_float, RISC_REG_X0, dst_size);

			if (VAR_FLOAT == dn->var->type)
				mov = risc_find_OpCode(RISC_MOVSS, dst_size, dst_size, RISC_G2E);
			else
				mov = risc_find_OpCode(RISC_MOVSD, dst_size, dst_size, RISC_G2E);

			idx_float++;
		} else {
			rs  = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_ret_regs[idx_int], dst_size);

			mov = risc_find_OpCode(RISC_MOV, dst_size, dst_size, RISC_G2E);

			idx_int++;
		}

		instruction_t*  inst;

		if (dn->color > 0) {
			rd = f->rops->find_register_color(dn->color);

			int rd_vars = f->rops->reg_cached_vars(rd);

			if (rd_vars > 1) {
				dn->color  = -1;
				dn->loaded = 0;
				vector_del(rd->dag_nodes, dn);

			} else if (rd_vars > 0
					&& !vector_find(rd->dag_nodes, dn)) {
				dn->color  = -1;
				dn->loaded = 0;

			} else {
				RISC_SELECT_REG_CHECK(&rd, dn, c, f, 0);

				if (dn->color == rs->color) {
					assert(nb_updated < max_updated);

					updated_regs[nb_updated++] = rs;
					continue;
				}

				int valid = _risc_dst_reg_valid(f, rd, updated_regs, nb_updated, idx_int, nb_int);
				if (valid) {
					inst = ctx->iops->MOV_G(c, rd, rs);
					RISC_INST_ADD_CHECK(c->instructions, inst);

					assert(nb_updated < max_updated);

					updated_regs[nb_updated++] = rd;
					continue;
				}

				assert(0  == vector_del(rd->dag_nodes, dn));
				dn->color  = -1;
				dn->loaded =  0;
			}
		}

		int rs_vars = f->rops->reg_cached_vars(rs);

		if (0 == rs_vars) {
			if (vector_add(rs->dag_nodes, dn) < 0)
				return -ENOMEM;

			assert(nb_updated < max_updated);

			updated_regs[nb_updated++] = rs;

			dn->color  = rs->color;
			dn->loaded = 1;

		} else {
			int ret = ctx->iops->G2M(c, f, rs, NULL, dn->var);
			if (ret < 0)
				return ret;
		}
	}
	return nb_updated;
}

static int _risc_inst_call_handler(native_t* ctx, _3ac_code_t* c)
{
	if (c->srcs->size < 1) {
		loge("\n");
		return -EINVAL;
	}

	instruction_t*  inst_sp2 = NULL;
	instruction_t*  inst_sp  = NULL;
	instruction_t*  inst;

	risc_context_t* risc   = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  src0   = c->srcs->data[0];
	variable_t*     var_pf = src0->dag_node->var;
	function_t*     pf     = var_pf->func_ptr;

	if (FUNCTION_PTR != var_pf->type || !pf) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* lr  = f->rops->find_register("lr");
	register_t* sp  = f->rops->find_register("sp");
	register_t* x0  = f->rops->find_register("x0");

	lr->used = 1;
	sp->used = 1;

	int data_rela_size = f->data_relas->size;
	int text_rela_size = f->text_relas->size;
	logd("f->data_relas->size: %d, f->text_relas->size: %d\n", f->data_relas->size, f->text_relas->size);

	uint32_t opcode;
	int ret;
	int i;

	if (pf->rets) {
		ret = _risc_call_save_ret_regs(c, f, pf);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	ret = f->rops->overflow_reg(x0, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (f->rops->call_rabi_varg && pf->vargs_flag)
		f->rops->call_rabi_varg(c, f);
	else
		f->rops->call_rabi(c, f, NULL, NULL, NULL);

	int32_t stack_size = _risc_inst_call_stack_size(c, f);
	if (stack_size > 0) {
		inst_sp  = risc_make_inst(c, 0);
		inst_sp2 = risc_make_inst(c, 0);
		RISC_INST_ADD_CHECK(c->instructions, inst_sp);
		RISC_INST_ADD_CHECK(c->instructions, inst_sp2);
	}

	ret = _risc_inst_call_argv(ctx, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	register_t* saved_regs[RISC_ABI_CALLER_SAVES_MAX];

	int save_size = f->rops->caller_save_regs(c, f, f->rops->abi_caller_saves, f->rops->ABI_CALLER_SAVES_NB, stack_size, saved_regs);
	if (save_size < 0) {
		loge("\n");
		return save_size;
	}

	logd("stack_size: %d, save_size: %d\n", stack_size, save_size);

	if (stack_size > 0) {
		assert(inst_sp);
		assert(inst_sp2);

		if (stack_size >  0xfff) {
			stack_size += 1 << 12;
			stack_size &= ~0xfff;

			if (stack_size < 0 || (stack_size >> 12) > 0xfff) {
				loge("\n");
				return -EINVAL;
			}
		}

		inst = ctx->iops->SUB_IMM(c, f, sp, sp, stack_size);
		if (inst) {
			memcpy(inst_sp->code, inst->code, 4);
			free(inst);
			inst = NULL;
		} else
			return -ENOMEM;

		if (save_size > 0xfff) {
			loge("\n");
			return -EINVAL;
		}

		if (save_size > 0) {
			inst = ctx->iops->SUB_IMM(c, f, sp, sp, save_size);
			if (inst) {
				memcpy(inst_sp2->code, inst->code, 4);
				free(inst);
				inst = NULL;
			} else
				return -ENOMEM;

		} else {
			vector_del(c->instructions, inst_sp2);

			free(inst_sp2);
			inst_sp2 = NULL;
		}

	} else {
		vector_del(c->instructions, inst_sp);
		vector_del(c->instructions, inst_sp2);

		free(inst_sp);
		free(inst_sp2);

		inst_sp  = NULL;
		inst_sp2 = NULL;
	}

	if (var_pf->const_literal_flag) {
		assert(0 == src0->dag_node->color);

		ret = ctx->iops->BL(c, f, pf);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

	} else {
		assert(0 != src0->dag_node->color);

		if (src0->dag_node->color > 0) {

			register_t* r_pf = NULL;

			ret = risc_select_reg(&r_pf, src0->dag_node, c, f, 1);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			inst   = ctx->iops->BLR(c, r_pf);
			RISC_INST_ADD_CHECK(c->instructions, inst);

		} else {
			loge("\n");
			return -EINVAL;
		}
	}

	if (stack_size > 0) {
		inst = ctx->iops->ADD_IMM(c, f, sp, sp, stack_size);
		RISC_INST_ADD_CHECK(c->instructions, inst);
	}

	int nb_updated = 0;
	register_t* updated_regs[RISC_ABI_RET_MAX * 2];

	if (pf->rets && pf->rets->size > 0 && c->dsts) {

		nb_updated = _risc_call_update_dsts(ctx, c, f, updated_regs, f->rops->ABI_RET_NB * 2);
		if (nb_updated < 0) {
			loge("\n");
			return nb_updated;
		}
	}

	if (save_size > 0) {
		ret = f->rops->pop_regs(c, f, saved_regs, save_size / f->rops->MAX_BYTES, updated_regs, nb_updated);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	return 0;
}

static int _risc_inst_bit_not_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t*  risc  = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	instruction_t*    inst = NULL;
	register_t*       rd   = NULL;
	register_t*       rs   = NULL;
	dag_node_t*       s    = src->dag_node;
	dag_node_t*       d    = dst->dag_node;

	uint32_t opcode;

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

	if (0 == s->color) {
		uint64_t u = s->var->data.u64;

		int ret = ctx->iops->I2G(c, rd, u, 8);
		if (ret < 0)
			return ret;

		opcode = (0xaa << 24) | (0x1 << 21) | (rd->id << 16) | (0x1f << 10) | rd->id;
		inst = ctx->iops->MVN(c, rd, rd);

	} else {
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst = ctx->iops->MVN(c, rd, rs);
	}

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_inc_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	register_t* rs    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    src   = c->srcs->data[0];
	instruction_t*    inst  = NULL;
	function_t*       f     = risc->f;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = ctx->iops->ADD_IMM(c, f, rs, rs, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

static int _risc_inst_inc_post_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* rd    = NULL;
	register_t* rs    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    src   = c->srcs->data[0];
	_3ac_operand_t*    dst   = c->dsts->data[0];
	instruction_t*    inst  = NULL;
	function_t*       f     = risc->f;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);
	RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = ctx->iops->MOV_G(c, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst   = ctx->iops->ADD_IMM(c, f, rs, rs, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_dec_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	register_t* rs    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    src   = c->srcs->data[0];
	instruction_t*    inst  = NULL;
	function_t*       f     = risc->f;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = ctx->iops->SUB_IMM(c, f, rs, rs, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

static int _risc_inst_dec_post_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* rd    = NULL;
	register_t* rs    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    src   = c->srcs->data[0];
	_3ac_operand_t*    dst   = c->dsts->data[0];
	instruction_t*    inst  = NULL;
	function_t*       f     = risc->f;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);
	RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = ctx->iops->MOV_G(c, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->SUB_IMM(c, f, rs, rs, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_neg_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	assert(0 != dst->dag_node->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rd   = NULL;
	register_t* rs   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       s    = src->dag_node;
	dag_node_t*       d    = dst->dag_node;
	variable_t*       v    = d->var;

	uint32_t opcode;

	int is_float = variable_float(v);
	int size     = f->rops->variable_size(v);

	if (!is_float) {

		RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

		if (0 == s->color) {
			uint64_t u = s->var->data.u64;

			int ret = ctx->iops->I2G(c, rd, u, 8);
			if (ret < 0)
				return ret;

			inst = ctx->iops->NEG(c, rd, rd);

		} else {
			RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

			inst = ctx->iops->NEG(c, rd, rs);
		}

		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

#if 0
	risc_OpCode_t*   pxor = risc_find_OpCode(RISC_PXOR,  8, 8, RISC_E2G);
	risc_OpCode_t*   sub  = risc_find_OpCode(RISC_SUBSS, 4, 4, RISC_E2G);

	if (v->size > 4)
		sub = risc_find_OpCode(RISC_SUBSD, 8, 8, RISC_E2G);

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);
	inst = ctx->iops->E2G(pxor, rd, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (src->dag_node->color > 0) {
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		inst = ctx->iops->E2G(sub, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);

	} else {
		rela_t* rela = NULL;

		v = src->dag_node->var;

		if (0 == src->dag_node->color) {
			v->global_flag = 1;
			v->local_flag  = 0;
			v->tmp_flag    = 0;
		}

		inst = ctx->iops->M2G(&rela, sub, rd, NULL, v);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		RISC_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
	}
#endif
	return -1;
}

static int _risc_inst_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vd  = dst   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0)
		return ret;

	ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))

		return ctx->iops->ADRP2G(c, f, rd, sib.base, sib.disp);

	if (sib.index)
		return ctx->iops->SIB2G(c, f, rd, &sib);

	return ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_binary_assign_pointer(native_t* ctx, _3ac_code_t* c, uint32_t op)
{
}

static int _risc_inst_inc_dec_post_pointer(native_t* ctx, _3ac_code_t* c, uint32_t u24)
{
}

static int _risc_inst_inc_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	uint32_t opcode;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vb  = base  ->dag_node->var;
	variable_t*       vm  = member->dag_node->var;
	register_t* r   = NULL;
	sib_t           sib = {0};

	int size = f->rops->variable_size(vm);

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vm);
	if (is_float) {
		loge("\n");
		return -EINVAL;
	}

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, r, &sib);
	else
		ret = ctx->iops->P2G(c, f, r, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, r, &sib);
	else
		ret = ctx->iops->G2P(c, f, r, sib.base, sib.disp, sib.size);
	return ret;
}

static int _risc_inst_dec_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	uint32_t opcode;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vb  = base  ->dag_node->var;
	variable_t*       vm  = member->dag_node->var;
	register_t* r   = NULL;
	sib_t           sib = {0};

	int size = f->rops->variable_size(vm);

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vm);
	if (is_float) {
		loge("\n");
		return -EINVAL;
	}

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, r, &sib);
	else
		ret = ctx->iops->P2G(c, f, r, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, r, &sib);
	else
		ret = ctx->iops->G2P(c, f, r, sib.base, sib.disp, sib.size);
	return ret;
}

static int _risc_inst_inc_post_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	uint32_t opcode;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vd  = dst   ->dag_node->var;
	variable_t*       vb  = base  ->dag_node->var;
	variable_t*       vm  = member->dag_node->var;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int size = f->rops->variable_size(vm);

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vm);
	if (is_float) {
		loge("\n");
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_dec_post_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	uint32_t opcode;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vd  = dst   ->dag_node->var;
	variable_t*       vb  = base  ->dag_node->var;
	variable_t*       vm  = member->dag_node->var;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int size = f->rops->variable_size(vm);

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vm);
	if (is_float) {
		loge("\n");
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_address_of_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rd  = NULL;
	sib_t           sib = {0};

	variable_t*       vd  = dst   ->dag_node->var;
	variable_t*       vb  = base  ->dag_node->var;
	variable_t*       vm  = member->dag_node->var;

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	return ctx->iops->ADRP2G(c, f, rd, sib.base, sib.disp);
}

static int _risc_inst_binary_assign_array_index(native_t* ctx, _3ac_code_t* c, uint32_t op)
{
	return 0;
}

static int _risc_inst_inc_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];

	uint32_t opcode;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;

	register_t* r      = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int size = vb->data_size;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;

	r = f->rops->find_register_color_bytes(r->color, size);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, r, &sib);
	else
		ret = ctx->iops->P2G(c, f, r, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, r, &sib);
	else
		ret = ctx->iops->G2P(c, f, r, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_dec_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];

	uint32_t opcode;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;

	register_t* r      = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int size = vb->data_size;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;

	r = f->rops->find_register_color_bytes(r->color, size);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, r, &sib);
	else
		ret = ctx->iops->P2G(c, f, r, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, r, &sib);
	else
		ret = ctx->iops->G2P(c, f, r, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_inc_post_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];

	uint32_t opcode;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base ->dag_node->var;
	variable_t*       vd     = dst  ->dag_node->var;

	register_t* rd     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int size = vb->data_size;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_dec_post_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];

	uint32_t opcode;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base ->dag_node->var;
	variable_t*       vd     = dst  ->dag_node->var;

	register_t* rd     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int size = vb->data_size;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  dst    = c->dsts->data[0];
	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vd  = dst  ->dag_node->var;
	variable_t*     vb  = base ->dag_node->var;
	variable_t*     vi  = index->dag_node->var;
	variable_t*     vs  = scale->dag_node->var;

	register_t* rd  = NULL;
	sib_t           sib = {0};

	instruction_t*  inst;

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	int ret = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (vb->nb_dimentions > 1) {

		if (sib.index)
			ret = ctx->iops->ADRSIB2G(c, f, rd, &sib);
		else
			ret = ctx->iops->ADRP2G(c, f, rd, sib.base, sib.disp);
		return ret;
	}

	if (sib.index)
		return ctx->iops->SIB2G(c, f, rd, &sib);

	return ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_address_of_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   base  = c->srcs->data[0];
	_3ac_operand_t*   index = c->srcs->data[1];
	_3ac_operand_t*   scale = c->srcs->data[2];
	instruction_t*   inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vd  = dst  ->dag_node->var;
	variable_t*       vb  = base ->dag_node->var;
	variable_t*       vi  = index->dag_node->var;
	variable_t*       vs  = scale->dag_node->var;

	register_t* rd  = NULL;
	sib_t           sib = {0};

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	int ret = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (sib.index)
		ret = ctx->iops->ADRSIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->ADRP2G(c, f, rd, sib.base, sib.disp);

	return ret;
}

static int _risc_inst_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	risc_context_t*  risc  = ctx->priv;
	_3ac_operand_t*   base  = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];
	instruction_t*   inst  = NULL;
	function_t*      f     = risc->f;
	register_t*      rd    = NULL;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	sib_t sib = {0};

	int size = base->dag_node->var->data_size;

	int ret  = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	return ctx->iops->P2G(c, f, rd, sib.base, 0, size);
}

static int _risc_inst_push_rax_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	risc_context_t*  risc  = ctx->priv;
	function_t*      f     = risc->f;
	register_t*      rax   = f->rops->find_register("rax");
	risc_OpCode_t*   push;
	instruction_t*   inst;

#if 0
	push = risc_find_OpCode(RISC_PUSH, 8,8, RISC_G);
	inst = ctx->iops->G(push, rax);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
#endif
	return -1;
}

static int _risc_inst_pop_rax_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	risc_context_t*  risc  = ctx->priv;
	function_t*      f     = risc->f;
	register_t*      rax   = f->rops->find_register("rax");
	risc_OpCode_t*   pop;
	instruction_t*   inst;
#if 0
	pop  = risc_find_OpCode(RISC_POP, 8,8, RISC_G);
	inst = ctx->iops->G(pop, rax);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
#endif
	return -1;
}

/*

struct va_list
{
	uint8_t*  iptr;
	uint8_t*  fptr;
	uint8_t*  optr;

	intptr_t  ireg;
	intptr_t  freg;
};
*/

static int _risc_inst_va_start_handler(native_t* ctx, _3ac_code_t* c)
{
	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	logd("c->srcs->size: %d\n", c->srcs->size);
	assert(3 == c->srcs->size);

	register_t* rbp   = f->rops->find_register("rbp");
	register_t* rptr  = NULL;
	register_t* rap   = NULL;
	instruction_t*  inst  = NULL;
	_3ac_operand_t*  ap    = c->srcs->data[0];
	_3ac_operand_t*  ptr   = c->srcs->data[2];
	risc_OpCode_t*   mov   = risc_find_OpCode(RISC_MOV, 8, 8, RISC_G2E);
	risc_OpCode_t*   lea   = risc_find_OpCode(RISC_LEA, 8, 8, RISC_E2G);
	variable_t*     v     = ap->dag_node->var;

	int offset_int            = -f->args_int   * 8 - 8;
	int offset_float          = -f->args_float * 8 - f->rops->ABI_NB * 8 - 8;
	int offset_others         = 16;

	if (v->bp_offset >= 0) {
		loge("\n");
		return -1;
	}
#if 0
	RISC_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	RISC_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	inst = ctx->iops->P2G(lea, rptr, rbp, offset_int);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap,  0, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);


	inst = ctx->iops->P2G(lea, rptr, rbp, offset_float);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap,  8, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);


	inst = ctx->iops->P2G(lea, rptr, rbp, offset_others);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap,  16, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);


	mov  = risc_find_OpCode(RISC_MOV, 4, 8, RISC_I2E);

	inst = ctx->iops->I2P(mov, rap,  24, (uint8_t*)&f->args_int, 4);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->I2P(mov, rap,  32, (uint8_t*)&f->args_float, 4);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
#endif
	return -1;
}

static int _risc_inst_va_end_handler(native_t* ctx, _3ac_code_t* c)
{
	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	assert(2 == c->srcs->size);

	register_t* rbp  = f->rops->find_register("rbp");
	register_t* rptr = NULL;
	register_t* rap  = NULL;
	instruction_t*  inst = NULL;
	_3ac_operand_t*  ap   = c->srcs->data[0];
	_3ac_operand_t*  ptr  = c->srcs->data[1];
	risc_OpCode_t*   mov  = risc_find_OpCode(RISC_MOV, 8, 8, RISC_G2E);
	risc_OpCode_t*   xor  = risc_find_OpCode(RISC_XOR, 8, 8, RISC_G2E);
	variable_t*     v    = ap->dag_node->var;

	if (v->bp_offset >= 0) {
		loge("\n");
		return -1;
	}

	ptr->dag_node->var->tmp_flag =  1;
	ptr->dag_node->color         = -1;
#if 0
	RISC_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	RISC_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	inst = ctx->iops->G2E(xor, rptr, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap, 0, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap, 8, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap, 16, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap, 24, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->G2P(mov, rap, 32, rptr);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	ptr->dag_node->var->tmp_flag = 0;
	ptr->dag_node->color         = 0;
	ptr->dag_node->loaded        = 0;

	assert(0 == vector_del(rptr->dag_nodes, ptr->dag_node));
	return 0;
#endif
	return -1;
}

static int _risc_inst_va_arg_handler(native_t* ctx, _3ac_code_t* c)
{
	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	assert(1 == c->dsts->size && 3 == c->srcs->size);

	register_t* rbp  = f->rops->find_register("rbp");
	register_t* rd   = NULL; // result
	register_t* rap  = NULL; // ap
	register_t* rptr = NULL; // ptr
	instruction_t*  inst = NULL;

	instruction_t*  inst_jge = NULL;
	instruction_t*  inst_jmp = NULL;

	_3ac_operand_t*  dst  = c->dsts->data[0];
	_3ac_operand_t*  ap   = c->srcs->data[0];
	_3ac_operand_t*  src  = c->srcs->data[1];
	_3ac_operand_t*  ptr  = c->srcs->data[2];
	variable_t*     v    = src->dag_node->var;

#if 0
	risc_OpCode_t*   inc  = risc_find_OpCode(RISC_INC, 8, 8, RISC_E);
	risc_OpCode_t*   add  = risc_find_OpCode(RISC_ADD, 4, 8, RISC_I2E);
	risc_OpCode_t*   sub  = risc_find_OpCode(RISC_SUB, 4, 8, RISC_I2E);
	risc_OpCode_t*   cmp  = risc_find_OpCode(RISC_CMP, 4, 8, RISC_I2E);
	risc_OpCode_t*   mov  = risc_find_OpCode(RISC_MOV, 8, 8, RISC_E2G);
	risc_OpCode_t*   jge  = risc_find_OpCode(RISC_JGE, 4, 4, RISC_I);
	risc_OpCode_t*   jmp  = risc_find_OpCode(RISC_JMP, 4, 4, RISC_I);
	risc_OpCode_t*   mov2 = NULL;

	RISC_SELECT_REG_CHECK(&rd,   dst->dag_node, c, f, 0);
	RISC_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	RISC_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	int is_float = variable_float(v);
	int size     = f->rops->variable_size(v);

	uint32_t nints   = RISC_ABI_NB;
	uint32_t nfloats = RISC_ABI_NB;
	uint32_t offset  = 0;
	uint32_t incptr  = 8;

	int idx_offset   = 24;
	int ptr_offset   = 0;

	if (is_float) {
		idx_offset   = 32;
		ptr_offset   = 8;
	}

	inst = ctx->iops->I2P(cmp, rap, idx_offset, (uint8_t*)&nints, 4);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst_jge = ctx->iops->I(jge, (uint8_t*)&offset, sizeof(offset));
	RISC_INST_ADD_CHECK(c->instructions, inst_jge);


	inst = ctx->iops->P2G(mov, rptr, rap, ptr_offset);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst = ctx->iops->I2P(sub, rap, ptr_offset, (uint8_t*)&incptr, 4);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst_jmp = ctx->iops->I(jmp, (uint8_t*)&offset, sizeof(offset));
	RISC_INST_ADD_CHECK(c->instructions, inst_jmp);
	offset += inst_jmp->len;

	uint8_t* p = (uint8_t*)&offset;
	int i;
	for (i = 0; i < 4; i++)
		inst_jge->code[jge->nb_OpCodes + i] = p[i];

	offset = 0;
	inst = ctx->iops->P2G(mov, rptr, rap, 16);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst = ctx->iops->I2P(add, rap, 16, (uint8_t*)&incptr, 4);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	for (i = 0; i < 4; i++)
		inst_jmp->code[jmp->nb_OpCodes + i] = p[i];

	inst = ctx->iops->P(inc, rap, idx_offset, 8);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (is_float) {
		if (4 == size)
			mov2 = risc_find_OpCode(RISC_MOVSS, 4, 4, RISC_E2G);
		else if (8 == size)
			mov2 = risc_find_OpCode(RISC_MOVSD, 8, 8, RISC_E2G);
		else
			assert(0);
	} else
		mov2 = risc_find_OpCode(RISC_MOV, size, size, RISC_E2G);

	inst = ctx->iops->P2G(mov2, rd, rptr, 0);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
#endif
	return -1;
}

static int _risc_inst_address_of_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->srcs || c->srcs->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	risc_context_t*  risc = ctx->priv;
	function_t*       f     = risc->f;

	_3ac_operand_t*    dst   = c->dsts->data[0];
	_3ac_operand_t*    src   = c->srcs->data[0];
	register_t* rd    = NULL;
	instruction_t*    inst;

	if (!src || !src->dag_node) {
		loge("\n");
		return -EINVAL;
	}
	assert(dst->dag_node->var->nb_pointers > 0);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int ret = risc_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	assert(dst->dag_node->color > 0);

	ret = f->rops->overflow_reg2(rd, dst->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	return ctx->iops->ADR2G(c, f, rd, src->dag_node->var);
}

static int _risc_inst_div_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rs0  = NULL;
	register_t* rs1  = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s0->var)) {

		if (0 == s0->color) {
			s0->color = -1;
			s0->var->global_flag = 1;

		} else if (0 == s1->color) {
			s1->color = -1;
			s1->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd,  d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rs0, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rs1, s1, c, f, 1);

		inst = ctx->iops->FDIV(c, rd, rs0, rs1);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s1->color) {

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		RISC_SELECT_REG_CHECK(&rs0, s0, c, f, 1);

		int ret = risc_select_free_reg(&rs1, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs1, s1->var->data.u64, rs1->bytes);
		if (ret < 0)
			return ret;

	} else if (0 == s0->color) {

		if (!variable_const_integer(s0->var)) {
			loge("\n");
			return -EINVAL;
		}

		RISC_SELECT_REG_CHECK(&rs1, s1, c, f, 1);

		int ret = risc_select_free_reg(&rs0, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs0, s0->var->data.u64, rs0->bytes);
		if (ret < 0)
			return ret;

	} else {
		RISC_SELECT_REG_CHECK(&rs0, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rs1, s1, c, f, 1);
	}

	if (variable_signed(s0->var))
		inst = ctx->iops->SDIV(c, rd, rs0, rs1);
	else
		inst = ctx->iops->DIV(c, rd, rs0, rs1);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_mod_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rs0  = NULL;
	register_t* rs1  = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s1->color) {

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		RISC_SELECT_REG_CHECK(&rs0, s0, c, f, 1);

		int ret = risc_select_free_reg(&rs1, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs1, s1->var->data.u64, rs1->bytes);
		if (ret < 0)
			return ret;

	} else if (0 == s0->color) {

		if (!variable_const_integer(s0->var)) {
			loge("\n");
			return -EINVAL;
		}

		RISC_SELECT_REG_CHECK(&rs1, s1, c, f, 1);

		int ret = risc_select_free_reg(&rs0, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs0, s0->var->data.u64, rs0->bytes);
		if (ret < 0)
			return ret;

	} else {
		RISC_SELECT_REG_CHECK(&rs0, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rs1, s1, c, f, 1);
	}

	if (variable_signed(s0->var))
		inst = ctx->iops->SDIV(c, rd, rs0, rs1);
	else
		inst = ctx->iops->DIV(c, rd, rs0, rs1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst   = ctx->iops->MSUB(c, rd, rs1, rd, rs0);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_mul_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rm   = NULL;
	register_t* rn   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (0 == s0->color)
		XCHG(s0, s1);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s0->var)) {

		if (0 == s0->color) {
			s0->color = -1;
			s0->var->global_flag = 1;

		} else if (0 == s1->color) {
			s1->color = -1;
			s1->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst = ctx->iops->FMUL(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
	RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);

	if (0 == s1->color) {

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		s1->color = -1;
		s1->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst   = ctx->iops->MUL(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s1->color  = 0;
		s1->loaded = 0;
		s1->var->tmp_flag = 0;
		assert(0 == vector_del(rm->dag_nodes, s1));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

	inst   = ctx->iops->MUL(c, rd, rn, rm);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_mul_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s->var)) {

		if (0 == s->color) {
			s->color = -1;
			s->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst = ctx->iops->FMUL(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

	if (0 == s->color) {

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		s->color = -1;
		s->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst   = ctx->iops->MUL(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s->color  = 0;
		s->loaded = 0;
		s->var->tmp_flag = 0;
		assert(0 == vector_del(rs->dag_nodes, s));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->MUL(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_add_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rm   = NULL;
	register_t* rn   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (0 == s0->color)
		XCHG(s0, s1);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		if (0 == s1->color) {
			s1->color = -1;
			s1->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst   = ctx->iops->FADD(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (0 == s1->color) {

		if (variable_const_string(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		uint64_t u = s1->var->data.u64;

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);

		inst   = ctx->iops->ADD_IMM(c, f, rd, rn, u);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
	RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);
	RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

	inst   = ctx->iops->ADD_G(c, rd, rn, rm);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_add_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s->var)) {

		if (0 == s->color) {
			s->color = -1;
			s->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst   = ctx->iops->FADD(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (0 == s->color) {

		if (variable_const_string(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		uint64_t u = s->var->data.u64;

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

		inst   = ctx->iops->ADD_IMM(c, f, rd, rd, u);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->ADD_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_sub_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s->var)) {

		if (0 == s->color) {
			s->color = -1;
			s->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst   = ctx->iops->FSUB(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (0 == s->color) {

		if (variable_const_string(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		uint64_t u = s->var->data.u64;

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

		inst   = ctx->iops->SUB_IMM(c, f, rd, rd, u);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->SUB_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_sub_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rm   = NULL;
	register_t* rn   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		if (0 == s0->color) {
			s0->color = -1;
			s0->var->global_flag = 1;

		} else if (0 == s1->color) {
			s1->color = -1;
			s1->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst    = ctx->iops->FSUB(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	int neg = 0;

	if (0 == s0->color) {
		neg = 1;
		XCHG(s0, s1);
	}

	if (0 == s1->color) {

		if (variable_const_string(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		uint64_t u = s1->var->data.u64;

		RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
		RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);

		inst   = ctx->iops->SUB_IMM(c, f, rd, rn, u);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		if (neg) {
			inst   = ctx->iops->NEG(c, rd, rd);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
	RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);
	RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

	inst   = ctx->iops->SUB_G(c, rd, rn, rm);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int risc_inst_bit_op_assign(native_t* ctx, _3ac_code_t* c, uint32_t op)
{
}

static int _risc_inst_bit_and_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	register_t* rm   = NULL;
	register_t* rn   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (0 == s0->color)
		XCHG(s0, s1);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
	RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);

	if (0 == s1->color) {

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		s1->color = -1;
		s1->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst   = ctx->iops->AND_G(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s1->color  = 0;
		s1->loaded = 0;
		s1->var->tmp_flag = 0;
		assert(0 == vector_del(rm->dag_nodes, s1));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

	inst   = ctx->iops->AND_G(c, rd, rn, rm);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_bit_or_handler(native_t* ctx, _3ac_code_t* c)
{
	RISC_INST_OP3_CHECK()

	instruction_t*    inst = NULL;
	register_t*       rm   = NULL;
	register_t*       rn   = NULL;
	register_t*       rd   = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	assert(0 != d->color);
	assert(0 != s0->color || 0 != s1->color);

	if (0 == s0->color)
		XCHG(s0, s1);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);
	RISC_SELECT_REG_CHECK(&rn, s0, c, f, 1);

	if (0 == s1->color) {

		if (!variable_const_integer(s1->var)) {
			loge("\n");
			return -EINVAL;
		}

		s1->color = -1;
		s1->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

		inst   = ctx->iops->OR_G(c, rd, rn, rm);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s1->color  = 0;
		s1->loaded = 0;
		s1->var->tmp_flag = 0;
		assert(0 == vector_del(rm->dag_nodes, s1));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rm, s1, c, f, 1);

	inst   = ctx->iops->OR_G(c, rd, rn, rm);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_and_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src->dag_node->var)) {

		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s->color) {

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		s->color = -1;
		s->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst   = ctx->iops->AND_G(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s->color  = 0;
		s->loaded = 0;
		s->var->tmp_flag = 0;
		assert(0 == vector_del(rs->dag_nodes, s));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->AND_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_or_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src->dag_node->var)) {

		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s->color) {

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		s->color = -1;
		s->var->tmp_flag = 1;
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst   = ctx->iops->OR_G(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		s->color  = 0;
		s->loaded = 0;
		s->var->tmp_flag = 0;
		assert(0 == vector_del(rs->dag_nodes, s));
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->OR_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_teq_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs;
	instruction_t*    inst;

	uint32_t opcode;

	if (0 == src->dag_node->color) {
		loge("src->dag_node->var: %p\n", src->dag_node->var);
		return -1;
	}

	RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst   = ctx->iops->TEQ(c, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_setz_handler(native_t* ctx, _3ac_code_t* c)
{
#define SET_INIT() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	risc_context_t* risc = ctx->priv; \
	function_t*      f     = risc->f; \
	_3ac_operand_t*   dst   = c->dsts->data[0]; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (!c->instructions) { \
		c->instructions = vector_alloc(); \
		if (!c->instructions) \
			return -ENOMEM; \
	} \
	\
	register_t* rd; \
	instruction_t*    inst; \
	\
	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	SET_INIT();
	inst = ctx->iops->SETZ(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_setnz_handler(native_t* ctx, _3ac_code_t* c)
{
	SET_INIT();
	inst = ctx->iops->SETNZ(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}
static int _risc_inst_setge_handler(native_t* ctx, _3ac_code_t* c)
{
	SET_INIT();
	inst = ctx->iops->SETGE(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}
static int _risc_inst_setgt_handler(native_t* ctx, _3ac_code_t* c)
{
	SET_INIT();
	inst = ctx->iops->SETGT(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}
static int _risc_inst_setlt_handler(native_t* ctx, _3ac_code_t* c)
{
	SET_INIT();
	inst = ctx->iops->SETLT(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}
static int _risc_inst_setle_handler(native_t* ctx, _3ac_code_t* c)
{
	SET_INIT();
	inst = ctx->iops->SETLE(c, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_logic_not_handler(native_t* ctx, _3ac_code_t* c)
{
	int ret = _risc_inst_teq_handler(ctx, c);
	if (ret < 0)
		return ret;

	return _risc_inst_setz_handler(ctx, c);
}

static int _risc_inst_cmp_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*    f     = risc->f;
	_3ac_operand_t* s0    = c->srcs->data[0];
	_3ac_operand_t* s1    = c->srcs->data[1];

	if (!s0 || !s0->dag_node)
		return -EINVAL;

	if (!s1 || !s1->dag_node)
		return -EINVAL;

	instruction_t*    inst;
	register_t* rs1  = NULL;
	register_t* rs0  = NULL;
	dag_node_t*       ds0  = s0->dag_node;
	dag_node_t*       ds1  = s1->dag_node;
	rela_t*           rela = NULL;

	uint32_t  opcode;

	if (ds0->var->size != ds1->var->size)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (0 == ds0->color) {
		loge("src0 should be a var\n");
		if (ds0->var->w)
			loge("src0: '%s'\n", ds0->var->w->text->data);
		else
			loge("src0: v_%#lx\n", 0xffff & (uintptr_t)ds0->var);
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rs0, ds0, c, f, 1);

	if (variable_float(ds0->var)) {

		if (0 == ds1->color) {
			ds1->color = -1;
			ds1->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rs0, ds0, c, f, 1);
		RISC_SELECT_REG_CHECK(&rs1, ds1, c, f, 1);

		inst    = ctx->iops->FCMP(c, rs0, rs1);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (0 == ds1->color) {

		uint64_t u = ds1->var->data.u64;

		if (u <= 0xfff)
			inst   = ctx->iops->CMP_IMM(c, f, rs0, u);

		else if (0 == (u & 0xfff) && (u >> 12) <= 0xfff)
			inst   = ctx->iops->CMP_IMM(c, f, rs0, u);

		else {
			ds1->loaded =  0;
			ds1->color  = -1;
			RISC_SELECT_REG_CHECK(&rs1, ds1, c, f, 1);

			inst   = ctx->iops->CMP_G(c, rs0, rs1);

			ds1->loaded = 0;
			ds1->color  = 0;
			assert(0 == vector_del(rs1->dag_nodes, ds1));
		}

		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rs1, ds1, c, f, 1);

	inst   = ctx->iops->CMP_G(c, rs0, rs1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

#define RISC_INST_CMP_SET(name, cc) \
static int _risc_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	int ret = _risc_inst_cmp_handler(ctx, c); \
	if (ret < 0) \
	   return ret; \
	return _risc_inst_set##cc##_handler(ctx, c); \
}
RISC_INST_CMP_SET(eq, z)
RISC_INST_CMP_SET(ne, nz)
RISC_INST_CMP_SET(gt, gt)
RISC_INST_CMP_SET(ge, ge)
RISC_INST_CMP_SET(lt, lt)
RISC_INST_CMP_SET(le, le)

static int _risc_inst_cast_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];
	instruction_t*   inst;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (0 == dst->dag_node->color)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rd = NULL;
	register_t* rs = NULL;
	dag_node_t*       d  = dst->dag_node;
	dag_node_t*       s  = src->dag_node;
	variable_t*       vs = s->var;
	variable_t*       vd = d->var;

	int src_size = f->rops->variable_size(vs);
	int dst_size = f->rops->variable_size(vd);

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

	uint32_t opcode = 0;

	if (variable_float(vs)) {

		if (0 == s->color) {
			s->color = -1;
			vs->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		if (variable_float(vd)) {

			if (rd->bytes == rs->bytes)
				inst = ctx->iops->FMOV_G(c, rd, rs);

			else if (4 == rs->bytes)
				inst = ctx->iops->CVTSS2SD(c, rd, rs);
			else
				inst = ctx->iops->CVTSD2SS(c, rd, rs);

		} else {
			if (variable_signed(vd))
				inst = ctx->iops->CVTF2SI(c, rd, rs);
			else
				inst = ctx->iops->CVTF2UI(c, rd, rs);
		}

		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;

	} else if (variable_float(vd)) {

		if (0 == s->color) {

			if (!variable_const_integer(vs))
				return -EINVAL;

			if (src_size < dst_size)
				variable_extend_bytes(vs, 8);

			int ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, dst_size);
			if (ret < 0)
				return ret;

		} else
			RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		if (variable_signed(vs))
			inst = ctx->iops->CVTSI2F(c, rd, rs);
		else
			inst = ctx->iops->CVTUI2F(c, rd, rs);

		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (vs->nb_dimentions > 0)
		return ctx->iops->ADR2G(c, f, rd, vs);

	logd("src_size: %d, dst_size: %d\n", src_size, dst_size);

	if (0 == s->color) {

		if (!variable_const_integer(vs))
			return -EINVAL;

		if (src_size < dst_size)
			variable_extend_bytes(vs, 8);

		return ctx->iops->I2G(c, rd, vs->data.u64, dst_size);
	}

	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	if (src_size < dst_size) {

		if (variable_signed(vs))
			inst = ctx->iops->MOVSX(c, rd, rs, src_size);
		else
			inst = ctx->iops->MOVZX(c, rd, rs, src_size);
	} else
		inst = ctx->iops->MOV_G(c, rd, rs);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_div_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(s->var)) {

		if (0 == s->color) {
			s->color = -1;
			s->var->global_flag = 1;
		}

		RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

		inst    = ctx->iops->FDIV(c, rd, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

	if (0 == s->color) {

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		int ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, s->var->data.u64, rs->bytes);
		if (ret < 0)
			return ret;

	} else
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	if (variable_signed(s->var))
		inst = ctx->iops->SDIV(c, rd, rd, rs);
	else
		inst = ctx->iops->DIV(c, rd, rd, rs);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_mod_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*   dst   = c->dsts->data[0];
	_3ac_operand_t*   src   = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (src->dag_node->var->size != dst->dag_node->var->size)
		return -EINVAL;

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	register_t* r    = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	assert(0 != d->color);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src->dag_node->var)) {

		assert(variable_float(dst->dag_node->var));
		return -EINVAL;
	}

	RISC_SELECT_REG_CHECK(&rd, d, c, f, 0);

	if (0 == s->color) {

		if (!variable_const_integer(s->var)) {
			loge("\n");
			return -EINVAL;
		}

		int ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, s->var->data.u64, rs->bytes);
		if (ret < 0)
			return ret;

	} else
		RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	int ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;

	if (variable_signed(s->var))
		inst = ctx->iops->SDIV(c, r, rd, rs);
	else
		inst = ctx->iops->DIV(c, r, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst   = ctx->iops->MSUB(c, rd, rs, r, rd);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_return_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	risc_context_t*   risc  = ctx->priv;
	function_t*       f     = risc->f;
	_3ac_operand_t*    src   = NULL;
	variable_t*       v     = NULL;
	instruction_t*    inst  = NULL;
	rela_t*           rela  = NULL;

	register_t* rd    = NULL;
	register_t* rs    = NULL;
	register_t* sp    = f->rops->find_register("sp");
	register_t* fp    = f->rops->find_register("fp");

	risc_OpCode_t*   pop;
	risc_OpCode_t*   mov;
	risc_OpCode_t*   ret;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int i;
	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		v   = src->dag_node->var;

		int size     = f->rops->variable_size (v);
		int is_float =      variable_float(v);

		if (i > 0 && is_float) {
			loge("\n");
			return -1;
		}

		int retsize = size > 4 ? 8 : 4;

		if (is_float) {
			rd = f->rops->find_register_type_id_bytes(is_float, 0, retsize);

			if (0 == src->dag_node->color) {
				src->dag_node->color = -1;
				v->global_flag       =  1;
			}

		} else {
			rd = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_ret_regs[i], retsize);

			if (0 == src->dag_node->color) {
				if (rd->bytes > size)
					variable_extend_bytes(v, rd->bytes);

				int ret = ctx->iops->I2G(c, rd, v->data.u64, rd->bytes);
				if (ret < 0)
					return ret;
				continue;
			}
		}

		logd("rd: %s, rd->dag_nodes->size: %d\n", rd->name, rd->dag_nodes->size);

		if (src->dag_node->color > 0) {

			int start = c->instructions->size;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

			if (!f->rops->color_conflict(rd->color, rs->color)) {

				int ret = risc_save_reg(rd, c, f);
				if (ret < 0)
					return ret;

				uint32_t opcode;

				if (rd->bytes > size) {

					if (variable_signed(v))
						inst = ctx->iops->MOVSX(c, rd, rs, size);

					else if (1 == size || 2 == size)
						inst = ctx->iops->MOVZX(c, rd, rs, size);
					else
						inst = ctx->iops->MOV_G(c, rd, rs);
				} else
					inst = ctx->iops->MOV_G(c, rd, rs);
				RISC_INST_ADD_CHECK(c->instructions, inst);

				instruction_t* tmp;
				int j;
				int k;
				for (j = start; j < c->instructions->size; j++) {
					tmp           = c->instructions->data[j];

					for (k = j - 1; k >= j - start; k--)
						c->instructions->data[k + 1] = c->instructions->data[k];

					c->instructions->data[j - start] = tmp;
				}
			}
		} else {
			int ret = risc_save_reg(rd, c, f);
			if (ret < 0)
				return ret;

			ret = ctx->iops->M2G(c, f, rd, NULL, v);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int _risc_inst_memset_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc  = ctx->priv;
	function_t*     f     = risc->f;
	_3ac_operand_t*  dst   = c->srcs->data[0];
	_3ac_operand_t*  data  = c->srcs->data[1];
	_3ac_operand_t*  count = c->srcs->data[2];
	instruction_t*  inst  = NULL;

	register_t*	rax   = f->rops->find_register("rax");
	register_t*	rcx   = f->rops->find_register("rcx");
	register_t*	rdi   = f->rops->find_register("rdi");
	register_t*	rd;
	risc_OpCode_t*   mov;
	risc_OpCode_t*   stos;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int ret = f->rops->overflow_reg2(rdi, dst->dag_node, c, f);
	if (ret < 0)
		return ret;

	ret = f->rops->overflow_reg2(rax, data->dag_node, c, f);
	if (ret < 0)
		return ret;

	ret = f->rops->overflow_reg2(rcx, count->dag_node, c, f);
	if (ret < 0)
		return ret;

#if 0
#define RISC_MEMSET_LOAD_REG(r, dn) \
	do { \
		int size = f->rops->variable_size(dn->var); \
		assert(8 == size); \
		\
		if (0 == dn->color) { \
			mov  = risc_find_OpCode(RISC_MOV, size, size, RISC_I2G); \
			inst = ctx->iops->I2G(mov, r, (uint8_t*)&dn->var->data, size); \
			RISC_INST_ADD_CHECK(c->instructions, inst); \
			\
		} else { \
			if (dn->color < 0) \
				dn->color = r->color; \
			RISC_SELECT_REG_CHECK(&rd, dn, c, f, 1); \
			\
			if (!RISC_COLOR_CONFLICT(rd->color, r->color)) { \
				mov  = risc_find_OpCode(RISC_MOV, size, size, RISC_G2E); \
				inst = ctx->iops->G2E(mov, r, rd); \
				RISC_INST_ADD_CHECK(c->instructions, inst); \
			} \
		} \
	} while (0)
	RISC_MEMSET_LOAD_REG(rdi, dst  ->dag_node);
	RISC_MEMSET_LOAD_REG(rax, data ->dag_node);
	RISC_MEMSET_LOAD_REG(rcx, count->dag_node);

	stos = risc_find_OpCode(RISC_STOS, 1, 8, RISC_G);
	inst = risc_make_inst(stos, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return 0;
#endif
	return -1;
}

static int _risc_inst_nop_handler(native_t* ctx, _3ac_code_t* c)
{
	return 0;
}

static int _risc_inst_end_handler(native_t* ctx, _3ac_code_t* c)
{
	risc_context_t* risc  = ctx->priv;
	instruction_t*  inst  = NULL;
	function_t*     f     = risc->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* sp  = f->rops->find_register("sp");
	register_t* fp  = f->rops->find_register("fp");

	int ret = f->rops->pop_callee_regs(c, f);
	if (ret < 0)
		return ret;

	inst = ctx->iops->MOV_SP(c, sp, fp);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->POP(c, fp);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	inst = ctx->iops->RET(c);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

#define RISC_INST_JMP(name, NAME) \
static int _risc_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	_3ac_operand_t* dst  = c->dsts->data[0]; \
	instruction_t* inst = NULL; \
	\
	if (!dst->bb) \
		return -EINVAL; \
	\
	if (!c->instructions) { \
		c->instructions = vector_alloc(); \
		if (!c->instructions) \
			return -ENOMEM; \
	} \
	\
	inst = ctx->iops->NAME(c); \
	RISC_INST_ADD_CHECK(c->instructions, inst); \
	return 0;\
}

RISC_INST_JMP(goto, JMP)
RISC_INST_JMP(jz,   JZ)
RISC_INST_JMP(jnz,  JNZ)
RISC_INST_JMP(jgt,  JGT)
RISC_INST_JMP(jge,  JGE)
RISC_INST_JMP(jlt,  JLT)
RISC_INST_JMP(jle,  JLE)

RISC_INST_JMP(ja,   JA)
RISC_INST_JMP(jb,   JB)
RISC_INST_JMP(jae,  JAE)
RISC_INST_JMP(jbe,  JBE)

static int _risc_inst_load_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* r   = NULL;
	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;

	_3ac_operand_t*  dst = c->dsts->data[0];
	dag_node_t*     dn  = dst->dag_node;

	int ret;
	int i;

	if (dn->color < 0)
		return 0;

	variable_t* v = dn->var;

	assert(dn->color > 0);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	r = f->rops->find_register_color(dn->color);

	if (f->rops->reg_used(r, dn)) {
		dn->color  = -1;
		dn->loaded =  0;
		vector_del(r->dag_nodes, dn);
		return 0;
	}

	ret = risc_load_reg(r, dn, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = vector_add_unique(r->dag_nodes, dn);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	return 0;
}

static int _risc_inst_reload_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* r   = NULL;
	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;

	_3ac_operand_t*  dst = c->dsts->data[0];
	dag_node_t*     dn  = dst->dag_node;
	dag_node_t*     dn2 = NULL;

	int ret;
	int i;

	if (dn->color < 0)
		return 0;
	assert(dn->color > 0);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	r   = f->rops->find_register_color(dn->color);

	ret = f->rops->overflow_reg2(r, dn, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	dn->loaded = 0;
	ret = risc_load_reg(r, dn, c, f);
	if (ret < 0)
		return ret;

	ret = vector_add_unique(r->dag_nodes, dn);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	return 0;
}

static int _risc_inst_save_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	_3ac_operand_t*  src = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	risc_context_t*  risc = ctx->priv;
	function_t*     f   = risc->f;
	dag_node_t*     dn  = src->dag_node;

	if (dn->color < 0)
		return 0;

	if (!dn->loaded)
		return 0;

	variable_t* v = dn->var;
	assert(dn->color > 0);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	return risc_save_var(dn, c, f);
}

static int _risc_inst_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;
	variable_t*       v    = s->var;

	uint32_t opcode;

	assert(0 != d->color);

	int size     = f->rops->variable_size(v);
	int is_float = variable_float(v);

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (is_float) {
		assert(variable_float(v));

		if (0 == s->color) {
			s->color = -1;
			v->global_flag = 1;
			return ctx->iops->M2GF(c, f, rd, NULL, v);
		}

		RISC_SELECT_REG_CHECK(&rs, d,  c, f, 1);

		inst   = ctx->iops->FMOV_G(c, rd, rs);
		RISC_INST_ADD_CHECK(c->instructions, inst);
		return 0;
	}

	if (0 == s->color) {

		if (variable_const_string(v))
			return ctx->iops->ISTR2G(c, f, rd, v);

		if (!variable_const_integer(v)) {
			loge("\n");
			return -EINVAL;
		}

		if (rd->bytes > size)
			variable_extend_bytes(v, rd->bytes);

		return ctx->iops->I2G(c, rd, v->data.u64, rd->bytes);
	}

	RISC_SELECT_REG_CHECK(&rs, s, c, f, 1);

	inst   = ctx->iops->MOV_G(c, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_shift(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src0  = c->srcs->data[0];
	_3ac_operand_t*   src1  = c->srcs->data[1];
	_3ac_operand_t*   dst   = c->dsts->data[0];

	if (!src0 || !src0->dag_node)
		return -EINVAL;

	if (!src1 || !src1->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs0  = NULL;
	register_t* rs1  = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst ->dag_node;
	dag_node_t*       s0   = src0->dag_node;
	dag_node_t*       s1   = src1->dag_node;

	uint32_t opcode;

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s0->color) {

		if (variable_signed(s0->var))
			variable_sign_extend(s0->var, 8);
		else
			variable_zero_extend(s0->var, 8);

		int ret = ctx->iops->I2G(c, rd, s0->var->data.u64, rd->bytes);
		if (ret < 0)
			return ret;

		rs0 = rd;
	} else
		RISC_SELECT_REG_CHECK(&rs0, s0,  c, f, 1);

	if (0 == s1->color) {

		int ret = risc_select_free_reg(&rs1, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs1, s1->var->data.u64, 1);
		if (ret < 0)
			return ret;
	} else
		RISC_SELECT_REG_CHECK(&rs1, s1,  c, f, 1);


	if (OP_SHR == c->op->type) {

		if (variable_signed(s0->var))
			inst = ctx->iops->ASR(c, rd, rs0, rs1);
		else
			inst = ctx->iops->SHR(c, rd, rs0, rs1);
	} else
		inst = ctx->iops->SHL(c, rd, rs0, rs1);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_shift_assign(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	_3ac_operand_t*   src   = c->srcs->data[0];
	_3ac_operand_t*   dst   = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t* rs   = NULL;
	register_t* rd   = NULL;
	instruction_t*    inst = NULL;
	dag_node_t*       d    = dst->dag_node;
	dag_node_t*       s    = src->dag_node;

	uint32_t opcode;

	RISC_SELECT_REG_CHECK(&rd, d,  c, f, 0);

	if (0 == s->color) {

		int ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, s->var->data.u64, 1);
		if (ret < 0)
			return ret;
	} else
		RISC_SELECT_REG_CHECK(&rs, s,  c, f, 1);

	if (OP_SHR == c->op->type) {

		if (variable_signed(s->var))
			inst = ctx->iops->ASR(c, rd, rd, rs);
		else
			inst = ctx->iops->SHR(c, rd, rd, rs);
	} else
		inst = ctx->iops->SHL(c, rd, rd, rs);

	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_shl_handler(native_t* ctx, _3ac_code_t* c)
{
	return _risc_inst_shift(ctx, c);
}

static int _risc_inst_shr_handler(native_t* ctx, _3ac_code_t* c)
{
	return _risc_inst_shift(ctx, c);
}

static int _risc_inst_shl_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	return _risc_inst_shift_assign(ctx, c);
}

static int _risc_inst_shr_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	return _risc_inst_shift_assign(ctx, c);
}

static int _risc_inst_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  src    = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vs  = src ->dag_node->var;
	variable_t*       vb  = base->dag_node->var;

	register_t* rs  = NULL;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	return ctx->iops->G2P(c, f, rs, sib.base, sib.disp, sib.size);
}

static int _risc_inst_add_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  src    = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vs  = src ->dag_node->var;
	variable_t*       vb  = base->dag_node->var;

	register_t* rs  = NULL;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);


	uint32_t opcode;

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	if (!is_float)
		inst = ctx->iops->ADD_G(c, rd, rd, rs);
	else
		inst = ctx->iops->FADD(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_sub_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  src    = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vs  = src ->dag_node->var;
	variable_t*       vb  = base->dag_node->var;

	register_t* rs  = NULL;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	uint32_t opcode;

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	if (!is_float)
		inst = ctx->iops->SUB_G(c, rd, rd, rs);
	else
		inst = ctx->iops->FSUB(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_and_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  src    = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vs  = src ->dag_node->var;
	variable_t*       vb  = base->dag_node->var;

	register_t* rs  = NULL;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = variable_float(vs);
	if (is_float)
		return -EINVAL;

	if (0 == src->dag_node->color) {

		ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
		if (ret < 0)
			return ret;

		rs = f->rops->find_register_color_bytes(rs->color, sib.size);
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	uint32_t opcode;

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->AND_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_or_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  src    = c->srcs->data[1];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vs  = src ->dag_node->var;
	variable_t*       vb  = base->dag_node->var;

	register_t* rs  = NULL;
	register_t* rd  = NULL;
	sib_t           sib = {0};

	int ret = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);


	uint32_t opcode;

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->OR_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t*  risc  = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	_3ac_operand_t*  src    = c->srcs->data[2];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vs  = src   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	register_t* rs  = NULL;
	sib_t           sib = {0};

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	return ctx->iops->G2P(c, f, rs, sib.base, sib.disp, sib.size);
}

static int _risc_inst_add_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	_3ac_operand_t*  src    = c->srcs->data[2];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vs  = src   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	register_t* rs  = NULL;
	sib_t           sib = {0};

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->ADD_G(c, rd, rd, rs);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_sub_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	_3ac_operand_t*  src    = c->srcs->data[2];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vs  = src   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	register_t* rs  = NULL;
	sib_t           sib = {0};

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->SUB_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_and_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	_3ac_operand_t*  src    = c->srcs->data[2];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vs  = src   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	register_t* rs  = NULL;
	sib_t           sib = {0};

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->OR_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_or_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  member = c->srcs->data[1];
	_3ac_operand_t*  src    = c->srcs->data[2];
	instruction_t*  inst;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!member || !member->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vs  = src   ->dag_node->var;
	variable_t*     vb  = base  ->dag_node->var;
	variable_t*     vm  = member->dag_node->var;

	register_t* rd  = NULL;
	register_t* rs  = NULL;
	sib_t           sib = {0};

	int ret = risc_pointer_reg(&sib, base->dag_node, member->dag_node, c, f);
	if (ret < 0)
		return ret;

	if (variable_is_struct(vm) || variable_is_array(vm))
		return -EINVAL;

	int is_float = variable_float(vs);

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, sib.size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, sib.size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->OR_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, sib.size);
}

static int _risc_inst_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];
	_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;
	variable_t*       vs     = src ->dag_node->var;

	register_t* rd     = NULL;
	register_t* rs     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int is_float = variable_float(vs);
	int size     = f->rops->variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rs, &sib);
	else
		ret = ctx->iops->G2P(c, f, rs, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_add_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];
	_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;
	variable_t*       vs     = src ->dag_node->var;

	register_t* rd     = NULL;
	register_t* rs     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int is_float = variable_float(vs);
	int size     = f->rops->variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	if (!is_float)
		inst = ctx->iops->ADD_G(c, rd, rd, rs);
	else
		inst = ctx->iops->FADD(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_sub_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];
	_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;
	variable_t*       vs     = src ->dag_node->var;

	register_t* rd     = NULL;
	register_t* rs     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int is_float = variable_float(vs);
	int size     = f->rops->variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (0 == src->dag_node->color) {

		if (is_float) {

			src->dag_node->color = -1;
			vs->global_flag      =  1;

			RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		} else {
			ret = risc_select_free_reg(&rs, c, f, 0);
			if (ret < 0)
				return ret;

			ret = ctx->iops->I2G(c, rs, vs->data.u64, size);
			if (ret < 0)
				return ret;

			rs = f->rops->find_register_color_bytes(rs->color, size);
		}
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	if (!is_float)
		inst = ctx->iops->SUB_G(c, rd, rd, rs);
	else
		inst = ctx->iops->FSUB(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	rs = rd;

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_and_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];
	_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;
	variable_t*       vs     = src ->dag_node->var;

	register_t* rd     = NULL;
	register_t* rs     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int is_float = variable_float(vs);
	int size     = f->rops->variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	if (is_float)
		return -EINVAL;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (0 == src->dag_node->color) {

		ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, vs->data.u64, size);
		if (ret < 0)
			return ret;

		rs = f->rops->find_register_color_bytes(rs->color, size);
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->AND_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		ret = ctx->iops->G2SIB(c, f, rd, &sib);
	else
		ret = ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
	return ret;
}

static int _risc_inst_or_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	risc_context_t*  risc    = ctx->priv;
	function_t*     f      = risc->f;

	_3ac_operand_t*  base   = c->srcs->data[0];
	_3ac_operand_t*  index  = c->srcs->data[1];
	_3ac_operand_t*  scale  = c->srcs->data[2];
	_3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*       vscale = scale->dag_node->var;
	variable_t*       vb     = base->dag_node->var;
	variable_t*       vs     = src ->dag_node->var;

	register_t* rd     = NULL;
	register_t* rs     = NULL;
	sib_t           sib    = {0};
	instruction_t*    inst;

	int is_float = variable_float(vs);
	int size     = f->rops->variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	if (is_float)
		return -EINVAL;

	int ret  = risc_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (0 == src->dag_node->color) {

		ret = risc_select_free_reg(&rs, c, f, 0);
		if (ret < 0)
			return ret;

		ret = ctx->iops->I2G(c, rs, vs->data.u64, size);
		if (ret < 0)
			return ret;

		rs = f->rops->find_register_color_bytes(rs->color, size);
	} else
		RISC_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	ret = risc_select_free_reg(&rd, c, f, is_float);
	if (ret < 0)
		return ret;

	if (sib.index)
		ret = ctx->iops->SIB2G(c, f, rd, &sib);
	else
		ret = ctx->iops->P2G(c, f, rd, sib.base, sib.disp, sib.size);
	if (ret < 0)
		return ret;

	inst   = ctx->iops->OR_G(c, rd, rd, rs);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	if (sib.index)
		return ctx->iops->G2SIB(c, f, rd, &sib);

	return ctx->iops->G2P(c, f, rd, sib.base, sib.disp, size);
}

static int _risc_inst_dec_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	register_t* r     = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    base  = c->srcs->data[0];
	instruction_t*    inst;
	function_t*       f     = risc->f;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	sib_t sib = {0};

	int size = base->dag_node->var->data_size;

	int ret  = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;
	r = f->rops->find_register_color_bytes(r->color, size);

	ret = ctx->iops->P2G(c, f, r, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->SUB_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, r, sib.base, 0, size);
}

static int _risc_inst_inc_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	register_t* r     = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    base  = c->srcs->data[0];
	instruction_t*    inst;
	function_t*       f     = risc->f;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	sib_t sib = {0};

	int size = base->dag_node->var->data_size;

	int ret  = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	ret = risc_select_free_reg(&r, c, f, 0);
	if (ret < 0)
		return ret;
	r = f->rops->find_register_color_bytes(r->color, size);

	ret = ctx->iops->P2G(c, f, r, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->ADD_IMM(c, f, r, r, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	return ctx->iops->G2P(c, f, r, sib.base, 0, size);
}

static int _risc_inst_dec_post_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* rd    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    base  = c->srcs->data[0];
	_3ac_operand_t*    dst   = c->dsts->data[0];
	instruction_t*    inst;
	function_t*       f     = risc->f;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	sib_t sib = {0};

	int size = base->dag_node->var->data_size;

	int ret  = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	ret = ctx->iops->P2G(c, f, rd, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	ret = ctx->iops->G2P(c, f, rd, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _risc_inst_inc_post_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t* rd    = NULL;
	risc_context_t*  risc = ctx->priv;
	_3ac_operand_t*    base  = c->srcs->data[0];
	_3ac_operand_t*    dst   = c->dsts->data[0];
	instruction_t*    inst;
	function_t*       f     = risc->f;

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	sib_t sib = {0};

	int size = base->dag_node->var->data_size;

	int ret  = risc_dereference_reg(&sib, base->dag_node, NULL, c, f);
	if (ret < 0)
		return ret;

	RISC_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	ret = ctx->iops->P2G(c, f, rd, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->ADD_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);

	ret = ctx->iops->G2P(c, f, rd, sib.base, 0, size);
	if (ret < 0)
		return ret;

	inst = ctx->iops->SUB_IMM(c, f, rd, rd, 1);
	RISC_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static risc_inst_handler_pt  risc_inst_handlers[N__3ac_OPS] =
{
	[OP_CALL        ]  =  _risc_inst_call_handler,
	[OP_ARRAY_INDEX ]  =  _risc_inst_array_index_handler,
	[OP_POINTER     ]  =  _risc_inst_pointer_handler,

	[OP_TYPE_CAST   ]  =  _risc_inst_cast_handler,
	[OP_LOGIC_NOT   ]  =  _risc_inst_logic_not_handler,
	[OP_BIT_NOT     ]  =  _risc_inst_bit_not_handler,
	[OP_NEG         ]  =  _risc_inst_neg_handler,

	[OP_VA_START    ]  =  _risc_inst_va_start_handler,
	[OP_VA_ARG      ]  =  _risc_inst_va_arg_handler,
	[OP_VA_END      ]  =  _risc_inst_va_end_handler,

	[OP_INC         ]  =  _risc_inst_inc_handler,
	[OP_DEC         ]  =  _risc_inst_dec_handler,

	[OP_INC_POST    ]  =  _risc_inst_inc_post_handler,
	[OP_DEC_POST    ]  =  _risc_inst_dec_post_handler,

	[OP_DEREFERENCE ]  =  _risc_inst_dereference_handler,
	[OP_ADDRESS_OF  ]  =  _risc_inst_address_of_handler,

	[OP_MUL         ]  =  _risc_inst_mul_handler,
	[OP_DIV         ]  =  _risc_inst_div_handler,
	[OP_MOD         ]  =  _risc_inst_mod_handler,

	[OP_ADD         ]  =  _risc_inst_add_handler,
	[OP_SUB         ]  =  _risc_inst_sub_handler,

	[OP_SHL         ]  =  _risc_inst_shl_handler,
	[OP_SHR         ]  =  _risc_inst_shr_handler,

	[OP_BIT_AND     ]  =  _risc_inst_bit_and_handler,
	[OP_BIT_OR      ]  =  _risc_inst_bit_or_handler,

	[OP_3AC_TEQ     ]  =  _risc_inst_teq_handler,
	[OP_3AC_CMP     ]  =  _risc_inst_cmp_handler,

	[OP_3AC_SETZ    ]  =  _risc_inst_setz_handler,
	[OP_3AC_SETNZ   ]  =  _risc_inst_setnz_handler,
	[OP_3AC_SETGT   ]  =  _risc_inst_setgt_handler,
	[OP_3AC_SETGE   ]  =  _risc_inst_setge_handler,
	[OP_3AC_SETLT   ]  =  _risc_inst_setlt_handler,
	[OP_3AC_SETLE   ]  =  _risc_inst_setle_handler,

	[OP_EQ          ]  =  _risc_inst_eq_handler,
	[OP_NE          ]  =  _risc_inst_ne_handler,
	[OP_GT          ]  =  _risc_inst_gt_handler,
	[OP_GE          ]  =  _risc_inst_ge_handler,
	[OP_LT          ]  =  _risc_inst_lt_handler,
	[OP_LE          ]  =  _risc_inst_le_handler,

	[OP_ASSIGN      ]  =  _risc_inst_assign_handler,

	[OP_ADD_ASSIGN  ]  =  _risc_inst_add_assign_handler,
	[OP_SUB_ASSIGN  ]  =  _risc_inst_sub_assign_handler,

	[OP_MUL_ASSIGN  ]  =  _risc_inst_mul_assign_handler,
	[OP_DIV_ASSIGN  ]  =  _risc_inst_div_assign_handler,
	[OP_MOD_ASSIGN  ]  =  _risc_inst_mod_assign_handler,

	[OP_SHL_ASSIGN  ]  =  _risc_inst_shl_assign_handler,
	[OP_SHR_ASSIGN  ]  =  _risc_inst_shr_assign_handler,

	[OP_AND_ASSIGN  ]  =  _risc_inst_and_assign_handler,
	[OP_OR_ASSIGN   ]  =  _risc_inst_or_assign_handler,

	[OP_RETURN      ]  =  _risc_inst_return_handler,
	[OP_GOTO        ]  =  _risc_inst_goto_handler,

	[OP_3AC_JZ      ]  =  _risc_inst_jz_handler,
	[OP_3AC_JNZ     ]  =  _risc_inst_jnz_handler,
	[OP_3AC_JGT     ]  =  _risc_inst_jgt_handler,
	[OP_3AC_JGE     ]  =  _risc_inst_jge_handler,
	[OP_3AC_JLT     ]  =  _risc_inst_jlt_handler,
	[OP_3AC_JLE     ]  =  _risc_inst_jle_handler,

	[OP_3AC_JA      ]  =  _risc_inst_ja_handler,
	[OP_3AC_JB      ]  =  _risc_inst_jb_handler,
	[OP_3AC_JAE     ]  =  _risc_inst_jae_handler,
	[OP_3AC_JBE     ]  =  _risc_inst_jbe_handler,

	[OP_3AC_NOP     ]  =  _risc_inst_nop_handler,
	[OP_3AC_END     ]  =  _risc_inst_end_handler,

	[OP_3AC_SAVE    ]  =  _risc_inst_save_handler,
	[OP_3AC_LOAD    ]  =  _risc_inst_load_handler,

	[OP_3AC_RESAVE  ]  =  _risc_inst_save_handler,
	[OP_3AC_RELOAD  ]  =  _risc_inst_reload_handler,

	[OP_3AC_INC     ]  =  _risc_inst_inc_handler,
	[OP_3AC_DEC     ]  =  _risc_inst_dec_handler,

	[OP_3AC_PUSH_RETS] =  _risc_inst_push_rax_handler,
	[OP_3AC_POP_RETS ] =  _risc_inst_pop_rax_handler,

	[OP_3AC_MEMSET  ]  =  _risc_inst_memset_handler,

	[OP_3AC_ASSIGN_DEREFERENCE    ]  =   _risc_inst_assign_dereference_handler,
	[OP_3AC_ASSIGN_ARRAY_INDEX    ]  =   _risc_inst_assign_array_index_handler,
	[OP_3AC_ASSIGN_POINTER        ]  =   _risc_inst_assign_pointer_handler,

	[OP_3AC_ADDRESS_OF_ARRAY_INDEX]  =   _risc_inst_address_of_array_index_handler,
	[OP_3AC_ADDRESS_OF_POINTER    ]  =   _risc_inst_address_of_pointer_handler,
};

risc_inst_handler_pt  risc_find_inst_handler(const int op_type)
{
	if (op_type < 0 || op_type >= N_3AC_OPS)
		return NULL;

	return risc_inst_handlers[op_type];
}

instruction_t* risc_make_inst(_3ac_code_t* c, uint32_t opcode)
{
	instruction_t* inst;

	inst = calloc(1, sizeof(instruction_t));
	if (!inst)
		return NULL;

	inst->c       = c;
	inst->code[0] = opcode & 0xff;
	inst->code[1] = (opcode >>  8) & 0xff;
	inst->code[2] = (opcode >> 16) & 0xff;
	inst->code[3] = (opcode >> 24) & 0xff;
	inst->len     = 4;

	return inst;
}
