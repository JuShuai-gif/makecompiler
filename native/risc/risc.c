#include"risc.h"
#include"ghr_elf.h"
#include"basic_block.h"
#include"3ac.h"

extern regs_ops_t    regs_ops_arm64;
extern regs_ops_t    regs_ops_arm32;
extern regs_ops_t    regs_ops_naja;

extern inst_ops_t    inst_ops_arm64;
extern inst_ops_t    inst_ops_arm32;
extern inst_ops_t    inst_ops_naja;

static inst_ops_t*   inst_ops_array[] =
{
	&inst_ops_arm64,
	&inst_ops_arm32,
	&inst_ops_naja,

	NULL
};

static regs_ops_t*   regs_ops_array[] =
{
	&regs_ops_arm64,
	&regs_ops_arm32,
	&regs_ops_naja,

	NULL
};

int	risc_open(native_t* ctx, const char* arch)
{
	inst_ops_t* iops = NULL;
	regs_ops_t* rops = NULL;

	int i;
	for (i = 0; inst_ops_array[i]; i++) {

		if (!strcmp(inst_ops_array[i]->name, arch)) {
			iops =  inst_ops_array[i];
			break;
		}
	}

	for (i = 0; regs_ops_array[i]; i++) {

		if (!strcmp(regs_ops_array[i]->name, arch)) {
			rops =  regs_ops_array[i];
			break;
		}
	}

	if (!iops || !rops)
		return -EINVAL;

	risc_context_t* risc = calloc(1, sizeof(risc_context_t));
	if (!risc)
		return -ENOMEM;

	ctx->iops = iops;
	ctx->rops = rops;
	ctx->priv = risc;
	return 0;
}

int risc_close(native_t* ctx)
{
	risc_context_t* risc = ctx->priv;

	if (risc) {
		ctx->rops->registers_clear();

		free(risc);
		risc = NULL;
	}
	return 0;
}

static int _risc_function_init(function_t* f, vector_t* local_vars)
{
	variable_t* v;

	int ret = f->rops->registers_init();
	if (ret < 0)
		return ret;

	int i;
	for (i = 0; i < local_vars->size; i++) {
		v  =        local_vars->data[i];

		v->bp_offset = 0;
	}

	f->rops->argv_rabi(f);

	int local_vars_size = 8 + f->rops->ABI_NB * 8 * 2;

	for (i = 0; i < local_vars->size; i++) {
		v  =        local_vars->data[i];

		if (v->arg_flag) {
			if (v->bp_offset != 0)
				continue;
		}

		int size = variable_size(v);
		if (size < 0)
			return size;

		local_vars_size	+= size;

		if (local_vars_size & 0x7)
			local_vars_size = (local_vars_size + 7) >> 3 << 3;

		v->bp_offset	 = -local_vars_size;
		v->local_flag	 = 1;
	}

	return local_vars_size;
}

static int _risc_save_rabi(function_t* f)
{
	register_t* rbp;
	instruction_t*  inst;
	risc_OpCode_t*   mov;

	register_t* rdi;
	register_t* rsi;
	register_t* rdx;
	register_t* rcx;
	register_t* r8;
	register_t* r9;

	register_t* xmm0;
	register_t* xmm1;
	register_t* xmm2;
	register_t* xmm3;
#if 0
	if (f->vargs_flag) {

		inst = NULL;
		mov  = risc_find_OpCode(RISC_MOV, 8,8, RISC_G2E);

		rbp  = f->rops->find_register("rbp");

		rdi  = f->rops->find_register("rdi");
		rsi  = f->rops->find_register("rsi");
		rdx  = f->rops->find_register("rdx");
		rcx  = f->rops->find_register("rcx");
		r8   = f->rops->find_register("r8");
		r9   = f->rops->find_register("r9");

#define RISC_SAVE_RABI(offset, rabi) \
		do { \
			inst = ctx->iops->G2P(mov, rbp, offset, rabi); \
			RISC_INST_ADD_CHECK(f->init_insts, inst); \
			f->init_code_bytes += inst->len; \
		} while (0)

		RISC_SAVE_RABI(-8,  rdi);
		RISC_SAVE_RABI(-16, rsi);
		RISC_SAVE_RABI(-24, rdx);
		RISC_SAVE_RABI(-32, rcx);
		RISC_SAVE_RABI(-40, r8);
		RISC_SAVE_RABI(-48, r9);

		mov  = risc_find_OpCode(RISC_MOVSD, 8,8, RISC_G2E);

		xmm0 = f->rops->find_register("xmm0");
		xmm1 = f->rops->find_register("xmm1");
		xmm2 = f->rops->find_register("xmm2");
		xmm3 = f->rops->find_register("xmm3");

		RISC_SAVE_RABI(-56, xmm0);
		RISC_SAVE_RABI(-64, xmm1);
		RISC_SAVE_RABI(-72, xmm2);
		RISC_SAVE_RABI(-80, xmm3);
	}
#endif
	return 0;
}

