#include"ast.h"
#include"operator_handler.h"
#include"3ac.h"

typedef struct {
	vector_t*	_breaks;
	vector_t*	_continues;
	vector_t*	_gotos;
	vector_t*	_labels;
	vector_t*	_ends;
} branch_ops_t;

typedef struct {
	branch_ops_t*	branch_ops;
	list_t*			_3ac_list_head;

} handler_data_t;

branch_ops_t* branch_ops_alloc()
{
	branch_ops_t* branch_ops = calloc(1, sizeof(branch_ops_t));

	if (branch_ops) {
		branch_ops->_breaks		= vector_alloc();
		branch_ops->_continues	= vector_alloc();
		branch_ops->_gotos		= vector_alloc();
		branch_ops->_labels     = vector_alloc();
		branch_ops->_ends       = vector_alloc();
	}

	return branch_ops;
}

void branch_ops_free(branch_ops_t* branch_ops)
{
	if (branch_ops) {
		vector_free(branch_ops->_breaks);
		vector_free(branch_ops->_continues);
		vector_free(branch_ops->_gotos);
		vector_free(branch_ops->_labels);
		vector_free(branch_ops->_ends);

		free(branch_ops);
		branch_ops = NULL;
	}
}

static int _expr_calculate_internal(ast_t* ast, node_t* node, void* data)
{
	if (!node)
		return 0;

	handler_data_t*     d = data;
	operator_handler_pt h;

	int i;

	if (0 == node->nb_nodes) {
		if (type_is_var(node->type) && node->var->w)
			logd("node->var->w->text->data: %s\n", node->var->w->text->data);

		assert(type_is_var(node->type) || LABEL == node->type || node->split_flag);
		return 0;
	}

	assert(type_is_operator(node->type));
	assert(node->nb_nodes > 0);

	if (!node->op) {
		loge("node->type: %d\n", node->type);
		return -1;
	}

	if (node->_3ac_done)
		return 0;
	node->_3ac_done = 1;

	if (OP_ASSOCIATIVITY_LEFT == node->op->associativity) {

		if (OP_LOGIC_AND != node->op->type && OP_LOGIC_OR != node->op->type) {

			for (i = 0; i < node->nb_nodes; i++) {
				if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
					loge("\n");
					return -1;
				}
			}
		}

		h = find_3ac_operator_handler(node->op->type);
		if (!h) {
			loge("\n");
			return -1;
		}

		return h(ast, node->nodes, node->nb_nodes, d);

	} else {
		if (!type_is_assign(node->op->type) && OP_ADDRESS_OF != node->op->type && OP_CREATE != node->op->type) {

			for (i = node->nb_nodes - 1; i >= 0; i--) {
				if (_expr_calculate_internal(ast, node->nodes[i], d) < 0) {
					loge("\n");
					return -1;
				}
			}
		}

		h = find_3ac_operator_handler(node->op->type);
		if (!h) {
			loge("op->type: %d, name: '%s'\n", node->op->type, node->op->name);
			return -1;
		}

		return h(ast, node->nodes, node->nb_nodes, d);
	}

	loge("\n");
	return -1;
}

static int _expr_calculate(ast_t* ast, expr_t* e, void* data)
{
	handler_data_t* d = data;

	assert(e);
	assert(e->nodes);

	node_t* root = e->nodes[0];

	if (type_is_var(root->type))
		return 0;

	if (_expr_calculate_internal(ast, root, d) < 0) {
		loge("\n");
		return -1;
	}

	return 0;
}

static int _3ac_code_NN(list_t* h, int op_type, node_t** dsts, int nb_dsts, node_t** srcs, int nb_srcs)
{
	_3ac_code_t* c = 3ac_code_NN(op_type, dsts, nb_dsts, srcs, nb_srcs);
	if (!c) {
		loge("\n");
		return -1;
	}

	list_add_tail(h, &c->list);
	return 0;
}

static int _3ac_code_N(list_t* h, int op_type, node_t* d, node_t** nodes, int nb_nodes)
{
	return _3ac_code_NN(h, op_type, &d, 1, nodes, nb_nodes);
}

static int _3ac_code_3(list_t* h, int op_type, node_t* d, node_t* n0, node_t* n1)
{
	node_t* srcs[2] = {n0, n1};

	return _3ac_code_NN(h, op_type, &d, 1, srcs, 2);
}

static int _3ac_code_2(list_t* h, int op_type, node_t* d, node_t* n0)
{
	return _3ac_code_NN(h, op_type, &d, 1, &n0, 1);
}

static int _3ac_code_1(list_t* h, int op_type, node_t* n0)
{
	return _3ac_code_NN(h, op_type, NULL, 0, &n0, 1);
}

static int _3ac_code_dst(list_t* h, int op_type, node_t* d)
{
	return _3ac_code_NN(h, op_type, &d, 1, NULL, 0);
}

static int _3ac_code_srcN(list_t* h, int op_type, node_t** nodes, int nb_nodes)
{
	return _3ac_code_NN(h, op_type, NULL, 0, nodes, nb_nodes);
}

static int _op_va_start(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;
	variable_t*     vptr;
	type_t*         tptr;
	node_t*         nptr;
	node_t*         parent = nodes[0]->parent;
	node_t*         srcs[3];

	tptr = block_find_type_type(ast->current_block, VAR_UINTPTR);
	vptr = VAR_ALLOC_BY_TYPE(NULL, tptr, 0, 0, NULL);
	if (!vptr)
		return -ENOMEM;
	vptr->data.u64 = 0;
	vptr->tmp_flag = 1;

	nptr = node_alloc(NULL, vptr->type, vptr);
	if (!nptr)
		return -ENOMEM;

	if (node_add_child(parent, nptr) < 0)
		return -ENOMEM;

	srcs[0] = parent->nodes[0];
	srcs[1] = parent->nodes[1];
	srcs[2] = nptr;

	return _3ac_code_srcN(d->_3ac_list_head, OP_VA_START, srcs, 3);
}

static int _op_va_end(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(1 == nb_nodes);

	handler_data_t* d = data;
	variable_t*     vptr;
	type_t*         tptr;
	node_t*         nptr;
	node_t*         parent = nodes[0]->parent;
	node_t*         srcs[2];

	tptr = block_find_type_type(ast->current_block, VAR_UINTPTR);
	vptr = VAR_ALLOC_BY_TYPE(NULL, tptr, 0, 0, NULL);
	if (!vptr)
		return -ENOMEM;
	vptr->data.u64   = 0;
	vptr->const_flag = 1;

	nptr = node_alloc(NULL, vptr->type, vptr);
	if (!nptr)
		return -ENOMEM;

	if (node_add_child(parent, nptr) < 0)
		return -ENOMEM;

	srcs[0] = parent->nodes[0];
	srcs[1] = nptr;

	return _3ac_code_srcN(d->_3ac_list_head, OP_VA_END, srcs, 2);
}

static int _op_va_arg(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;
	variable_t*     v;
	variable_t*     vptr;
	type_t*         tptr;
	node_t*         nptr;
	node_t*         parent = nodes[0]->parent;
	node_t*         srcs[3];

	v = _operand_get(parent);
	v->tmp_flag = 1;

	v = _operand_get(nodes[1]);
	v->const_flag = 1;

	tptr = block_find_type_type(ast->current_block, VAR_UINTPTR);
	vptr = VAR_ALLOC_BY_TYPE(NULL, tptr, 0, 0, NULL);
	if (!vptr)
		return -ENOMEM;

	vptr->data.u64   = 0;
	vptr->tmp_flag   = 1;
	vptr->extra_flag = 1;

	nptr = node_alloc(NULL, vptr->type, vptr);
	if (!nptr)
		return -ENOMEM;

	if (node_add_child(parent, nptr) < 0)
		return -ENOMEM;

	srcs[0] = parent->nodes[0];
	srcs[1] = parent->nodes[1];
	srcs[2] = nptr;

	return _3ac_code_NN(d->_3ac_list_head, OP_VA_ARG, &parent, 1, srcs, 3);
}

static int _op_pointer(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;
	variable_t*     v;

	node_t* parent = nodes[0]->parent;

	v = _operand_get(parent);
	v->tmp_flag = 1;

	return _3ac_code_3(d->_3ac_list_head, OP_POINTER, parent, nodes[0], nodes[1]);
}

