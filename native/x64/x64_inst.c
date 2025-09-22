#include "x64.h"


#define X64_INST_OP3_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 2) \
		return -EINVAL; \
	\
	x64_context_t* x64  = ctx->priv; \
	function_t*    f    = x64->f; \
	\
	_3ac_operand_t* dst  = c->dsts->data[0]; \
	_3ac_operand_t* src0 = c->srcs->data[0]; \
	_3ac_operand_t* src1 = c->srcs->data[1]; \
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

static int _x64_inst_float_op2(int OpCode_type, dag_node_t* dst, dag_node_t* src, _3ac_code_t* c, function_t* f)
{
	if (0 == src->color) {
		src->color = -1;
		src->var->global_flag = 1;
	}

	OpCode_type = x64_float_OpCode_type(OpCode_type, src->var->type);
	if (OpCode_type < 0) {
		loge("\n");
		return -1;
	}

	return x64_inst_op2(OpCode_type, dst, src, c, f);
}

static int _x64_inst_float_op3(int OpCode_type, dag_node_t* dst, dag_node_t* src0, dag_node_t* src1, _3ac_code_t* c, function_t* f)
{
	int ret  = 0;

	if (0 == src0->color) {
		src0->color = -1;
		src0->var->global_flag = 1;
	}

	if (0 == src1->color) {
		src1->color = -1;
		src1->var->global_flag = 1;
	}

	if (VAR_FLOAT == src0->var->type)
		ret = x64_inst_op2(X64_MOVSS, dst, src0, c, f);
	else
		ret = x64_inst_op2(X64_MOVSD, dst, src0, c, f);

	if (ret < 0) {
		loge("\n");
		return ret;
	}

	OpCode_type = x64_float_OpCode_type(OpCode_type, src0->var->type);
	if (OpCode_type < 0) {
		loge("\n");
		return -1;
	}

	return x64_inst_op2(OpCode_type, dst, src1, c, f);
}

static int _x64_inst_call_stack_size(_3ac_code_t* c)
{
	int stack_size = 0;

	int i;
	for (i = 1; i < c->srcs->size; i++) {
		_3ac_operand_t*  src = c->srcs->data[i];
		variable_t*     v   = src->dag_node->var;

		if (src->dag_node->rabi2)
			continue;

		int size = x64_variable_size(v);
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

static int _x64_inst_call_argv(_3ac_code_t* c, function_t* f)
{
	register_t*     rsp  = x64_find_register("rsp");

	x64_OpCode_t*   lea;
	x64_OpCode_t*   mov;
	x64_OpCode_t*   movx;
	instruction_t*  inst;
	instruction_t*  inst_movx[X64_ABI_NB + X64_ABI_FLOAT_NB] = {NULL};

	int nb_movx   = 0;
	int nb_floats = 0;
	int ret;
	int i;

	for (i = c->srcs->size - 1; i >= 1; i--) {
		_3ac_operand_t*  src   = c->srcs->data[i];
		variable_t*     v     = src->dag_node->var;
		register_t*     rd    = src->rabi;
		register_t*     rabi  = src->dag_node->rabi2;
		register_t*     rs    = NULL;

		int size     = x64_variable_size(v);
		int is_float = variable_float(v);

		if (!rabi) {
			if (!is_float)
				rabi = x64_find_register_type_id_bytes(0, X64_REG_RDI,  size);
			else
				rabi = x64_find_register_type_id_bytes(1, X64_REG_XMM0, size);

			ret = x64_overflow_reg(rabi, c, f);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
		}

		movx = NULL;

		if (!is_float) {
			mov  = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);

			if (size < 8) {
				if (variable_signed(v))
					movx = x64_find_OpCode(X64_MOVSX, size, 8, X64_E2G);
				else if (size < 4)
					movx = x64_find_OpCode(X64_MOVZX, size, 8, X64_E2G);
			}

			if (0 == src->dag_node->color) {

				ret = x64_overflow_reg(rabi, c, f);
				if (ret < 0)
					return ret;

				ret = x64_load_const(rabi, src->dag_node, c, f);
				if (ret < 0)
					return ret;

				rabi = x64_find_register_color_bytes(rabi->color, 8);
				rs   = rabi;
			}
		} else {
			nb_floats++;

			if (0 == src->dag_node->color) {
				src->dag_node->color = -1;
				v->global_flag       =  1;
			}

			if (VAR_FLOAT == v->type) {
				mov  = x64_find_OpCode(X64_MOVSS,    4, 4, X64_G2E);
				movx = x64_find_OpCode(X64_CVTSS2SD, 4, 8, X64_E2G);
			} else
				mov  = x64_find_OpCode(X64_MOVSD, size, size, X64_G2E);
		}


		if (!rs) {
			if (!src->dag_node->loaded)
				src->dag_node->color = x64_find_register_color_bytes(rabi->color, size)->color;
			else
				logd("src->dag_node->color: %#lx\n", src->dag_node->color);

			if (rd && X64_COLOR_CONFLICT(rd->color, src->dag_node->color)) {

				ret = x64_overflow_reg2(rd, src->dag_node, c, f);
				if (ret < 0) {
					loge("\n");
					return ret;
				}
			}

			X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);
			rs = x64_find_register_color_bytes(rs->color, 8);
		}

		if (!rd) {
			if (movx) {
				inst = x64_make_inst_E2G(movx, rs,  rs);
				X64_INST_ADD_CHECK(c->instructions, inst);
			}

			inst = x64_make_inst_G2P(mov, rsp, v->sp_offset, rs);
			X64_INST_ADD_CHECK(c->instructions, inst);
			continue;
		}

		ret = x64_overflow_reg2(rd, src->dag_node, c, f);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		if (!X64_COLOR_CONFLICT(rd->color, rs->color)) {
			if (movx) {
				inst = x64_make_inst_E2G(movx, rs,  rs);
				X64_INST_ADD_CHECK(c->instructions, inst);
			}

			rd   = x64_find_register_color_bytes(rd->color, rs->bytes);
			inst = x64_make_inst_G2E(mov, rd, rs);
			X64_INST_ADD_CHECK(c->instructions, inst);

		} else if (movx) {
			inst = x64_make_inst_E2G(movx, rs,  rs);
			if (!inst) {
				loge("\n");
				return -ENOMEM;
			}

			inst_movx[nb_movx++] = inst;
		}
	}

	for (i = 0; i < nb_movx; i++) {
		X64_INST_ADD_CHECK(c->instructions, inst_movx[i]);
	}

	return nb_floats;
}

static int _x64_call_save_ret_regs(_3ac_code_t* c, function_t* f, function_t* pf)
{
	register_t* r;
	variable_t* v;

	int i;
	for (i = 0; i < pf->rets->size; i++) {
		v  =        pf->rets->data[i];

		int is_float = variable_float(v);

		if (is_float) {

			if (i > 0) {
				loge("\n");
				return -1;
			}

			r = x64_find_register_type_id_bytes(is_float, 0, 8);
		} else
			r = x64_find_register_type_id_bytes(is_float, x64_abi_ret_regs[i], 8);

		int ret = x64_overflow_reg(r, c, f);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}
	return 0;
}

static int _x64_dst_reg_valid(register_t* rd, register_t** updated_regs, int nb_updated, int abi_idx, int abi_total)
{
	register_t* r;

	int i;
	for (i = 0; i < nb_updated; i++) {

		r  = updated_regs[i];

		if (X64_COLOR_CONFLICT(r->color, rd->color))
			return 0;
	}

	for (i = abi_idx; i < abi_total; i++) {

		r  = x64_find_register_type_id_bytes(X64_COLOR_TYPE(rd->color), x64_abi_ret_regs[i], rd->bytes);

		if (X64_COLOR_CONFLICT(r->color, rd->color))
			return 0;
	}

	return 1;
}

