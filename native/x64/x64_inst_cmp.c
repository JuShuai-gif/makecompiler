#include "x64.h"


static int _inst_cmp(dag_node_t* src0, dag_node_t* src1, _3ac_code_t* c, function_t* f)
{
	if (0 == src0->color) {
		loge("src0 should be a var\n");
		if (src0->var->w)
			loge("src0: '%s'\n", src0->var->w->text->data);
		else
			loge("src0: v_%#lx\n", 0xffff & (uintptr_t)src0->var);
		return -EINVAL;
	}

	x64_OpCode_t*   cmp;
	instruction_t*  inst;
	register_t* rs1;
	register_t* rs0  = NULL;
	rela_t*         rela = NULL;

	X64_SELECT_REG_CHECK(&rs0, src0, c, f, 1);

	int src1_size = x64_variable_size(src1->var);

	if (variable_float(src0->var)) {
		assert(variable_float(src1->var));

		if (VAR_FLOAT == src0->var->type)
			cmp = x64_find_OpCode(X64_UCOMISS, rs0->bytes, src1_size, X64_E2G);
		else
			cmp = x64_find_OpCode(X64_UCOMISD, rs0->bytes, src1_size, X64_E2G);

		if (0 == src1->color) {
			src1->color = -1;
			src1->var->global_flag = 1;

			X64_SELECT_REG_CHECK(&rs1, src1, c, f, 1);
			inst = x64_make_inst_E2G(cmp, rs0, rs1);
			X64_INST_ADD_CHECK(c->instructions, inst);
			return 0;
		}
	} else {
		if (0 == src1->color) {
			cmp  = x64_find_OpCode(X64_CMP, rs0->bytes, src1_size, X64_I2E);

			if (cmp) {
				inst = x64_make_inst_I2E(cmp, rs0, (uint8_t*)&src1->var->data, src1_size);
				X64_INST_ADD_CHECK(c->instructions, inst);
				return 0;
			}

			src1->loaded =  0;
			src1->color  = -1;
			X64_SELECT_REG_CHECK(&rs1, src1, c, f, 1);

			cmp  = x64_find_OpCode(X64_CMP, rs0->bytes, src1_size, X64_G2E);
			inst = x64_make_inst_G2E(cmp, rs0, rs1);
			X64_INST_ADD_CHECK(c->instructions, inst);

			src1->loaded = 0;
			src1->color  = 0;
			assert(0 == vector_del(rs1->dag_nodes, src1));
			return 0;
		}

		cmp = x64_find_OpCode(X64_CMP, rs0->bytes, src1_size, X64_E2G);
	}

	if (src1->color > 0) {
		X64_SELECT_REG_CHECK(&rs1, src1, c, f, 1);
		inst = x64_make_inst_E2G(cmp, rs0, rs1);
		X64_INST_ADD_CHECK(c->instructions, inst);
	} else {
		inst = x64_make_inst_M2G(&rela, cmp, rs0, NULL, src1->var);
		X64_INST_ADD_CHECK(c->instructions, inst);
		X64_RELA_ADD_CHECK(f->data_relas, rela, c, src1->var, NULL);
	}

	return 0;
}

static int _inst_set(int setcc_type, dag_node_t* dst, _3ac_code_t* c, function_t* f)
{
	x64_OpCode_t* setcc = x64_find_OpCode(setcc_type, 1,1, X64_E);
	if (!setcc)
		return -EINVAL;

	x64_OpCode_t*   mov;
	instruction_t*  inst;
	register_t* rd;
	rela_t*         rela = NULL;

	X64_SELECT_REG_CHECK(&rd, dst, c, f, 0);

	if (rd->bytes > 1) {
		uint64_t imm      = 0;

		// can't clear zero with 'xor', because it affects 'eflags'.

		mov = x64_find_OpCode(X64_MOV, rd->bytes, rd->bytes, X64_I2G);
		inst = x64_make_inst_I2G(mov, rd, (uint8_t*)&imm, rd->bytes);
		X64_INST_ADD_CHECK(c->instructions, inst);

		rd = x64_find_register_color_bytes(rd->color, 1);
	}

	inst = x64_make_inst_E(setcc, rd);
	X64_INST_ADD_CHECK(c->instructions, inst);

	return 0;
}

int x64_inst_teq(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;
	_3ac_operand_t*  src = c->srcs->data[0];
	variable_t*     v   = src->dag_node->var;

	x64_OpCode_t*   test;
	instruction_t*  inst;
	register_t*     rs;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (0 == src->dag_node->color) {
		loge("src->dag_node->var: %p\n", src->dag_node->var);
		return -1;
	}

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	X64_SELECT_REG_CHECK(&rs, src->dag_node, c, f, 1);

	test = x64_find_OpCode(X64_TEST, v->size, v->size, X64_G2E);
	inst = x64_make_inst_G2E(test, rs, rs);
	X64_INST_ADD_CHECK(c->instructions, inst);
	return 0;
}

int x64_inst_cmp(native_t* ctx, _3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;

	x64_context_t* x64  = ctx->priv;
	function_t*    f    = x64->f;
	_3ac_operand_t* src0 = c->srcs->data[0];
	_3ac_operand_t* src1 = c->srcs->data[1];

	if (!src0 || !src0->dag_node)
		return -EINVAL;

	if (!src1 || !src1->dag_node)
		return -EINVAL;

	if (src0->dag_node->var->size != src1->dag_node->var->size)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	return _inst_cmp(src0->dag_node, src1->dag_node, c, f);
}

int x64_inst_set(native_t* ctx, _3ac_code_t* c, int setcc_type)
{
	if (!c->dsts || c->dsts->size <= 0)
		return -EINVAL;

	_3ac_operand_t* dst = c->dsts->data[0];

	if (0 == dst->dag_node->color)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	x64_context_t* x64 = ctx->priv;
	function_t*    f   = x64->f;

	int ret = _inst_set(setcc_type, dst->dag_node, c, f);
	if (ret < 0)
		return ret;
	return 0;
}

int x64_inst_cmp_set(native_t* ctx, _3ac_code_t* c, int setcc_type)
{
	int ret = x64_inst_cmp(ctx, c);
	if (ret < 0)
		return ret;

	return x64_inst_set(ctx, c, setcc_type);
}

