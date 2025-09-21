#include"x64.h"

static int _x64_rcg_node_cmp(const void* v0, const void* v1)
{
	const graph_node_t* gn1 = v1;
	const x64_rcg_node_t*   rn1 = gn1->data;
	const x64_rcg_node_t*   rn0 = v0;

	if (rn0->dag_node || rn1->dag_node)
		return rn0->dag_node != rn1->dag_node;

	return rn0->reg != rn1->reg;
}

int x64_rcg_find_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg)
{
	x64_rcg_node_t* rn = calloc(1, sizeof(x64_rcg_node_t));
	if (!rn)
		return -ENOMEM;

	rn->dag_node = dn;
	rn->reg      = reg;

	graph_node_t* gn = vector_find_cmp(g->nodes, rn, _x64_rcg_node_cmp);
	free(rn);
	rn = NULL;

	if (!gn)
		return -1;

	*pp = gn;
	return 0;
}

int _x64_rcg_make_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg, x64_OpCode_t* OpCode)
{
	x64_rcg_node_t* rn = calloc(1, sizeof(x64_rcg_node_t));
	if (!rn)
		return -ENOMEM;

	rn->dag_node = dn;
	rn->reg      = reg;
	rn->OpCode   = OpCode;

	graph_node_t* gn = vector_find_cmp(g->nodes, rn, _x64_rcg_node_cmp);
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

static int _x64_rcg_make_edge(graph_node_t* gn0, graph_node_t* gn1)
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

static int _x64_rcg_active_vars(graph_t* g, vector_t* active_vars)
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

		ret = _x64_rcg_make_node(&gn0, g, dn0, NULL, NULL);
		if (ret < 0)
			return ret;

		for (j  = 0; j < i; j++) {

			ds1 = active_vars->data[j];
			dn1 = ds1->dag_node;

			if (!ds1->active)
				continue;

			ret = _x64_rcg_make_node(&gn1, g, dn1, NULL, NULL);
			if (ret < 0)
				return ret;

			assert(gn0 != gn1);

			ret = _x64_rcg_make_edge(gn0, gn1);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int _x64_rcg_operands(graph_t* g, vector_t* operands)
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

		int ret = _x64_rcg_make_node(&gn0, g, dn0, NULL, NULL);
		if (ret < 0)
			return ret;

		for (j = 0; j < i; j++) {

			operand1  = operands->data[j];
			dn1       = operand1->dag_node;

			if (variable_const(dn1->var))
				continue;

			ret = _x64_rcg_make_node(&gn1, g, dn1, NULL, NULL);
			if (ret < 0)
				return ret;

			if (gn1 == gn0)
				continue;

			ret = _x64_rcg_make_edge(gn0, gn1);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

static int _x64_rcg_to_active_vars(graph_t* g, graph_node_t* gn0, vector_t* active_vars)
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

		ret = _x64_rcg_make_node(&gn1, g, dn1, NULL, NULL);
		if (ret < 0)
			return ret;

		if (gn0 == gn1)
			continue;

		ret = _x64_rcg_make_edge(gn0, gn1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _x64_rcg_make(_3ac_code_t* c, graph_t* g, dag_node_t* dn,
		register_t* reg, x64_OpCode_t* OpCode)
{
	graph_node_t* gn0 = NULL;
	graph_node_t* gn1;
	dag_node_t*   dn1;
	dn_status_t*  ds1;

	int ret;
	int i;

	if (dn || reg) {
		ret = _x64_rcg_make_node(&gn0, g, dn, reg, OpCode);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	logd("g->nodes->size: %d, active_vars: %d\n", g->nodes->size, c->active_vars->size);

	ret = _x64_rcg_active_vars(g, c->active_vars);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (gn0)
		return _x64_rcg_to_active_vars(g, gn0, c->active_vars);

	return 0;
}

static int _x64_rcg_make2(_3ac_code_t* c, dag_node_t* dn, register_t* reg, x64_OpCode_t* OpCode)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	return _x64_rcg_make(c, c->rcg, dn, reg, OpCode);
}

static int _x64_rcg_call(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	x64_context_t*  x64 = ctx->priv;
	function_t*     f   = x64->f;
	dag_node_t*     dn  = NULL;
	register_t* r   = NULL;
	_3ac_operand_t*  src = NULL;
	_3ac_operand_t*  dst = NULL;
	graph_node_t*   gn  = NULL;

	int i;
	int ret;

	if (c->srcs->size < 1)
		return -EINVAL;

	ret = _x64_rcg_active_vars(g, c->active_vars);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = _x64_rcg_operands(g, c->srcs);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	if (c->dsts) {

		if (c->dsts->size > X64_ABI_RET_NB) {
			loge("\n");
			return -EINVAL;
		}

		ret = _x64_rcg_operands(g, c->dsts);
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
			int size     = x64_variable_size (dn->var);

			if (0 == i)
				r =  x64_find_register_type_id_bytes(is_float, X64_REG_AX, size);

			else if (!is_float)
				r =  x64_find_register_type_id_bytes(is_float, x64_abi_ret_regs[i], size);
			else
				r = NULL;

			gn  = NULL;
			ret = _x64_rcg_make_node(&gn, g, dn, r, NULL);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			ret = _x64_rcg_to_active_vars(g, gn, c->active_vars);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
		}
	}

	int nb_ints   = 0;
	int nb_floats = 0;

	x64_call_rabi(&nb_ints, &nb_floats, c);

	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		if (variable_const(dn->var))
			continue;

		gn  = NULL;
		ret = _x64_rcg_make_node(&gn, g, dn, dn->rabi2, NULL);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	_3ac_operand_t*  src_pf = c->srcs->data[0];
	graph_node_t*   gn_pf  = NULL;
	dag_node_t*     dn_pf  = src_pf->dag_node;

	if (!dn_pf->var->const_literal_flag) {

		ret = _x64_rcg_make_node(&gn_pf, g, dn_pf, NULL, NULL);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		for (i = 0; i < nb_ints; i++) {

			register_t* rabi    = NULL;
			graph_node_t*   gn_rabi = NULL;

			rabi = x64_find_register_type_id_bytes(0, x64_abi_regs[i], dn_pf->var->size);

			ret  = _x64_rcg_make_node(&gn_rabi, g, NULL, rabi, NULL);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			assert(gn_pf != gn_rabi);

			ret = _x64_rcg_make_edge(gn_pf, gn_rabi);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

static int _x64_rcg_call_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _x64_rcg_call(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _x64_rcg_call(ctx, c, g);
}

static int _x64_rcg_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_bit_not_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_logic_not_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_neg_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_address_of_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_cast_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_mul_div_mod(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	dag_node_t*    dn   = NULL;
	_3ac_operand_t* src  = c->srcs->data[c->srcs->size - 1];
	x64_context_t* x64  = ctx->priv;
	_3ac_operand_t* dst;

	if (!src || !src->dag_node)
		return -EINVAL;

	if (c->dsts) {
		dst = c->dsts->data[0];
		dn  = dst->dag_node;
	}

	if (variable_float(src->dag_node->var))
		return _x64_rcg_make(c, g, dn, NULL, NULL);

	int size = x64_variable_size(src->dag_node->var);
	int ret  = 0;

	register_t* rl = x64_find_register_type_id_bytes(0, X64_REG_AX, size);
	register_t* rh;

	if (1 == size)
		rh = x64_find_register("ah");
	else
		rh = x64_find_register_type_id_bytes(0, X64_REG_DX, size);

	dst = c->dsts->data[0];
	switch (c->op->type) {
		case OP_MUL:
		case OP_DIV:
		case OP_MUL_ASSIGN:
		case OP_DIV_ASSIGN:
			ret = _x64_rcg_make(c, g, dst->dag_node, rl, NULL);
			break;

		case OP_MOD:
		case OP_MOD_ASSIGN:
			ret = _x64_rcg_make(c, g, dst->dag_node, rh, NULL);
			break;

		default:
			break;
	};

	if (ret < 0)
		return ret;

	ret = _x64_rcg_make(c, g, NULL, rl, NULL);
	if (ret < 0)
		return ret;

	ret = _x64_rcg_make(c, g, NULL, rh, NULL);
	if (ret < 0)
		return ret;

	return 0;
}

static int _x64_rcg_mul_div_mod2(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _x64_rcg_mul_div_mod(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _x64_rcg_mul_div_mod(ctx, c, g);
}

static int _x64_rcg_mul_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_div_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_mod_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_add_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_sub_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_shift(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (!c->srcs || c->srcs->size < 1)
		return -EINVAL;

	dag_node_t*     dn    = NULL;
	_3ac_operand_t*  count = c->srcs->data[c->srcs->size - 1];
	graph_node_t*   gn    = NULL;
	graph_node_t*   gn_cl = NULL;
	register_t* cl    = x64_find_register_type_id_bytes(0, X64_REG_CL, count->dag_node->var->size);

	if (!count || !count->dag_node)
		return -EINVAL;

	if (c->dsts) {
		_3ac_operand_t* dst = c->dsts->data[0];

		dn = dst->dag_node;
	}

	if (variable_const(count->dag_node->var))
		return _x64_rcg_make(c, g, dn, NULL, NULL);

	int ret = _x64_rcg_make_node(&gn, g, count->dag_node, cl, NULL);
	if (ret < 0)
		return ret;

	ret = _x64_rcg_make(c, g, dn, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = _x64_rcg_make_node(&gn_cl, g, NULL, cl, NULL);
	if (ret < 0)
		return ret;

	if (dn) {
		ret = _x64_rcg_make_node(&gn, g, dn, NULL, NULL);
		if (ret < 0)
			return ret;

		ret = _x64_rcg_make_edge(gn_cl, gn);
		if (ret < 0)
			return ret;
	}

	int i;
	for (i = 0; i < c->srcs->size - 1; i++) {
		_3ac_operand_t* src    = c->srcs->data[i];
		graph_node_t*  gn_src = NULL;

		ret = _x64_rcg_make_node(&gn_src, g, src->dag_node, NULL, NULL);
		if (ret < 0)
			return ret;

		ret = _x64_rcg_make_edge(gn_cl, gn_src);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _x64_rcg_shift2(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	if (c->rcg)
		graph_free(c->rcg);

	c->rcg = graph_alloc();
	if (!c->rcg)
		return -ENOMEM;

	int ret = _x64_rcg_shift(ctx, c, c->rcg);
	if (ret < 0)
		return ret;

	return _x64_rcg_shift(ctx, c, g);
}

static int _x64_rcg_shl_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_shift2(ctx, c, g);
}

static int _x64_rcg_shr_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_shift2(ctx, c, g);
}

static int _x64_rcg_bit_and_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_bit_or_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_dump_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_cmp_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_teq_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

#define X64_RCG_SET(setcc) \
static int _x64_rcg_##setcc##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL); \
}
X64_RCG_SET(setz)
X64_RCG_SET(setnz)
X64_RCG_SET(setgt)
X64_RCG_SET(setge)
X64_RCG_SET(setlt)
X64_RCG_SET(setle)

static int _x64_rcg_eq_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_ne_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_gt_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_lt_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_add_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_sub_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_mul_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_div_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_mod_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_mul_div_mod2(ctx, c, g);
}

static int _x64_rcg_shl_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_shift2(ctx, c, g);
}

static int _x64_rcg_shr_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return _x64_rcg_shift2(ctx, c, g);
}

static int _x64_rcg_and_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_or_assign_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_vla_alloc_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_vla_free_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_return_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int i;

	register_t* r;
	_3ac_operand_t*  src;
	graph_node_t*   gn;
	dag_node_t*     dn;

	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = _x64_rcg_make(c, g, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	if (!c->srcs)
		return 0;

	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		int is_float = variable_float(dn->var);
		int size     = x64_variable_size (dn->var);

		size = size > 4 ? 8 : 4;

		if (is_float) {
			if (i > 0) {
				loge("\n");
				return -1;
			}

			r = x64_find_register_type_id_bytes(is_float, 0, size);
		} else
			r = x64_find_register_type_id_bytes(is_float, x64_abi_ret_regs[i], size);

		ret = _x64_rcg_make_node(&gn, g, dn, r, NULL);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _x64_rcg_memset_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int i;

	_3ac_operand_t*  src;
	graph_node_t*   gn;
	dag_node_t*     dn;
	register_t*     r;

	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	ret = _x64_rcg_make(c, g, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];
		dn  =        src->dag_node;

		int size = x64_variable_size (dn->var);
		size     = size > 4 ? 8 : 4;

		if (0 == i)
			r = x64_find_register_type_id_bytes(0, X64_REG_DI, size);

		else if (1 == i)
			r = x64_find_register_type_id_bytes(0, X64_REG_AX, size);

		else if (2 == i)
			r = x64_find_register_type_id_bytes(0, X64_REG_CX, size);
		else
			return -EINVAL;

		ret = _x64_rcg_make_node(&gn, g, dn, r, NULL);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _x64_rcg_goto_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

#define X64_RCG_JCC(cc) \
static int _x64_rcg_##cc##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return 0; \
}

X64_RCG_JCC(jz)
X64_RCG_JCC(jnz)
X64_RCG_JCC(jgt)
X64_RCG_JCC(jge)
X64_RCG_JCC(jlt)
X64_RCG_JCC(jle)

X64_RCG_JCC(ja)
X64_RCG_JCC(jae)
X64_RCG_JCC(jb)
X64_RCG_JCC(jbe)

static int _x64_rcg_save_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;

	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_load_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}
static int _x64_rcg_nop_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}
static int _x64_rcg_end_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

#define X64_RCG_BINARY_ASSIGN(name) \
static int _x64_rcg_##name##_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
} \
static int _x64_rcg_##name##_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
} \
static int _x64_rcg_##name##_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
}