static int _risc_function_finish(native_t* ctx, function_t* f)
{
	assert(!f->init_code);

	f->init_code = _3ac_code_alloc();
	if (!f->init_code)
		return -ENOMEM;

	f->init_code->instructions = vector_alloc();

	if (!f->init_code->instructions) {
		_3ac_code_free(f->init_code);
		return -ENOMEM;
	}

	register_t* sp   = f->rops->find_register("sp");
	register_t* fp   = f->rops->find_register("fp");
	register_t* r;
	instruction_t*    inst = NULL;

	uint32_t opcode;

	if (f->bp_used_flag) {

		inst   = ctx->iops->PUSH(NULL, fp);
		RISC_INST_ADD_CHECK(f->init_code->instructions, inst);
		f->init_code_bytes  = inst->len;

		inst   = ctx->iops->MOV_SP(NULL, fp, sp);
		RISC_INST_ADD_CHECK(f->init_code->instructions, inst);
		f->init_code_bytes += inst->len;

		uint32_t local = f->local_vars_size + fp->bytes;

		if (!(local & 0xf))
			local = (local + 0xf) & ~0xf;

		if (local  >  0xfff) {
			local  += 1 << 12;
			local >>= 12;

			if (local > 0xfff) {
				loge("NOT support too big local var size: %u\n", f->local_vars_size);
				return -EINVAL;
			}

			local <<= 12;
		}

		inst   = ctx->iops->SUB_IMM(f->init_code, f, sp, sp, local);
		RISC_INST_ADD_CHECK(f->init_code->instructions, inst);
		f->init_code_bytes += inst->len;

		int ret = _risc_save_rabi(f);
		if (ret < 0)
			return ret;

	} else
		f->init_code_bytes = 0;

	int ret = f->rops->push_callee_regs(f->init_code, f);
	if (ret < 0)
		return ret;

	f->rops->registers_clear();
	return 0;
}

static void _risc_rcg_node_printf(risc_rcg_node_t* rn, function_t* f)
{
	if (rn->dag_node) {
		variable_t* v = rn->dag_node->var;

		if (v->w) {
			logw("v_%d_%d/%s, %p,%p, ",
					v->w->line, v->w->pos, v->w->text->data,
					v, rn->dag_node);

			if (v->bp_offset < 0)
				printf("bp_offset: -%#x, ", -v->bp_offset);
			else
				printf("bp_offset:  %#x, ",  v->bp_offset);

			printf("color: %ld, type: %ld, id: %ld, mask: %ld",
					rn->dag_node->color,
					RISC_COLOR_TYPE(rn->dag_node->color),
					RISC_COLOR_ID(rn->dag_node->color),
					RISC_COLOR_MASK(rn->dag_node->color));

		} else {
			logw("v_%#lx, %p,%p,  color: %ld, type: %ld, id: %ld, mask: %ld",
					(uintptr_t)v & 0xffff, v, rn->dag_node,
					rn->dag_node->color,
					RISC_COLOR_TYPE(rn->dag_node->color),
					RISC_COLOR_ID(rn->dag_node->color),
					RISC_COLOR_MASK(rn->dag_node->color));
		}

		if (rn->dag_node->color > 0) {
			register_t* r = f->rops->find_register_color(rn->dag_node->color);
			printf(", reg: %s\n", r->name);
		} else {
			printf("\n");
		}
	} else if (rn->reg) {
		logw("r/%s, color: %ld, type: %ld, major: %ld, minor: %ld\n",
				rn->reg->name, rn->reg->color,
				RISC_COLOR_TYPE(rn->reg->color),
				RISC_COLOR_ID(rn->reg->color),
				RISC_COLOR_MASK(rn->reg->color));
	}
}

