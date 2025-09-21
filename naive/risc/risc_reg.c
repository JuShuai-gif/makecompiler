#include"risc.h"

int risc_save_var2(dag_node_t* dn, register_t* r, _3ac_code_t* c, function_t* f)
{
	variable_t*     v    = dn->var;
	rela_t*         rela = NULL;
	risc_OpCode_t*   mov;
	instruction_t*  inst;

	int size     = f->rops->variable_size (v);
	int is_float =      variable_float(v);

	loge("size: %d, r: %s, r->bytes: %d, is_float: %d\n", size, r->name, r->bytes, is_float);
	if (v->w)
		logw("save var: v_%d_%d/%s, ", v->w->line, v->w->pos, v->w->text->data);
	else
		logw("save var: v_%#lx, ", 0xffff & (uintptr_t)v);
	assert(size == r->bytes);

	if (variable_const(v)) {
		logw("const literal var: v_%s_%d_%d not save\n", v->w->text->data, v->w->line, v->w->pos);
		goto end;
	}

	// if temp var in register, alloc it in stack
	if (0 == v->bp_offset && !v->global_flag && !v->local_flag) {

		int tmp  = f->local_vars_size;
		tmp     += size;

		if (tmp & 0x7)
			tmp = (tmp + 7) >> 3 << 3;

		v->bp_offset  = -tmp;
		v->tmp_flag   = 1;

		f->local_vars_size = tmp;
	}

#if 1
	if (v->w)
		logw("save var: v_%d_%d/%s, ", v->w->line, v->w->pos, v->w->text->data);
	else
		logw("save var: v_%#lx, ", 0xffff & (uintptr_t)v);
	printf("size: %d, bp_offset: %d, r: %s\n", size, v->bp_offset, r->name);
#endif

	int ret = f->iops->G2M(c, f, r, NULL, v);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

end:
	// if this var is function argment, it become a normal local var
	v->arg_flag =  0;
	dn->color   = -1;
	dn->loaded  =  0;

	vector_del(r->dag_nodes, dn);
	return 0;
}

int risc_save_var(dag_node_t* dn, _3ac_code_t* c, function_t* f)
{
	if (dn->color <= 0) {
		loge("\n");
		return -EINVAL;
	}

	register_t* r = f->rops->find_register_color(dn->color);

	return risc_save_var2(dn, r, c, f);
}

int risc_save_reg(register_t* r, _3ac_code_t* c, function_t* f)
{
	int i = 0;
	while (i < r->dag_nodes->size) {

		dag_node_t* dn = r->dag_nodes->data[i];

		int ret = risc_save_var(dn, c, f);
		if (ret < 0) {
			loge("i: %d, size: %d, r: %s, dn->var: %s\n", i, r->dag_nodes->size, r->name, dn->var->w->text->data);
			return ret;
		}
	}

	return 0;
}

int risc_load_const(register_t* r, dag_node_t* dn, _3ac_code_t* c, function_t* f)
{
	instruction_t* inst;
	variable_t*    v;

	v = dn->var;
	r->used = 1;

	int size     = f->rops->variable_size (v);
	int is_float =      variable_float(v);

	if (FUNCTION_PTR == v->type) {

		if (v->func_ptr) {
			assert(v->const_literal_flag);

			v->global_flag = 1;
			v->local_flag  = 0;
			v->tmp_flag    = 0;

			return f->iops->M2G(c, f, r, NULL, v);

		} else {
			loge("\n");
			return -EINVAL;
		}

	} else if (variable_const_string(v)) {

		return f->iops->ISTR2G(c, f, r, v);

	} else if (v->nb_dimentions > 0) {
		assert(v->const_literal_flag);

		return f->iops->ADR2G(c, f, r, v);
	}

	return f->iops->I2G(c, r, v->data.u64, size);
}