static int _x64_call_update_dsts(_3ac_code_t* c, function_t* f, register_t** updated_regs, int max_updated)
{
	_3ac_operand_t*  dst;
	dag_node_t*     dn;
	variable_t*     v;

	register_t*     rd;
	register_t*     rs;
	x64_OpCode_t*   mov;

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
		int dst_size = x64_variable_size (v);

		if (is_float) {
			if (i > 0) {
				loge("\n");
				return -1;
			}

			rs = x64_find_register_type_id_bytes(is_float, X64_REG_RAX, dst_size);

			if (VAR_FLOAT == dn->var->type)
				mov = x64_find_OpCode(X64_MOVSS, dst_size, dst_size, X64_G2E);
			else
				mov = x64_find_OpCode(X64_MOVSD, dst_size, dst_size, X64_G2E);

			idx_float++;
		} else {
			rs  = x64_find_register_type_id_bytes(is_float, x64_abi_ret_regs[idx_int], dst_size);

			mov = x64_find_OpCode(X64_MOV, dst_size, dst_size, X64_G2E);

			idx_int++;
		}

		instruction_t*  inst;

		if (dn->color > 0) {
			rd = x64_find_register_color(dn->color);

			int rd_vars = x64_reg_cached_vars(rd);

			if (rd_vars > 1) {
				dn->color  = -1;
				dn->loaded = 0;
				vector_del(rd->dag_nodes, dn);

			} else if (rd_vars > 0
					&& !vector_find(rd->dag_nodes, dn)) {
				dn->color  = -1;
				dn->loaded = 0;

			} else {
				X64_SELECT_REG_CHECK(&rd, dn, c, f, 0);

				if (dn->color == rs->color) {
					assert(nb_updated < max_updated);

					updated_regs[nb_updated++] = rs;
					continue;
				}

				int valid = _x64_dst_reg_valid(rd, updated_regs, nb_updated, idx_int, nb_int);
				if (valid) {
					inst = x64_make_inst_G2E(mov, rd, rs);
					X64_INST_ADD_CHECK(c->instructions, inst);

					assert(nb_updated < max_updated);

					updated_regs[nb_updated++] = rd;
					continue;
				}

				assert(0  == vector_del(rd->dag_nodes, dn));
				dn->color  = -1;
				dn->loaded =  0;
			}
		}

		int rs_vars = x64_reg_cached_vars(rs);

		if (0 == rs_vars) {
			if (vector_add(rs->dag_nodes, dn) < 0)
				return -ENOMEM;

			assert(nb_updated < max_updated);

			updated_regs[nb_updated++] = rs;

			dn->color  = rs->color;
			dn->loaded = 1;
		} else {
			rela_t* rela = NULL;

			if (0 == v->bp_offset && !v->global_flag && !v->local_flag) {

				int size = f->local_vars_size + dst_size;

				if (size & 0x7)
					size = (size + 7) >> 3 << 3;

				v->bp_offset = -size;
				v->tmp_flag  = 1;

				f->local_vars_size = size;

				logd("v->bp_offset: %d, local_flag: %d, tmp_flag: %d, rs->name: %s\n", v->bp_offset, v->local_flag, v->tmp_flag, rs->name);
			}

			inst = x64_make_inst_G2M(&rela, mov, dn->var, NULL, rs);
			X64_INST_ADD_CHECK(c->instructions, inst);
			X64_RELA_ADD_CHECK(f->data_relas, rela, c, dn->var, NULL);
		}
	}
	return nb_updated;
}

static int _x64_inst_call_handler(native_t* ctx, _3ac_code_t* c)
{
	if (c->srcs->size < 1) {
		loge("\n");
		return -EINVAL;
	}

	x64_context_t* x64    = ctx->priv;
	function_t*    f      = x64->f;

	_3ac_operand_t* src0   = c->srcs->data[0];
	variable_t*    var_pf = src0->dag_node->var;
	function_t*    pf     = var_pf->func_ptr;

	if (FUNCTION_PTR != var_pf->type || !pf) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	register_t*     rsp  = x64_find_register("rsp");
	register_t*     rax  = x64_find_register("rax");
	register_t*     eax  = x64_find_register("eax");
//	x64_OpCode_t*   xor;
	x64_OpCode_t*   mov;
	x64_OpCode_t*   sub;
	x64_OpCode_t*   add;
	x64_OpCode_t*   call;
	instruction_t*  inst;
	instruction_t*  inst_rsp = NULL;

	int data_rela_size = f->data_relas->size;
	int text_rela_size = f->text_relas->size;
	logd("f->data_relas->size: %d, f->text_relas->size: %d\n", f->data_relas->size, f->text_relas->size);

	int ret;
	int i;

	if (pf->rets) {
		ret = _x64_call_save_ret_regs(c, f, pf);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	ret = x64_overflow_reg(rax, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	x64_call_rabi(NULL, NULL, c);

	int32_t stack_size = _x64_inst_call_stack_size(c);
	if (stack_size > 0) {
		sub          = x64_find_OpCode(X64_SUB,  4,4, X64_I2E);
		inst_rsp = x64_make_inst_I2E(sub, rsp, (uint8_t*)&stack_size, 4);
		X64_INST_ADD_CHECK(c->instructions, inst_rsp);
	}

	ret = _x64_inst_call_argv(c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	uint32_t imm = ret > 0;

	mov  = x64_find_OpCode(X64_MOV, 4,4, X64_I2G);
	inst = x64_make_inst_I2G(mov, eax, (uint8_t*)&imm, sizeof(imm));
	X64_INST_ADD_CHECK(c->instructions, inst);

	register_t* saved_regs[X64_ABI_CALLER_SAVES_NB];

	int save_size = x64_caller_save_regs(c, x64_abi_caller_saves, X64_ABI_CALLER_SAVES_NB, stack_size, saved_regs);
	if (save_size < 0) {
		loge("\n");
		return save_size;
	}

	if (stack_size > 0) {
		int32_t size = stack_size + save_size;
		assert(inst_rsp);
		memcpy(inst_rsp->code + inst_rsp->len - 4, &size, 4);
	}

	if (var_pf->const_literal_flag) {
		assert(0 == src0->dag_node->color);

		int32_t offset = 0;
		call = x64_find_OpCode(X64_CALL, 4,4, X64_I);
		inst = x64_make_inst_I(call, (uint8_t*)&offset, 4);
		X64_INST_ADD_CHECK(c->instructions, inst);

		inst->OpCode = (OpCode_t*)call;

		rela_t* rela = calloc(1, sizeof(rela_t));
		if (!rela)
			return -ENOMEM;

		rela->inst_offset = 1;
		X64_RELA_ADD_CHECK(f->text_relas, rela, c, NULL, pf);
	} else {
		assert(0 != src0->dag_node->color);

		call = x64_find_OpCode(X64_CALL, 8,8, X64_E);

		if (src0->dag_node->color > 0) {

			register_t* r_pf = NULL;

			ret = x64_select_reg(&r_pf, src0->dag_node, c, f, 1);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			inst = x64_make_inst_E(call, r_pf);
			X64_INST_ADD_CHECK(c->instructions, inst);

			inst->OpCode = (OpCode_t*)call;
		} else {
			rela_t* rela = NULL;

			inst = x64_make_inst_M(&rela, call, var_pf, NULL);
			X64_INST_ADD_CHECK(c->instructions, inst);
			X64_RELA_ADD_CHECK(f->text_relas, rela, c, NULL, pf);

			inst->OpCode = (OpCode_t*)call;
		}
	}

	if (stack_size > 0) {
		add  = x64_find_OpCode(X64_ADD, 4, 4, X64_I2E);
		inst = x64_make_inst_I2E(add, rsp, (uint8_t*)&stack_size, 4);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	int nb_updated = 0;
	register_t* updated_regs[X64_ABI_RET_NB * 2];

	if (pf->rets && pf->rets->size > 0 && c->dsts) {

		nb_updated = _x64_call_update_dsts(c, f, updated_regs, X64_ABI_RET_NB * 2);
		if (nb_updated < 0) {
			loge("\n");
			return nb_updated;
		}
	}

	if (save_size > 0) {
		ret = x64_pop_regs(c->instructions, saved_regs, save_size >> 3, updated_regs, nb_updated);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	f->call_flag = 1;
	return 0;
}

static int _x64_inst_unary(native_t* ctx, _3ac_code_t* c, int OpCode_type)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;
	_3ac_operand_t* src = c->srcs->data[0];
	_3ac_operand_t* dst = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int ret = x64_inst_op2(X64_MOV, dst->dag_node, src->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	instruction_t*  inst   = NULL;
	register_t*     rd     = NULL;
	variable_t*     var    = dst->dag_node->var;

	x64_OpCode_t*   OpCode = x64_find_OpCode(OpCode_type, var->size, var->size, X64_E);
	if (!OpCode) {
		loge("\n");
		return -1;
	}

	if (dst->dag_node->color > 0) {
		X64_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);
		inst = x64_make_inst_E(OpCode, rd);
		X64_INST_ADD_CHECK(c->instructions, inst);

	} else {
		rela_t* rela = NULL;

		inst = x64_make_inst_M(&rela, OpCode, var, NULL);
		X64_INST_ADD_CHECK(c->instructions, inst);
		X64_RELA_ADD_CHECK(f->data_relas, rela, c, var, NULL);
	}

	return 0;
}

static int _x64_inst_unary_assign(native_t* ctx, _3ac_code_t* c, int OpCode_type)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;
	_3ac_operand_t* src = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	instruction_t*  inst   = NULL;
	register_t*     rs     = NULL;
	variable_t*     var    = src->dag_node->var;

	x64_OpCode_t*   OpCode = x64_find_OpCode(OpCode_type, var->size, var->size, X64_E);
	if (!OpCode) {
		loge("\n");
		return -1;
	}

	if (src->dag_node->color > 0) {
		X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);
		inst = x64_make_inst_E(OpCode, rs);
		X64_INST_ADD_CHECK(c->instructions, inst);

	} else if (0 == src->dag_node->color) {
		loge("\n");
		return -EINVAL;
	} else {
		rela_t* rela = NULL;

		inst = x64_make_inst_M(&rela, OpCode, var, NULL);
		X64_INST_ADD_CHECK(c->instructions, inst);
		X64_RELA_ADD_CHECK(f->data_relas, rela, c, var, NULL);
	}

	return 0;
}

static int _x64_inst_bit_not_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_unary(ctx, c, X64_NOT);
}

static int _x64_inst_neg_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;
	_3ac_operand_t* src = c->srcs->data[0];
	_3ac_operand_t* dst = c->dsts->data[0];

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

	variable_t* v = dst->dag_node->var;

	int is_float      = variable_float(v);
	int size          = x64_variable_size (v);

	if (!is_float)
		return _x64_inst_unary(ctx, c, X64_NEG);

	instruction_t*  inst = NULL;
	register_t*     rd   = NULL;
	register_t*     rs   = NULL;
	x64_OpCode_t*   pxor = x64_find_OpCode(X64_PXOR,  8, 8, X64_E2G);
	x64_OpCode_t*   sub  = x64_find_OpCode(X64_SUBSS, 4, 4, X64_E2G);

	if (v->size > 4)
		sub = x64_find_OpCode(X64_SUBSD, 8, 8, X64_E2G);

	X64_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	inst = x64_make_inst_E2G(pxor, rd, rd);
	X64_INST_ADD_CHECK(c->instructions, inst);

	if (src->dag_node->color > 0) {
		X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

		inst = x64_make_inst_E2G(sub, rd, rs);
		X64_INST_ADD_CHECK(c->instructions, inst);

	} else {
		rela_t* rela = NULL;

		v = src->dag_node->var;

		if (0 == src->dag_node->color) {
			v->global_flag = 1;
			v->local_flag  = 0;
			v->tmp_flag    = 0;
		}

		inst = x64_make_inst_M2G(&rela, sub, rd, NULL, v);
		X64_INST_ADD_CHECK(c->instructions, inst);
		X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
	}

	return 0;
}

