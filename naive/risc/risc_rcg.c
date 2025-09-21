#include"risc.h"

static int _risc_rcg_node_cmp(const void* v0, const void* v1)
{
	const graph_node_t* gn1 = v1;
	const risc_rcg_node_t* rn1 = gn1->data;
	const risc_rcg_node_t* rn0 = v0;

	if (rn0->dag_node || rn1->dag_node)
		return rn0->dag_node != rn1->dag_node;

	return rn0->reg != rn1->reg;
}

int risc_rcg_find_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg)
{
	risc_rcg_node_t* rn = calloc(1, sizeof(risc_rcg_node_t));
	if (!rn)
		return -ENOMEM;

	rn->dag_node = dn;
	rn->reg      = reg;

	graph_node_t* gn = vector_find_cmp(g->nodes, rn, _risc_rcg_node_cmp);
	free(rn);
	rn = NULL;

	if (!gn)
		return -1;

	*pp = gn;
	return 0;
}

int _risc_rcg_make_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg)
{
	risc_rcg_node_t* rn = calloc(1, sizeof(risc_rcg_node_t));
	if (!rn)
		return -ENOMEM;

	rn->dag_node = dn;
	rn->reg      = reg;
	rn->OpCode   = NULL;

	graph_node_t* gn = vector_find_cmp(g->nodes, rn, _risc_rcg_node_cmp);
	if (!gn) {

		gn = graph_node_alloc();
		if (!gn) {
			free(rn);
			return -ENOMEM;
		}

		gn->data = rn;

		if (reg)
			gn->color = reg->color;

		int ret = graph_add_node(g, gn);
		if (ret < 0) {
			free(rn);
			graph_node_free(gn);
			return ret;
		}
	} else {
		if (reg)
			gn->color = reg->color;

		free(rn);
		rn = NULL;
	}

	*pp = gn;
	return 0;
}

static int _risc_rcg_make_edge(graph_node_t* gn0, graph_node_t* gn1)
{
	if (gn0 == gn1)
		return 0;

	if (!vector_find(gn0->neighbors, gn1)) {

		assert(!vector_find(gn1->neighbors, gn0));

		int ret = graph_make_edge(gn0, gn1);
		if (ret < 0)
			return ret;

		ret = graph_make_edge(gn1, gn0);
		if (ret < 0)
			return ret;
	} else
		assert(vector_find(gn1->neighbors, gn0));

	return 0;
}

