#include "x64.h"
#include "elf.h"
#include "basic_block.h"
#include "3ac.h"


static int _x64_peephole_mov(vector_t* std_insts, instruction_t* inst)
{
	_3ac_code_t*    c  = inst->c;
	basic_block_t* bb = c->basic_block;

	instruction_t* inst2;
	instruction_t* std;
	x64_OpCode_t*  OpCode;

	int j;
	for (j  = std_insts->size - 1; j >= 0; j--) {
		std = std_insts->data[j];
#if 0
		loge("std j: %d\n", j);
		_3ac_code_print(std->c, NULL);
		instruction_print(std);

		loge("inst: \n");
		instruction_print(inst);
		printf("\n");
#endif
		if (X64_LEA == std->OpCode->type) {

			if (inst_data_same(&std->dst, &inst->src)
					&& x64_inst_data_is_reg(&inst->dst)
					&& x64_inst_data_is_local(&std->src)) {

				if (std->src.index)
					inst2 = x64_make_inst_SIB2G((x64_OpCode_t*)std->OpCode,
							inst->dst.base,
							std->src.base, std->src.index, std->src.scale, std->src.disp);
				else
					inst2 = x64_make_inst_P2G((x64_OpCode_t*)std->OpCode, inst->dst.base, std->src.base, std->src.disp);

				if (!inst2)
					return -ENOMEM;

				memcpy(inst->code, inst2->code, inst2->len);
				inst->len = inst2->len;

				inst->OpCode    = std->OpCode;
				inst->src.base  = std->src.base;
				inst->src.index = std->src.index;
				inst->src.scale = std->src.scale;
				inst->src.disp  = std->src.disp;
				inst->src.flag  = std->src.flag;

				free(inst2);
				inst2 = NULL;
			}
			break;
		}

		if (inst_data_same(&std->dst, &inst->dst)) {

			if (inst_data_same(&std->src, &inst->src)) {

				assert(0 == vector_del(inst->c->instructions, inst));

				free(inst);
				inst = NULL;
				return X64_PEEPHOLE_DEL;
			}

			assert(0 == vector_del(std_insts, std));

			if (std->nb_used > 0)
				continue;

			assert(0 == vector_del(std->c->instructions, std));

			free(std);
			std = NULL;
			continue;

		} else if (inst_data_same(&std->src, &inst->src)) {

			if (std->src.flag && std->dst.base->bytes == inst->dst.base->bytes) {

				inst2 = x64_make_inst_E2G((x64_OpCode_t*)inst->OpCode, inst->dst.base, std->dst.base);
				if (!inst2)
					return -ENOMEM;

				memcpy(inst->code, inst2->code, inst2->len);
				inst->len = inst2->len;

				inst->src.base  = std->dst.base;
				inst->src.index = NULL;
				inst->src.scale = 0;
				inst->src.disp  = 0;
				inst->src.flag  = 0;

				free(inst2);
				inst2 = NULL;
			}
			continue;

		} else if (inst_data_same(&std->dst, &inst->src)) {
			std->nb_used++;

			if (inst_data_same(&std->src, &inst->dst)) {

				assert(0 == vector_del(inst->c->instructions, inst));

				free(inst);
				inst = NULL;
				return X64_PEEPHOLE_DEL;
			}

			if (inst->src.flag) {
				assert(std->dst.flag);

				if (std->src.base)
					inst2  = x64_make_inst_E2G((x64_OpCode_t*)inst->OpCode, inst->dst.base, std->src.base);
				else {
					OpCode = x64_find_OpCode(inst->OpCode->type, inst->dst.base->bytes, inst->dst.base->bytes, X64_I2G);

					if (std->src.imm_size < inst->dst.base->bytes)
						std->src.imm = zero_extend(std->src.imm, std->src.imm_size);

					inst2  = x64_make_inst_I2G(OpCode, inst->dst.base, (uint8_t*)&std->src.imm, inst->dst.base->bytes);
				}
				if (!inst2)
					return -ENOMEM;

				memcpy(inst->code, inst2->code, inst2->len);
				inst->len = inst2->len;

				inst->src.base  = std->src.base;
				inst->src.index = NULL;
				inst->src.scale = 0;
				inst->src.disp  = 0;
				inst->src.flag  = 0;

				free(inst2);
				inst2 = NULL;
				continue;

			} else if (inst->dst.flag) {

				if (x64_inst_data_is_reg(&inst->src)
						&& x64_inst_data_is_reg(&std->src)
						&& inst->src.base != std->src.base) {

					if (!inst->dst.index)
						inst2 = x64_make_inst_G2P((x64_OpCode_t*)inst->OpCode,
								inst->dst.base, inst->dst.disp,
								std->src.base);
					else
						inst2 = x64_make_inst_G2SIB((x64_OpCode_t*)inst->OpCode,
								inst->dst.base,
								inst->dst.index, inst->dst.scale, inst->dst.disp,
								std->src.base);
					if (!inst2)
						return -ENOMEM;

					memcpy(inst->code, inst2->code, inst2->len);
					inst->len = inst2->len;

					inst->src.base  = std->src.base;
					inst->src.index = NULL;
					inst->src.scale = 0;
					inst->src.disp  = 0;
					inst->src.flag  = 0;

					free(inst2);
					inst2 = NULL;
					continue;
				}
			}
		} else if (inst_data_same(&std->src, &inst->dst)) {

			assert(0 == vector_del(std_insts, std));

		} else if (x64_inst_data_is_reg(&std->src)) {

			register_t* r0;
			register_t* r1;

			if (x64_inst_data_is_reg(&inst->dst)) {

				r0 = std ->src.base;
				r1 = inst->dst.base;

				if (X64_COLOR_CONFLICT(r0->color, r1->color))
					assert(0 == vector_del(std_insts, std));
			}

		} else if (x64_inst_data_is_reg(&std->dst)) {

			if (inst->src.base == std->dst.base
					|| inst->src.index == std->dst.base
					|| inst->dst.base  == std->dst.base
					|| inst->dst.index == std->dst.base)
				std->nb_used++;
		}
	}

	assert(0 == vector_add_unique(std_insts, inst));
	return 0;
}

