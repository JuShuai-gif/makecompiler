#include"3ac.h"

typedef struct dag_operator_s {
	int type;
	int associativity;

	int (*func)(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes);

} dag_operator_t;

static int _3ac_code_N(list_t* h, int op_type, dag_node_t* d, dag_node_t** nodes, int nb_nodes)
{
	_3ac_operator_t* _3ac_op = _3ac_find_operator(op_type);
	if (!_3ac_op) {
		loge("\n");
		return -1;
	}

	vector_t* srcs = vector_alloc();

	int i;
	for (i = 0; i < nb_nodes; i++) {
		_3ac_operand_t* src = _3ac_operand_alloc();
		src->dag_node = nodes[i];
		vector_add(srcs, src);
	}

	_3ac_code_t* c = _3ac_code_alloc();
	c->op	= _3ac_op;
	c->srcs	= srcs;

	if (d) {
		_3ac_operand_t* dst = _3ac_operand_alloc();

		dst->dag_node = d;

		c->dsts = vector_alloc();

		if (vector_add(c->dsts, dst) < 0)
			return -ENOMEM;
	}

	list_add_tail(h, &c->list);
	return 0;
}

static int _3ac_code_3(list_t* h, int op_type, dag_node_t* d, dag_node_t* n0, dag_node_t* n1)
{
	_3ac_operator_t* _3ac_op = _3ac_find_operator(op_type);
	if (!_3ac_op) {
		loge("\n");
		return -1;
	}

	_3ac_operand_t* src0	= _3ac_operand_alloc();
	_3ac_operand_t* src1	= _3ac_operand_alloc();

	src0->dag_node	= n0;
	src1->dag_node	= n1;

	vector_t* srcs = vector_alloc();
	vector_add(srcs, src0);
	vector_add(srcs, src1);

	_3ac_code_t* c = _3ac_code_alloc();
	c->op	= _3ac_op;
	c->srcs	= srcs;

	if (d) {
		_3ac_operand_t* dst = _3ac_operand_alloc();

		dst->dag_node = d;

		c->dsts = vector_alloc();

		if (vector_add(c->dsts, dst) < 0)
			return -ENOMEM;
	}

	list_add_tail(h, &c->list);
	return 0;
}

static int _3ac_code_2(list_t* h, int op_type, dag_node_t* d, dag_node_t* n0)
{
	_3ac_operator_t* _3ac_op = _3ac_find_operator(op_type);
	if (!_3ac_op) {
		loge("\n");
		return -1;
	}

	_3ac_operand_t* src0	= _3ac_operand_alloc();

	src0->dag_node	= n0;

	vector_t* srcs = vector_alloc();
	vector_add(srcs, src0);

	_3ac_code_t* c = _3ac_code_alloc();
	c->op	= _3ac_op;
	c->srcs	= srcs;

	if (d) {
		_3ac_operand_t* dst = _3ac_operand_alloc();

		dst->dag_node = d;

		c->dsts = vector_alloc();

		if (vector_add(c->dsts, dst) < 0)
			return -ENOMEM;
	}

	list_add_tail(h, &c->list);
	return 0;
}

static int _3ac_code_dst(list_t* h, int op_type, dag_node_t* d)
{
	_3ac_operator_t* _3ac_op = _3ac_find_operator(op_type);
	if (!_3ac_op) {
		loge("\n");
		return -1;
	}

	_3ac_code_t* c = _3ac_code_alloc();
	c->op = _3ac_op;

	if (d) {
		_3ac_operand_t* dst = _3ac_operand_alloc();

		dst->dag_node = d;

		c->dsts = vector_alloc();

		if (vector_add(c->dsts, dst) < 0)
			return -ENOMEM;
	}

	list_add_tail(h, &c->list);
	return 0;
}

static int _3ac_code_1(list_t* h, int op_type, dag_node_t* n0)
{
	_3ac_operator_t* _3ac_op = _3ac_find_operator(op_type);
	if (!_3ac_op) {
		loge("\n");
		return -1;
	}

	_3ac_operand_t* src0	= _3ac_operand_alloc();

	src0->dag_node = n0;

	vector_t* srcs = vector_alloc();
	vector_add(srcs, src0);

	_3ac_code_t* code = _3ac_code_alloc();
	code->op	= _3ac_op;
	code->srcs	= srcs;
	list_add_tail(h, &code->list);
	return 0;
}

static int _dag_op_array_index(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(3 == nb_nodes);
	return _3ac_code_N(h, OP_ARRAY_INDEX, parent, nodes, nb_nodes);
}