static int _risc_rcg_active_vars(graph_t* g, vector_t* active_vars)
{
	graph_node_t* gn0;
	graph_node_t* gn1;

	dn_status_t*  ds0;
	dn_status_t*  ds1;

	dag_node_t*   dn0;
	dag_node_t*   dn1;

	int ret;
	int i;
	int j;

	for (i  = 0; i < active_vars->size; i++) {

		ds0 =        active_vars->data[i];
		dn0 =        ds0->dag_node;

		if (!ds0->active)
			continue;

		ret = _risc_rcg_make_node(&gn0, g, dn0, NULL);
		if (ret < 0)
			return ret;

		for (j  = 0; j < i; j++) {

			ds1 = active_vars->data[j];
			dn1 = ds1->dag_node;

			if (!ds1->active)
				continue;

			ret = _risc_rcg_make_node(&gn1, g, dn1, NULL);
			if (ret < 0)
				return ret;

			assert(gn0 != gn1);

			ret = _risc_rcg_make_edge(gn0, gn1);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int _risc_rcg_operands(graph_t* g, vector_t* operands)
{
	_3ac_operand_t*  operand0;
	_3ac_operand_t*  operand1;

	graph_node_t*   gn0;
	graph_node_t*   gn1;

	dag_node_t*     dn0;
	dag_node_t*     dn1;

	int i;
	int j;

	for (i = 0; i < operands->size; i++) {

		operand0  = operands->data[i];
		dn0       = operand0->dag_node;

		if (variable_const(dn0->var))
			continue;

		int ret = _risc_rcg_make_node(&gn0, g, dn0, NULL);
		if (ret < 0)
			return ret;

		for (j = 0; j < i; j++) {

			operand1  = operands->data[j];
			dn1       = operand1->dag_node;

			if (variable_const(dn1->var))
				continue;

			ret = _risc_rcg_make_node(&gn1, g, dn1, NULL);
			if (ret < 0)
				return ret;

			if (gn1 == gn0)
				continue;

			ret = _risc_rcg_make_edge(gn0, gn1);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int _risc_rcg_to_active_vars(graph_t* g, graph_node_t* gn0, vector_t* active_vars)
{
	graph_node_t* gn1;
	dn_status_t*  ds1;
	dag_node_t*   dn1;

	int ret;
	int i;

	for (i  = 0; i < active_vars->size; i++) {

		ds1 =        active_vars->data[i];
		dn1 =        ds1->dag_node;

		if (!ds1->active)
			continue;

		ret = _risc_rcg_make_node(&gn1, g, dn1, NULL);
		if (ret < 0)
			return ret;

		if (gn0 == gn1)
			continue;

		ret = _risc_rcg_make_edge(gn0, gn1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int risc_rcg_make(_3ac_code_t* c, graph_t* g, dag_node_t* dn, register_t* reg)
{
	graph_node_t* gn0 = NULL;
	graph_node_t* gn1;
	dag_node_t*   dn1;
	dn_status_t*  ds1;

	int ret;
	int i;

	if (dn || reg) {
		ret = _risc_rcg_make_node(&gn0, g, dn, reg);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	logd("g->nodes->size: %d, active_vars: %d\n", g->nodes->size, c->active_vars->size);

	ret = _risc_rcg_active_vars(g, c->active_vars);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (gn0)
		return _risc_rcg_to_active_vars(g, gn0, c->active_vars);

	return 0;
}

static int _risc_rcg_make2(_3ac_code_t* c, dag_node_t* dn, register_t* reg)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	return risc_rcg_make(c, c->rcg, dn, reg);
}

static int _risc_rcg_call(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	risc_context_t* risc = ctx->priv;
	function_t*     f    = risc->f;
	dag_node_t*     dn   = NULL;
	register_t*     r    = NULL;
	_3ac_operand_t*  src  = NULL;
	_3ac_operand_t*  dst  = NULL;
	graph_node_t*   gn   = NULL;

	int i;
	int ret;

	if (c->srcs->size < 1)
		return -EINVAL;

	ret = _risc_rcg_active_vars(g, c->active_vars);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = _risc_rcg_operands(g, c->srcs);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (c->dsts) {

		if (c->dsts->size > f->rops->ABI_RET_NB) {
			loge("\n");
			return -EINVAL;
		}

		ret = _risc_rcg_operands(g, c->dsts);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		for (i  = 0; i < c->dsts->size; i++) {
			dst =        c->dsts->data[i];
			dn  =        dst->dag_node;

			if (VAR_VOID == dn->var->type && 0 == dn->var->nb_pointers)
				continue;

			int is_float = variable_float(dn->var);
			int size     = f->rops->variable_size (dn->var);

			if (0 == i)
				r =  f->rops->find_register_type_id_bytes(is_float, RISC_REG_X0, size);

			else if (!is_float)
				r =  f->rops->find_register_type_id_bytes(is_float, f->rops->abi_ret_regs[i], size);
			else
				r = NULL;

			gn  = NULL;
			ret = _risc_rcg_make_node(&gn, g, dn, r);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			ret = _risc_rcg_to_active_vars(g, gn, c->active_vars);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
		}
	}

	int nb_ints = 0;

	f->rops->call_rabi(c, f, &nb_ints, NULL, NULL);

	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		if (variable_const(dn->var))
			continue;

		gn  = NULL;
		ret = _risc_rcg_make_node(&gn, g, dn, dn->rabi2);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	_3ac_operand_t*  src_pf = c->srcs->data[0];
	graph_node_t*   gn_pf  = NULL;
	dag_node_t*     dn_pf  = src_pf->dag_node;

	if (!dn_pf->var->const_literal_flag) {

		ret = _risc_rcg_make_node(&gn_pf, g, dn_pf, NULL);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		for (i = 0; i < nb_ints; i++) {

			register_t*     rabi    = NULL;
			graph_node_t*   gn_rabi = NULL;

			rabi = f->rops->find_register_type_id_bytes(0, f->rops->abi_regs[i], dn_pf->var->size);

			ret  = _risc_rcg_make_node(&gn_rabi, g, NULL, rabi);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			assert(gn_pf != gn_rabi);

			ret = _risc_rcg_make_edge(gn_pf, gn_rabi);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int _risc_rcg_call_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _risc_rcg_call(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _risc_rcg_call(ctx, c, g);
}

static int _risc_rcg_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_bit_not_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_logic_not_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_neg_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_address_of_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_cast_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_mul_div_mod(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	dag_node_t*    dn   = NULL;
	_3ac_operand_t* src  = c->srcs->data[c->srcs->size - 1];
	risc_context_t* risc  = ctx->priv;
	_3ac_operand_t* dst;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (c->dsts) {
		dst = c->dsts->data[0];
		dn  = dst->dag_node;
	}

	return risc_rcg_make(c, g, dn, NULL);
}

static int _risc_rcg_mul_div_mod2(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _risc_rcg_mul_div_mod(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _risc_rcg_mul_div_mod(ctx, c, g);
}

static int _risc_rcg_mul_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_div_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_mod_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_add_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_sub_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_shift(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	dag_node_t*     dn    = NULL;
	_3ac_operand_t*  count = c->srcs->data[c->srcs->size - 1];
	graph_node_t*   gn    = NULL;

	if (!count || !count->dag_node)
		return -EINVAL;

	if (c->dsts) {
		_3ac_operand_t* dst = c->dsts->data[0];

		dn = dst->dag_node;
	}

	return risc_rcg_make(c, g, dn, NULL);
}

static int _risc_rcg_shift2(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _risc_rcg_shift(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _risc_rcg_shift(ctx, c, g);
}

static int _risc_rcg_shl_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_shift2(ctx, c, g);
}

static int _risc_rcg_shr_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_shift2(ctx, c, g);
}

static int _risc_rcg_bit_and_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_bit_or_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_cmp_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_teq_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

#define RISC_RCG_SET(setcc) \
static int _risc_rcg_##setcc##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _risc_rcg_make2(c, dst->dag_node, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, dst->dag_node, NULL); \
}
RISC_RCG_SET(setz)
RISC_RCG_SET(setnz)
RISC_RCG_SET(setgt)
RISC_RCG_SET(setge)
RISC_RCG_SET(setlt)
RISC_RCG_SET(setle)

static int _risc_rcg_eq_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_ne_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_gt_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_lt_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_add_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_sub_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_mul_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_div_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_mod_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_mul_div_mod2(ctx, c, g);
}

static int _risc_rcg_shl_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_shift2(ctx, c, g);
}

static int _risc_rcg_shr_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _risc_rcg_shift2(ctx, c, g);
}

static int _risc_rcg_and_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_or_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_return_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	risc_context_t* risc = ctx->priv;
	function_t*     f    = risc->f;

	_3ac_operand_t*  src;
	graph_node_t*   gn;
	dag_node_t*     dn;
	register_t*     r;

	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = risc_rcg_make(c, g, NULL, NULL);
	if (ret < 0)
		return ret;

	if (!c->srcs)
		return 0;

	int i;
	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		int is_float = variable_float(dn->var);
		int size     = f->rops->variable_size(dn->var);

		size = size > 4 ? 8 : 4;

		if (is_float) {
			if (i > 0) {
				loge("\n");
				return -1;
			}

			r = f->rops->find_register_type_id_bytes(is_float, 0, size);
		} else
			r = f->rops->find_register_type_id_bytes(is_float, f->rops->abi_ret_regs[i], size);

		ret = _risc_rcg_make_node(&gn, g, dn, r);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _risc_rcg_memset_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int i;

	register_t* r;
	_3ac_operand_t*  src;
	graph_node_t*   gn;
	dag_node_t*     dn;

	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = risc_rcg_make(c, g, NULL, NULL);
	if (ret < 0)
		return ret;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;
#if 0
#endif
	return 0;
}

static int _risc_rcg_goto_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

#define RISC_RCG_JCC(cc) \
static int _risc_rcg_##cc##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return 0; \
}

RISC_RCG_JCC(jz)
RISC_RCG_JCC(jnz)
RISC_RCG_JCC(jgt)
RISC_RCG_JCC(jge)
RISC_RCG_JCC(jlt)
RISC_RCG_JCC(jle)

RISC_RCG_JCC(ja)
RISC_RCG_JCC(jae)
RISC_RCG_JCC(jb)
RISC_RCG_JCC(jbe)

static int _risc_rcg_save_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_load_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;

	return risc_rcg_make(c, g, NULL, NULL);
}
static int _risc_rcg_nop_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}
static int _risc_rcg_end_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

#define RISC_RCG_BINARY_ASSIGN(name) \
static int _risc_rcg_##name##_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
} \
static int _risc_rcg_##name##_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
} \
static int _risc_rcg_##name##_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
}

RISC_RCG_BINARY_ASSIGN(add)
RISC_RCG_BINARY_ASSIGN(sub)
RISC_RCG_BINARY_ASSIGN(and)
RISC_RCG_BINARY_ASSIGN(or)

#define RISC_RCG_SHIFT_ASSIGN(name) \
static int _risc_rcg_##name##_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _risc_rcg_shift2(ctx, c, g); \
} \
static int _risc_rcg_##name##_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _risc_rcg_shift2(ctx, c, g); \
} \
static int _risc_rcg_##name##_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _risc_rcg_shift2(ctx, c, g); \
}
RISC_RCG_SHIFT_ASSIGN(shl)
RISC_RCG_SHIFT_ASSIGN(shr)