static int _x64_inst_inc(native_t* ctx, _3ac_code_t* c, int INC, int ADD)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;
	_3ac_operand_t* src = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color)
		return -EINVAL;

	variable_t*     v    = src->dag_node->var;
	register_t*     rs   = NULL;
	x64_OpCode_t*   OpCode;
	instruction_t*  inst = NULL;

	int imm_size = 1;
	if (v->data_size > 0xff)
		imm_size = 4;

	if (v->nb_pointers > 0)
		OpCode = x64_find_OpCode(ADD, imm_size, v->size, X64_I2E);
	else
		OpCode = x64_find_OpCode(INC, v->size, v->size, X64_E);

	if (!OpCode) {
		loge("v->size: %d, imm_size: %d\n", v->size, imm_size);
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (v->nb_pointers > 0) {
		if (src->dag_node->color > 0) {
			X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);
			inst = x64_make_inst_I2E(OpCode, rs, (uint8_t*)&v->data_size, imm_size);
			X64_INST_ADD_CHECK(c->instructions, inst);
		} else {
			rela_t* rela = NULL;

			inst = x64_make_inst_I2M(&rela, OpCode, v, NULL, (uint8_t*)&v->data_size, imm_size);
			X64_INST_ADD_CHECK(c->instructions, inst);
			X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
		}

	} else {
		if (src->dag_node->color > 0) {
			X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);
			inst = x64_make_inst_E(OpCode, rs);
			X64_INST_ADD_CHECK(c->instructions, inst);
		} else {
			rela_t* rela = NULL;

			inst = x64_make_inst_M(&rela, OpCode, v, NULL);
			X64_INST_ADD_CHECK(c->instructions, inst);
			X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
		}
	}

	return 0;
}

static int _x64_inst_inc_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_inc(ctx, c, X64_INC, X64_ADD);
}

static int _x64_inst_dec_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_inc(ctx, c, X64_DEC, X64_SUB);
}

static int _x64_inst_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_inst_pointer(ctx, c, 0);
}

static int _x64_inst_assign_array_index(native_t* ctx, _3ac_code_t* c, int OpCode_type)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	x64_context_t*  x64    = ctx->priv;
	function_t*     f      = x64->f;

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

	variable_t*     vscale = scale->dag_node->var;
	variable_t*     vb     = base->dag_node->var;
	variable_t*     vs     = src ->dag_node->var;

	register_t*     rs     = NULL;
	x64_sib_t           sib    = {0};

	x64_OpCode_t*   OpCode;
	instruction_t*  inst;

	int is_float = variable_float(vs);
	int size     = x64_variable_size (vs);

	if (size > vscale->data.i)
		size = vscale->data.i;

	int ret  = x64_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (vb->nb_dimentions > 1) {
		OpCode = x64_find_OpCode(X64_LEA, 8, 8, X64_E2G);
	} else {
		if (is_float) {
			OpCode_type = x64_float_OpCode_type(OpCode_type, vs->type);

			if (0 == src->dag_node->color) {
				src->dag_node->color = -1;
				vs->global_flag      =  1;
			}
		} else if (0 == src->dag_node->color) {

			OpCode = x64_find_OpCode(OpCode_type, size, size, X64_I2E);

			if (OpCode) {
				if (sib.index)
					inst = x64_make_inst_I2SIB(OpCode, sib.base, sib.index, sib.scale, sib.disp, (uint8_t*)&vs->data, size);
				else
					inst = x64_make_inst_I2P(OpCode, sib.base, sib.disp, (uint8_t*)&vs->data, size);

				X64_INST_ADD_CHECK(c->instructions, inst);
				return 0;
			}

			if (0 == src->dag_node->color)
				src->dag_node->color = -1;
		}

		OpCode = x64_find_OpCode(OpCode_type, size, size, X64_G2E);
	}

	if (!OpCode) {
		loge("\n");
		return -EINVAL;
	}

	ret = x64_select_reg(&rs, src->dag_node, c, f, 1);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	rs = x64_find_register_color_bytes(rs->color, size);

	if (sib.index) {
		inst = x64_make_inst_G2SIB(OpCode, sib.base, sib.index, sib.scale, sib.disp, rs);
		X64_INST_ADD_CHECK(c->instructions, inst);
	} else {
		inst = x64_make_inst_G2P(OpCode, sib.base, sib.disp, rs);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}
	return 0;
}