static int _x64_peephole_cmp(vector_t* std_insts, instruction_t* inst)
{
	_3ac_code_t*    c  = inst->c;
	basic_block_t* bb = c->basic_block;

	instruction_t* inst2;
	instruction_t* std;

	int j;
	for (j  = std_insts->size - 1; j >= 0; j--) {
		std = std_insts->data[j];

		if (X64_LEA == std->OpCode->type)
			break;

		if (inst->src.flag) {

			if (inst_data_same(&inst->src, &std->src))

				inst->src.base = std->dst.base;

			else if (inst_data_same(&inst->src, &std->dst))

				inst->src.base = std->src.base;
			else
				goto check;

			inst2 = x64_make_inst_E2G((x64_OpCode_t*) inst->OpCode, inst->dst.base, inst->src.base);
			if (!inst2)
				return -ENOMEM;

			inst->src.index = NULL;
			inst->src.scale = 0;
			inst->src.disp  = 0;
			inst->src.flag  = 0;

		} else if (inst->dst.flag) {

			if (inst_data_same(&inst->dst, &std->src))

				inst->dst.base  = std->dst.base;

			else if (inst_data_same(&inst->dst, &std->dst))

				inst->dst.base  = std->src.base;
			else
				goto check;

			if (inst->src.imm_size > 0)
				inst2 = x64_make_inst_I2E((x64_OpCode_t*)inst->OpCode,
						inst->dst.base,
						(uint8_t*)&inst->src.imm, inst->src.imm_size);
			else
				inst2 = x64_make_inst_G2E((x64_OpCode_t*)inst->OpCode,
						inst->dst.base,
						inst->src.base);
			if (!inst2)
				return -ENOMEM;

			inst->dst.index = NULL;
			inst->dst.scale = 0;
			inst->dst.disp  = 0;
			inst->dst.flag  = 0;
		} else
			goto check;

		memcpy(inst->code, inst2->code, inst2->len);
		inst->len = inst2->len;

		free(inst2);
		inst2 = NULL;

check:
		if (x64_inst_data_is_reg(&std->dst)) {

			if (inst->src.base == std->dst.base
					|| inst->src.index == std->dst.base
					|| inst->dst.index == std->dst.base
					|| inst->dst.base  == std->dst.base)
				std->nb_used++;
		}

		if (inst_data_same(&inst->src, &std->dst)
		 || inst_data_same(&inst->dst, &std->dst))
			std->nb_used++;
	}

	return 0;
}