static int _dag_op_pointer(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(2 == nb_nodes);
	return _3ac_code_N(h, OP_POINTER, parent, nodes, nb_nodes);
}

static int _dag_op_neg(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_2(h, OP_NEG, parent, nodes[0]);
}

static int _dag_op_bit_not(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_2(h, OP_BIT_NOT, parent, nodes[0]);
}

static int _dag_op_address_of(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	switch (nb_nodes) {
		case 1:
			return _3ac_code_2(h, OP_ADDRESS_OF, parent, nodes[0]);
			break;

		case 2:
			return _3ac_code_N(h, OP_3AC_ADDRESS_OF_POINTER, parent, nodes, nb_nodes);
			break;

		case 3:
			return _3ac_code_N(h, OP_3AC_ADDRESS_OF_ARRAY_INDEX, parent, nodes, nb_nodes);
			break;
		default:
			break;
	};

	loge("\n");
	return -1;
}

static int _dag_op_dereference(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);

	return _3ac_code_2(h, OP_DEREFERENCE, parent, nodes[0]);
}

static int _dag_op_type_cast(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_2(h, OP_TYPE_CAST, parent, nodes[0]);
}

static int _dag_op_logic_not(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);

	return _3ac_code_2(h, OP_LOGIC_NOT, parent, nodes[0]);
}

static int _dag_op_inc(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_1(h, OP_3AC_INC, nodes[0]);
}

static int _dag_op_dec(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_1(h, OP_3AC_DEC, nodes[0]);
}

#define DAG_BINARY(name, op) \
static int _dag_op_##name(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	assert(2 == nb_nodes); \
	return _3ac_code_3(h, OP_##op, parent, nodes[0], nodes[1]); \
}
DAG_BINARY(add, ADD)
DAG_BINARY(sub, SUB)

DAG_BINARY(shl, SHL)
DAG_BINARY(shr, SHR)

DAG_BINARY(and, BIT_AND)
DAG_BINARY(or,  BIT_OR)

DAG_BINARY(mul, MUL)
DAG_BINARY(div, DIV)
DAG_BINARY(mod, MOD)

static int _dag_op_assign(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(2 == nb_nodes);

	_3ac_operand_t*  dst;
	_3ac_code_t*     c;
	list_t*         l;

	if (!parent->direct)
		return _3ac_code_2(h, OP_ASSIGN, nodes[0], nodes[1]);

	if (!nodes[1]->direct) {
		nodes [1]->direct = nodes[0];

		l = list_tail(h);
		c = list_data(l, _3ac_code_t, list);

		assert(1 == c->dsts->size);
		dst = c->dsts->data[0];

		dst->node     = nodes[0]->node;
		dst->dag_node = nodes[0];
		return 0;
	}

	return _3ac_code_2(h, OP_ASSIGN, nodes[0], nodes[1]->direct);
}

#define DAG_BINARY_ASSIGN(name, op) \
static int _dag_op_##name(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	assert(2 == nb_nodes); \
	return _3ac_code_2(h, OP_##op, nodes[0], nodes[1]); \
}
DAG_BINARY_ASSIGN(add_assign, ADD_ASSIGN)
DAG_BINARY_ASSIGN(sub_assign, SUB_ASSIGN)

DAG_BINARY_ASSIGN(mul_assign, MUL_ASSIGN)
DAG_BINARY_ASSIGN(div_assign, DIV_ASSIGN)
DAG_BINARY_ASSIGN(mod_assign, MOD_ASSIGN)

DAG_BINARY_ASSIGN(shl_assign, SHL_ASSIGN)
DAG_BINARY_ASSIGN(shr_assign, SHR_ASSIGN)

DAG_BINARY_ASSIGN(and_assign, AND_ASSIGN)
DAG_BINARY_ASSIGN(or_assign,  OR_ASSIGN)

#define DAG_ASSIGN_DEREFERENCE(name, op) \
static int _dag_op_##name##_dereference(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	return _3ac_code_N(h, OP_3AC_##op##_DEREFERENCE, NULL, nodes, nb_nodes); \
}
DAG_ASSIGN_DEREFERENCE(assign,     ASSIGN);

#define DAG_ASSIGN_ARRAY_INDEX(name, op) \
static int _dag_op_##name##_array_index(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	assert(4 == nb_nodes); \
	return _3ac_code_N(h, OP_3AC_##op##_ARRAY_INDEX, NULL, nodes, nb_nodes); \
}
DAG_ASSIGN_ARRAY_INDEX(assign,     ASSIGN);