X64_RCG_BINARY_ASSIGN(add)
X64_RCG_BINARY_ASSIGN(sub)
X64_RCG_BINARY_ASSIGN(and)
X64_RCG_BINARY_ASSIGN(or)

#define X64_RCG_SHIFT_ASSIGN(name) \
static int _x64_rcg_##name##_assign_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _x64_rcg_shift2(ctx, c, g); \
} \
static int _x64_rcg_##name##_assign_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _x64_rcg_shift2(ctx, c, g); \
} \
static int _x64_rcg_##name##_assign_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	return _x64_rcg_shift2(ctx, c, g); \
}
X64_RCG_SHIFT_ASSIGN(shl)
X64_RCG_SHIFT_ASSIGN(shr)

#define X64_RCG_UNARY_ASSIGN(name) \
static int _x64_rcg_##name##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
} \
static int _x64_rcg_##name##_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
} \
static int _x64_rcg_##name##_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
} \
static int _x64_rcg_##name##_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, NULL, NULL, NULL); \
}
X64_RCG_UNARY_ASSIGN(inc)
X64_RCG_UNARY_ASSIGN(dec)

#define X64_RCG_UNARY_POST_ASSIGN(name) \
static int _x64_rcg_##name##_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL); \
} \
static int _x64_rcg_##name##_dereference_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL); \
} \
static int _x64_rcg_##name##_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL); \
} \
static int _x64_rcg_##name##_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g) \
{ \
	_3ac_operand_t* dst = c->dsts->data[0]; \
	\
	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL); \
	if (ret < 0) \
		return ret; \
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL); \
}
X64_RCG_UNARY_POST_ASSIGN(inc_post)
X64_RCG_UNARY_POST_ASSIGN(dec_post)