static int _op_array_scale(ast_t* ast, node_t* parent, node_t** pscale)
{
	variable_t* v_member;
	variable_t* v_scale;
	type_t*     t_scale;
	node_t*     n_scale;

	v_member = _operand_get(parent);

	int size = variable_size(v_member);
	assert(size > 0);

	t_scale = block_find_type_type(ast->current_block, VAR_INTPTR);
	v_scale = VAR_ALLOC_BY_TYPE(NULL, t_scale, 0, 0, NULL);
	if (!v_scale)
		return -ENOMEM;
	v_scale->data.i = size;

	n_scale = node_alloc(NULL, v_scale->type, v_scale);
	if (!n_scale)
		return -ENOMEM;

	*pscale = n_scale;
	return 0;
}

static int _op_array_index(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d   = data;

	node_t*     parent  = nodes[0]->parent;
	node_t*     n_index = NULL;
	node_t*     n_scale = NULL;
	node_t*     srcs[3];
	variable_t* v;

	int ret = _expr_calculate_internal(ast, nodes[1], d);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	ret = _op_array_scale(ast, parent, &n_scale);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	n_index = nodes[1];
	while (OP_EXPR == n_index->type)
		n_index = n_index->nodes[0];

	srcs[0] = nodes[0];
	srcs[1] = n_index;
	srcs[2] = n_scale;

	v = _operand_get(parent);
	v->tmp_flag = 1;

	return _3ac_code_N(d->_3ac_list_head, OP_ARRAY_INDEX, parent, srcs, 3);
}

static int _op_block(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	if (0 == nb_nodes)
		return 0;

	operator_handler_pt h;
	operator_t*         op;
	handler_data_t*     d  = data;
	block_t*            b  = (block_t*)(nodes[0]->parent);
	block_t*            up = ast->current_block;
	node_t*             node;

	if (b->node._3ac_done)
		return 0;

	ast->current_block = b;

	int i;
	int j;

	for (i = 0; i < nb_nodes; i++) {

		node = nodes[i];
		op   = node->op;

		if (!op) {
			op = find_base_operator_by_type(node->type);
			if (!op) {
				loge("\n");
				return -1;
			}
		}

		h = find_3ac_operator_handler(op->type);
		if (!h) {
			loge("\n");
			return -1;
		}

		if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
			ast->current_block = up;
			return -1;
		}

		// for goto
		if (LABEL == node->type) {

			label_t*       l    = node->label;
			list_t*        tail = list_tail(d->_3ac_list_head);
			_3ac_code_t*	   end  = list_data(tail, _3ac_code_t, list);
			_3ac_code_t*    c;
			_3ac_operand_t* dst;

			if (vector_add(d->branch_ops->_labels, end) < 0) {
				loge("\n");
				return -1;
			}
			end->label = l;

			int j;
			for (j = 0; j < d->branch_ops->_gotos->size; j++) {
				c  =        d->branch_ops->_gotos->data[j];

				if (!c)
					continue;

				assert(l->w);
				assert(c->label->w);

				if (!strcmp(l->w->text->data, c->label->w->text->data)) {
					dst       = c->dsts->data[0];
					dst->code = end;
				}
			}
		}
	}

	ast->current_block = up;
	return 0;
}

static int _op_return(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	handler_data_t* d = data;
	function_t*     f = (function_t*) ast->current_block;

	while (f && FUNCTION  != f->node.type)
		f = (function_t*) f->node.parent;

	if (!f) {
		loge("\n");
		return -1;
	}

	if (nb_nodes > f->rets->size) {
		loge("\n");
		return -1;
	}

	node_t* parent;
	node_t* srcs[4];
	expr_t* e;

	int i;
	for (i = 0; i < nb_nodes && i < 4; i++) {
		e  = nodes[i];

		if (_expr_calculate_internal(ast, e->nodes[0], d) < 0) {
			loge("\n");
			return -1;
		}

		srcs[i] = e->nodes[0];
	}

	if (i > 0) {
		if (_3ac_code_srcN(d->_3ac_list_head, OP_RETURN, srcs, i) < 0)
			return -1;
	}

	_3ac_code_t* end = _3ac_jmp_code(OP_GOTO, NULL, NULL);

	list_add_tail(d->_3ac_list_head, &end->list);

	return vector_add(d->branch_ops->_ends, end);
}

static int _op_break(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	handler_data_t* d  = data;
	node_t*         up = (node_t*)ast->current_block;

	while (up
			&& OP_WHILE  != up->type
			&& OP_DO     != up->type
			&& OP_FOR    != up->type
			&& OP_SWITCH != up->type)
		up = up->parent;

	if (!up) {
		loge("\n");
		return -1;
	}

	_3ac_code_t* jmp = _3ac_jmp_code(OP_GOTO, NULL, NULL);

	list_add_tail(d->_3ac_list_head, &jmp->list);

	return vector_add(d->branch_ops->_breaks, jmp);
}

static int _op_continue(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	handler_data_t* d  = data;
	node_t*         up = (node_t*)ast->current_block;

	while (up
			&& OP_WHILE  != up->type
			&& OP_DO     != up->type
			&& OP_FOR    != up->type
			&& OP_SWITCH != up->type)
		up = up->parent;

	if (!up) {
		loge("\n");
		return -1;
	}

	_3ac_code_t* jmp = _3ac_jmp_code(OP_GOTO, NULL, NULL);

	list_add_tail(d->_3ac_list_head, &jmp->list);

	vector_add(d->branch_ops->_continues, jmp);
	return 0;
}

static int _op_label(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	return 0;
}

static int _op_goto(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(1 == nb_nodes);

	handler_data_t* d  = data;
	node_t*         nl = nodes[0];
	label_t*        l  = nl->label;

	assert(LABEL == nl->type);
	assert(l->w);

	_3ac_operand_t* dst;
	_3ac_code_t*    c;
	_3ac_code_t*    jmp = _3ac_jmp_code(OP_GOTO, l, NULL);

	list_add_tail(d->_3ac_list_head, &jmp->list);

	int i;
	for (i = 0; i < d->branch_ops->_labels->size; i++) {
		c  =        d->branch_ops->_labels->data[i];

		if (!strcmp(l->w->text->data, c->label->w->text->data)) {
			dst = jmp->dsts->data[0];
			dst->code = c;
			break;
		}
	}

	vector_add(d->branch_ops->_gotos, jmp);
	return 0;
}

static int _op_cond(ast_t* ast, expr_t* e, handler_data_t* d)
{
	while (e && OP_EXPR == e->type)
		e = e->nodes[0];

	assert(e);

	if (_expr_calculate_internal(ast, e, d) < 0) {
		loge("\n");
		return -1;
	}

	int is_float   =  0;
	int is_default =  0;
	int jmp_op     = -1;

	if (e->nb_nodes > 0) {

		node_t*     node = e->nodes[0];
		variable_t* v    = _operand_get(node);

		if (variable_float(v))
			is_float = 1;
	}

	switch (e->type) {
		case OP_EQ:
			jmp_op = OP_3AC_JNZ;
			break;
		case OP_NE:
			jmp_op = OP_3AC_JZ;
			break;

		case OP_GT:
			if (!is_float)
				jmp_op = OP_3AC_JLE;
			else
				jmp_op = OP_3AC_JBE;
			break;

		case OP_GE:
			if (!is_float)
				jmp_op = OP_3AC_JLT;
			else
				jmp_op = OP_3AC_JB;
			break;

		case OP_LT:
			if (!is_float)
				jmp_op = OP_3AC_JGE;
			else
				jmp_op = OP_3AC_JAE;
			break;

		case OP_LE:
			if (!is_float)
				jmp_op = OP_3AC_JGT;
			else
				jmp_op = OP_3AC_JA;
			break;

		case OP_LOGIC_NOT:
			jmp_op = OP_3AC_JNZ;
			break;

		default:
			if (type_is_assign(e->type)) {

				e = e->nodes[0];

				while (e && OP_EXPR == e->type)
					e = e->nodes[0];

				assert(e);

				if (_expr_calculate_internal(ast, e, d) < 0) {
					loge("\n");
					return -1;
				}
			}

			if (_3ac_code_1(d->_3ac_list_head, OP_3AC_TEQ, e) < 0) {
				loge("\n");
				return -1;
			}

			jmp_op     = OP_3AC_JZ;
			is_default = 1;
			break;
	};

	if (!is_default) {

		list_t*     l    = list_tail(d->_3ac_list_head);
		_3ac_code_t* c    = list_data(l, _3ac_code_t, list);
		vector_t*   dsts = c->dsts;

		if (OP_LOGIC_NOT == e->type)
			c->op  = _3ac_find_operator(OP_3AC_TEQ);
		else
			c->op  = _3ac_find_operator(OP_3AC_CMP);
		c->dsts = NULL;

		vector_clear(dsts, ( void (*)(void*) ) _3ac_operand_free);
		vector_free (dsts);
		dsts = NULL;
	}

	return jmp_op;
}