static void _risc_inst_printf(_3ac_code_t* c)
{
	if (!c->instructions)
		return;

	int i;
	for (i = 0; i < c->instructions->size; i++) {
		instruction_t* inst = c->instructions->data[i];
		int j;
		for (j = 0; j < inst->len; j++)
			printf("%02x ", inst->code[j]);
		printf("\n");
	}
	printf("\n");
}

static int _risc_argv_prepare(graph_t* g, basic_block_t* bb, function_t* f)
{
	graph_node_t* gn;
	dag_node_t*   dn;
	variable_t*   v;
	list_t*       l;

	int i;
	for (i = 0; i < f->argv->size; i++) {
		v  =        f->argv->data[i];

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

		int ret = _risc_rcg_make_node(&gn, g, dn, v->rabi);
		if (ret < 0)
			return ret;

		dn->rabi = v->rabi;

		if (dn->rabi)
			dn->loaded =  1;
		else
			dn->color  = -1;
	}

	return 0;
}

static int _risc_argv_save(basic_block_t* bb, function_t* f)
{
	list_t*     l;
	_3ac_code_t* c;

	if (!list_empty(&bb->code_list_head)) {

		l = list_head(&bb->code_list_head);
		c = list_data(l, _3ac_code_t, list);
	} else {
		c = _3ac_code_alloc();
		if (!c)
			return -ENOMEM;

		c->op = _3ac_find_operator(OP_3AC_NOP);
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
		variable_t*    v = f->argv->data[i];

		assert(v->arg_flag);

		dag_node_t*    dn;
		dag_node_t*    dn2;
		dn_status_t*   active;
		register_t*    rabi;

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
			active    = bb->dn_colors_entry->data[j];
			dn2       = active->dag_node;

			if (dn2 != dn && dn2->color > 0 && f->rops->color_conflict(dn2->color, rabi->color)) {
				save_flag = 1;
				break;
			}
		}

		if (save_flag) {
			int ret = risc_save_var2(dn, rabi, c, f);
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

static int _risc_make_bb_rcg(graph_t* g, basic_block_t* bb, native_t* ctx)
{
	list_t*          l;
	_3ac_code_t*      c;
	risc_rcg_handler_pt  h;

	for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_next(l)) {

		c = list_data(l, _3ac_code_t, list);

		h = risc_find_rcg_handler(c->op->type);
		if (!h) {
			loge("_3ac operator '%s' not supported\n", c->op->name);
			return -EINVAL;
		}

		int ret = h(ctx, c, g);
		if (ret < 0)
			return ret;
	}

	logd("g->nodes->size: %d\n", g->nodes->size);
	return 0;
}

static int _risc_bb_regs_from_graph(basic_block_t* bb, graph_t* g, function_t* f)
{
	int i;
	for (i = 0; i < g->nodes->size; i++) {
		graph_node_t*   gn = g->nodes->data[i];
		risc_rcg_node_t*     rn = gn->data;
		dag_node_t*     dn = rn->dag_node;
		dn_status_t*    ds;

		if (!dn) {
			_risc_rcg_node_printf(rn, f);
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

		_risc_rcg_node_printf(rn, f);
	}
	printf("\n");

	return 0;
}

static int _risc_select_bb_regs(basic_block_t* bb, native_t* ctx)
{
	risc_context_t*	risc = ctx->priv;
	function_t*     f   = risc->f;

	graph_t* g = graph_alloc();
	if (!g)
		return -ENOMEM;

	vector_t*   colors = NULL;

	int ret = 0;
	int i;

	if (0 == bb->index) {
		ret = _risc_argv_prepare(g, bb, f);
		if (ret < 0)
			goto error;
	}

	ret = _risc_make_bb_rcg(g, bb, ctx);
	if (ret < 0)
		goto error;

	colors = f->rops->register_colors();
	if (!colors) {
		ret = -ENOMEM;
		goto error;
	}

	ret = risc_graph_kcolor(g, 16, colors, f);
	if (ret < 0)
		goto error;

	ret = _risc_bb_regs_from_graph(bb, g, f);

error:
	if (colors)
		vector_free(colors);

	graph_free(g);
	g = NULL;
	return ret;
}


static int _risc_select_bb_group_regs(bb_group_t* bbg, native_t* ctx)
{
	risc_context_t*	risc = ctx->priv;
	function_t*     f    = risc->f;

	graph_t* g = graph_alloc();
	if (!g)
		return -ENOMEM;

	vector_t*      colors = NULL;
	basic_block_t* bb;

	int ret = 0;
	int i;

	if (0 == bbg->pre->index) {
		ret = _risc_argv_prepare(g, bb, f);
		if (ret < 0)
			goto error;
	}

	for (i = 0; i < bbg->body->size; i++) {
		bb =        bbg->body->data[i];

		ret = _risc_make_bb_rcg(g, bb, ctx);
		if (ret < 0)
			goto error;
	}

	colors = f->rops->register_colors();
	if (!colors) {
		ret = -ENOMEM;
		goto error;
	}

	ret = risc_graph_kcolor(g, 16, colors, f);
	if (ret < 0)
		goto error;

	ret = _risc_bb_regs_from_graph(bbg->pre, g, f);
	if (ret < 0)
		goto error;

error:
	if (colors)
		vector_free(colors);

	graph_free(g);
	g = NULL;
	return ret;
}

static int _risc_make_insts_for_list(native_t* ctx, basic_block_t* bb, int bb_offset)
{
	_3ac_code_t* cmp = NULL;
	_3ac_code_t* c   = NULL;
	list_t*     h   = &bb->code_list_head;
	list_t*     l;

	for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {

		c  = list_data(l, _3ac_code_t, list);

		risc_inst_handler_pt h = risc_find_inst_handler(c->op->type);
		if (!h) {
			loge("_3ac operator '%s' not supported\n", c->op->name);
			return -EINVAL;
		}

		int ret = h(ctx, c);
		if (ret < 0) {
			_3ac_code_print(c, NULL);
			loge("_3ac op '%s' make inst failed\n", c->op->name);
			return ret;
		}

		if (!c->instructions)
			continue;

		_3ac_code_print(c, NULL);
		_risc_inst_printf(c);
	}

	return bb_offset;
}

static void _risc_set_offset_for_jmps(native_t* ctx, function_t* f)
{
	int i;

	for (i = 0; i < f->jmps->size; i++) {
		_3ac_code_t*    c      = f->jmps->data[i];

		assert(c->instructions && 1 == c->instructions->size);

		_3ac_operand_t* dst    = c->dsts->data[0];
		basic_block_t* cur_bb = c->basic_block;
		basic_block_t* dst_bb = dst->bb;

		instruction_t* inst   = c->instructions->data[0];
		basic_block_t* bb     = NULL;
		list_t*        l      = NULL;
		int32_t            bytes  = 0;

		if (cur_bb->index < dst_bb->index) {
			for (l = &cur_bb->list; l != &dst_bb->list; l = list_next(l)) {

				bb     = list_data(l, basic_block_t, list);

				bytes += bb->code_bytes;
			}
		} else {
			for (l = &dst_bb->list; l != &cur_bb->list; l = list_next(l)) {

				bb     = list_data(l, basic_block_t, list);

				bytes -= bb->code_bytes;
			}
		}

		assert(0 == (bytes & 0x3));

		ctx->iops->set_jmp_offset(inst, bytes);
	}
}

static void _risc_set_offset_for_relas(native_t* ctx, function_t* f, vector_t* relas)
{
	int i;
	for (i = 0; i < relas->size; i++) {

		rela_t*        rela   = relas->data[i];
		_3ac_code_t*    c      = rela->code;
		instruction_t* inst   = rela->inst;
		basic_block_t* cur_bb = c->basic_block;

		instruction_t* inst2;
		basic_block_t* bb;
		list_t*        l;

		int bytes = f->init_code_bytes;

		for (l = list_head(&f->basic_block_list_head); l != &cur_bb->list;
				l = list_next(l)) {

			bb     = list_data(l, basic_block_t, list);
			bytes += bb->code_bytes;
		}

		bytes += c->bb_offset;

		int j;
		for (j = 0; j < c->instructions->size; j++) {
			inst2     = c->instructions->data[j];

			if (inst2 == inst)
				break;

			bytes += inst2->len;
		}

		rela->inst_offset += bytes;
	}
}

static int _risc_bbg_fix_saves(bb_group_t* bbg, function_t* f)
{
	basic_block_t* pre;
	basic_block_t* post;
	basic_block_t* bb;
	_3ac_operand_t* src;
	dag_node_t*    dn;
	dag_node_t*    dn2;
	_3ac_code_t*    c;
	_3ac_code_t*    c2;
	list_t*        l;
	list_t*        l2;

	int i;
//	int j;

	pre  = bbg->pre;
	post = bbg->posts->data[0];

	for (l = list_head(&post->save_list_head); l != list_sentinel(&post->save_list_head); ) {
		c  = list_data(l, _3ac_code_t, list);
		l  = list_next(l);

		assert(c->srcs && 1 == c->srcs->size);
		src = c->srcs->data[0];
		dn  = src->dag_node;

		variable_t* v = dn->var;

		intptr_t color   = risc_bb_find_color(pre->dn_colors_exit, dn);
		int      updated = 0;

		for (i = 0; i < bbg->body->size; i++) {
			bb =        bbg->body->data[i];

			intptr_t color2 = risc_bb_find_color(bb->dn_colors_exit, dn);

			if (color2 != color)
				updated++;
		}

		if (0 == updated) {
			if (color <= 0)
				continue;

			int ret = risc_bb_save_dn(color, dn, c, post, f);
			if (ret < 0)
				return ret;

			list_del(&c->list);
			list_add_tail(&post->code_list_head, &c->list);

			for (i = 1; i < bbg->posts->size; i++) {
				bb =        bbg->posts->data[i];

				l2 = list_head(&bb->save_list_head);
				c2  = list_data(l2, _3ac_code_t, list);

				assert(c2->srcs && 1 == c2->srcs->size);
				src = c2->srcs->data[0];
				dn2 = src->dag_node;

				ret = risc_bb_save_dn(color, dn2, c2, bb, f);
				if (ret < 0)
					return ret;

				list_del(&c2->list);
				list_add_tail(&bb->code_list_head, &c2->list);
			}
		} else {
			list_del(&c->list);
			_3ac_code_free(c);
			c = NULL;

			for (i = 1; i < bbg->posts->size; i++) {
				bb =        bbg->posts->data[i];

				l2 = list_head(&bb->save_list_head);
				c2  = list_data(l2, _3ac_code_t, list);

				list_del(&c2->list);

				_3ac_code_free(c2);
				c2 = NULL;
			}
#if 0
			variable_t* v = dn->var;
			loge("save: v_%d_%d/%s, dn->color: %ld\n",
					v->w->line, v->w->pos, v->w->text->data, dn->color);
#endif
			for (i = 0; i < bbg->body->size; i++) {
				bb =        bbg->body->data[i];

				for (l2 = list_head(&bb->save_list_head); l2 != list_sentinel(&bb->save_list_head); l2 = list_next(l2)) {
					c   = list_data(l2, _3ac_code_t, list);

					assert(c->srcs && 1 == c->srcs->size);
					src = c->srcs->data[0];

					if (dn == src->dag_node)
						break;
				}
				if (l2 == list_sentinel(&bb->save_list_head))
					continue;

				intptr_t color = risc_bb_find_color(bb->dn_colors_exit, dn);
				if (color <= 0)
					continue;

				int ret = risc_bb_save_dn(color, dn, c, bb, f);
				if (ret < 0)
					return ret;

				list_del(&c->list);
				list_add_tail(&bb->code_list_head, &c->list);
			}
		}
	}

	return 0;
}

static void _risc_bbg_fix_loads(bb_group_t* bbg)
{
	if (0 == bbg->body->size)
		return;

	basic_block_t* pre;
	basic_block_t* bb;
	_3ac_operand_t* dst;
	dn_status_t*   ds;
	dag_node_t*    dn;
	_3ac_code_t*    c;
	list_t*        l;

	int i;
	int j;

	pre = bbg->pre;
	bb  = bbg->body->data[0];

	for (i = 0; i < pre->dn_colors_exit->size; i++) {
		ds =        pre->dn_colors_exit->data[i];

		dn = ds->dag_node;

		if (ds->color <= 0)
			continue;

		if (ds->color == dn->color)
			continue;
#if 1
		variable_t* v = dn->var;
		if (v->w)
			logw("v_%d_%d/%s, ", v->w->line, v->w->pos, v->w->text->data);
		else
			logw("v_%#lx, ", 0xffff & (uintptr_t)v);
		printf("ds->color: %ld, dn->color: %ld\n", ds->color, dn->color);
#endif
		for (l = list_head(&pre->code_list_head); l != list_sentinel(&pre->code_list_head); ) {
			c  = list_data(l, _3ac_code_t, list);
			l  = list_next(l);

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

static void _risc_set_offsets(function_t* f)
{
	instruction_t* inst;
	basic_block_t* bb;
	_3ac_code_t*    c;
	list_t*        l;
	list_t*        l2;

	int i;

	for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
			l = list_next(l)) {

		bb = list_data(l, basic_block_t, list);

		bb->code_bytes = 0;

		for (l2 = list_head(&bb->code_list_head); l2 != list_sentinel(&bb->code_list_head);
				l2 = list_next(l2)) {

			c = list_data(l2, _3ac_code_t, list);

			c->inst_bytes = 0;
			c->bb_offset  = bb->code_bytes;

			if (!c->instructions)
				continue;

			for (i = 0; i < c->instructions->size; i++) {
				inst      = c->instructions->data[i];

				c->inst_bytes += inst->len;
			}

			bb->code_bytes += c->inst_bytes;
		}
	}
}

int	_risc_select_inst(native_t* ctx)
{
	risc_context_t* risc = ctx->priv;
	function_t*      f     = risc->f;
	basic_block_t*   bb;
	basic_block_t*   end;
	bb_group_t*      bbg;

	int i;
	int ret = 0;
	list_t* l;

	for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
			l = list_next(l)) {

		bb = list_data(l, basic_block_t, list);

		if (bb->group_flag || bb->loop_flag)
			continue;

		if (bb->end_flag) {
			end = bb;
			continue;
		}

		ret = _risc_select_bb_regs(bb, ctx);
		if (ret < 0)
			return ret;

		risc_init_bb_colors(bb, f);

		if (0 == bb->index) {
			ret = _risc_argv_save(bb, f);
			if (ret < 0)
				return ret;
		}
		loge("************ bb: %d, cmp_flag: %d\n", bb->index, bb->cmp_flag);

		ret = _risc_make_insts_for_list(ctx, bb, 0);
		if (ret < 0)
			return ret;
	}

	for (i  = 0; i < f->bb_groups->size; i++) {
		bbg =        f->bb_groups->data[i];

		ret = _risc_select_bb_group_regs(bbg, ctx);
		if (ret < 0)
			return ret;

		risc_init_bb_colors(bbg->pre, f);

		if (0 == bbg->pre->index) {
			ret = _risc_argv_save(bbg->pre, f);
			if (ret < 0)
				return ret;
		}

		int j;
		for (j = 0; j < bbg->body->size; j++) {
			bb =        bbg->body->data[j];

			assert(!bb->native_flag);

			if (0 != j) {
				ret = risc_load_bb_colors2(bb, bbg, f);
				if (ret < 0)
					return ret;
			}

			logd("************ bb: %d, cmp_flag: %d\n", bb->index, bb->cmp_flag);
			ret = _risc_make_insts_for_list(ctx, bb, 0);
			if (ret < 0)
				return ret;
			bb->native_flag = 1;
			logd("************ bb: %d\n", bb->index);

			ret = risc_save_bb_colors(bb->dn_colors_exit, bbg, bb);
			if (ret < 0)
				return ret;
		}
	}

	for (i  = 0; i < f->bb_loops->size; i++) {
		bbg =        f->bb_loops->data[i];

		ret = _risc_select_bb_group_regs(bbg, ctx);
		if (ret < 0)
			return ret;

		risc_init_bb_colors(bbg->pre, f);

		if (0 == bbg->pre->index) {
			ret = _risc_argv_save(bbg->pre, f);
			if (ret < 0)
				return ret;
		}

		ret = _risc_make_insts_for_list(ctx, bbg->pre, 0);
		if (ret < 0)
			return ret;

		ret = risc_save_bb_colors(bbg->pre->dn_colors_exit, bbg, bbg->pre);
		if (ret < 0)
			return ret;

		int j;
		for (j = 0; j < bbg->body->size; j++) {
			bb =        bbg->body->data[j];

			assert(!bb->native_flag);

			ret = risc_load_bb_colors(bb, bbg, f);
			if (ret < 0)
				return ret;

			ret = _risc_make_insts_for_list(ctx, bb, 0);
			if (ret < 0)
				return ret;
			bb->native_flag = 1;

			ret = risc_save_bb_colors(bb->dn_colors_exit, bbg, bb);
			if (ret < 0)
				return ret;
		}

		_risc_bbg_fix_loads(bbg);

		ret = _risc_bbg_fix_saves(bbg, f);
		if (ret < 0)
			return ret;

		for (j = 0; j < bbg->body->size; j++) {
			bb =        bbg->body->data[j];

			ret = risc_fix_bb_colors(bb, bbg, f);
			if (ret < 0)
				return ret;
		}
	}

	ret = _risc_make_insts_for_list(ctx, end, 0);
	if (ret < 0)
		return ret;
#if 0
	if (risc_optimize_peephole(ctx, f) < 0) {
		loge("\n");
		return -1;
	}
#endif
	_risc_set_offsets(f);

	_risc_set_offset_for_jmps( ctx, f);

	return 0;
}

static int _find_local_vars(node_t* node, void* arg, vector_t* results)
{
	block_t* b = (block_t*)node;

	if ((OP_BLOCK == b->node.type || FUNCTION == b->node.type) && b->scope) {

		int i;
		for (i = 0; i < b->scope->vars->size; i++) {

			variable_t* var = b->scope->vars->data[i];

			int ret = vector_add(results, var);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

int risc_select_inst(native_t* ctx, function_t* f)
{
	risc_context_t* risc = ctx->priv;

	risc->f = f;
	f->iops = ctx->iops;
	f->rops = ctx->rops;

	vector_t* local_vars = vector_alloc();
	if (!local_vars)
		return -ENOMEM;

	int ret = node_search_bfs((node_t*)f, NULL, local_vars, -1, _find_local_vars);
	if (ret < 0)
		return ret;

	int local_vars_size = _risc_function_init(f, local_vars);
	if (local_vars_size < 0)
		return -1;

	int i;
	for (i = 0; i < local_vars->size; i++) {
		variable_t* v = local_vars->data[i];
		assert(v->w);
		logi("v: %p, name: %s_%d_%d, size: %d, bp_offset: %d, arg_flag: %d\n",
				v, v->w->text->data, v->w->line, v->w->pos,
				variable_size(v), v->bp_offset, v->arg_flag);
	}

	f->local_vars_size = local_vars_size;
	f->bp_used_flag    = 1;

	ret = _risc_select_inst(ctx);
	if (ret < 0)
		return ret;

	ret = _risc_function_finish(ctx, f);
	if (ret < 0)
		return ret;

	_risc_set_offset_for_relas(ctx, f, f->text_relas);
	_risc_set_offset_for_relas(ctx, f, f->data_relas);

	if (f->iops->set_rel_veneer)
		return f->iops->set_rel_veneer(f);

	return 0;
}

native_ops_t	native_ops_risc =
{
	.name            = "risc",

	.open            = risc_open,
	.close           = risc_close,

	.select_inst     = risc_select_inst,
};
