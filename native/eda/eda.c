#include"eda.h"
#include"basic_block.h"
#include"3ac.h"

int	eda_open(native_t* ctx, const char* arch)
{
	eda_context_t* eda = calloc(1, sizeof(eda_context_t));
	if (!eda)
		return -ENOMEM;

	ctx->priv = eda;
	return 0;
}

int eda_close(native_t* ctx)
{
	eda_context_t* eda = ctx->priv;

	if (eda) {
		free(eda);
		eda = NULL;
	}
	return 0;
}

static int _eda_make_insts_for_list(native_t* ctx, list_t* h)
{
	3ac_code_t* c;
	list_t*     l;

	for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
		c  = list_data(l, 3ac_code_t, list);

		eda_inst_handler_pt h = eda_find_inst_handler(c->op->type);
		if (!h) {
			loge("3ac operator '%s' not supported\n", c->op->name);
			return -EINVAL;
		}

		int ret = h(ctx, c);
		if (ret < 0) {
			3ac_code_print(c, NULL);
			loge("3ac op '%s' make inst failed\n", c->op->name);
			return ret;
		}

		if (!c->instructions)
			continue;

		3ac_code_print(c, NULL);
	}

	return 0;
}

static int __eda_dfs_mask(function_t* f, ScfEpin* mask, basic_block_t* root)
{
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* po;

	if (root->visit_flag)
		return 0;
	root->visit_flag = 1;

	if (root->mask_pin) {
		int ret = __eda_bit_and(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p0, root->mask_pin);
		EDA_PIN_ADD_PIN_EF(f->ef, p1, mask);
		root->mask_pin = po;
		logi("root->index: %d, mask_pin: c%ldp%ld\n", root->index, root->mask_pin->cid, root->mask_pin->id);
	} else {
		root->mask_pin = mask;
		logi("root->index: %d, mask_pin: c%ldp%ld\n", root->index, root->mask_pin->cid, root->mask_pin->id);
	}

	int i;
	for (i = 0; i < root->nexts->size; i++) {
		int ret = __eda_dfs_mask(f, mask, root->nexts->data[i]);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int __eda_bb_mask(function_t* f, ScfEpin* mask, basic_block_t* root, basic_block_t* cmp)
{
	basic_block_t* bb;
	list_t*        l;
	int i;

	for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head); l = list_next(l)) {
		bb = list_data(l, basic_block_t, list);
		bb->visit_flag = 0;
	}

	for (i = cmp->dominators->size - 1; i >= 0; i--) {
		bb = cmp->dominators->data[i];

		if (bb->dfo < cmp->dfo) {
			bb->visit_flag = 1;
			logw("dom->index: %d, dfo: %d, cmp->index: %d, dfo: %d\n", bb->index, bb->dfo, cmp->index, cmp->dfo);
			break;
		}
	}

	cmp->visit_flag = 1;
	return __eda_dfs_mask(f, mask, root);
}