static int _op_node(ast_t* ast, node_t* node, handler_data_t* d)
{
	operator_t* op = node->op;

	if (!op) {
		op = find_base_operator_by_type(node->type);
		if (!op) {
			loge("\n");
			return -1;
		}
	}

	operator_handler_pt	h = find_3ac_operator_handler(op->type);
	if (!h) {
		loge("\n");
		return -1;
	}

	if (h(ast, node->nodes, node->nb_nodes, d) < 0) {
		loge("\n");
		return -1;
	}
	return 0;
}

static int _op_if(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	if (2 != nb_nodes && 3 != nb_nodes) {
		loge("\n");
		return -1;
	}

	handler_data_t* d = data;

	expr_t* e      = nodes[0];
	node_t* parent = e->parent;

	int jmp_op = _op_cond(ast, e, d);
	if (jmp_op < 0) {
		loge("\n");
		return -1;
	}

	_3ac_operand_t* dst;
	_3ac_code_t*    jmp_else  = _3ac_jmp_code(jmp_op, NULL, NULL);
	_3ac_code_t*    jmp_endif = NULL;
	list_t*        l;

	list_add_tail(d->_3ac_list_head, &jmp_else->list);

	int i;
	for (i = 1; i < nb_nodes; i++) {
		node_t* node = nodes[i];

		if (_op_node(ast, node, d) < 0) {
			loge("\n");
			return -1;
		}

		if (1 == i) {
			if (3 == nb_nodes) {
				jmp_endif = _3ac_jmp_code(OP_GOTO, NULL, NULL);
				list_add_tail(d->_3ac_list_head, &jmp_endif->list);
			}

			l   = list_tail(d->_3ac_list_head);
			dst = jmp_else->dsts->data[0];
			dst->code = list_data(l, _3ac_code_t, list);
		}
	}

	int ret = vector_add(d->branch_ops->_breaks, jmp_else);
	if (ret < 0)
		return ret;

	if (jmp_endif) {
		l   = list_tail(d->_3ac_list_head);
		dst = jmp_endif->dsts->data[0];
		dst->code = list_data(l, _3ac_code_t, list);

		ret = vector_add(d->branch_ops->_breaks, jmp_endif);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int _op_end_loop(list_t* start_prev, list_t* continue_prev, _3ac_code_t* jmp_end, branch_ops_t* up_branch_ops, handler_data_t* d)
{
	// change 'while (cond) {} ' to
	// 'if (cond) {
	//    do {
    //    } while (cond);
    //  }'
	// for optimizer

	// copy cond expr
	_3ac_operand_t* dst;
	_3ac_code_t*    c;
	_3ac_code_t*    c2;

	list_t*        l;
	list_t*        l2;
	list_t*        cond_prev = list_tail(d->_3ac_list_head);

	for (l = list_next(start_prev); l != &jmp_end->list; l = list_next(l)) {
		c  = list_data(l, _3ac_code_t, list);

		c2 = _3ac_code_clone(c);
		if (!c2)
			return -ENOMEM;

		list_add_tail(d->_3ac_list_head, &c2->list);
	}

	for (l = list_next(cond_prev); l != list_sentinel(d->_3ac_list_head); l = list_next(l)) {
		c  = list_data(l, _3ac_code_t, list);

		if (!type_is_jmp(c->op->type))
			continue;

		for (l2 = list_next(cond_prev); l2 != list_sentinel(d->_3ac_list_head); l2 = list_next(l2)) {
			c2  = list_data(l2, _3ac_code_t, list);

			dst = c->dsts->data[0];

			if (dst->code == c2->origin) {
				dst->code =  c2;
				break;
			}
		}
		assert(l2 != list_sentinel(d->_3ac_list_head));

		if (vector_add(d->branch_ops->_breaks, c) < 0)
			return -1;
	}

	int jmp_op = -1;
	switch (jmp_end->op->type) {

		case OP_3AC_JNZ:
			jmp_op = OP_3AC_JZ;
			break;
		case OP_3AC_JZ:
			jmp_op = OP_3AC_JNZ;
			break;
		case OP_3AC_JLE:
			jmp_op = OP_3AC_JGT;
			break;
		case OP_3AC_JLT:
			jmp_op = OP_3AC_JGE;
			break;
		case OP_3AC_JGE:
			jmp_op = OP_3AC_JLT;
			break;
		case OP_3AC_JGT:
			jmp_op = OP_3AC_JLE;
			break;

		case OP_3AC_JA:
			jmp_op = OP_3AC_JBE;
			break;
		case OP_3AC_JAE:
			jmp_op = OP_3AC_JB;
			break;

		case OP_3AC_JB:
			jmp_op = OP_3AC_JAE;
			break;
		case OP_3AC_JBE:
			jmp_op = OP_3AC_JA;
			break;

		default:
			jmp_op = -1;
			break;
	};

	// add loop when true
	_3ac_code_t*	loop = _3ac_jmp_code(jmp_op, NULL, NULL);
	list_add_tail(d->_3ac_list_head, &loop->list);

	// should get the real start here,
	_3ac_code_t*	start = list_data(list_next(&jmp_end->list), _3ac_code_t, list);

	dst       = loop->dsts->data[0];
	dst->code = start;

	// set jmp destination for 'continue',
	// it's the 'real' dst & needs not to re-fill

	int i;
	for (i = 0; i < d->branch_ops->_continues->size; i++) {
		c  =        d->branch_ops->_continues->data[i];

		assert(c->dsts);

		dst = c->dsts->data[0];
		assert(!dst->code);

		/* 'continue' will goto 'while' and re-check the condition.

		   don't goto 'do' directly, because this will jmp the cond check, and may cause a dead loop.

		   if (cond) {
		       do {
			   }
			   while (cond_)
	       }
		*/

		if (continue_prev)
			dst->code = list_data(list_next(continue_prev), _3ac_code_t, list);
		else
			dst->code = list_data(list_next(cond_prev), _3ac_code_t, list);
	}

	// get the end, it's NOT the 'real' dst, but the prev of the 'real'
	_3ac_code_t*	end_prev  = list_data(list_tail(d->_3ac_list_head), _3ac_code_t, list);

	for (i = 0; i < d->branch_ops->_breaks->size; i++) {
		c  =        d->branch_ops->_breaks->data[i];

		assert(c->dsts);

		dst = c->dsts->data[0];

		if (!dst->code)
			dst->code = end_prev;
	}

	if (jmp_end) {
		dst       = jmp_end->dsts->data[0];
		dst->code = end_prev;
	}

	if (up_branch_ops) {
		for (i = 0; i < d->branch_ops->_breaks->size; i++) {
			c  =        d->branch_ops->_breaks->data[i];

			if (vector_add(up_branch_ops->_breaks, c) < 0)
				return -1;
		}

		if (jmp_end) {
			if (vector_add(up_branch_ops->_breaks, jmp_end) < 0)
				return -1;
		}

		for (i = 0; i < d->branch_ops->_gotos->size; i++) {
			c  =        d->branch_ops->_gotos->data[i];

			if (vector_add(up_branch_ops->_gotos, c) < 0)
				return -1;
		}

		for (i = 0; i < d->branch_ops->_ends->size; i++) {
			c  =        d->branch_ops->_ends->data[i];

			if (vector_add(up_branch_ops->_ends, c) < 0)
				return -1;
		}
	}

	return 0;
}

static int _op_do(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;
	expr_t*         e = nodes[1];

	assert(OP_EXPR == e->type);

	list_t* start_prev = list_tail(d->_3ac_list_head);

	int jmp_op = _op_cond(ast, e, d);
	if (jmp_op < 0) {
		loge("\n");
		return -1;
	}

	list_t*     l;
	_3ac_code_t* c;
	_3ac_code_t* jmp_end = _3ac_jmp_code(jmp_op, NULL, NULL);

	list_add_tail(d->_3ac_list_head, &jmp_end->list);

	branch_ops_t* local_branch_ops = branch_ops_alloc();
	branch_ops_t* up_branch_ops    = d->branch_ops;
	d->branch_ops                      = local_branch_ops;

	if (_op_node(ast, nodes[0], d) < 0) {
		loge("\n");
		return -1;
	}

	if (_op_end_loop(start_prev, NULL, jmp_end, up_branch_ops, d) < 0) {
		loge("\n");
		return -1;
	}

	d->branch_ops    = up_branch_ops;
	branch_ops_free(local_branch_ops);
	local_branch_ops = NULL;

	// delete 'cond check' at 'start of loop'
	vector_del(d->branch_ops->_breaks, jmp_end);

	for (l = list_next(start_prev); l != &jmp_end->list; ) {
		c  = list_data(l, _3ac_code_t, list);
		l  = list_next(l);

		list_del(&c->list);
		_3ac_code_free(c);
		c = NULL;
	}

	list_del(&jmp_end->list);
	_3ac_code_free(jmp_end);
	jmp_end = NULL;

	return 0;
}

static int _op_while(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes || 1 == nb_nodes);

	handler_data_t* d = data;
	expr_t*         e = nodes[0];

	assert(OP_EXPR == e->type);

	// we don't know the real start of the while loop here,
	// we only know it's the next of 'start_prev'
	list_t* start_prev = list_tail(d->_3ac_list_head);

	int jmp_op = _op_cond(ast, e, d);
	if (jmp_op < 0) {
		loge("\n");
		return -1;
	}

	_3ac_code_t* jmp_end = _3ac_jmp_code(jmp_op, NULL, NULL);
	list_add_tail(d->_3ac_list_head, &jmp_end->list);

	branch_ops_t* local_branch_ops = branch_ops_alloc();
	branch_ops_t* up_branch_ops    = d->branch_ops;
	d->branch_ops                      = local_branch_ops;

	// while body
	if (2 == nb_nodes) {
		if (_op_node(ast, nodes[1], d) < 0) {
			loge("\n");
			return -1;
		}
	}

	if (_op_end_loop(start_prev, NULL, jmp_end, up_branch_ops, d) < 0) {
		loge("\n");
		return -1;
	}

	d->branch_ops    = up_branch_ops;
	branch_ops_free(local_branch_ops);
	local_branch_ops = NULL;
	return 0;
}

static int _op_vla_alloc(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(4 == nb_nodes);

	handler_data_t* d = data;
	variable_t*     v;
	type_t*         t;
	node_t*         parent = nodes[0]->parent;
	node_t*         zero;

	t = block_find_type_type(ast->current_block, VAR_INT);
	v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 0, NULL);
	if (!v)
		return -ENOMEM;
	v->data.u64 = 0;
	v->const_literal_flag = 1;

	zero = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!zero)
		return -ENOMEM;

	int ret = node_add_child(parent, zero);
	if (ret < 0) {
		node_free(zero);
		return ret;
	}

	_3ac_operand_t*  dst;
	_3ac_code_t*     cmp;
	_3ac_code_t*     jgt;
	_3ac_code_t*     core;
	node_t*         srcs[] = {nodes[1], zero};

	cmp = _3ac_code_NN(OP_3AC_CMP, NULL, 0, srcs, 2);
	if (!cmp)
		return -ENOMEM;

	jgt = _3ac_jmp_code(OP_3AC_JGT, NULL, NULL);
	if (!jgt) {
		_3ac_code_free(cmp);
		return -ENOMEM;
	}

	core = _3ac_code_NN(OP_3AC_DUMP, NULL, 0, nodes + 1, 3);
	if (!core) {
		_3ac_code_free(jgt);
		_3ac_code_free(cmp);
		return -ENOMEM;
	}

	list_add_tail(d->_3ac_list_head, &cmp->list);
	list_add_tail(d->_3ac_list_head, &jgt->list);
	list_add_tail(d->_3ac_list_head, &core->list);

	dst = jgt->dsts->data[0];
	dst->code = core;

	ret = vector_add(d->branch_ops->_breaks, jgt);
	if (ret < 0)
		return ret;

	return _3ac_code_N(d->_3ac_list_head, OP_VLA_ALLOC, nodes[0], nodes + 1, 3);
}