#define RISC_RCG_UNARY_ASSIGN(name) \
static int _risc_rcg_##name##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
} \
static int _risc_rcg_##name##_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
} \
static int _risc_rcg_##name##_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
} \
static int _risc_rcg_##name##_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _risc_rcg_make2(c, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, NULL, NULL); \
}
RISC_RCG_UNARY_ASSIGN(inc)
RISC_RCG_UNARY_ASSIGN(dec)

#define RISC_RCG_UNARY_POST_ASSIGN(name) \
static int _risc_rcg_##name##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _risc_rcg_make2(c, dst->dag_node, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, dst->dag_node, NULL); \
} \
static int _risc_rcg_##name##_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _risc_rcg_make2(c, dst->dag_node, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, dst->dag_node, NULL); \
} \
static int _risc_rcg_##name##_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _risc_rcg_make2(c, dst->dag_node, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, dst->dag_node, NULL); \
} \
static int _risc_rcg_##name##_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _risc_rcg_make2(c, dst->dag_node, NULL); \
	if (ret < 0) \
		return ret; \
	return risc_rcg_make(c, g, dst->dag_node, NULL); \
}
RISC_RCG_UNARY_POST_ASSIGN(inc_post)
RISC_RCG_UNARY_POST_ASSIGN(dec_post)