static int _x64_inst_array_index(native_t* ctx, _3ac_code_t* c, int lea_flag)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	x64_context_t*  x64    = ctx->priv;
	function_t*     f      = x64->f;

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

	register_t*     rd  = NULL;
	x64_sib_t           sib = {0};

	x64_OpCode_t*   OpCode;
	instruction_t*  inst;

	int ret = x64_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = x64_array_index_reg(&sib, base->dag_node, index->dag_node, scale->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (vb->nb_dimentions > 1 || (vb->type >= STRUCT && 0 == vb->nb_pointers) || lea_flag) {
		OpCode = x64_find_OpCode(X64_LEA, rd->bytes, rd->bytes, X64_E2G);

	} else {
		int is_float = variable_float(vd);
		if (is_float) {
			if (VAR_FLOAT == vd->type)
				OpCode = x64_find_OpCode(X64_MOVSS, rd->bytes, rd->bytes, X64_E2G);
			else if (VAR_DOUBLE == vd->type)
				OpCode = x64_find_OpCode(X64_MOVSD, rd->bytes, rd->bytes, X64_E2G);
		} else
			OpCode = x64_find_OpCode(X64_MOV,   rd->bytes, rd->bytes, X64_E2G);
	}

	if (!OpCode) {
		loge("\n");
		return -EINVAL;
	}

	if (sib.index) {
		inst = x64_make_inst_SIB2G(OpCode, rd, sib.base, sib.index, sib.scale, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	} else {
		inst = x64_make_inst_P2G(OpCode, rd, sib.base, sib.disp);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}
	return 0;
}

static int _x64_inst_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_array_index(ctx, c, 0);
}

static int _x64_inst_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_inst_dereference(ctx, c);
}

static int _x64_inst_address_of_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->srcs || c->srcs->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	x64_context_t*  x64  = ctx->priv;
	function_t*     f    = x64->f;

	_3ac_operand_t*  dst  = c->dsts->data[0];
	_3ac_operand_t*  src  = c->srcs->data[0];
	register_t*     rd   = NULL;
	rela_t*         rela = NULL;

	x64_OpCode_t*   lea;
	instruction_t*  inst;

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

	int ret = x64_select_reg(&rd, dst->dag_node, c, f, 0);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	assert(dst->dag_node->color > 0);

	ret = x64_overflow_reg2(rd, dst->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	lea  = x64_find_OpCode(X64_LEA, 8,8, X64_E2G);
	inst = x64_make_inst_M2G(&rela, lea, rd, NULL, src->dag_node->var);
	X64_INST_ADD_CHECK(c->instructions, inst);
	X64_RELA_ADD_CHECK(f->data_relas, rela, c, src->dag_node->var, NULL);
	return 0;
}

static int _div_mod(native_t* ctx, _3ac_code_t* c, int mod_flag)
{
	X64_INST_OP3_CHECK()

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));

		return _x64_inst_float_op3(X64_DIV, dst->dag_node, src0->dag_node, src1->dag_node, c, f);
	}

	int ret = x64_inst_op2(X64_MOV, dst->dag_node, src0->dag_node, c, f);
	if (ret < 0)
		return ret;

	return x64_inst_int_div(dst->dag_node, src1->dag_node, c, f, mod_flag);
}

static int _x64_inst_div_handler(native_t* ctx, _3ac_code_t* c)
{
	return _div_mod(ctx, c, 0);
}

static int _x64_inst_mod_handler(native_t* ctx, _3ac_code_t* c)
{
	return _div_mod(ctx, c, 1);
}

static int _x64_inst_mul_handler(native_t* ctx, _3ac_code_t* c)
{
	X64_INST_OP3_CHECK()

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));

		return _x64_inst_float_op3(X64_MUL, dst->dag_node, src0->dag_node, src1->dag_node, c, f);
	}

	int ret = x64_inst_op2(X64_MOV, dst->dag_node, src0->dag_node, c, f);
	if (ret < 0)
		return ret;
	return x64_inst_int_mul(dst->dag_node, src1->dag_node, c, f);
}

static int _x64_inst_op3(native_t* ctx, _3ac_code_t* c, int OpCode_type)
{
	X64_INST_OP3_CHECK()

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src0->dag_node->var)) {

		assert(variable_float(src1->dag_node->var));
		assert(variable_float(dst->dag_node->var));

		return _x64_inst_float_op3(OpCode_type, dst->dag_node, src0->dag_node, src1->dag_node, c, f);
	}

	int ret = x64_inst_op2(X64_MOV, dst->dag_node, src0->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	ret = x64_inst_op2(OpCode_type, dst->dag_node, src1->dag_node, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	return ret;
}

#define X64_INST_OP3(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return _x64_inst_op3(ctx, c, X64_##op); \
}
X64_INST_OP3(add,     ADD)
X64_INST_OP3(sub,     SUB)
X64_INST_OP3(bit_and, AND)
X64_INST_OP3(bit_or,  OR)


static int _x64_inst_teq_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_inst_teq(ctx, c);
}

static int _x64_inst_logic_not_handler(native_t* ctx, _3ac_code_t* c)
{
	int ret = x64_inst_teq(ctx, c);
	if (ret < 0)
		return ret;

	return x64_inst_set(ctx, c, X64_SETZ);
}

static int _x64_inst_cmp_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_inst_cmp(ctx, c);
}

#define X64_INST_SET(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_inst_set(ctx, c, X64_##op); \
}
X64_INST_SET(setz,  SETZ)
X64_INST_SET(setnz, SETNZ)
X64_INST_SET(setgt, SETG)
X64_INST_SET(setge, SETGE)
X64_INST_SET(setlt, SETL)
X64_INST_SET(setle, SETLE)


#define X64_INST_CMP_SET(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_inst_cmp_set(ctx, c, X64_##op); \
}
X64_INST_CMP_SET(eq, SETZ)
X64_INST_CMP_SET(ne, SETNZ)
X64_INST_CMP_SET(gt, SETG)
X64_INST_CMP_SET(ge, SETGE)
X64_INST_CMP_SET(lt, SETL)
X64_INST_CMP_SET(le, SETLE)

static int _x64_inst_cast_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;
	_3ac_operand_t* src = c->srcs->data[0];
	_3ac_operand_t* dst = c->dsts->data[0];

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

	x64_OpCode_t*   lea;
	instruction_t*  inst;

	register_t*     rd   = NULL;
	rela_t*         rela = NULL;

	variable_t*     vd   = dst->dag_node->var;
	variable_t*     vs   = src->dag_node->var;

	int src_size = x64_variable_size(vs);
	int dst_size = x64_variable_size(vd);

	if (variable_float(vs) || variable_float(vd))
		return x64_inst_float_cast(dst->dag_node, src->dag_node, c, f);

	if (src_size < dst_size)
		return x64_inst_movx(dst->dag_node, src->dag_node, c, f);

	logw("src_size: %d, dst_size: %d\n", src_size, dst_size);

	if (src->dag_node->var->nb_dimentions > 0)
		return x64_inst_op2(X64_LEA, dst->dag_node, src->dag_node, c, f);

	return x64_inst_op2(X64_MOV, dst->dag_node, src->dag_node, c, f);
}