static int _op_default(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	return 0;
}

static int _op_case(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	return 0;
}

static int _op_switch(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;
	expr_t*         e = nodes[0];
	node_t*         b = nodes[1];

	assert(OP_EXPR == e->type);

	while (e && OP_EXPR == e->type)
		e = e->nodes[0];

	if (_expr_calculate_internal(ast, e, d) < 0) {
		loge("\n");
		return -1;
	}

	branch_ops_t* up_branch_ops = d->branch_ops;
	block_t*      up            = ast->current_block;

	d->branch_ops      = branch_ops_alloc();
	ast->current_block = (block_t*)b;

	_3ac_operand_t* dst;
	_3ac_code_t*    cmp;
	_3ac_code_t*    end;
	_3ac_code_t*    c;
	_3ac_code_t*    jnot  = NULL;
	_3ac_code_t*    jnext = NULL;

	node_t*        child;
	expr_t*        e2;
	list_t*        l;

	int i;
	for (i = 0; i < b->nb_nodes; i++) {
		child     = b->nodes[i];

		if (OP_CASE == child->type || OP_DEFAULT == child->type) {

			if (jnot) {
				jnext = _3ac_jmp_code(OP_GOTO, NULL, NULL);

				list_add_tail(d->_3ac_list_head, &jnext->list);
				vector_add(up_branch_ops->_breaks, jnext);

				dst       = jnot->dsts->data[0];
				dst->code = jnext;
				jnot      = NULL;
			}

			if (OP_CASE == child->type) {

				e2 = child->nodes[0];
				assert(OP_EXPR == e2->type);

				while (e2 && OP_EXPR == e2->type)
					e2 = e2->nodes[0];

				if (OP_CALL == e2->type) {
					assert(3    == e2->nb_nodes);
					e2->nodes[1]->_3ac_done = 1;
				}

				if (_expr_calculate_internal(ast, e2, d) < 0) {
					loge("\n");
					return -1;
				}

				node_t* srcs[2] = {e, e2};

				if (OP_CALL != e2->type)
					cmp = _3ac_code_NN(OP_3AC_CMP, NULL, 0, srcs, 2);
				else
					cmp = _3ac_code_NN(OP_3AC_TEQ, NULL, 0, &e2, 1);

				jnot = _3ac_jmp_code(OP_3AC_JNZ, NULL, NULL);

				list_add_tail(d->_3ac_list_head, &cmp->list);
				list_add_tail(d->_3ac_list_head, &jnot->list);

				vector_add(up_branch_ops->_breaks, jnot);
			}

			if (jnext) {
				l         = list_tail(d->_3ac_list_head);
				dst       = jnext->dsts->data[0];
				dst->code = list_data(l, _3ac_code_t, list);
				jnext     = NULL;
			}

		} else {
			if (_op_node(ast, child, d) < 0) {
				loge("\n");
				return -1;
			}
		}
	}

	l   = list_tail(d->_3ac_list_head);
	end = list_data(l, _3ac_code_t, list);

	for (i = 0; i < d->branch_ops->_breaks->size; i++) {
		c  =        d->branch_ops->_breaks->data[i];

		dst = c->dsts->data[0];
		if (!dst->code)
			dst->code = end;

		if (vector_add(up_branch_ops->_breaks, c) < 0)
			return -1;
	}

	for (i = 0; i < d->branch_ops->_gotos->size; i++) {
		c  =        d->branch_ops->_gotos->data[i];

		if (vector_add(up_branch_ops->_gotos, c) < 0)
			return -1;
	}

	for (i = 0; i < d->branch_ops->_ends->size; i++) {
		c         = d->branch_ops->_ends->data[i];

		if (vector_add(up_branch_ops->_ends, c) < 0)
			return -1;
	}

	branch_ops_free(d->branch_ops);
	d->branch_ops      = up_branch_ops;
	ast->current_block = up;
	return 0;
}

static int _op_for(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(4 == nb_nodes);

	handler_data_t* d = data;

	if (nodes[0]) {
		if (_op_node(ast, nodes[0], d) < 0) {
			loge("\n");
			return -1;
		}
	}

	list_t*     start_prev = list_tail(d->_3ac_list_head);
	_3ac_code_t* jmp_end    = NULL;

	if (nodes[1]) {
		assert(OP_EXPR == nodes[1]->type);

		int jmp_op = _op_cond(ast, nodes[1], d);
		if (jmp_op < 0) {
			loge("\n");
			return -1;
		}

		jmp_end = _3ac_jmp_code(jmp_op, NULL, NULL);
		list_add_tail(d->_3ac_list_head, &jmp_end->list);
	}

	branch_ops_t* local_branch_ops = branch_ops_alloc();
	branch_ops_t* up_branch_ops    = d->branch_ops;
	d->branch_ops                      = local_branch_ops;

	if (nodes[3]) {
		if (_op_node(ast, nodes[3], d) < 0) {
			loge("\n");
			return -1;
		}
	}

	list_t* continue_prev = list_tail(d->_3ac_list_head);

	if (nodes[2]) {
		if (_op_node(ast, nodes[2], d) < 0) {
			loge("\n");
			return -1;
		}
	}

	if (_op_end_loop(start_prev, continue_prev, jmp_end, up_branch_ops, d) < 0) {
		loge("\n");
		return -1;
	}

	d->branch_ops    = up_branch_ops;
	branch_ops_free(local_branch_ops);
	local_branch_ops = NULL;
	return 0;
}