static int _risc_rcg_address_of_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;
	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_address_of_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;
	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static int _risc_rcg_push_rax_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}
static int _risc_rcg_pop_rax_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

static int _risc_rcg_va_start_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;
	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_va_end_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _risc_rcg_make2(c, NULL, NULL);
	if (ret < 0)
		return ret;
	return risc_rcg_make(c, g, NULL, NULL);
}

static int _risc_rcg_va_arg_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _risc_rcg_make2(c, dst->dag_node, NULL);
	if (ret < 0)
		return ret;
	return risc_rcg_make(c, g, dst->dag_node, NULL);
}

static risc_rcg_handler_pt  risc_rcg_handlers[N__3ac_OPS] =
{
	[OP_CALL 		]  =  _risc_rcg_call_handler,
	[OP_ARRAY_INDEX ]  =  _risc_rcg_array_index_handler,

	[OP_TYPE_CAST   ]  =  _risc_rcg_cast_handler,
	[OP_LOGIC_NOT  	]  =  _risc_rcg_logic_not_handler,
	[OP_BIT_NOT     ]  =  _risc_rcg_bit_not_handler,
	[OP_NEG  		]  =  _risc_rcg_neg_handler,

	[OP_VA_START    ]  =  _risc_rcg_va_start_handler,
	[OP_VA_ARG      ]  =  _risc_rcg_va_arg_handler,
	[OP_VA_END      ]  =  _risc_rcg_va_end_handler,

	[OP_INC         ]  =  _risc_rcg_inc_handler,
	[OP_DEC         ]  =  _risc_rcg_dec_handler,

	[OP_INC_POST    ]  =  _risc_rcg_inc_post_handler,
	[OP_DEC_POST    ]  =  _risc_rcg_dec_post_handler,

	[OP_DEREFERENCE ]  =  _risc_rcg_dereference_handler,
	[OP_ADDRESS_OF  ]  =  _risc_rcg_address_of_handler,
	[OP_POINTER     ]  =  _risc_rcg_pointer_handler,

	[OP_MUL  		]  =  _risc_rcg_mul_handler,
	[OP_DIV  		]  =  _risc_rcg_div_handler,
	[OP_MOD         ]  =  _risc_rcg_mod_handler,

	[OP_ADD  		]  =  _risc_rcg_add_handler,
	[OP_SUB  		]  =  _risc_rcg_sub_handler,

	[OP_SHL         ]  =  _risc_rcg_shl_handler,
	[OP_SHR         ]  =  _risc_rcg_shr_handler,

	[OP_BIT_AND     ]  =  _risc_rcg_bit_and_handler,
	[OP_BIT_OR      ]  =  _risc_rcg_bit_or_handler,

	[OP_EQ  		]  =  _risc_rcg_eq_handler,
	[OP_NE  		]  =  _risc_rcg_ne_handler,
	[OP_GT  		]  =  _risc_rcg_gt_handler,
	[OP_LT  		]  =  _risc_rcg_lt_handler,

	[OP_ASSIGN  	]  =  _risc_rcg_assign_handler,
	[OP_ADD_ASSIGN  ]  =  _risc_rcg_add_assign_handler,
	[OP_SUB_ASSIGN  ]  =  _risc_rcg_sub_assign_handler,

	[OP_MUL_ASSIGN  ]  =  _risc_rcg_mul_assign_handler,
	[OP_DIV_ASSIGN  ]  =  _risc_rcg_div_assign_handler,
	[OP_MOD_ASSIGN  ]  =  _risc_rcg_mod_assign_handler,

	[OP_SHL_ASSIGN  ]  =  _risc_rcg_shl_assign_handler,
	[OP_SHR_ASSIGN  ]  =  _risc_rcg_shr_assign_handler,

	[OP_AND_ASSIGN  ]  =  _risc_rcg_and_assign_handler,
	[OP_OR_ASSIGN   ]  =  _risc_rcg_or_assign_handler,

	[OP_RETURN  	]  =  _risc_rcg_return_handler,

	[OP_3AC_CMP     ]  =  _risc_rcg_cmp_handler,
	[OP_3AC_TEQ     ]  =  _risc_rcg_teq_handler,

	[OP_3AC_SETZ    ]  =  _risc_rcg_setz_handler,
	[OP_3AC_SETNZ   ]  =  _risc_rcg_setnz_handler,
	[OP_3AC_SETGT   ]  =  _risc_rcg_setgt_handler,
	[OP_3AC_SETGE   ]  =  _risc_rcg_setge_handler,
	[OP_3AC_SETLT   ]  =  _risc_rcg_setlt_handler,
	[OP_3AC_SETLE   ]  =  _risc_rcg_setle_handler,

	[OP_GOTO        ]  =  _risc_rcg_goto_handler,
	[OP_3AC_JZ      ]  =  _risc_rcg_jz_handler,
	[OP_3AC_JNZ     ]  =  _risc_rcg_jnz_handler,
	[OP_3AC_JGT     ]  =  _risc_rcg_jgt_handler,
	[OP_3AC_JGE     ]  =  _risc_rcg_jge_handler,
	[OP_3AC_JLT     ]  =  _risc_rcg_jlt_handler,
	[OP_3AC_JLE     ]  =  _risc_rcg_jle_handler,

	[OP_3AC_JA      ]  =  _risc_rcg_ja_handler,
	[OP_3AC_JAE     ]  =  _risc_rcg_jae_handler,
	[OP_3AC_JB      ]  =  _risc_rcg_jb_handler,
	[OP_3AC_JBE     ]  =  _risc_rcg_jbe_handler,

	[OP_3AC_SAVE    ]  =  _risc_rcg_save_handler,
	[OP_3AC_LOAD    ]  =  _risc_rcg_load_handler,

	[OP_3AC_RESAVE  ]  =  _risc_rcg_save_handler,
	[OP_3AC_RELOAD  ]  =  _risc_rcg_load_handler,

	[OP_3AC_NOP     ]  =  _risc_rcg_nop_handler,
	[OP_3AC_END     ]  =  _risc_rcg_end_handler,

	[OP_3AC_INC     ]  =  _risc_rcg_inc_handler,
	[OP_3AC_DEC     ]  =  _risc_rcg_dec_handler,

	[OP_3AC_PUSH_RETS] =  _risc_rcg_push_rax_handler,
	[OP_3AC_POP_RETS ] =  _risc_rcg_pop_rax_handler,

	[OP_3AC_MEMSET  ]  =  _risc_rcg_memset_handler,


	[OP_3AC_ASSIGN_DEREFERENCE    ]  =  _risc_rcg_assign_dereference_handler,
	[OP_3AC_ASSIGN_ARRAY_INDEX    ]  =  _risc_rcg_assign_array_index_handler,
	[OP_3AC_ASSIGN_POINTER        ]  =  _risc_rcg_assign_pointer_handler,

	[OP_3AC_ADDRESS_OF_ARRAY_INDEX]  =  _risc_rcg_address_of_array_index_handler,
	[OP_3AC_ADDRESS_OF_POINTER    ]  =  _risc_rcg_address_of_pointer_handler,
};

risc_rcg_handler_pt  risc_find_rcg_handler(const int op_type)
{
	if (op_type < 0 || op_type >= N_3AC_OPS)
		return NULL;

	return risc_rcg_handlers[op_type];
}