#define DAG_ASSIGN_POINTER(name, op) \
static int _dag_op_##name##_pointer(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	return _3ac_code_N(h, OP_3AC_##op##_POINTER, NULL, nodes, nb_nodes); \
}
DAG_ASSIGN_POINTER(assign,     ASSIGN);

static int _dag_op_return(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	return _3ac_code_N(h, OP_RETURN, NULL, nodes, nb_nodes);
}

static int _dag_op_vla_alloc(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(4 == nb_nodes);
	return _3ac_code_N(h, OP_VLA_ALLOC, nodes[0], nodes + 1, 3);
}

static int _dag_op_cmp(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(2 == nb_nodes);
	return _3ac_code_3(h, OP_3AC_CMP, NULL, nodes[0], nodes[1]);
}

static int _dag_op_teq(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes)
{
	assert(1 == nb_nodes);
	return _3ac_code_1(h, OP_3AC_TEQ, nodes[0]);
}

#define OP_SETCC(name, op_type) \
static int _dag_op_##name(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{ \
	assert(1 == nb_nodes); \
	return _3ac_code_dst(h, op_type, nodes[0]); \
}
OP_SETCC(setz,  OP_3AC_SETZ);
OP_SETCC(setnz, OP_3AC_SETNZ);
OP_SETCC(setlt, OP_3AC_SETLT);
OP_SETCC(setle, OP_3AC_SETLE);
OP_SETCC(setgt, OP_3AC_SETGT);
OP_SETCC(setge, OP_3AC_SETGE);

#define OP_CMP(name, operator, op_type) \
static int _dag_op_##name(list_t* h, dag_node_t* parent, dag_node_t** nodes, int nb_nodes) \
{\
	assert(2 == nb_nodes);\
	return _3ac_code_3(h, op_type, parent, nodes[0], nodes[1]); \
}

OP_CMP(eq, ==, OP_EQ)
OP_CMP(gt, >, OP_GT)
OP_CMP(lt, <, OP_LT)

dag_operator_t	dag_operators[] =
{
	{OP_ARRAY_INDEX,    OP_ASSOCIATIVITY_LEFT, _dag_op_array_index},
	{OP_POINTER,        OP_ASSOCIATIVITY_LEFT, _dag_op_pointer},

	{OP_BIT_NOT,        OP_ASSOCIATIVITY_LEFT, _dag_op_bit_not},
	{OP_LOGIC_NOT, 	    OP_ASSOCIATIVITY_RIGHT, _dag_op_logic_not},
	{OP_NEG,            OP_ASSOCIATIVITY_RIGHT, _dag_op_neg},

	{OP_3AC_INC,        OP_ASSOCIATIVITY_RIGHT, _dag_op_inc},
	{OP_3AC_DEC,        OP_ASSOCIATIVITY_RIGHT, _dag_op_dec},

	{OP_ADDRESS_OF,     OP_ASSOCIATIVITY_RIGHT, _dag_op_address_of},
	{OP_DEREFERENCE,    OP_ASSOCIATIVITY_RIGHT, _dag_op_dereference},
	{OP_TYPE_CAST,      OP_ASSOCIATIVITY_RIGHT, _dag_op_type_cast},

	{OP_MUL,            OP_ASSOCIATIVITY_LEFT, _dag_op_mul},
	{OP_DIV,            OP_ASSOCIATIVITY_LEFT, _dag_op_div},
	{OP_MOD,            OP_ASSOCIATIVITY_LEFT, _dag_op_mod},

	{OP_ADD,            OP_ASSOCIATIVITY_LEFT, _dag_op_add},
	{OP_SUB,            OP_ASSOCIATIVITY_LEFT, _dag_op_sub},

	{OP_SHL,            OP_ASSOCIATIVITY_LEFT, _dag_op_shl},
	{OP_SHR,            OP_ASSOCIATIVITY_LEFT, _dag_op_shr},

	{OP_BIT_AND,        OP_ASSOCIATIVITY_LEFT, _dag_op_and},
	{OP_BIT_OR,         OP_ASSOCIATIVITY_LEFT, _dag_op_or},

	{OP_EQ,             OP_ASSOCIATIVITY_LEFT, _dag_op_eq},
	{OP_GT,             OP_ASSOCIATIVITY_LEFT, _dag_op_gt},
	{OP_LT,             OP_ASSOCIATIVITY_LEFT, _dag_op_lt},

	{OP_VLA_ALLOC,      OP_ASSOCIATIVITY_LEFT, _dag_op_vla_alloc},
	{OP_RETURN,         OP_ASSOCIATIVITY_LEFT, _dag_op_return},
	{OP_3AC_CMP,        OP_ASSOCIATIVITY_LEFT, _dag_op_cmp},
	{OP_3AC_TEQ,        OP_ASSOCIATIVITY_LEFT, _dag_op_teq},

	{OP_3AC_SETZ,       OP_ASSOCIATIVITY_LEFT, _dag_op_setz},
	{OP_3AC_SETNZ,      OP_ASSOCIATIVITY_LEFT, _dag_op_setnz},
	{OP_3AC_SETLT,      OP_ASSOCIATIVITY_LEFT, _dag_op_setlt},
	{OP_3AC_SETLE,      OP_ASSOCIATIVITY_LEFT, _dag_op_setle},
	{OP_3AC_SETGT,      OP_ASSOCIATIVITY_LEFT, _dag_op_setgt},
	{OP_3AC_SETGE,      OP_ASSOCIATIVITY_LEFT, _dag_op_setge},

	{OP_ASSIGN,         OP_ASSOCIATIVITY_RIGHT, _dag_op_assign},
	{OP_ADD_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_add_assign},
	{OP_SUB_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_sub_assign},

	{OP_MUL_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_mul_assign},
	{OP_DIV_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_div_assign},
	{OP_MOD_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_mod_assign},

	{OP_SHL_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_shl_assign},
	{OP_SHR_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_shr_assign},

	{OP_AND_ASSIGN,     OP_ASSOCIATIVITY_RIGHT, _dag_op_and_assign},
	{OP_OR_ASSIGN,      OP_ASSOCIATIVITY_RIGHT, _dag_op_or_assign},

	{OP_3AC_ASSIGN_ARRAY_INDEX,        OP_ASSOCIATIVITY_RIGHT, _dag_op_assign_array_index},
	{OP_3AC_ASSIGN_POINTER,            OP_ASSOCIATIVITY_RIGHT, _dag_op_assign_pointer},
	{OP_3AC_ASSIGN_DEREFERENCE,        OP_ASSOCIATIVITY_RIGHT, _dag_op_assign_dereference},
};