static int __op_call(ast_t* ast, function_t* f, void* data)
{
	logd("f: %p, f->node->w: %s\n", f, f->node.w->text->data);

	handler_data_t* d = data;

	block_t*       up               = ast->current_block;
	branch_ops_t*  local_branch_ops = branch_ops_alloc();
	branch_ops_t*  tmp_branch_ops   = d->branch_ops;

	ast->current_block = (block_t*)f;
	d->branch_ops      = local_branch_ops; // use local_branch_ops, because branch code should NOT jmp over the function block

	if (_op_block(ast, f->node.nodes, f->node.nb_nodes, d) < 0)
		return -1;

	list_t*        next;
	_3ac_operand_t* dst;
	_3ac_code_t*    c;
	_3ac_code_t*    end = _3ac_code_NN(OP_3AC_END, NULL, 0, NULL, 0);

	list_add_tail(d->_3ac_list_head, &end->list);

	// re-fill 'break'

	int i;
	for (i = 0; i < local_branch_ops->_breaks->size; i++) {
		c  =        local_branch_ops->_breaks->data[i];

		dst = c->dsts->data[0];

		if (dst->code) {
			next      = list_next(&dst->code->list);
			dst->code = list_data(next, _3ac_code_t, list);
		} else {
			loge("'break' has a bug!\n");
			return -1;
		}
	}

	// re-fill 'goto'
	for (i = 0; i < local_branch_ops->_gotos->size; i++) {
		c  =        local_branch_ops->_gotos->data[i];

		dst = c->dsts->data[0];

		if (dst->code) {
			next      = list_next(&dst->code->list);
			dst->code = list_data(next, _3ac_code_t, list);
		} else {
			loge("all 'goto' should get its label in this function\n");
			return -1;
		}
	}

	// re-fill 'end'
	for (i = 0; i < local_branch_ops->_ends->size; i++) {
		c  =        local_branch_ops->_ends->data[i];

		dst = c->dsts->data[0];

		assert(!dst->code);

		if (&c->list == list_prev(&end->list))
			c->op = _3ac_find_operator(OP_3AC_NOP);
		else
			dst->code = end;
	}

	branch_ops_free(local_branch_ops);
	local_branch_ops   = NULL;

	d->branch_ops      = tmp_branch_ops;
	ast->current_block = up;
	return 0;
}

int function_to_3ac(ast_t* ast, function_t* f, list_t* _3ac_list_head)
{
	handler_data_t d = {0};
	d._3ac_list_head  	 = _3ac_list_head;

	int ret = __op_call(ast, f, &d);

	if (ret < 0) {
		loge("\n");
		return -1;
	}

	return 0;
}

static int _op_create(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(nb_nodes > 0);

	handler_data_t* d      = data;
	node_t*         parent = nodes[0]->parent;

	_3ac_operand_t*  dst;
	_3ac_code_t*     jz;
	_3ac_code_t*     jmp;

	variable_t*     v;
	type_t*         t;
	node_t*         node;
	node_t*         nthis;
	node_t*         nerr;
	list_t*         l;

	int ret;
	int i;

	nthis = parent->result_nodes->data[0];
	nerr  = parent->result_nodes->data[1];

	nthis->type         = OP_CALL;
	nthis->result       = nthis->var;
	nthis->var          = NULL;
	nthis->op           = find_base_operator_by_type(OP_CALL);
	nthis->split_flag   = 0;
	nthis->split_parent = NULL;
	node_add_child(nthis, nodes[0]);
	node_add_child(nthis, nodes[1]);

	nerr->type         = OP_CALL;
	nerr->result       = nerr->var;
	nerr->var          = NULL;
	nerr->op           = find_base_operator_by_type(OP_CALL);
	nerr->split_flag   = 0;
	nerr->split_parent = NULL;

	parent->nodes[0] = nerr;
	parent->nb_nodes = 1;
	nerr->parent     = parent;

	v = _operand_get(nthis);
	v->tmp_flag = 1;

	v = _operand_get(nerr);
	v->tmp_flag = 1;

	nthis->_3ac_done = 1;
	nerr ->_3ac_done = 1;

	ret = _3ac_code_N(d->_3ac_list_head, OP_CALL, nthis, nthis->nodes, nthis->nb_nodes);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = _3ac_code_1(d->_3ac_list_head, OP_3AC_TEQ, nthis);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	jz  = _3ac_jmp_code(OP_3AC_JZ, NULL, NULL);
	list_add_tail(d->_3ac_list_head, &jz->list);

	for (i = 3; i < nb_nodes; i++) {

		ret = _expr_calculate_internal(ast, nodes[i], d);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	for (i = 2; i < nb_nodes; i++)
		node_add_child(nerr, nodes[i]);

	for (i = 1; i < nb_nodes; i++)
		nodes[i] = NULL;

	ret = _3ac_code_N(d->_3ac_list_head, OP_CALL, nerr, nerr->nodes, nerr->nb_nodes);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	jmp = _3ac_jmp_code(OP_GOTO,   NULL, NULL);
	list_add_tail(d->_3ac_list_head, &jmp->list);

	vector_add(d->branch_ops->_breaks, jz);
	vector_add(d->branch_ops->_breaks, jmp);

	dst = jz->dsts->data[0];
	dst->code = jmp;

	t = block_find_type_type(ast->current_block, VAR_INT);
	v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 0, NULL);
	if (!v)
		return -ENOMEM;

	node = node_alloc(NULL, v->type, v);
	if (!node) {
		variable_free(v);
		return -ENOMEM;
	}
	v->data.i64 = -ENOMEM;

	ret = _3ac_code_N(d->_3ac_list_head, OP_ASSIGN, nerr, &node, 1);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	l   = list_tail(d->_3ac_list_head);
	dst = jmp->dsts->data[0];
	dst->code = list_data(l, _3ac_code_t, list);

	return 0;
}

static int _op_call(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(nb_nodes > 0);

	handler_data_t* d      = data;
	variable_t*     v      = NULL;
	variable_t*     fmt    = NULL;
	function_t*     f      = NULL;
	vector_t*       argv   = NULL;
	node_t*         parent = nodes[0]->parent;

	int i;
	int ret = _expr_calculate_internal(ast, nodes[0], d);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	v = _operand_get(nodes[0]);
	f = v->func_ptr;

	if (f->vargs_flag) {
		if (f->argv->size > nb_nodes - 1)
			return -1;
	} else if (f->argv->size != nb_nodes - 1)
		return -1;

	argv = vector_alloc();
	if (!argv) {
		loge("\n");
		return -ENOMEM;
	}

	ret = vector_add(argv, nodes[0]);
	if (ret < 0) {
		vector_free(argv);
		return ret;
	}

	for (i = 1; i < nb_nodes; i++) {
		node_t*     arg   = nodes[i];
		node_t*     child = NULL;

		while (OP_EXPR == arg->type)
			arg = arg->nodes[0];

		if (type_is_assign(arg->type)) {

			assert(2 == arg->nb_nodes);
			child     = arg->nodes[0];

			child->_3ac_done = 0;

			ret = _expr_calculate_internal(ast, child, d);
			if (ret < 0) {
				vector_free(argv);
				return ret;
			}

			arg = child;
		}

		ret = vector_add(argv, arg);
		if (ret < 0) {
			vector_free(argv);
			return ret;
		}

		v = _operand_get(arg);

		if (!v->global_flag
				&& !v->static_flag
				&& !v->member_flag
				&& !v->local_flag
				&& !v->const_flag)
			v->tmp_flag = 1;

		if (!strcmp(f->node.w->text->data, "async") && i > 2) {

			fmt = _operand_get(nodes[2]);

			if (variable_float(v))
				ret = string_cat_cstr_len(fmt->data.s, "f", 1);
			else
				ret = string_cat_cstr_len(fmt->data.s, "d", 1);

			if (ret < 0)
				return ret;
		}
	}

	if (parent->result_nodes) {

		node_t* node;

		for (i = 0; i < parent->result_nodes->size; i++) {
			node      = parent->result_nodes->data[i];

			v = _operand_get(node);

			if (VAR_VOID == v->type && 0 == v->nb_pointers)
				v->const_flag = 1;
			v->tmp_flag = 1;
		}

		ret = _3ac_code_NN(d->_3ac_list_head, OP_CALL,
				(node_t**)parent->result_nodes->data, parent->result_nodes->size,
				(node_t**)argv->data, argv->size);

	} else {
		v = _operand_get(parent);
		if (v) {
			if (VAR_VOID == v->type && 0 == v->nb_pointers)
				v->const_flag = 1;
			v->tmp_flag = 1;
		}

		ret = _3ac_code_N(d->_3ac_list_head, OP_CALL, parent, (node_t**)argv->data, argv->size);
	}

	vector_free(argv);
	return ret;
}