static int _x64_inst_mul_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t* x64  = ctx->priv;
	function_t*    f    = x64->f;

	_3ac_operand_t* dst  = c->dsts->data[0];
	_3ac_operand_t* src  = c->srcs->data[0];

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

	if (variable_float(src->dag_node->var)) {

		assert(variable_float(dst->dag_node->var));

		return _x64_inst_float_op2(X64_MUL, dst->dag_node, src->dag_node, c, f);
	}

	return x64_inst_int_mul(dst->dag_node, src->dag_node, c, f);
}

static int _div_mod_assign(native_t* ctx, _3ac_code_t* c, int mod_flag)
{
	if (!c->dsts || c->dsts->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->srcs || c->srcs->size != 1) {
		loge("\n");
		return -EINVAL;
	}

	x64_context_t*  x64  = ctx->priv;
	function_t*     f    = x64->f;

	_3ac_operand_t* dst   = c->dsts->data[0];
	_3ac_operand_t* src   = c->srcs->data[0];

	if (!src || !src->dag_node) {
		loge("\n");
		return -EINVAL;
	}

	if (!dst || !dst->dag_node) {
		loge("\n");
		return -EINVAL;
	}

	if (src->dag_node->var->size != dst->dag_node->var->size) {
		loge("\n");
		return -EINVAL;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	if (variable_float(src->dag_node->var)) {
		assert(variable_float(dst->dag_node->var));

		return _x64_inst_float_op2(X64_DIV, dst->dag_node, src->dag_node, c, f);
	}

	return x64_inst_int_div(dst->dag_node, src->dag_node, c, f, mod_flag);
}

static int _x64_inst_div_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	return _div_mod_assign(ctx, c, 0);
}

static int _x64_inst_mod_assign_handler(native_t* ctx, _3ac_code_t* c)
{
	return _div_mod_assign(ctx, c, 1);
}

static int _x64_inst_dump_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	x64_context_t* x64  = ctx->priv;
	function_t*    f    = x64->f;
	_3ac_operand_t* src  = c->srcs->data[0];
	_3ac_operand_t* logf = c->srcs->data[1];
	_3ac_operand_t* msg  = c->srcs->data[2];

	if (!logf || !logf->dag_node)
		return -EINVAL;

	if (!msg || !msg->dag_node)
		return -EINVAL;

	instruction_t*  inst = NULL;
	register_t*     rs   = NULL;
	register_t*     rd   = NULL;
	register_t*     rdi  = x64_find_register("rdi");
	register_t*     rsi  = x64_find_register("rsi");
	register_t*     rax  = x64_find_register("rax");

	x64_OpCode_t*   lea  = x64_find_OpCode(X64_LEA, 8, 8, X64_E2G);
	x64_OpCode_t*   call = x64_find_OpCode(X64_CALL,4, 4, X64_I);
	x64_OpCode_t*   xor  = x64_find_OpCode(X64_XOR, 8, 8, X64_G2E);
	x64_OpCode_t*   mov  = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = x64_make_inst_G2E(mov, rsi, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);

	rela_t* rela = NULL;

	inst = x64_make_inst_M2G(&rela, lea, rdi, NULL, msg->dag_node->var);
	X64_INST_ADD_CHECK(c->instructions, inst);
	X64_RELA_ADD_CHECK(f->data_relas, rela, c, msg->dag_node->var, NULL);

	inst = x64_make_inst_G2E(xor, rax, rax);
	X64_INST_ADD_CHECK(c->instructions, inst);

	// call printf() to show msg
	uint32_t imm = 0;
	inst = x64_make_inst_I(call, (uint8_t*)&imm, sizeof(imm));
	X64_INST_ADD_CHECK(c->instructions, inst);

	rela = calloc(1, sizeof(rela_t));
	if (!rela)
		return -ENOMEM;
	rela->inst_offset = 1;
	X64_RELA_ADD_CHECK(f->text_relas, rela, c, NULL, logf->dag_node->var->func_ptr);

	inst = x64_make_inst_G2E(xor, rax, rax);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rax, 0, rax);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _x64_inst_vla_alloc_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	x64_context_t* x64  = ctx->priv;
	function_t*    f    = x64->f;
	_3ac_operand_t* dst  = c->dsts->data[0];
	_3ac_operand_t* src  = c->srcs->data[0];

	if (!dst || !dst->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	instruction_t*  inst = NULL;
	register_t*     rs   = NULL;
	register_t*     rd   = NULL;
	register_t*     rsp  = x64_find_register("rsp");

	x64_OpCode_t*   add  = x64_find_OpCode(X64_ADD, 4, 8, X64_I2E);
	x64_OpCode_t*   and  = x64_find_OpCode(X64_AND, 4, 8, X64_I2E);
	x64_OpCode_t*   sub  = x64_find_OpCode(X64_SUB, 8, 8, X64_E2G);
	x64_OpCode_t*   mov  = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	uint32_t imm  = 0xf;
	inst = x64_make_inst_I2E(add, rs, (uint8_t*)&imm, sizeof(imm));
	X64_INST_ADD_CHECK(c->instructions, inst);

	imm  = ~0xf;
	inst = x64_make_inst_I2E(and, rs, (uint8_t*)&imm, sizeof(imm));
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_E2G(sub, rsp, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);

	int ret = x64_save_var(src->dag_node, c, f);
	if (ret < 0)
		return ret;

	X64_SELECT_REG_CHECK(&rd, dst->dag_node, c, f, 0);

	inst = x64_make_inst_G2E(mov, rd, rsp);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _x64_inst_vla_free_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	x64_context_t* x64  = ctx->priv;
	function_t*    f    = x64->f;
	_3ac_operand_t* src  = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	instruction_t*  inst = NULL;
	register_t*     rs   = NULL;
	register_t*     rsp  = x64_find_register("rsp");
	x64_OpCode_t*   add  = x64_find_OpCode(X64_ADD, 8, 8, X64_E2G);
	x64_OpCode_t*   xor  = x64_find_OpCode(X64_XOR, 8, 8, X64_G2E);

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	inst = x64_make_inst_E2G(add, rsp, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2E(xor, rs, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);

	return x64_save_var(src->dag_node, c, f);
}

static int _x64_inst_return_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	x64_context_t*  x64  = ctx->priv;
	function_t*     f    = x64->f;
	_3ac_operand_t*  src  = NULL;
	instruction_t*  inst = NULL;
	variable_t*     v    = NULL;
	rela_t*         rela = NULL;

	register_t*     rd   = NULL;
	register_t*     rs   = NULL;
	register_t*     rsp  = x64_find_register("rsp");
	register_t*     rbp  = x64_find_register("rbp");

	x64_OpCode_t*   pop;
	x64_OpCode_t*   mov;
	x64_OpCode_t*   ret;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int i;
	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		v   = src->dag_node->var;

		int size     = x64_variable_size (v);
		int is_float = variable_float(v);

		if (i > 0 && is_float) {
			loge("\n");
			return -1;
		}

		int retsize = size > 4 ? 8 : 4;

		if (src->dag_node->color > 0)
			rs = x64_find_register_color_bytes(src->dag_node->color, size);
		else
			rs = NULL;

		if (is_float) {
			rd = x64_find_register_type_id_bytes(is_float, 0, retsize);

			if (0 == src->dag_node->color) {
				src->dag_node->color = -1;
				v->global_flag       =  1;
			}

			if (VAR_FLOAT == v->type)
				mov = x64_find_OpCode(X64_MOVSS, size, rd->bytes, X64_E2G);
			else
				mov = x64_find_OpCode(X64_MOVSD, size, rd->bytes, X64_E2G);
		} else {
			rd = x64_find_register_type_id_bytes(is_float, x64_abi_ret_regs[i], retsize);

			if (rd->bytes > size) {
				if (variable_signed(v))
					mov = x64_find_OpCode(X64_MOVSX, size, rd->bytes, X64_E2G);
				else
					mov = x64_find_OpCode(X64_MOVZX, size, rd->bytes, X64_E2G);
			} else
				mov = x64_find_OpCode(X64_MOV, rd->bytes, rd->bytes, X64_E2G);
		}

		logd("rd: %s, rd->dag_nodes->size: %d\n", rd->name, rd->dag_nodes->size);

		int ret = x64_save_reg(rd, c, f);
		if (ret < 0)
			return ret;

		if (src->dag_node->color > 0) {
			int j;
			for (j = 0; j < i; j++) {
				if (x64_abi_ret_regs[j] == rs->id)
					break;
			}

			if (j < i) {
				vector_del(rs->dag_nodes, src->dag_node);

				logd("i: %d, j: %d, rd: %s, rs: %s, rs->dag_node->size: %d\n", i, j, rd->name, rs->name, rs->dag_nodes->size);

				src->dag_node->color = x64_find_register_color_bytes(rd->color, size)->color;

				X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

				if (rd->bytes > size) {
					inst = x64_make_inst_E2G(mov, rd, rs);
					X64_INST_ADD_CHECK(c->instructions, inst);
				}
			} else {
				X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

				if (!X64_COLOR_CONFLICT(rd->color, rs->color) || rd->bytes > size) {
					inst = x64_make_inst_E2G(mov, rd, rs);
					X64_INST_ADD_CHECK(c->instructions, inst);
				}
			}
		} else if (0 == src->dag_node->color) {
			assert(0 == is_float);

			if (rd->bytes > size)
				variable_extend_bytes(v, rd->bytes);

			mov  = x64_find_OpCode(X64_MOV, rd->bytes, rd->bytes, X64_I2G);
			inst = x64_make_inst_I2G(mov, rd, (uint8_t*)&v->data, rd->bytes);
			X64_INST_ADD_CHECK(c->instructions, inst);
		} else {
			inst = x64_make_inst_M2G(&rela, mov, rd, NULL, v);
			X64_INST_ADD_CHECK(c->instructions, inst);
			X64_RELA_ADD_CHECK(f->data_relas, rela, c, v, NULL);
		}
	}
	return 0;
}