static int __eda_jmp_mask(function_t* f, 3ac_code_t* c, basic_block_t* cmp)
{
	3ac_operand_t* dst = c->dsts->data[0];
	basic_block_t* bb;
	list_t*        l;

	ScfEpin* __true  = NULL;
	ScfEpin* __false = NULL;
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* p2;
	ScfEpin* po;

	int ret;
	int i;
	switch (c->op->type) {

		case OP_3AC_JZ:
			ret = __eda_bit_not(f, &__false, &__true);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_ZERO], __false);
			break;
		case OP_3AC_JNZ:
			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_ZERO], __true);
			break;

		case OP_3AC_JLT:
			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_SIGN], __true);
			break;
		case OP_3AC_JGE:
			ret = __eda_bit_not(f, &__false, &__true);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_SIGN], __false);
			break;

		case OP_3AC_JGT:
			ret = __eda_bit_not(f, &p1, &p2);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_SIGN], p1);

			ret = __eda_bit_and(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p1, p2);
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_ZERO], p0);

			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, po, __true);
			break;

		case OP_3AC_JLE:
			ret = __eda_bit_not(f, &p0, &p2);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_ZERO], p0);

			ret = __eda_bit_or(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p0, p2);
			EDA_PIN_ADD_CONN(f->ef, cmp->flag_pins[EDA_FLAG_SIGN], p1);

			ret = __eda_bit_not(f, &__true, &__false);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, po, __true);
			break;

		case OP_GOTO:
			logi("'%s'\n", c->op->name);
			return 0;
			break;
		default:
			loge("'%s' not support\n", c->op->name);
			return -EINVAL;
			break;
	};

	if (__true)
		__true->flags |= EDA_PIN_CF;

	for (i = 0; i < cmp->nexts->size; i++) {
		bb =        cmp->nexts->data[i];

		if (bb == dst->bb)
			ret = __eda_bb_mask(f, __true, bb, cmp);
		else
			ret = __eda_bb_mask(f, __false, bb, cmp);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int _eda_fix_jmps(native_t* ctx, function_t* f)
{
	basic_block_t* cur_bb;
	basic_block_t* bb;
	3ac_code_t*    c;
	list_t*        l;

	int i;
	for (i = 0; i < f->jmps->size; i++) {
		c  =        f->jmps->data[i];

		cur_bb = c->basic_block;

		for (l = list_prev(&cur_bb->list); l != list_sentinel(&f->basic_block_list_head); l = list_prev(l)) {
			bb = list_data(l, basic_block_t, list);

			if (!bb->jmp_flag)
				break;
		}

		if (l == list_sentinel(&f->basic_block_list_head))
			continue;

		int ret = __eda_jmp_mask(f, c, bb);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static void _eda_peep_hole_2not(ScfEfunction* f, ScfEpin* p0, ScfEpin* p3)
{
	ScfEcomponent* c;
	ScfEline*      el = f->elines[p3->lid];
	ScfEpin*       p;

	assert(0 == eline__del_pin(el, p3->cid, p3->id));

	long i;
	for (i = 0; i + 1 < el->n_pins; i += 2) {

		c  = f->components[el->pins[i]];
		p  = c->pins      [el->pins[i + 1]];

		epin__del_component(p, p3->cid, p3->id);

		p->flags |= p0->flags;

		if (p0->io_lid >= 0)
			p->io_lid = p0->io_lid;
	}
}

static int _eda_peep_hole(ScfEfunction* f)
{
	int ret = pins_same_line(f);
	if (ret < 0)
		return ret;

	ScfEcomponent* c0;
	ScfEcomponent* c1;
	ScfEline*      el0;
	ScfEline*      el1;
	ScfEpin*       p0;
	ScfEpin*       p1;
	ScfEpin*       p2;
	ScfEpin*       p3;

	long i;
	long j;

	for (i = f->n_components - 1; i >= 0; i--) {
		c0 = f->components[i];

		if (EDA_NOT != c0->type)
			continue;

		p0  = c0->pins[EDA_NOT_IN];
		p1  = c0->pins[EDA_NOT_OUT];

		el0 = f->elines[p0->lid];
		el1 = f->elines[p1->lid];

		if (2 == el1->n_pins) {
			assert(0 == efunction__del_component(f, c0));
			ScfEcomponent_free(c0);

			for (j = 2; j + 1 < el0->n_pins; j += 2) {

				c1 = f->components[el0->pins[j]];
				p1 = c1->pins     [el0->pins[j + 1]];

				int ret = epin__add_component(p1, el0->pins[0], el0->pins[1]);
				if (ret < 0)
					return ret;
			}
			continue;
		}

		if (2 != el0->n_pins || 4 != el1->n_pins)
			continue;

		if (el1->pins[0] == c0->id) {
			c1 = f->components[el1->pins[2]];
			p2 = c1->pins     [el1->pins[3]];
		} else {
			c1 = f->components[el1->pins[0]];
			p2 = c1->pins     [el1->pins[1]];
		}

		if (EDA_NOT != c1->type || EDA_NOT_IN != p2->id)
			continue;
		p3 = c1->pins[EDA_NOT_OUT];

		_eda_peep_hole_2not(f, p0, p3);

		if (i > c1->id)
			i--;

		assert(0 == efunction__del_component(f, c0));
		assert(0 == efunction__del_component(f, c1));

		ScfEcomponent_free(c0);
		ScfEcomponent_free(c1);
	}

	for (i = 0; i < f->n_elines; i++) {
		el0       = f->elines[i];

		ScfEline_free(el0);
	}

	free(f->elines);
	f->elines = NULL;
	f->n_elines = 0;
	return 0;
}

int	_eda_select_inst(native_t* ctx)
{
	eda_context_t*	eda = ctx->priv;
	function_t*     f   = eda->f;
	basic_block_t*  bb;
	bb_group_t*     bbg;
	list_t*         l;

	int i;
	int j;
	int ret = 0;

	_eda_fix_jmps(ctx, f);

	for (i = 0; i < f->bb_loops->size; i++) {
		bbg       = f->bb_loops->data[i];

		for (j = 0; j < bbg->posts->size; j++) {
			bb =        bbg->posts->data[j];

			list_mov2(&bb->code_list_head, &bb->save_list_head);
		}
	}

	for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head); l = list_next(l)) {
		bb = list_data(l, basic_block_t, list);

		if (bb->jmp_flag)
			continue;

		logd("************ bb: %d\n", bb->index);
		ret = _eda_make_insts_for_list(ctx, &bb->code_list_head);
		if (ret < 0)
			return ret;
		logd("************ bb: %d\n", bb->index);
	}

#if 1
	ret = _eda_peep_hole(f->ef);
	if (ret < 0)
		return ret;
#endif
	return 0;
}

int eda_select_inst(native_t* ctx, function_t* f)
{
	eda_context_t* eda = ctx->priv;
	ScfEcomponent*     B   = NULL;
	dag_node_t*    dn;
	list_t*        l;

	eda->f = f;

	assert(!f->ef);

	f->ef = efunction__alloc(f->node.w->text->data);
	if (!f->ef)
		return -ENOMEM;

	EDA_INST_ADD_COMPONENT(f->ef, B, EDA_Battery);

	B->pins[EDA_Battery_NEG]->flags = EDA_PIN_NEG;
	B->pins[EDA_Battery_POS]->flags = EDA_PIN_POS;

	int ret = _eda_select_inst(ctx);
	if (ret < 0)
		return ret;

	ScfEcomponent*  c;
	ScfEpin*        p;
	ScfEpin*        ck    = NULL;
	ScfEpin*        delay = NULL;

	long i;
	long j;
	for (i = 0; i < f->ef->n_components; i++) {
		c  =        f->ef->components[i];

		logd("c: %ld, type: %s\n", c->id, component_types[c->type]);

		for (j = 0; j < c->n_pins; j++) {
			p  =        c->pins[j];

			logd("cid: %ld, pid: %ld, flags: %#lx\n", p->cid, p->id, p->flags);

			if (p->flags & EDA_PIN_CK) {
				if (ck)
					EDA_PIN_ADD_PIN_EF(f->ef, ck, p);
				else
					ck = p;

				p->flags &= ~EDA_PIN_CK;
			}

			if (p->flags & EDA_PIN_DELAY) {
				if (delay)
					EDA_PIN_ADD_PIN_EF(f->ef, delay, p);
				else
					delay = p;

				p->flags &= ~EDA_PIN_DELAY;
			}
#if 0
			long k;
			for (k = 0; k + 1 < p->n_tos; k += 2)
				printf("to cid: %ld, pid: %ld\n", p->tos[k], p->tos[k + 1]);
			printf("\n");
#endif
		}
		logd("-----\n\n");
	}

	if (ck) {
		ScfEcomponent*  NAND;
		ScfEcomponent*  AND;

		EDA_INST_ADD_COMPONENT(f->ef, NAND, EDA_NAND);
		EDA_INST_ADD_COMPONENT(f->ef, AND,  EDA_AND);

		EDA_PIN_ADD_PIN(NAND, EDA_NAND_POS, B, EDA_Battery_POS);
		EDA_PIN_ADD_PIN(NAND, EDA_NAND_NEG, B, EDA_Battery_NEG);

		EDA_PIN_ADD_PIN(AND,  EDA_AND_POS,  B, EDA_Battery_POS);
		EDA_PIN_ADD_PIN(AND,  EDA_AND_NEG,  B, EDA_Battery_NEG);

		EDA_PIN_ADD_PIN(NAND, EDA_NAND_IN0, NAND, EDA_NAND_IN1);
		EDA_PIN_ADD_PIN(NAND, EDA_NAND_IN0, AND,  EDA_AND_IN0);
		EDA_PIN_ADD_PIN(NAND, EDA_NAND_OUT, AND,  EDA_AND_IN1);

		EDA_PIN_ADD_PIN_EF(f->ef, ck, AND->pins[EDA_AND_OUT]);

		NAND->pins[EDA_NAND_IN0]->flags |= EDA_PIN_CK | EDA_PIN_IN;

		NAND->model = EDA_TTL_DELAY;
	}

	if (delay) {
		ScfEcomponent*  R;
		ScfEcomponent*  C;
		ScfEcomponent*  T;

		EDA_INST_ADD_COMPONENT(f->ef, R, EDA_Resistor);
		EDA_INST_ADD_COMPONENT(f->ef, C, EDA_Capacitor);
		EDA_INST_ADD_COMPONENT(f->ef, T, EDA_NPN);

		EDA_PIN_ADD_PIN(R, 1, B, EDA_Battery_POS);
		EDA_PIN_ADD_PIN(R, 0, C, 1);
		EDA_PIN_ADD_PIN(R, 0, T, EDA_NPN_B);
		EDA_PIN_ADD_PIN(C, 0, T, EDA_NPN_E);
		EDA_PIN_ADD_PIN(C, 0, B, EDA_Battery_NEG);

		EDA_PIN_ADD_PIN_EF(f->ef, delay, T->pins[EDA_NPN_C]);

		R->r  = 1600;
		C->uf = 4e-4;
	}

	return 0;
}

native_ops_t	native_ops_eda = {
	.name            = "eda",

	.open            = eda_open,
	.close           = eda_close,

	.select_inst     = eda_select_inst,
};