static int _op_expr(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
#if 1
	assert(1 == nb_nodes);

	handler_data_t* d = data;

	int ret = _expr_calculate_internal(ast, nodes[0], d);
	if (ret < 0)
		return -1;
#endif
	return 0;
}

#define OP_UNARY(name, op_type) \
static int _op_##name(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	assert(1 == nb_nodes); \
	handler_data_t* d = data; \
	node_t* parent    = nodes[0]->parent; \
	\
	_operand_get(parent)->tmp_flag = 1; \
	\
	return _3ac_code_2(d->_3ac_list_head, op_type, parent, nodes[0]); \
}

OP_UNARY(neg,         OP_NEG)
OP_UNARY(positive,    OP_POSITIVE)
OP_UNARY(dereference, OP_DEREFERENCE)
OP_UNARY(logic_not,   OP_LOGIC_NOT)
OP_UNARY(bit_not,     OP_BIT_NOT)

static int _op_address_of(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(1 == nb_nodes);

	handler_data_t* d = data;

	node_t* parent    = nodes[0]->parent;
	node_t* child     = nodes[0];

	if (type_is_var(child->type))
		return _3ac_code_2(d->_3ac_list_head, OP_ADDRESS_OF, parent, child);

	if (OP_ARRAY_INDEX == child->type) {
		assert(2 == child->nb_nodes);

		node_t* n_scale = NULL;
		node_t* srcs[3];

		int i;
		for (i = 0; i < 2; i++) {
			if (_expr_calculate_internal(ast, child->nodes[i], d) < 0) {
				loge("\n");
				return -1;
			}
		}

		int ret = _op_array_scale(ast, child, &n_scale);
		if (ret < 0)
			return ret;

		srcs[0] = child->nodes[0];
		srcs[1] = child->nodes[1];
		srcs[2] = n_scale;

		return _3ac_code_N(d->_3ac_list_head, OP_3AC_ADDRESS_OF_ARRAY_INDEX, parent, srcs, 3);
	}

	if (OP_POINTER == child->type) {
		assert(2 == child->nb_nodes);

		node_t* srcs[2];

		int i;
		for (i = 0; i < 2; i++) {
			if (_expr_calculate_internal(ast, child->nodes[i], d) < 0) {
				loge("\n");
				return -1;
			}
		}

		srcs[0] = child->nodes[0];
		srcs[1] = child->nodes[1];

		variable_t* v = _operand_get(srcs[1]);
		if (v->bit_size > 0) {
			loge("can't pointer to the bit member '%s' of a struct, file: %s, line: %d\n",
					v->w->text->data, parent->w->file->data, parent->w->line);
			return -EINVAL;
		}

		return _3ac_code_N(d->_3ac_list_head, OP_3AC_ADDRESS_OF_POINTER, parent, srcs, 2);
	}

	loge("\n");
	return -1;
}

static int _op_type_cast(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);

	handler_data_t* d = data;

	node_t* parent = nodes[0]->parent;

	_operand_get(parent)->tmp_flag = 1;

	return _3ac_code_2(d->_3ac_list_head, OP_TYPE_CAST, parent, nodes[1]);
}

#define OP_BINARY(name, op_type) \
static int _op_##name(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	assert(2 == nb_nodes); \
	handler_data_t* d      = data; \
	node_t*         parent = nodes[0]->parent; \
	\
	_operand_get(parent)->tmp_flag = 1; \
	\
	return _3ac_code_3(d->_3ac_list_head, op_type, parent, nodes[0], nodes[1]); \
}

OP_BINARY(add,     OP_ADD)
OP_BINARY(sub,     OP_SUB)
OP_BINARY(mul,     OP_MUL)
OP_BINARY(div,     OP_DIV)
OP_BINARY(mod,     OP_MOD)
OP_BINARY(shl,     OP_SHL)
OP_BINARY(shr,     OP_SHR)
OP_BINARY(bit_and, OP_BIT_AND)
OP_BINARY(bit_or,  OP_BIT_OR)

static int _op_left_value_array_index(ast_t* ast, int type, node_t* left, node_t* right, handler_data_t* d)
{
	assert(2 == left->nb_nodes);

	node_t* n_index = NULL;
	node_t* n_scale = NULL;
	node_t* srcs[4];

	int i;
	for (i = 0; i < 2; i++) {
		if (_expr_calculate_internal(ast, left->nodes[i], d) < 0) {
			loge("\n");
			return -1;
		}
	}

	n_index = left->nodes[1];
	while (OP_EXPR == n_index->type)
		n_index = n_index->nodes[0];

	int ret = _op_array_scale(ast, left, &n_scale);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	srcs[0]   = left->nodes[0];
	srcs[1]   = n_index;
	srcs[i++] = n_scale;
	if (right)
		srcs[i++] = right;

	if (_3ac_code_srcN(d->_3ac_list_head, type, srcs, i) < 0) {
		loge("\n");
		return -1;
	}
	return 0;
}

static int _op_left_value(ast_t* ast, int type, node_t* left, node_t* right, handler_data_t* d)
{
	assert(1 == left->nb_nodes || 2 == left->nb_nodes);

	node_t* srcs[3];

	int i;
	for (i = 0; i < left->nb_nodes; i++) {
		if (_expr_calculate_internal(ast, left->nodes[i], d) < 0) {
			loge("\n");
			return -1;
		}

		srcs[i] = left->nodes[i];
	}

	if (right)
		srcs[i++] = right;

	assert(i <= 3);

	if (_3ac_code_srcN(d->_3ac_list_head, type, srcs, i) < 0) {
		loge("\n");
		return -1;
	}
	return 0;
}

static int _op_right_value(ast_t* ast, node_t** pright, handler_data_t* d)
{
	node_t* right = *pright;

	if (_expr_calculate_internal(ast, right, d) < 0)
		return -1;

	if (type_is_assign(right->type)) {
		right = right->nodes[0];

		while (OP_EXPR == right->type)
			right = right->nodes[0];

		assert(!type_is_assign(right->type));

		right->_3ac_done = 0;

		if (_expr_calculate_internal(ast, right, d) < 0)
			return -1;

	} else {
		while (OP_EXPR == right->type)
			right = right->nodes[0];

		if (OP_CALL == right->type && !right->split_flag) {

			if (right->result_nodes) {

				assert(right->result_nodes->size > 0);

				right = right->result_nodes->data[0];
			} else
				assert(right->result);
		}
	}

	*pright = right;
	return 0;
}

static int _op_assign(ast_t* ast, node_t** nodes, int nb_nodes, void* data)
{
	assert(2 == nb_nodes);
	handler_data_t* d = data;

	node_t*     parent = nodes[0]->parent;
	node_t*     node0  = nodes[0];
	node_t*     node1  = nodes[1];
	variable_t* v0     = _operand_get(node0);

	if ( _op_right_value(ast, &node1, d) < 0)
		return -1;

	while (OP_EXPR == node0->type)
		node0 = node0->nodes[0];

	switch (node0->type) {
		case OP_DEREFERENCE:
			return _op_left_value(ast, OP_3AC_ASSIGN_DEREFERENCE, node0, node1, d);
			break;
		case OP_ARRAY_INDEX:
			return _op_left_value_array_index(ast, OP_3AC_ASSIGN_ARRAY_INDEX, node0, node1, d);
			break;
		case OP_POINTER:
			return _op_left_value(ast, OP_3AC_ASSIGN_POINTER, node0, node1, d);
			break;
		default:
			break;
	};

	return _3ac_code_2(d->_3ac_list_head, OP_ASSIGN, node0, node1);
}