static int _x64_inst_memset_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	x64_context_t*  x64   = ctx->priv;
	function_t*     f     = x64->f;
	_3ac_operand_t*  dst   = c->srcs->data[0];
	_3ac_operand_t*  data  = c->srcs->data[1];
	_3ac_operand_t*  count = c->srcs->data[2];
	instruction_t*  inst  = NULL;

	register_t*	    rax   = x64_find_register("rax");
	register_t*	    rcx   = x64_find_register("rcx");
	register_t*	    rdi   = x64_find_register("rdi");
	register_t*	    rd;
	x64_OpCode_t*   mov;
	x64_OpCode_t*   stos;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	int ret = x64_overflow_reg2(rdi, dst->dag_node, c, f);
	if (ret < 0)
		return ret;

	ret = x64_overflow_reg2(rax, data->dag_node, c, f);
	if (ret < 0)
		return ret;

	ret = x64_overflow_reg2(rcx, count->dag_node, c, f);
	if (ret < 0)
		return ret;

#define X64_MEMSET_LOAD_REG(r, dn) \
	do { \
		int size = x64_variable_size(dn->var); \
		assert(8 == size); \
		\
		if (0 == dn->color) { \
			mov  = x64_find_OpCode(X64_MOV, size, size, X64_I2G); \
			inst = x64_make_inst_I2G(mov, r, (uint8_t*)&dn->var->data, size); \
			X64_INST_ADD_CHECK(c->instructions, inst); \
			\
		} else { \
			if (dn->color < 0) \
				dn->color = r->color; \
			X64_SELECT_REG_CHECK(&rd, dn, c, f, 1); \
			\
			if (!X64_COLOR_CONFLICT(rd->color, r->color)) { \
				mov  = x64_find_OpCode(X64_MOV, size, size, X64_G2E); \
				inst = x64_make_inst_G2E(mov, r, rd); \
				X64_INST_ADD_CHECK(c->instructions, inst); \
			} \
		} \
	} while (0)

	X64_MEMSET_LOAD_REG(rdi, dst  ->dag_node);
	X64_MEMSET_LOAD_REG(rax, data ->dag_node);
	X64_MEMSET_LOAD_REG(rcx, count->dag_node);

	stos = x64_find_OpCode(X64_STOS, 1, 8, X64_G);
	inst = x64_make_inst(stos, 1);
	X64_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

static int _x64_inst_nop_handler(native_t* ctx, _3ac_code_t* c)
{
	return 0;
}

static int _x64_inst_end_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	return 0;
}

#define X64_INST_JMP(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_inst_jmp(ctx, c, X64_##op); \
}

X64_INST_JMP(goto, JMP)
X64_INST_JMP(jz,   JZ)
X64_INST_JMP(jnz,  JNZ)
X64_INST_JMP(jgt,  JG)
X64_INST_JMP(jge,  JGE)
X64_INST_JMP(jlt,  JL)
X64_INST_JMP(jle,  JLE)

X64_INST_JMP(ja,   JA)
X64_INST_JMP(jb,   JB)
X64_INST_JMP(jae,  JAE)
X64_INST_JMP(jbe,  JBE)

static int _x64_inst_load_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t*     r   = NULL;
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

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

	r = x64_find_register_color(dn->color);

	if (x64_reg_used(r, dn)) {
		dn->color  = -1;
		dn->loaded =  0;
		vector_del(r->dag_nodes, dn);
		return 0;
	}

	ret = x64_load_reg(r, dn, c, f);
	if (ret < 0)
		return ret;

	ret = vector_add_unique(r->dag_nodes, dn);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	return 0;
}

static int _x64_inst_reload_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	register_t*     r   = NULL;
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

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

	r   = x64_find_register_color(dn->color);

	ret = x64_overflow_reg2(r, dn, c, f);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	dn->loaded = 0;
	ret = x64_load_reg(r, dn, c, f);
	if (ret < 0)
		return ret;

	ret = vector_add_unique(r->dag_nodes, dn);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	return 0;
}

static int _x64_inst_save_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	_3ac_operand_t*  src = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;
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

	return x64_save_var(dn, c, f);
}

#define X64_INST_BINARY_ASSIGN(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_binary_assign(ctx, c, X64_##op); \
}

X64_INST_BINARY_ASSIGN(assign,     MOV)
X64_INST_BINARY_ASSIGN(add_assign, ADD)
X64_INST_BINARY_ASSIGN(sub_assign, SUB)
X64_INST_BINARY_ASSIGN(and_assign, AND)
X64_INST_BINARY_ASSIGN(or_assign,  OR)

static int _x64_inst_assign_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_assign_pointer(ctx, c);
}
static int _x64_inst_assign_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_assign_array_index(ctx, c, X64_MOV);
}
static int _x64_inst_assign_dereference_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_assign_dereference(ctx, c);
}