static int _x64_peephole_movx(vector_t* std_insts, instruction_t* inst)
{
	if (!x64_inst_data_is_reg(&inst->src) || !x64_inst_data_is_reg(&inst->dst)) {
		vector_clear(std_insts, NULL);
		return 0;
	}

	_3ac_code_t*    c  = inst->c;
	basic_block_t* bb = c->basic_block;
	instruction_t* std;
	x64_OpCode_t*  OpCode;
	int j;

	for (j  = std_insts->size - 1; j >= 0; j--) {
		std = std_insts->data[j];

		if (inst_data_same(&std->dst, &inst->src)) {
			std->nb_used++;

			if (std->OpCode == inst->OpCode
					&& inst_data_same(&std->src, &inst->src)
					&& inst_data_same(&std->dst, &inst->dst)) {

				assert(0 == vector_del(inst->c->instructions, inst));

				free(inst);
				inst = NULL;
				return X64_PEEPHOLE_DEL;
			}
		}
	}

	assert(0 == vector_add_unique(std_insts, inst));
	return 0;
}

static int x64_inst_is_useful(instruction_t* inst, instruction_t* std)
{
	if (inst_data_same(&inst->dst, &std->src))
		return 1;

	if (x64_inst_data_is_reg(&inst->dst)) {

		register_t* r0 = inst->dst.base;
		register_t* r1 = std->src.base;

		if (X64_CALL == std->OpCode->type) {

			if (X64_COLOR_CONFLICT(r0->color,        x64_find_register("rdi")->color)
					|| X64_COLOR_CONFLICT(r0->color, x64_find_register("rsi")->color)
					|| X64_COLOR_CONFLICT(r0->color, x64_find_register("rdx")->color)
					|| X64_COLOR_CONFLICT(r0->color, x64_find_register("rcx")->color)
					|| X64_COLOR_CONFLICT(r0->color, x64_find_register("r8")->color)
					|| X64_COLOR_CONFLICT(r0->color, x64_find_register("r9")->color))
				return 1;

		} else {
			if (x64_inst_data_is_reg(&std->src)) {
				if (X64_COLOR_CONFLICT(r0->color, r1->color))
					return 1;
			}

			if (std->src.base  == inst->dst.base
					|| std->src.index == inst->dst.base
					|| std->dst.index == inst->dst.base
					|| std->dst.base  == inst->dst.base)
				return 1;
		}

	} else if (x64_inst_data_is_local(&inst->dst)) {

		if (inst_data_same(&inst->dst, &std->src))
			return 1;

		if (x64_find_register("rsp") == inst->dst.base)
			return 1;

		if (OP_VA_START == inst->c->op->type
				|| OP_VA_ARG == inst->c->op->type
				|| OP_VA_END == inst->c->op->type)
			return 1;

		if (x64_inst_data_is_pointer(&std->dst) || x64_inst_data_is_pointer(&std->src))
			return 1;

		switch (std->OpCode->type)
		{
			case X64_CMP:
			case X64_TEST:
				if (inst_data_same(&inst->dst, &std->dst))
					return 1;
				break;

			case X64_MOV:
			case X64_LEA:
				if (std->src.base == inst->dst.base) // maybe array member
					return 1;
				break;
			default:
				break;
		};
	}
	return 0;
}

static int x64_inst_useful_3ac(instruction_t* inst, _3ac_code_t* c)
{
	instruction_t* inst2;
	int j = 0;

	if (inst->c == c) {
		for ( ; j < c->instructions->size; j++) {
			inst2 = c->instructions->data[j];

			if (inst2 == inst)
				break;
		}

		assert(j < c->instructions->size);
		++j;
	}

	if (c->instructions) {
		for ( ; j < c->instructions->size; j++) {
			inst2 = c->instructions->data[j];

			if (x64_inst_is_useful(inst, inst2))
				return 1;
		}
	}
	return 0;
}

static int x64_inst_useful_bb(instruction_t* inst, basic_block_t* bb)
{
	_3ac_code_t* c;
	list_t*     l;

	if (bb == inst->c->basic_block)
		l  = &inst->c->list;
	else
		l = list_head(&bb->code_list_head);

	for ( ; l != list_sentinel(&bb->code_list_head); l = list_next(l)) {
		c      = list_data(l, _3ac_code_t, list);

		if (x64_inst_useful_3ac(inst, c))
			return 1;
	}

	return 0;
}