#define OP_BINARY_ASSIGN(name, op) \
static int _op_##name##_assign(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	assert(2 == nb_nodes); \
	handler_data_t* d = data; \
	\
	node_t*     parent = nodes[0]->parent; \
	node_t*     node0  = nodes[0]; \
	node_t*     node1  = nodes[1]; \
	variable_t* v1     = _operand_get(nodes[1]); \
	\
	if ( _op_right_value(ast, &node1, d) < 0) \
		return -1; \
	\
	while (OP_EXPR == node0->type) \
		node0 = node0->nodes[0]; \
	\
	if (_expr_calculate_internal(ast, node0, d) < 0) { \
		loge("\n"); \
		return -1; \
	} \
	\
	if ( _3ac_code_2(d->_3ac_list_head, parent->type, node0, node1) < 0) { \
		loge("\n"); \
		return -1; \
	} \
	\
	switch (node0->type) { \
		case OP_DEREFERENCE: \
			return _op_left_value(ast, OP_3AC_ASSIGN_DEREFERENCE, node0, node0, d); \
			break; \
		case OP_ARRAY_INDEX: \
			return _op_left_value_array_index(ast, OP_3AC_ASSIGN_ARRAY_INDEX, node0, node0, d); \
			break; \
		case OP_POINTER: \
			return _op_left_value(ast, OP_3AC_ASSIGN_POINTER, node0, node0, d); \
			break; \
		default: \
			break; \
	}; \
	\
	return 0; \
}

OP_BINARY_ASSIGN(add, ADD)
OP_BINARY_ASSIGN(sub, SUB)
OP_BINARY_ASSIGN(and, AND)
OP_BINARY_ASSIGN(or,  OR)

OP_BINARY_ASSIGN(shl, SHL)
OP_BINARY_ASSIGN(shr, SHR)
OP_BINARY_ASSIGN(mul, MUL)
OP_BINARY_ASSIGN(div, DIV)
OP_BINARY_ASSIGN(mod, MOD)

#define OP_UNARY_ASSIGN(name, op) \
static int __op_##name(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	assert(1 == nb_nodes); \
	handler_data_t* d = data; \
	\
	node_t*     node0 = nodes[0]; \
	\
	while (OP_EXPR == node0->type) \
		node0 = node0->nodes[0]; \
	\
	if (_expr_calculate_internal(ast, node0, d) < 0) { \
		loge("\n"); \
		return -1; \
	} \
	\
	if (_3ac_code_1(d->_3ac_list_head, OP_3AC_##op, node0) < 0) { \
		loge("\n"); \
		return -1; \
	} \
	\
	switch (node0->type) { \
		case OP_DEREFERENCE: \
			return _op_left_value(ast, OP_3AC_ASSIGN_DEREFERENCE, node0, node0, d); \
			break; \
		case OP_ARRAY_INDEX: \
			return _op_left_value_array_index(ast, OP_3AC_ASSIGN_ARRAY_INDEX, node0, node0, d); \
			break; \
		case OP_POINTER: \
			return _op_left_value(ast, OP_3AC_ASSIGN_POINTER, node0, node0, d); \
			break; \
		default: \
			break; \
	}; \
	\
	return 0; \
}
OP_UNARY_ASSIGN(inc, INC)
OP_UNARY_ASSIGN(dec, DEC)

#define OP_UNARY_ASSIGN2(name0, name1, op_type, post_flag) \
static int _op_##name0(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	assert(1 == nb_nodes); \
	handler_data_t* d  = data; \
	\
	node_t*     node0  = nodes[0]; \
	node_t*     parent = nodes[0]->parent; \
	\
	int ret = __op_##name1(ast, nodes, nb_nodes, data); \
	if (ret < 0) { \
		loge("\n"); \
		return -1; \
	} \
	\
	while (OP_EXPR == node0->type) \
		node0 = node0->nodes[0]; \
	\
	list_t*     l  = list_tail(d->_3ac_list_head); \
	_3ac_code_t* c  = list_data(l, _3ac_code_t, list); \
	\
	ret = _3ac_code_2(d->_3ac_list_head, OP_ASSIGN, parent, node0); \
	if (ret < 0) { \
		loge("\n"); \
		return -1; \
	} \
	_operand_get(parent)->tmp_flag = 1; \
	\
	if (post_flag) { \
	    list_del(&c->list); \
		list_add_tail(d->_3ac_list_head, &c->list); \
	} \
	return 0; \
}
OP_UNARY_ASSIGN2(inc,      inc, INC, 0)
OP_UNARY_ASSIGN2(dec,      dec, DEC, 0)
OP_UNARY_ASSIGN2(inc_post, inc, INC, 1)
OP_UNARY_ASSIGN2(dec_post, dec, DEC, 1)

#define OP_CMP(name, op_type) \
static int _op_##name(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{\
	assert(2 == nb_nodes);\
	handler_data_t* d  = data;\
	node_t*     parent = nodes[0]->parent; \
	\
	variable_t* v0 = _operand_get(nodes[0]);\
	variable_t* v1 = _operand_get(nodes[1]);\
	if (variable_const(v0)) { \
		if (variable_const(v1)) {\
			loge("result to compare 2 const var should be calculated before\n"); \
			return -EINVAL; \
		} \
		int op_type2 = op_type; \
		switch (op_type) { \
			case OP_GT: \
				op_type2 = OP_LT; \
				break; \
			case OP_GE: \
				op_type2 = OP_LE; \
				break; \
			case OP_LE: \
				op_type2 = OP_GE; \
				break; \
			case OP_LT: \
				op_type2 = OP_GT; \
				break; \
		} \
		parent->type = op_type2; \
		XCHG(nodes[0], nodes[1]); \
		return _3ac_code_3(d->_3ac_list_head, op_type2, parent, nodes[0], nodes[1]); \
	} \
	return _3ac_code_3(d->_3ac_list_head, op_type, parent, nodes[0], nodes[1]); \
}

OP_CMP(eq, OP_EQ)
OP_CMP(ne, OP_NE)
OP_CMP(gt, OP_GT)
OP_CMP(ge, OP_GE)
OP_CMP(lt, OP_LT)
OP_CMP(le, OP_LE)

static int _op_logic_and_jmp(ast_t* ast, node_t* node, handler_data_t* d)
{
	node_t* parent = node->parent;

	if (_expr_calculate_internal(ast, node, d) < 0) {
		loge("\n");
		return -1;
	}

	int is_float   = 0;
	int is_default = 0;
	int jmp_op;
	int set_op;

	while (OP_EXPR == node->type)
		node = node->nodes[0];

	if (node->nb_nodes > 0) {

		variable_t* v = _operand_get(node->nodes[0]);

		if (variable_float(v))
			is_float = 1;
	}

	switch (node->type) {
		case OP_EQ:
			set_op = OP_3AC_SETZ;
			jmp_op = OP_3AC_JNZ;
			break;
		case OP_NE:
			set_op = OP_3AC_SETNZ;
			jmp_op = OP_3AC_JZ;
			break;

		case OP_GT:
			if (!is_float) {
				set_op = OP_3AC_SETGT;
				jmp_op = OP_3AC_JLE;
			} else {
				set_op = OP_3AC_SETA;
				jmp_op = OP_3AC_JBE;
			}
			break;

		case OP_GE:
			if (!is_float) {
				set_op = OP_3AC_SETGE;
				jmp_op = OP_3AC_JLT;
			} else {
				set_op = OP_3AC_SETAE;
				jmp_op = OP_3AC_JB;
			}
			break;

		case OP_LT:
			if (!is_float) {
				set_op = OP_3AC_SETLT;
				jmp_op = OP_3AC_JGE;
			} else {
				set_op = OP_3AC_SETB;
				jmp_op = OP_3AC_JAE;
			}
			break;
		case OP_LE:
			if (!is_float) {
				set_op = OP_3AC_SETLE;
				jmp_op = OP_3AC_JGT;
			} else {
				set_op = OP_3AC_SETB;
				jmp_op = OP_3AC_JA;
			}
			break;

		default:
			if (_3ac_code_1(d->_3ac_list_head, OP_3AC_TEQ, node) < 0) {
				loge("\n");
				return -1;
			}

			if (_3ac_code_dst(d->_3ac_list_head, OP_3AC_SETNZ, parent) < 0) {
				loge("\n");
				return -1;
			}

			jmp_op     = OP_3AC_JZ;
			is_default = 1;
			break;
	};

	if (!is_default) {
		list_t*     l    = list_tail(d->_3ac_list_head);
		_3ac_code_t* c    = list_data(l, _3ac_code_t, list);
		vector_t*   dsts = c->dsts;

		c->op   = _3ac_find_operator(OP_3AC_CMP);
		c->dsts = NULL;

		vector_clear(dsts, ( void (*)(void*) ) _3ac_operand_free);
		vector_free (dsts);
		dsts = NULL;

		if (_3ac_code_dst(d->_3ac_list_head, set_op, parent) < 0) {
			loge("\n");
			return -1;
		}
	}

	variable_t* v = _operand_get(parent);
	v->tmp_flag = 1;
	return jmp_op;
}