static int _x64_rcg_address_of_array_index_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_address_of_pointer_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static int _x64_rcg_push_rax_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}
static int _x64_rcg_pop_rax_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	return 0;
}

static int _x64_rcg_va_start_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;
	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_va_end_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	int ret = _x64_rcg_make2(c, NULL, NULL, NULL);
	if (ret < 0)
		return ret;
	return _x64_rcg_make(c, g, NULL, NULL, NULL);
}

static int _x64_rcg_va_arg_handler(native_t* ctx, _3ac_code_t* c, graph_t* g)
{
	_3ac_operand_t* dst = c->dsts->data[0];

	int ret = _x64_rcg_make2(c, dst->dag_node, NULL, NULL);
	if (ret < 0)
		return ret;
	return _x64_rcg_make(c, g, dst->dag_node, NULL, NULL);
}

static x64_rcg_handler_pt  x64_rcg_handlers[N_3AC_OPS] =
{
	[OP_CALL        ]  =  _x64_rcg_call_handler,
	[OP_ARRAY_INDEX ]  =  _x64_rcg_array_index_handler,

	[OP_TYPE_CAST   ]  =  _x64_rcg_cast_handler,
	[OP_LOGIC_NOT   ]  =  _x64_rcg_logic_not_handler,
	[OP_BIT_NOT     ]  =  _x64_rcg_bit_not_handler,
	[OP_NEG  	    ]  =  _x64_rcg_neg_handler,

	[OP_VA_START    ]  =  _x64_rcg_va_start_handler,
	[OP_VA_ARG      ]  =  _x64_rcg_va_arg_handler,
	[OP_VA_END      ]  =  _x64_rcg_va_end_handler,

	[OP_INC         ]  =  _x64_rcg_inc_handler,
	[OP_DEC         ]  =  _x64_rcg_dec_handler,

	[OP_INC_POST    ]  =  _x64_rcg_inc_post_handler,
	[OP_DEC_POST    ]  =  _x64_rcg_dec_post_handler,

	[OP_DEREFERENCE ]  =  _x64_rcg_dereference_handler,
	[OP_ADDRESS_OF  ]  =  _x64_rcg_address_of_handler,
	[OP_POINTER     ]  =  _x64_rcg_pointer_handler,

	[OP_MUL  	    ]  =  _x64_rcg_mul_handler,
	[OP_DIV  	    ]  =  _x64_rcg_div_handler,
	[OP_MOD         ]  =  _x64_rcg_mod_handler,

	[OP_ADD  	    ]  =  _x64_rcg_add_handler,
	[OP_SUB  	    ]  =  _x64_rcg_sub_handler,

	[OP_SHL         ]  =  _x64_rcg_shl_handler,
	[OP_SHR         ]  =  _x64_rcg_shr_handler,

	[OP_BIT_AND     ]  =  _x64_rcg_bit_and_handler,
	[OP_BIT_OR      ]  =  _x64_rcg_bit_or_handler,

	[OP_EQ  	    ]  =  _x64_rcg_eq_handler,
	[OP_NE  	    ]  =  _x64_rcg_ne_handler,
	[OP_GT  	    ]  =  _x64_rcg_gt_handler,
	[OP_LT  	    ]  =  _x64_rcg_lt_handler,

	[OP_ASSIGN      ]  =  _x64_rcg_assign_handler,
	[OP_ADD_ASSIGN  ]  =  _x64_rcg_add_assign_handler,
	[OP_SUB_ASSIGN  ]  =  _x64_rcg_sub_assign_handler,

	[OP_MUL_ASSIGN  ]  =  _x64_rcg_mul_assign_handler,
	[OP_DIV_ASSIGN  ]  =  _x64_rcg_div_assign_handler,
	[OP_MOD_ASSIGN  ]  =  _x64_rcg_mod_assign_handler,

	[OP_SHL_ASSIGN  ]  =  _x64_rcg_shl_assign_handler,
	[OP_SHR_ASSIGN  ]  =  _x64_rcg_shr_assign_handler,

	[OP_AND_ASSIGN  ]  =  _x64_rcg_and_assign_handler,
	[OP_OR_ASSIGN   ]  =  _x64_rcg_or_assign_handler,

	[OP_VLA_ALLOC   ]  =  _x64_rcg_vla_alloc_handler,
	[OP_VLA_FREE    ]  =  _x64_rcg_vla_free_handler,

	[OP_RETURN      ]  =  _x64_rcg_return_handler,

	[OP_3AC_CMP     ]  =  _x64_rcg_cmp_handler,
	[OP_3AC_TEQ     ]  =  _x64_rcg_teq_handler,
	[OP_3AC_DUMP    ]  =  _x64_rcg_dump_handler,

	[OP_3AC_SETZ    ]  =  _x64_rcg_setz_handler,
	[OP_3AC_SETNZ   ]  =  _x64_rcg_setnz_handler,
	[OP_3AC_SETGT   ]  =  _x64_rcg_setgt_handler,
	[OP_3AC_SETGE   ]  =  _x64_rcg_setge_handler,
	[OP_3AC_SETLT   ]  =  _x64_rcg_setlt_handler,
	[OP_3AC_SETLE   ]  =  _x64_rcg_setle_handler,

	[OP_GOTO        ]  =  _x64_rcg_goto_handler,
	[OP_3AC_JZ      ]  =  _x64_rcg_jz_handler,
	[OP_3AC_JNZ     ]  =  _x64_rcg_jnz_handler,
	[OP_3AC_JGT     ]  =  _x64_rcg_jgt_handler,
	[OP_3AC_JGE     ]  =  _x64_rcg_jge_handler,
	[OP_3AC_JLT     ]  =  _x64_rcg_jlt_handler,
	[OP_3AC_JLE     ]  =  _x64_rcg_jle_handler,

	[OP_3AC_JA      ]  =  _x64_rcg_ja_handler,
	[OP_3AC_JAE     ]  =  _x64_rcg_jae_handler,
	[OP_3AC_JB      ]  =  _x64_rcg_jb_handler,
	[OP_3AC_JBE     ]  =  _x64_rcg_jbe_handler,

	[OP_3AC_SAVE    ]  =  _x64_rcg_save_handler,
	[OP_3AC_LOAD    ]  =  _x64_rcg_load_handler,

	[OP_3AC_RESAVE  ]  =  _x64_rcg_save_handler,
	[OP_3AC_RELOAD  ]  =  _x64_rcg_load_handler,

	[OP_3AC_NOP     ]  =  _x64_rcg_nop_handler,
	[OP_3AC_END     ]  =  _x64_rcg_end_handler,

	[OP_3AC_INC     ]  =  _x64_rcg_inc_handler,
	[OP_3AC_DEC     ]  =  _x64_rcg_dec_handler,

	[OP_3AC_PUSH_RETS] =  _x64_rcg_push_rax_handler,
	[OP_3AC_POP_RETS]  =  _x64_rcg_pop_rax_handler,

	[OP_3AC_MEMSET  ]  =  _x64_rcg_memset_handler,

	[OP_3AC_ASSIGN_DEREFERENCE    ]  =  _x64_rcg_assign_dereference_handler,
	[OP_3AC_ASSIGN_ARRAY_INDEX    ]  =  _x64_rcg_assign_array_index_handler,
	[OP_3AC_ASSIGN_POINTER        ]  =  _x64_rcg_assign_pointer_handler,

	[OP_3AC_ADDRESS_OF_ARRAY_INDEX]  =  _x64_rcg_address_of_array_index_handler,
	[OP_3AC_ADDRESS_OF_POINTER    ]  =  _x64_rcg_address_of_pointer_handler,
};

x64_rcg_handler_pt  x64_find_rcg_handler(const int op_type)
{
	if (op_type < 0 || op_type >= N_3AC_OPS)
		return NULL;

	return x64_rcg_handlers[op_type];
}