#define X64_INST_SHIFT(name, op) \
static int _x64_inst_##name##_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_shift(ctx, c, X64_##op); \
} \
static int _x64_inst_##name##_assign_handler(native_t* ctx, _3ac_code_t* c) \
{ \
	return x64_shift_assign(ctx, c, X64_##op); \
}
X64_INST_SHIFT(shl, SHL)
X64_INST_SHIFT(shr, SHR)

static int _x64_inst_address_of_array_index_handler(native_t* ctx, _3ac_code_t* c)
{
	return _x64_inst_array_index(ctx, c, 1);
}

static int _x64_inst_address_of_pointer_handler(native_t* ctx, _3ac_code_t* c)
{
	return x64_inst_pointer(ctx, c, 1);
}

static int _x64_inst_push_rets_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

	instruction_t*  inst;
	x64_OpCode_t*   push = x64_find_OpCode(X64_PUSH, 8,8, X64_G);
	register_t*     r;

	int n = 0;
	int i;

	if (!f->void_flag) {
		n = f->rets->size;
		if (n > X64_ABI_RET_NB)
			return -EINVAL;
	}

	for (i = 0; i < n; i++) {
		r  = x64_find_register_type_id_bytes(0, x64_abi_ret_regs[i], 8);

		inst = x64_make_inst_G(push, r);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	if (n & 0x1) {
		r    = x64_find_register_type_id_bytes(0, x64_abi_ret_regs[n - 1], 8);
		inst = x64_make_inst_G(push, r);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}
	return 0;
}

static int _x64_inst_pop_rets_handler(native_t* ctx, _3ac_code_t* c)
{
	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

	instruction_t*  inst;
	x64_OpCode_t*   pop  = x64_find_OpCode(X64_POP, 8,8, X64_G);
	register_t*     r;

	int n = 0;
	int i;

	if (!f->void_flag) {
		n = f->rets->size;
		if (n > X64_ABI_RET_NB)
			return -EINVAL;
	}

	if (n & 0x1) {
		r    = x64_find_register_type_id_bytes(0, x64_abi_ret_regs[n - 1], 8);
		inst = x64_make_inst_G(pop, r);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	for (i = n - 1; i >= 0; i--) {
		r  = x64_find_register_type_id_bytes(0, x64_abi_ret_regs[i], 8);

		inst = x64_make_inst_G(pop, r);
		X64_INST_ADD_CHECK(c->instructions, inst);
	}

	return 0;
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

static int _x64_inst_va_start_handler(native_t* ctx, _3ac_code_t* c)
{
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	assert(3 == c->srcs->size);

	register_t*     rbp   = x64_find_register("rbp");
	register_t*     rptr  = NULL;
	register_t*     rap   = NULL;
	instruction_t*  inst  = NULL;
	_3ac_operand_t*  ap    = c->srcs->data[0];
	_3ac_operand_t*  ptr   = c->srcs->data[2];
	x64_OpCode_t*   mov   = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);
	x64_OpCode_t*   lea   = x64_find_OpCode(X64_LEA, 8, 8, X64_E2G);
	variable_t*     v     = ap->dag_node->var;

	int offset_int            = -f->args_int   * 8 - 8;
	int offset_float          = -f->args_float * 8 - X64_ABI_NB * 8 - 8;
	int offset_others         = 16;

	if (v->bp_offset >= 0) {
		loge("\n");
		return -1;
	}

	X64_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	X64_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	inst = x64_make_inst_P2G(lea, rptr, rbp, offset_int);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap,  0, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);


	inst = x64_make_inst_P2G(lea, rptr, rbp, offset_float);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap,  8, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);


	inst = x64_make_inst_P2G(lea, rptr, rbp, offset_others);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap,  16, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);


	mov  = x64_find_OpCode(X64_MOV, 4, 8, X64_I2E);

	inst = x64_make_inst_I2P(mov, rap,  24, (uint8_t*)&f->args_int, 4);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_I2P(mov, rap,  32, (uint8_t*)&f->args_float, 4);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