static int __x64_inst_useful_bb_next(basic_block_t* bb, void* data, vector_t* queue)
{
	instruction_t* inst = data;
	basic_block_t* bb2;
	int j;

	if (x64_inst_useful_bb(inst, bb))
		return 1;

	for (j = 0; j < bb->nexts->size; j++) {
		bb2       = bb->nexts->data[j];

		int ret = vector_add(queue, bb2);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _x64_peephole_function(vector_t* tmp_insts, function_t* f)
{
	instruction_t*  inst;
	basic_block_t*  bb;
	_3ac_code_t*     c;
	int i;

	for (i   = tmp_insts->size - 1; i >= 0; i--) {
		inst = tmp_insts->data[i];

		if (X64_MOV != inst->OpCode->type)
			continue;

		if (x64_inst_data_is_reg(&inst->dst)) {

			if (x64_reg_is_retval(inst->dst.base))
				continue;

		} else if (!x64_inst_data_is_local(&inst->dst))
			continue;

		c  = inst->c;
		bb = c->basic_block;

		int ret = basic_block_search_bfs(bb, __x64_inst_useful_bb_next, inst);
		if (ret < 0)
			return ret;

		if (ret > 0)
			continue;

		assert(0 == vector_del(c->instructions,  inst));
		assert(0 == vector_del(tmp_insts,        inst));

//		logd("del: \n");
//		instruction_print(inst);

		free(inst);
		inst = NULL;
	}

	int n_locals = 0;

	for (i = 0; i < tmp_insts->size; i++) {
		inst      = tmp_insts->data[i];

		if (x64_inst_data_is_local(&inst->src) || x64_inst_data_is_local(&inst->dst))
			n_locals++;
	}

	if (n_locals > 0)
		f->bp_used_flag = 1;
	else
		f->bp_used_flag = 0;

	logw("%s(), f->bp_used_flag: %d\n", f->node.w->text->data, f->bp_used_flag);
	return 0;
}

int x64_optimize_peephole(native_t* ctx, function_t* f)
{
	instruction_t*  std;
	instruction_t*  inst;
	basic_block_t*  bb;
	_3ac_operand_t*  dst;
	_3ac_code_t*     c;
	list_t*         l;
	list_t*         l2;

	vector_t*       std_insts;
	vector_t*       tmp_insts; // instructions for register or local variable

	std_insts = vector_alloc();
	if (!std_insts)
		return -ENOMEM;

	tmp_insts = vector_alloc();
	if (!tmp_insts) {
		vector_free(tmp_insts);
		return -ENOMEM;
	}

	int jmp_back_flag = 0;

	int ret = 0;
	int i;
	int j;

	for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
		l  = list_next(l)) {

		bb = list_data(l, basic_block_t, list);

		if (bb->jmp_flag) {
			vector_clear(std_insts, NULL);

			l2 = list_head(&bb->code_list_head);
			c  = list_data(l2, _3ac_code_t, list);

			assert(1 == c->dsts->size);
			dst = c->dsts->data[0];

			if (dst->bb->index < bb->index)
				jmp_back_flag = 1;
			continue;
		}

		if (bb->jmp_dst_flag) {
			vector_clear(std_insts, NULL);
		}

		for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);
			l2  = list_next(l2)) {

			c   = list_data(l2, _3ac_code_t, list);

			if (!c->instructions)
				continue;

			for (i = 0; i < c->instructions->size; ) {
				inst      = c->instructions->data[i];

				assert(inst->OpCode);

				inst->c = c;
//				instruction_print(inst);

				ret = 0;
				switch (inst->OpCode->type) {

					case X64_CMP:
					case X64_TEST:
						ret = _x64_peephole_cmp(std_insts, inst);
						break;

					case X64_MOV:
						ret = _x64_peephole_mov(std_insts, inst);
						break;

					case X64_LEA:
						ret = vector_add_unique(std_insts, inst);
						break;

					case X64_MOVSS:
					case X64_MOVSD:
						break;
					default:
						vector_clear(std_insts, NULL);
						break;
				};

				if (ret < 0)
					goto error;

				if (X64_PEEPHOLE_DEL == ret)
					continue;

				ret = vector_add(tmp_insts, inst);
				if (ret < 0)
					goto error;
				i++;
			}
		}
	}

	ret = _x64_peephole_function(tmp_insts, f);
error:
	vector_free(tmp_insts);
	vector_free(std_insts);
	return ret;
}



