dag_operator_t* dag_operator_find(int type)
{
	int i;
	for (i = 0; i < sizeof(dag_operators) / sizeof(dag_operators[0]); i++) {
		if (dag_operators[i].type == type)
			return &(dag_operators[i]);
	}
	return NULL;
}

int	dag_expr_calculate(list_t* h, dag_node_t* node)
{
	if (!node)
		return 0;

	if (!node->childs || 0 == node->childs->size) {

		variable_t* v = node->var;
#if 0
		if (v) {
			if (v->w)
				logw("node: %p, v_%d_%d/%s\n", node, v->w->line, v->w->pos, v->w->text->data);
			else
				logw("node: %p, v_%#lx\n", node, 0xffff & (uintptr_t)v);
		} else
			logw("node: %p\n", node);
#endif
		//assert(type_is_var(node->type));
		return 0;
	}

	assert(type_is_operator(node->type));
	assert(node->childs->size > 0);
#if 1
	if (node->done)
		return 0;
	node->done = 1;
#endif
	dag_operator_t* op = dag_operator_find(node->type);
	if (!op) {
		loge("node->type: %d\n", node->type);
		if (node->var && node->var->w)
			loge("node->var: %s\n", node->var->w->text->data);
		return -1;
	}

	if (OP_ASSOCIATIVITY_LEFT == op->associativity) {
		// left associativity
		int i;
		for (i = 0; i < node->childs->size; i++) {
			if (dag_expr_calculate(h, node->childs->data[i]) < 0) {
				loge("\n");
				return -1;
			}
		}

		if (op->func(h, node, (dag_node_t**)node->childs->data, node->childs->size) < 0) {
			loge("\n");
			return -1;
		}
		return 0;
	} else {
		// right associativity
		int i;
		for (i = node->childs->size - 1; i >= 0; i--) {
			if (dag_expr_calculate(h, node->childs->data[i]) < 0) {
				loge("\n");
				return -1;
			}
		}

		if (op->func(h, node, (dag_node_t**)node->childs->data, node->childs->size) < 0) {
			loge("\n");
			return -1;
		}
		return 0;
	}

	loge("\n");
	return -1;
}