int risc_load_reg(register_t* r, dag_node_t* dn, _3ac_code_t* c, function_t* f)
{
	if (dn->loaded)
		return 0;

	risc_OpCode_t* mov;
	instruction_t*  inst;
	rela_t*         rela = NULL;

	int is_float =      variable_float(dn->var);
	int var_size = f->rops->variable_size (dn->var);

	r->used = 1;

	if (!is_float) {

		if (variable_const(dn->var)) {

			int ret = risc_load_const(r, dn, c, f);
			if (ret < 0)
				return ret;

			dn->loaded = 1;
			return 0;
		}

		if (!dn->var->global_flag && !dn->var->local_flag && !dn->var->tmp_flag)
			return 0;

		if (variable_const_string(dn->var)) {

			int ret = f->iops->ISTR2G(c, f, r, dn->var);
			if (ret < 0)
				return ret;

			dn->loaded = 1;
			return 0;
		}

		if (0 == dn->var->bp_offset && !dn->var->global_flag) {
			loge("\n");
			return -EINVAL;
		}

		if ((dn->var->nb_dimentions > 0 && dn->var->const_literal_flag)
				|| (dn->var->type >= STRUCT && 0 == dn->var->nb_pointers)) {

			int ret = f->iops->ADR2G(c, f, r, dn->var);
			if (ret < 0)
				return ret;

			dn->loaded = 1;
			return 0;
		}

		int ret = f->iops->M2G(c, f, r, NULL, dn->var);
		if (ret < 0)
			return ret;

		dn->loaded = 1;
		return 0;
	}

	if (!dn->var->global_flag && !dn->var->local_flag && !dn->var->tmp_flag)
		return 0;

	if (0 == dn->var->bp_offset && !dn->var->global_flag) {
		loge("\n");
		return -EINVAL;
	}

	int ret = f->iops->M2GF(c, f, r, NULL, dn->var);
	if (ret < 0)
		return ret;

	dn->loaded = 1;
	return 0;
}