static int _x64_inst_va_end_handler(native_t* ctx, _3ac_code_t* c)
{
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	assert(2 == c->srcs->size);

	register_t*     rbp  = x64_find_register("rbp");
	register_t*     rptr = NULL;
	register_t*     rap  = NULL;
	instruction_t*  inst = NULL;
	_3ac_operand_t*  ap   = c->srcs->data[0];
	_3ac_operand_t*  ptr  = c->srcs->data[1];
	x64_OpCode_t*   mov  = x64_find_OpCode(X64_MOV, 8, 8, X64_G2E);
	x64_OpCode_t*   xor  = x64_find_OpCode(X64_XOR, 8, 8, X64_G2E);
	variable_t*     v    = ap->dag_node->var;

	if (v->bp_offset >= 0) {
		loge("\n");
		return -1;
	}

	ptr->dag_node->var->tmp_flag =  1;
	ptr->dag_node->color         = -1;

	X64_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	X64_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	inst = x64_make_inst_G2E(xor, rptr, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap, 0, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap, 8, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap, 16, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap, 24, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst = x64_make_inst_G2P(mov, rap, 32, rptr);
	X64_INST_ADD_CHECK(c->instructions, inst);

	ptr->dag_node->var->tmp_flag = 0;
	ptr->dag_node->color         = 0;
	ptr->dag_node->loaded        = 0;

	assert(0 == vector_del(rptr->dag_nodes, ptr->dag_node));
	return 0;
}

static int _x64_inst_va_arg_handler(native_t* ctx, _3ac_code_t* c)
{
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	assert(1 == c->dsts->size && 3 == c->srcs->size);

	instruction_t*  inst = NULL;
	register_t*     rbp  = x64_find_register("rbp");
	register_t*     rd   = NULL; // result
	register_t*     rap  = NULL; // ap
	register_t*     rptr = NULL; // ptr

	instruction_t*  inst_jge = NULL;
	instruction_t*  inst_jmp = NULL;

	_3ac_operand_t*  dst  = c->dsts->data[0];
	_3ac_operand_t*  ap   = c->srcs->data[0];
	_3ac_operand_t*  src  = c->srcs->data[1];
	_3ac_operand_t*  ptr  = c->srcs->data[2];
	variable_t*     v    = src->dag_node->var;

	x64_OpCode_t*   inc  = x64_find_OpCode(X64_INC, 8, 8, X64_E);
	x64_OpCode_t*   add  = x64_find_OpCode(X64_ADD, 4, 8, X64_I2E);
	x64_OpCode_t*   sub  = x64_find_OpCode(X64_SUB, 4, 8, X64_I2E);
	x64_OpCode_t*   cmp  = x64_find_OpCode(X64_CMP, 4, 8, X64_I2E);
	x64_OpCode_t*   mov  = x64_find_OpCode(X64_MOV, 8, 8, X64_E2G);
	x64_OpCode_t*   jge  = x64_find_OpCode(X64_JGE, 4, 4, X64_I);
	x64_OpCode_t*   jmp  = x64_find_OpCode(X64_JMP, 4, 4, X64_I);
	x64_OpCode_t*   mov2 = NULL;

	X64_SELECT_REG_CHECK(&rd,   dst->dag_node, c, f, 0);
	X64_SELECT_REG_CHECK(&rap,  ap ->dag_node, c, f, 1);
	X64_SELECT_REG_CHECK(&rptr, ptr->dag_node, c, f, 0);

	int is_float = variable_float(v);
	int size     = x64_variable_size(v);

	uint32_t nints   = X64_ABI_NB;
	uint32_t nfloats = X64_ABI_FLOAT_NB;
	uint32_t offset  = 0;
	uint32_t incptr  = 8;

	int idx_offset   = 24;
	int ptr_offset   = 0;

	if (is_float) {
		idx_offset   = 32;
		ptr_offset   = 8;
	}

	inst = x64_make_inst_I2P(cmp, rap, idx_offset, (uint8_t*)&nints, 4);
	X64_INST_ADD_CHECK(c->instructions, inst);

	inst_jge = x64_make_inst_I(jge, (uint8_t*)&offset, sizeof(offset));
	X64_INST_ADD_CHECK(c->instructions, inst_jge);


	inst = x64_make_inst_P2G(mov, rptr, rap, ptr_offset);
	X64_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst = x64_make_inst_I2P(sub, rap, ptr_offset, (uint8_t*)&incptr, 4);
	X64_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst_jmp = x64_make_inst_I(jmp, (uint8_t*)&offset, sizeof(offset));
	X64_INST_ADD_CHECK(c->instructions, inst_jmp);
	offset += inst_jmp->len;

	uint8_t* p = (uint8_t*)&offset;
	int i;
	for (i = 0; i < 4; i++)
		inst_jge->code[jge->nb_OpCodes + i] = p[i];

	offset = 0;
	inst = x64_make_inst_P2G(mov, rptr, rap, 16);
	X64_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	inst = x64_make_inst_I2P(add, rap, 16, (uint8_t*)&incptr, 4);
	X64_INST_ADD_CHECK(c->instructions, inst);
	offset += inst->len;

	for (i = 0; i < 4; i++)
		inst_jmp->code[jmp->nb_OpCodes + i] = p[i];

	inst = x64_make_inst_P(inc, rap, idx_offset, 8);
	X64_INST_ADD_CHECK(c->instructions, inst);

	if (is_float) {
		if (4 == size)
			mov2 = x64_find_OpCode(X64_MOVSS, 4, 4, X64_E2G);
		else if (8 == size)
			mov2 = x64_find_OpCode(X64_MOVSD, 8, 8, X64_E2G);
		else
			assert(0);
	} else
		mov2 = x64_find_OpCode(X64_MOV, size, size, X64_E2G);

	inst = x64_make_inst_P2G(mov2, rd, rptr, 0);
	X64_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

static x64_inst_handler_pt  x64_inst_handlers[] =
{
	[OP_CALL        ]  =  _x64_inst_call_handler,
	[OP_ARRAY_INDEX ]  =  _x64_inst_array_index_handler,
	[OP_POINTER     ]  =  _x64_inst_pointer_handler,

	[OP_TYPE_CAST   ]  =  _x64_inst_cast_handler,
	[OP_LOGIC_NOT  	]  =  _x64_inst_logic_not_handler,
	[OP_BIT_NOT     ]  =  _x64_inst_bit_not_handler,
	[OP_NEG  		]  =  _x64_inst_neg_handler,

	[OP_VA_START    ]  =  _x64_inst_va_start_handler,
	[OP_VA_ARG      ]  =  _x64_inst_va_arg_handler,
	[OP_VA_END      ]  =  _x64_inst_va_end_handler,

	[OP_INC         ]  =  _x64_inst_inc_handler,
	[OP_DEC         ]  =  _x64_inst_dec_handler,

	[OP_DEREFERENCE ]  =  _x64_inst_dereference_handler,
	[OP_ADDRESS_OF  ]  =  _x64_inst_address_of_handler,

	[OP_MUL  		]  =  _x64_inst_mul_handler,
	[OP_DIV  		]  =  _x64_inst_div_handler,
	[OP_MOD         ]  =  _x64_inst_mod_handler,

	[OP_ADD  		]  =  _x64_inst_add_handler,
	[OP_SUB  		]  =  _x64_inst_sub_handler,

	[OP_SHL         ]  =  _x64_inst_shl_handler,
	[OP_SHR         ]  =  _x64_inst_shr_handler,

	[OP_BIT_AND     ]  =  _x64_inst_bit_and_handler,
	[OP_BIT_OR      ]  =  _x64_inst_bit_or_handler,

	[OP_3AC_TEQ     ]  =  _x64_inst_teq_handler,
	[OP_3AC_CMP     ]  =  _x64_inst_cmp_handler,

	[OP_3AC_SETZ    ]  =  _x64_inst_setz_handler,
	[OP_3AC_SETNZ   ]  =  _x64_inst_setnz_handler,
	[OP_3AC_SETGT   ]  =  _x64_inst_setgt_handler,
	[OP_3AC_SETGE   ]  =  _x64_inst_setge_handler,
	[OP_3AC_SETLT   ]  =  _x64_inst_setlt_handler,
	[OP_3AC_SETLE   ]  =  _x64_inst_setle_handler,

	[OP_EQ          ]  =  _x64_inst_eq_handler,
	[OP_NE          ]  =  _x64_inst_ne_handler,
	[OP_GT          ]  =  _x64_inst_gt_handler,
	[OP_GE          ]  =  _x64_inst_ge_handler,
	[OP_LT          ]  =  _x64_inst_lt_handler,
	[OP_LE          ]  =  _x64_inst_le_handler,

	[OP_ASSIGN  	]  =  _x64_inst_assign_handler,

	[OP_ADD_ASSIGN  ]  =  _x64_inst_add_assign_handler,
	[OP_SUB_ASSIGN  ]  =  _x64_inst_sub_assign_handler,

	[OP_MUL_ASSIGN  ]  =  _x64_inst_mul_assign_handler,
	[OP_DIV_ASSIGN  ]  =  _x64_inst_div_assign_handler,
	[OP_MOD_ASSIGN  ]  =  _x64_inst_mod_assign_handler,

	[OP_SHL_ASSIGN  ]  =  _x64_inst_shl_assign_handler,
	[OP_SHR_ASSIGN  ]  =  _x64_inst_shr_assign_handler,

	[OP_AND_ASSIGN  ]  =  _x64_inst_and_assign_handler,
	[OP_OR_ASSIGN   ]  =  _x64_inst_or_assign_handler,

	[OP_VLA_ALLOC   ]  =  _x64_inst_vla_alloc_handler,
	[OP_VLA_FREE    ]  =  _x64_inst_vla_free_handler,

	[OP_RETURN      ]  =  _x64_inst_return_handler,
	[OP_GOTO        ]  =  _x64_inst_goto_handler,

	[OP_3AC_DUMP    ]  =  _x64_inst_dump_handler,

	[OP_3AC_JZ      ]  =  _x64_inst_jz_handler,
	[OP_3AC_JNZ     ]  =  _x64_inst_jnz_handler,
	[OP_3AC_JGT     ]  =  _x64_inst_jgt_handler,
	[OP_3AC_JGE     ]  =  _x64_inst_jge_handler,
	[OP_3AC_JLT     ]  =  _x64_inst_jlt_handler,
	[OP_3AC_JLE     ]  =  _x64_inst_jle_handler,

	[OP_3AC_JA      ]  =  _x64_inst_ja_handler,
	[OP_3AC_JB      ]  =  _x64_inst_jb_handler,
	[OP_3AC_JAE     ]  =  _x64_inst_jae_handler,
	[OP_3AC_JBE     ]  =  _x64_inst_jbe_handler,

	[OP_3AC_NOP     ]  =  _x64_inst_nop_handler,
	[OP_3AC_END     ]  =  _x64_inst_end_handler,

	[OP_3AC_SAVE    ]  =  _x64_inst_save_handler,
	[OP_3AC_LOAD    ]  =  _x64_inst_load_handler,

	[OP_3AC_RESAVE  ]  =  _x64_inst_save_handler,
	[OP_3AC_RELOAD  ]  =  _x64_inst_reload_handler,

	[OP_3AC_INC     ]  =  _x64_inst_inc_handler,
	[OP_3AC_DEC     ]  =  _x64_inst_dec_handler,

	[OP_3AC_PUSH_RETS] =  _x64_inst_push_rets_handler,
	[OP_3AC_POP_RETS]  =  _x64_inst_pop_rets_handler,

	[OP_3AC_MEMSET  ]  =  _x64_inst_memset_handler,

	[OP_3AC_ASSIGN_DEREFERENCE    ]  =  _x64_inst_assign_dereference_handler,
	[OP_3AC_ASSIGN_ARRAY_INDEX    ]  =  _x64_inst_assign_array_index_handler,
	[OP_3AC_ASSIGN_POINTER        ]  =  _x64_inst_assign_pointer_handler,

	[OP_3AC_ADDRESS_OF_ARRAY_INDEX]  =  _x64_inst_address_of_array_index_handler,
	[OP_3AC_ADDRESS_OF_POINTER    ]  =  _x64_inst_address_of_pointer_handler,
};

x64_inst_handler_pt  x64_find_inst_handler(const int op_type)
{
	if (op_type < 0 || op_type >= N_3AC_OPS)
		return NULL;

	return x64_inst_handlers[op_type];
}