static int _op_logic_or_jmp(ast_t* ast, node_t* node, handler_data_t* d)
{
	node_t* parent = node->parent;

	if (_expr_calculate_internal(ast, node, d) < 0) {
		loge("\n");
		return -1;
	}

	int is_float   = 0;
	int is_default = 0;
	int jmp_op;
	int set_op;

	while (OP_EXPR == node->type)
		node = node->nodes[0];

	if (node->nb_nodes > 0) {

		variable_t* v = _operand_get(node->nodes[0]);

		if (variable_float(v))
			is_float = 1;
	}

	switch (node->type) {
		case OP_EQ:
			set_op = OP_3AC_SETZ;
			jmp_op = OP_3AC_JZ;
			break;
		case OP_NE:
			set_op = OP_3AC_SETNZ;
			jmp_op = OP_3AC_JNZ;
			break;

		case OP_GT:
			if (!is_float) {
				set_op = OP_3AC_SETGT;
				jmp_op = OP_3AC_JGT;
			} else {
				set_op = OP_3AC_SETA;
				jmp_op = OP_3AC_JA;
			}
			break;

		case OP_GE:
			if (!is_float) {
				set_op = OP_3AC_SETGE;
				jmp_op = OP_3AC_JGE;
			} else {
				set_op = OP_3AC_SETAE;
				jmp_op = OP_3AC_JAE;
			}
			break;

		case OP_LT:
			if (!is_float) {
				set_op = OP_3AC_SETLT;
				jmp_op = OP_3AC_JLT;
			} else {
				set_op = OP_3AC_SETB;
				jmp_op = OP_3AC_JB;
			}
			break;

		case OP_LE:
			if (!is_float) {
				set_op = OP_3AC_SETLE;
				jmp_op = OP_3AC_JLE;
			} else {
				set_op = OP_3AC_SETBE;
				jmp_op = OP_3AC_JBE;
			}
			break;

		default:
			if (_3ac_code_1(d->_3ac_list_head, OP_3AC_TEQ, node) < 0) {
				loge("\n");
				return -1;
			}

			if (_3ac_code_dst(d->_3ac_list_head, OP_3AC_SETNZ, parent) < 0) {
				loge("\n");
				return -1;
			}

			jmp_op     = OP_3AC_JNZ;
			is_default = 1;
			break;
	};

	if (!is_default) {
		list_t*     l    = list_tail(d->_3ac_list_head);
		_3ac_code_t* c    = list_data(l, _3ac_code_t, list);
		vector_t*   dsts = c->dsts;

		c->op   = _3ac_find_operator(OP_3AC_CMP);
		c->dsts = NULL;

		vector_clear(dsts, ( void(*)(void*) ) _3ac_operand_free);
		vector_free (dsts);
		dsts = NULL;

		if (_3ac_code_dst(d->_3ac_list_head, set_op, parent) < 0) {
			loge("\n");
			return -1;
		}
	}

	variable_t* v = _operand_get(parent);
	v->tmp_flag = 1;
	return jmp_op;
}

#define OP_LOGIC(name) \
static int _op_logic_##name(ast_t* ast, node_t** nodes, int nb_nodes, void* data) \
{ \
	handler_data_t* d = data; \
	node_t* parent    = nodes[0]->parent; \
	int jmp_op = _op_logic_##name##_jmp(ast, nodes[0], d); \
	if (jmp_op < 0) \
		return -1; \
	\
	_3ac_code_t* jmp = _3ac_jmp_code(jmp_op, NULL, NULL); \
	if (!jmp) \
		return -ENOMEM; \
	\
	list_add_tail(d->_3ac_list_head, &jmp->list); \
	if (_op_logic_##name##_jmp(ast, nodes[1], d) < 0) \
		return -1; \
	\
	jmp->dsts = vector_alloc(); \
	if (!jmp->dsts) \
		return -ENOMEM; \
	_3ac_operand_t* dst = _3ac_operand_alloc(); \
	if (!dst) \
		return -ENOMEM; \
	if (vector_add(jmp->dsts, dst) < 0) \
		return -ENOMEM; \
	\
	list_t* l  = list_tail(d->_3ac_list_head); \
	dst->code = list_data(l, _3ac_code_t, list); \
	\
	int ret = vector_add(d->branch_ops->_breaks, jmp); \
	if (ret < 0) \
		return ret; \
	return 0; \
}
OP_LOGIC(and)
OP_LOGIC(or)

operator_handler_pt  __operator_handlers[N_3AC_OPS] =
{
	[OP_EXPR  ]       = _op_expr,
	[OP_CALL  ]       = _op_call,
	[OP_CREATE]       = _op_create,

	[OP_ARRAY_INDEX]  = _op_array_index,
	[OP_POINTER    ]  = _op_pointer,

	[OP_VA_START]     = _op_va_start,
	[OP_VA_ARG  ]     = _op_va_arg,
	[OP_VA_END  ]     = _op_va_end,

	[OP_TYPE_CAST]    = _op_type_cast,
	[OP_LOGIC_NOT]    = _op_logic_not,
	[OP_BIT_NOT  ]    = _op_bit_not,
	[OP_NEG      ]    = _op_neg,
	[OP_POSITIVE ]    = _op_positive,

	[OP_INC]          = _op_inc,
	[OP_DEC]          = _op_dec,

	[OP_INC_POST]     = _op_inc_post,
	[OP_DEC_POST]     = _op_dec_post,

	[OP_DEREFERENCE]  = _op_dereference,
	[OP_ADDRESS_OF ]  = _op_address_of,

	[OP_MUL]          = _op_mul,
	[OP_DIV]          = _op_div,
	[OP_MOD]          = _op_mod,

	[OP_ADD]          = _op_add,
	[OP_SUB]          = _op_sub,

	[OP_SHL]          = _op_shl,
	[OP_SHR]          = _op_shr,

	[OP_BIT_AND]      = _op_bit_and,
	[OP_BIT_OR ]      = _op_bit_or,

	[OP_EQ]           = _op_eq,
	[OP_NE]           = _op_ne,
	[OP_GT]           = _op_gt,
	[OP_LT]           = _op_lt,
	[OP_GE]           = _op_ge,
	[OP_LE]           = _op_le,

	[OP_LOGIC_AND]    = _op_logic_and,
	[OP_LOGIC_OR ]    = _op_logic_or,

	[OP_ASSIGN    ]   = _op_assign,
	[OP_ADD_ASSIGN]   = _op_add_assign,
	[OP_SUB_ASSIGN]   = _op_sub_assign,
	[OP_MUL_ASSIGN]   = _op_mul_assign,
	[OP_DIV_ASSIGN]   = _op_div_assign,
	[OP_MOD_ASSIGN]   = _op_mod_assign,
	[OP_SHL_ASSIGN]   = _op_shl_assign,
	[OP_SHR_ASSIGN]   = _op_shr_assign,
	[OP_AND_ASSIGN]   = _op_and_assign,
	[OP_OR_ASSIGN ]   = _op_or_assign,


	[OP_BLOCK   ]     = _op_block,
	[OP_RETURN  ]     = _op_return,
	[OP_BREAK   ]     = _op_break,
	[OP_CONTINUE]     = _op_continue,
	[OP_GOTO    ]     = _op_goto,
	[LABEL      ]     = _op_label,

	[OP_IF   ]        = _op_if,
	[OP_WHILE]        = _op_while,
	[OP_DO   ]        = _op_do,
	[OP_FOR  ]        = _op_for,

	[OP_SWITCH ]      = _op_switch,
	[OP_CASE   ]      = _op_case,
	[OP_DEFAULT]      = _op_default,

	[OP_VLA_ALLOC]    = _op_vla_alloc,
};

operator_handler_pt  find_3ac_operator_handler(const int type)
{
	if (type < 0 || type >= N_3AC_OPS)
		return NULL;

	return __operator_handlers[type];
}