int risc_select_reg(register_t** preg, dag_node_t* dn, _3ac_code_t* c, function_t* f, int load_flag)
{
	if (0 == dn->color)
		return -EINVAL;

	register_t* r;

	int ret;
	int is_float = variable_float(dn->var);
	int var_size = f->rops->variable_size(dn->var);

	if (dn->color > 0) {
		r   = f->rops->find_register_color(dn->color);
#if 1
		ret = f->rops->overflow_reg3(r, dn, c, f);
		if (ret < 0) {
			loge("\n");
			return -1;
		}
#endif
	} else {
		r   = f->rops->select_overflowed_reg(dn, c, is_float);
		if (!r) {
			loge("\n");
			return -1;
		}

		ret = f->rops->overflow_reg(r, c, f);
		if (ret < 0) {
			loge("overflow reg failed\n");
			return ret;
		}
		assert(0 == r->dag_nodes->size);

		r = f->rops->find_register_type_id_bytes(is_float, r->id, var_size);
		assert(0 == r->dag_nodes->size);

		dn->color = r->color;
	}

	ret = vector_add_unique(r->dag_nodes, dn);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (load_flag) {
		ret = risc_load_reg(r, dn, c, f);
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

int risc_select_free_reg(register_t** preg, _3ac_code_t* c, function_t* f, int is_float)
{
	register_t* r;

	r   = f->rops->select_overflowed_reg(NULL, c, is_float);
	if (!r) {
		loge("\n");
		return -1;
	}

	int ret = f->rops->overflow_reg(r, c, f);
	if (ret < 0) {
		loge("overflow reg failed\n");
		return ret;
	}
	assert(0 == r->dag_nodes->size);

	r = f->rops->find_register_type_id_bytes(0, r->id, f->rops->MAX_BYTES);

	assert(0 == r->dag_nodes->size);

	ret = risc_rcg_make(c, c->rcg, NULL, r);
	if (ret < 0)
		return ret;

	r->used = 1;

	*preg = r;
	return 0;
}

int risc_dereference_reg(sib_t* sib, dag_node_t* base, dag_node_t* member, _3ac_code_t* c, function_t* f)
{
	register_t* rb = NULL;
	variable_t*     vb = base->var;

	logw("base->color: %ld\n", base->color);

	int ret  = risc_select_reg(&rb, base, c, f, 1);
	if (ret < 0) {
		loge("\n");
		return ret;
	}
	logw("base->color: %ld\n", base->color);

	if (vb->nb_pointers + vb->nb_dimentions > 1 || vb->type >= STRUCT)
		sib->size = 8;
	else {
		sib->size = vb->data_size;
		assert(8 >= vb->data_size);
	}

	sib->base  = rb;
	sib->index = NULL;
	sib->scale = 0;
	sib->disp  = 0;
	return 0;
}

int risc_pointer_reg(sib_t* sib, dag_node_t* base, dag_node_t* member, _3ac_code_t* c, function_t* f)
{
	variable_t*       vb = base  ->var;
	variable_t*       vm = member->var;
	register_t* rb = NULL;
	instruction_t*    inst;

	int     ret;
	int32_t disp = 0;

	if (vb->nb_pointers > 0 && 0 == vb->nb_dimentions) {

		ret = risc_select_reg(&rb, base, c, f, 1);
		if (ret < 0)
			return ret;

	} else if (vb->local_flag) {

		rb   = f->rops->find_register("fp");
		disp = vb->bp_offset;

	} else if (vb->global_flag) {

		ret  = risc_select_reg(&rb, base, c, f, 0);
		if (ret < 0)
			return ret;

		ret = f->iops->ADR2G(c, f, rb, vb);
		if (ret < 0)
			return ret;

	} else {
		ret = risc_select_reg(&rb, base, c, f, 0);
		if (ret < 0)
			return ret;
	}

	disp += vm->offset;

	sib->base  = rb;
	sib->index = NULL;
	sib->scale = 0;
	sib->disp  = disp;
	sib->size  = f->rops->variable_size(vm);
	return 0;
}

int risc_array_index_reg(sib_t* sib, dag_node_t* base, dag_node_t* index, dag_node_t* scale, _3ac_code_t* c, function_t* f)
{
	variable_t*     vb   = base ->var;
	variable_t*     vi   = index->var;

	register_t* rb = NULL;
	register_t* ri = NULL;
	register_t* rs = NULL;
	register_t* rd = NULL;

	risc_OpCode_t*   xor;
	risc_OpCode_t*   add;
	risc_OpCode_t*   shl;
	risc_OpCode_t*   lea;
	risc_OpCode_t*   mov;
	instruction_t*  inst;

	int ret;
	int i;

	uint32_t opcode;
	int32_t  disp = 0;

	if (vb->nb_pointers > 0 && 0 == vb->nb_dimentions) {

		ret = risc_select_reg(&rb, base, c, f, 1);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	} else if (vb->local_flag) {

		rb   = f->rops->find_register("fp");
		disp = vb->bp_offset;

	} else if (vb->global_flag) {
		rela_t* rela = NULL;

		if (0 == base->color)
			base->color = -1;

		ret  = risc_select_reg(&rb, base, c, f, 0);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		ret = f->iops->ADR2G(c, f, rb, vb);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

	} else {
		ret = risc_select_reg(&rb, base, c, f, 0);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	int32_t s = scale->var->data.i;
	assert(s > 0);

	if (scale->var->size > f->rops->MAX_BYTES)
		scale->var->size = f->rops->MAX_BYTES;

	if (vb->nb_pointers + vb->nb_dimentions > 1 || vb->type >= STRUCT)
		sib->size = f->rops->MAX_BYTES;
	else {
		sib->size = vb->data_size;
		assert(8 >= vb->data_size);
	}

	if (0 == index->color) {
		disp += vi->data.i * s;

		sib->base  = rb;
		sib->index = NULL;
		sib->scale = 0;
		sib->disp  = disp;
		return 0;
	}

	ret = risc_select_reg(&ri, index, c, f, 1);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	register_t* ri2 = f->rops->find_register_color_bytes(ri->color, f->rops->MAX_BYTES);

	if (ri->bytes < ri2->bytes) {

		if (variable_signed(index->var)) {

			inst   = f->iops->MOVSX(c, ri, ri, ri->bytes);
			RISC_INST_ADD_CHECK(c->instructions, inst);
		}

		ri = ri2;
	}

	if (1 != s
			&& 2 != s
			&& 4 != s
			&& 8 != s) {

		ret = risc_select_reg(&rs, scale, c, f, 0);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		ret = f->iops->I2G(c, rs, s, sizeof(s));
		if (ret < 0)
			return ret;

		inst   = f->iops->MUL(c, rs, rs, ri);
		RISC_INST_ADD_CHECK(c->instructions, inst);

		ri = rs;
		s  = 1;
	}

	if (disp != 0) {

		if (!rs) {
			ret = risc_select_reg(&rs, scale, c, f, 0);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			if (disp > 0 && disp <= 0xfff)
				inst = f->iops->ADD_IMM(c, f, rs, rb, disp);

			else if (disp < 0 && -disp <= 0xfff)
				inst = f->iops->SUB_IMM(c, f, rs, rb, -disp);

			else {
				ret = f->iops->I2G(c, rs, disp, 4);
				if (ret < 0)
					return ret;

				inst = f->iops->ADD_G(c, rs, rs, rb);
			}

			RISC_INST_ADD_CHECK(c->instructions, inst);

		} else {
			assert(1 == s);

			if (disp > 0 && disp <= 0xfff)
				inst = f->iops->ADD_IMM(c, f, rs, rb, disp);

			else if (disp < 0 && -disp <= 0xfff)
				inst = f->iops->SUB_IMM(c, f, rs, rb, -disp);

			else {
				ret = risc_select_free_reg(&rd, c, f, 0);
				if (ret < 0)
					return ret;

				ret = f->iops->I2G(c, rd, disp, 4);
				if (ret < 0)
					return ret;

				inst = f->iops->ADD_G(c, rs, rb, rd);
			}

			RISC_INST_ADD_CHECK(c->instructions, inst);
			ri = NULL;
		}

		rb = rs;
	}

	sib->base  = rb;
	sib->index = ri;
	sib->scale = s;
	sib->disp  = 0;
	return 0;
}


