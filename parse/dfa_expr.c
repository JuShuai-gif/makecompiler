#include"dfa.h"
#include"dfa_util.h"
#include"utils_stack.h"
#include"parse.h"

extern dfa_module_t dfa_module_expr;

typedef struct {
	stack_t*      ls_exprs;
	stack_t*      lp_exprs;
	block_t*      parent_block;
	variable_t*   current_var;
	type_t*       current_struct;

} expr_module_data_t;

int _type_find_type(dfa_t* dfa, dfa_identity_t* id);

static int _expr_is_expr(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	if (LEX_WORD_SEMICOLON == w->type
			|| lex_is_operator(w)
			|| lex_is_const(w)
			|| lex_is_identity(w))
		return 1;
	return 0;
}

static int _expr_is_number(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return lex_is_const(w);
}

static int _expr_is_unary_op(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	if (LEX_WORD_LS == w->type
			|| LEX_WORD_RS == w->type
			|| LEX_WORD_LP == w->type
			|| LEX_WORD_RP == w->type
			|| LEX_WORD_KEY_SIZEOF == w->type)
		return 0;

	operator_t* op = find_base_operator(w->text->data, 1);
	if (op)
		return 1;
	return 0;
}

static int _expr_is_unary_post(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_INC == w->type
		|| LEX_WORD_DEC == w->type;
}

static int _expr_is_binary_op(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	if (LEX_WORD_LS == w->type
			|| LEX_WORD_RS == w->type
			|| LEX_WORD_LP == w->type
			|| LEX_WORD_RP == w->type)
		return 0;

	operator_t* op = find_base_operator(w->text->data, 2);
	if (op)
		return 1;
	return 0;
}

int _expr_add_var(parse_t* parse, dfa_data_t* d)
{
	expr_module_data_t* md   = d->module_datas[dfa_module_expr.index];
	variable_t*     var  = NULL;
	node_t*         node = NULL;
	type_t*         pt   = NULL;
	function_t*     f    = NULL;
	dfa_identity_t*     id   = stack_pop(d->current_identities);
	lex_word_t*     w;

	assert(id && id->identity);
	w = id->identity;

	if (ast_find_variable(&var, parse->ast, w->text->data) < 0)
		return DFA_ERROR;

	if (!var) {
		logw("var '%s' not found, maybe it's a function\n", w->text->data);

		if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
			return DFA_ERROR;
		assert(pt);

		if (ast_find_function(&f, parse->ast, w->text->data) < 0)
			return DFA_ERROR;

		if (!f) {
			loge("function '%s' not found\n", w->text->data);
			return DFA_ERROR;
		}

		var = VAR_ALLOC_BY_TYPE(id->identity, pt, 1, 1, f);
		if (!var)
			return -ENOMEM;

		var->const_literal_flag = 1;
	}

	logd("var: %s, member_flag: %d, line: %d\n", var->w->text->data, var->member_flag, var->w->line);

	node = node_alloc(w, var->type, var);
	if (!node)
		return -ENOMEM;

	if (!d->expr) {
		d->expr = expr_alloc();
		if (!d->expr)
			return -ENOMEM;
	}

	logd("d->expr: %p, node: %p\n", d->expr, node);

	if (expr_add_node(d->expr, node) < 0) {
		loge("add var node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	if (var->type >= STRUCT) {

		int ret = ast_find_type_type(&md->current_struct, parse->ast, var->type);
		if (ret < 0)
			return DFA_ERROR;
		assert(md->current_struct);
	}
	md->current_var = var;

	free(id);
	id = NULL;
	return DFA_OK;
}

static int _expr_action_expr(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (!d->expr) {
		d->expr = expr_alloc();
		if (!d->expr) {
			loge("expr alloc failed\n");
			return DFA_ERROR;
		}
	}

	logd("d->expr: %p\n", d->expr);

	return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}

static int _expr_action_number(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	logd("w: %s\n", w->text->data);

	int type;
	int nb_pointers = 0;

	switch (w->type) {
		case LEX_WORD_CONST_CHAR:
			type = VAR_U32;
			break;

		case LEX_WORD_CONST_STRING:
			type = VAR_CHAR;
			nb_pointers = 1;

			if (!strcmp(w->text->data, "__func__")) {

				function_t* f = (function_t*)parse->ast->current_block;

				while (f && FUNCTION != f->node.type)
					f = (function_t*) f->node.parent;

				if (!f) {
					loge("line: %d, '__func__' isn't in a function\n", w->line);
					return DFA_ERROR;
				}

				if (function_signature(parse->ast, f) < 0)
					return DFA_ERROR;

				w->text->data[2] = '\0';
				w->text->len     = 2;

				int ret = string_cat(w->text, f->signature);
				if (ret < 0)
					return DFA_ERROR;

				ret = string_cat_cstr_len(w->text, "__", 2);
				if (ret < 0)
					return DFA_ERROR;

				w->data.s = string_clone(f->signature);
				if (!w->data.s)
					return DFA_ERROR;
			}
			break;

		case LEX_WORD_CONST_INT:
			type = VAR_INT;
			break;
		case LEX_WORD_CONST_U32:
			type = VAR_U32;
			break;

		case LEX_WORD_CONST_FLOAT:
			type = VAR_FLOAT;
			break;

		case LEX_WORD_CONST_DOUBLE:
			type = VAR_DOUBLE;
			break;

		case LEX_WORD_CONST_I64:
			type = VAR_I64;
			break;

		case LEX_WORD_CONST_U64:
			type = VAR_U64;
			break;

		default:
			loge("unknown number type\n");
			return DFA_ERROR;
	};

	type_t* t = block_find_type_type(parse->ast->current_block, type);
	if (!t) {
		loge("\n");
		return DFA_ERROR;
	}

	variable_t* var = VAR_ALLOC_BY_TYPE(w, t, 1, nb_pointers, NULL);
	if (!var) {
		loge("var '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	node_t* n = node_alloc(w, var->type, var);
	if (!n) {
		loge("var node '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	if (expr_add_node(d->expr, n) < 0) {
		loge("add var node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;
}

static int _expr_action_op(dfa_t* dfa, vector_t* words, void* data, int nb_operands)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	operator_t*  op;
	node_t*      node;

	op = find_base_operator(w->text->data, nb_operands);
	if (!op) {
		loge("find op '%s' error, nb_operands: %d\n", w->text->data, nb_operands);
		return DFA_ERROR;
	}

	node = node_alloc(w, op->type, NULL);
	if (!node) {
		loge("op node '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	if (expr_add_node(d->expr, node) < 0) {
		loge("add op node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;
}

static int _expr_action_unary_op(dfa_t* dfa, vector_t* words, void* data)
{
	logd("\n");
	return _expr_action_op(dfa, words, data, 1);
}

static int _expr_action_binary_op(dfa_t* dfa, vector_t* words, void* data)
{
	logd("\n");

	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 1];
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id    = stack_top(d->current_identities);
	variable_t*     v;

	if (LEX_WORD_STAR == w->type) {

		if (id && id->identity) {

			v = block_find_variable(parse->ast->current_block, id->identity->text->data);
			if (!v) {
				logw("'%s' not var\n", id->identity->text->data);

				if (d->expr) {
					expr_free(d->expr);
					d->expr = NULL;
				}
				return DFA_NEXT_SYNTAX;
			}
		}
	}

	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0)
			return DFA_ERROR;
	}

	if (LEX_WORD_ARROW == w->type || LEX_WORD_DOT == w->type) {
		assert(md->current_struct);

		if (!md->parent_block)
			md->parent_block = parse->ast->current_block;

		parse->ast->current_block = (block_t*)md->current_struct;

	} else if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	return _expr_action_op(dfa, words, data, 2);
}

static int _expr_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];

	expr_t* e = expr_alloc();
	if (!e) {
		loge("\n");
		return DFA_ERROR;
	}

	logi("d->expr: %p, e: %p\n", d->expr, e);

	stack_push(md->lp_exprs, d->expr);
	d->expr = e;

	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block = NULL;
	}

	return DFA_NEXT_WORD;
}

static int _expr_action_rp_cast(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id    = stack_top(d->current_identities);

	if (!id) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!id->type) {
		if (_type_find_type(dfa, id) < 0) {
			loge("\n");
			return DFA_ERROR;
		}
	}

	if (!id->type || !id->type_w) {
		loge("\n");
		return DFA_ERROR;
	}

	if (d->nb_sizeofs > 0) {
		logw("DFA_NEXT_SYNTAX\n");

		if (d->expr) {
			expr_free(d->expr);
			d->expr = NULL;
		}

		return DFA_NEXT_SYNTAX;
	}

	if (d->current_va_arg) {
		logw("DFA_NEXT_SYNTAX\n");
		return DFA_NEXT_SYNTAX;
	}

	variable_t* var       = NULL;
	node_t*     node_var  = NULL;
	node_t*     node_cast = NULL;

	var = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, id->func_ptr);
	if (!var) {
		loge("var alloc failed\n");
		return DFA_ERROR;
	}

	node_var = node_alloc(NULL, var->type, var);
	if (!node_var) {
		loge("var node alloc failed\n");
		return DFA_ERROR;
	}

	node_cast = node_alloc(id->type_w, OP_TYPE_CAST, NULL);
	if (!node_cast) {
		loge("cast node alloc failed\n");
		return DFA_ERROR;
	}
	node_add_child(node_cast, node_var);

	// '(' lp action pushed a expr before
	expr_t* e = stack_pop(md->lp_exprs);

	logd("type cast: d->expr: %p, d->expr->parent: %p, e: %p\n", d->expr, d->expr->parent, e);

	assert(e);

	expr_add_node(e, node_cast);
	d->expr = e;

	stack_pop(d->current_identities);
	free(id);
	return DFA_NEXT_WORD;
}

static int _expr_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	if (id && id->identity) {

		variable_t* v = NULL;
		function_t* f = NULL;

		if (ast_find_variable(&v, parse->ast, id->identity->text->data) < 0)
			return DFA_ERROR;

		if (!v) {
			logw("'%s' not var\n", id->identity->text->data);

			if (ast_find_function(&f, parse->ast, id->identity->text->data) < 0)
				return DFA_ERROR;

			if (!f) {
				logw("'%s' not function\n", id->identity->text->data);
				return DFA_NEXT_SYNTAX;
			}
		}

		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	expr_t* parent = stack_pop(md->lp_exprs);

	logd("d->expr: %p, d->expr->parent: %p, lp: %p\n\n", d->expr, d->expr->parent, parent);

	if (parent) {
		expr_add_node(parent, d->expr);
		d->expr = parent;
	}

	return DFA_NEXT_WORD;
}

static int _expr_action_unary_post(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_data_t*      d     = data;
	node_t*      n     = NULL;
	dfa_identity_t*  id    = stack_top(d->current_identities);

	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	if (LEX_WORD_INC == w->type)
		n = node_alloc(w, OP_INC_POST, NULL);

	else if (LEX_WORD_DEC == w->type)
		n = node_alloc(w, OP_DEC_POST, NULL);
	else {
		loge("\n");
		return DFA_ERROR;
	}

	if (!n) {
		loge("node alloc error\n");
		return DFA_ERROR;
	}

	expr_add_node(d->expr, n);

	logd("n: %p, expr: %p, parent: %p\n", n, d->expr, d->expr->parent);

	return DFA_NEXT_WORD;
}

static int _expr_action_ls(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	lex_word_t*      w     = words->data[words->size - 1];
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block = NULL;
	}

	operator_t* op = find_base_operator_by_type(OP_ARRAY_INDEX);
	assert(op);

	node_t* n = node_alloc(w, op->type, NULL);
	if (!n) {
		loge("node alloc error\n");
		return DFA_ERROR;
	}
	n->op = op;

	expr_add_node(d->expr, n);

	stack_push(md->ls_exprs, d->expr);

	expr_t* index = expr_alloc();
	if (!index) {
		loge("index expr alloc error\n");
		return DFA_ERROR;
	}

	d->expr = index;

	return DFA_NEXT_WORD;
}

static int _expr_action_rs(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	expr_t* ls_parent = stack_pop(md->ls_exprs);

	assert (d->expr);
	while (d->expr->parent)
		d->expr = d->expr->parent;

	if (ls_parent) {
		expr_add_node(ls_parent, d->expr);
		d->expr = ls_parent;
	}

	return DFA_NEXT_WORD;
}

int _expr_multi_rets(expr_t* e)
{
	if (OP_ASSIGN != e->nodes[0]->type)
		return 0;

	node_t*  parent = e->parent;
	node_t*  assign = e->nodes[0];
	node_t*  call   = assign->nodes[1];

	while (call) {
		if (OP_EXPR == call->type)
			call = call->nodes[0];
		else
			break;
	}

	int nb_rets;

	if (!call)
		return 0;

	if (OP_CALL == call->type) {

		node_t* n_pf = call->nodes[0];

		if (OP_POINTER == n_pf->type) {
			assert(2       == n_pf->nb_nodes);

			n_pf = n_pf->nodes[1];
		}

		variable_t* v_pf = _operand_get(n_pf);
		function_t* f    = v_pf->func_ptr;

		if (f->rets->size <= 1)
			return 0;

		nb_rets = f->rets->size;

	} else if (OP_CREATE == call->type)
		nb_rets = 2;
	else
		return 0;

	assert(call->nb_nodes > 0);

	node_t*  ret;
	block_t* b;

	int i;
	int j;
	int k;

	b = block_alloc_cstr("multi_rets");
	if (!b)
		return -ENOMEM;

	for (i  = parent->nb_nodes - 2; i >= 0; i--) {
		ret = parent->nodes[i];

		if (ret->semi_flag)
			break;

		if (b->node.nb_nodes >= nb_rets - 1)
			break;

		node_add_child((node_t*)b, ret);
		parent->nodes[i] = NULL;
	}

	j = 0;
	k = b->node.nb_nodes - 1;
	while (j < k) {
		XCHG(b->node.nodes[j], b->node.nodes[k]);
		j++;
		k--;
	}

	node_add_child((node_t*)b, assign->nodes[0]);
	assign->nodes[0] = (node_t*)b;
	b->node.parent   = assign;

	i++;
	assert(i >= 0);

	parent->nodes[i] = e;
	parent->nb_nodes = i + 1;

	logd("parent->nb_nodes: %d\n", parent->nb_nodes);

	return 0;
}

static int _expr_fini_expr(parse_t* parse, dfa_data_t* d, int semi_flag)
{
	expr_module_data_t* md = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id = stack_top(d->current_identities);

	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0)
			return DFA_ERROR;
	}

	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	logd("d->expr: %p\n", d->expr);

	if (d->expr) {
		while (d->expr->parent)
			d->expr = d->expr->parent;

		if (0 == d->expr->nb_nodes) {

			expr_free(d->expr);
			d->expr = NULL;

		} else if (!d->expr_local_flag) {

			node_t* parent;

			if (d->current_node)
				parent = d->current_node;
			else
				parent = (node_t*)parse->ast->current_block;

			node_add_child(parent, d->expr);

			logd("d->expr->parent->type: %d\n", d->expr->parent->type);

			if (_expr_multi_rets(d->expr) < 0) {
				loge("\n");
				return DFA_ERROR;
			}

			d->expr->semi_flag = semi_flag;
			d->expr = NULL;
		}

		logd("d->expr: %p, d->expr_local_flag: %d\n", d->expr, d->expr_local_flag);
	}

	return DFA_OK;
}

static int _expr_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_expr_fini_expr(parse, d, 0) < 0)
		return DFA_ERROR;

	return DFA_NEXT_WORD;
}

static int _expr_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	if (_expr_fini_expr(parse, d, 1) < 0)
		return DFA_ERROR;

	return DFA_OK;
}

static int _dfa_init_module_expr(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, expr, entry,      _expr_is_expr,        _expr_action_expr);
	DFA_MODULE_NODE(dfa, expr, number,     _expr_is_number,      _expr_action_number);
	DFA_MODULE_NODE(dfa, expr, unary_op,   _expr_is_unary_op,    _expr_action_unary_op);
	DFA_MODULE_NODE(dfa, expr, binary_op,  _expr_is_binary_op,   _expr_action_binary_op);
	DFA_MODULE_NODE(dfa, expr, unary_post, _expr_is_unary_post,  _expr_action_unary_post);

	DFA_MODULE_NODE(dfa, expr, lp,         dfa_is_lp,        _expr_action_lp);
	DFA_MODULE_NODE(dfa, expr, rp,         dfa_is_rp,        _expr_action_rp);
	DFA_MODULE_NODE(dfa, expr, rp_cast,    dfa_is_rp,        _expr_action_rp_cast);

	DFA_MODULE_NODE(dfa, expr, ls,         dfa_is_ls,        _expr_action_ls);
	DFA_MODULE_NODE(dfa, expr, rs,         dfa_is_rs,        _expr_action_rs);

	DFA_MODULE_NODE(dfa, expr, comma,      dfa_is_comma,     _expr_action_comma);
	DFA_MODULE_NODE(dfa, expr, semicolon,  dfa_is_semicolon, _expr_action_semicolon);

	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	assert(!md);

	md = calloc(1, sizeof(expr_module_data_t));
	if (!md) {
		loge("expr_module_data_t alloc error\n");
		return DFA_ERROR;
	}

	md->ls_exprs = stack_alloc();
	if (!md->ls_exprs)
		goto _ls_exprs;

	md->lp_exprs = stack_alloc();
	if (!md->lp_exprs)
		goto _lp_exprs;

	d->module_datas[dfa_module_expr.index] = md;

	return DFA_OK;

_lp_exprs:
	stack_free(md->ls_exprs);
_ls_exprs:
	loge("\n");

	free(md);
	md = NULL;
	return DFA_ERROR;
}

static int _dfa_fini_module_expr(dfa_t* dfa)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	if (md) {
		if (md->ls_exprs)
			stack_free(md->ls_exprs);

		if (md->lp_exprs)
			stack_free(md->lp_exprs);

		free(md);
		md = NULL;
		d->module_datas[dfa_module_expr.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_expr(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, expr,     entry,       expr);
	DFA_GET_MODULE_NODE(dfa, expr,     number,      number);
	DFA_GET_MODULE_NODE(dfa, expr,     unary_op,    unary_op);
	DFA_GET_MODULE_NODE(dfa, expr,     binary_op,   binary_op);
	DFA_GET_MODULE_NODE(dfa, expr,     unary_post,  unary_post);

	DFA_GET_MODULE_NODE(dfa, expr,     lp,          lp);
	DFA_GET_MODULE_NODE(dfa, expr,     rp,          rp);
	DFA_GET_MODULE_NODE(dfa, expr,     rp_cast,     rp_cast);

	DFA_GET_MODULE_NODE(dfa, expr,     ls,          ls);
	DFA_GET_MODULE_NODE(dfa, expr,     rs,          rs);

	DFA_GET_MODULE_NODE(dfa, expr,     comma,       comma);
	DFA_GET_MODULE_NODE(dfa, expr,     semicolon,   semicolon);

	DFA_GET_MODULE_NODE(dfa, identity, identity,    identity);
	DFA_GET_MODULE_NODE(dfa, call,     lp,          call_lp);
	DFA_GET_MODULE_NODE(dfa, call,     rp,          call_rp);

	DFA_GET_MODULE_NODE(dfa, sizeof,   _sizeof,     _sizeof);
	DFA_GET_MODULE_NODE(dfa, sizeof,   rp,          sizeof_rp);

	DFA_GET_MODULE_NODE(dfa, create,   create,      create);
	DFA_GET_MODULE_NODE(dfa, create,   identity,    create_id);
	DFA_GET_MODULE_NODE(dfa, create,   rp,          create_rp);

	DFA_GET_MODULE_NODE(dfa, type,     entry,       type_entry);
	DFA_GET_MODULE_NODE(dfa, type,     base_type,   base_type);
	DFA_GET_MODULE_NODE(dfa, type,     star,        star);

	DFA_GET_MODULE_NODE(dfa, va_arg,   arg,         va_arg);
	DFA_GET_MODULE_NODE(dfa, va_arg,   rp,          va_rp);

	DFA_GET_MODULE_NODE(dfa, container, container,  container);
	DFA_GET_MODULE_NODE(dfa, container, rp,         container_rp);

	// add expr to syntaxes
	vector_add(dfa->syntaxes, expr);

	// expr start with number, identity, an unary_op, '(',
	// like: a = b, *p = 1, (a + b)
	// number start may be only useful in return statement.
	dfa_node_add_child(expr,       number);
	dfa_node_add_child(expr,       identity);
	dfa_node_add_child(expr,       unary_op);
	dfa_node_add_child(expr,       unary_post);
	dfa_node_add_child(expr,       lp);
	dfa_node_add_child(expr,       semicolon);

	// container(ptr, type, member)
	dfa_node_add_child(expr,         container);
	dfa_node_add_child(container_rp, rp);
	dfa_node_add_child(container_rp, binary_op);
	dfa_node_add_child(container_rp, comma);
	dfa_node_add_child(container_rp, semicolon);

	// create class object
	dfa_node_add_child(expr,       create);
	dfa_node_add_child(create_id,  semicolon);
	dfa_node_add_child(create_rp,  semicolon);

	// va_arg(ap, type)
	dfa_node_add_child(expr,       va_arg);
	dfa_node_add_child(va_rp,      rp);
	dfa_node_add_child(va_rp,      binary_op);
	dfa_node_add_child(va_rp,      comma);
	dfa_node_add_child(va_rp,      semicolon);

	// sizeof()
	dfa_node_add_child(expr,       _sizeof);
	dfa_node_add_child(sizeof_rp,  rp);
	dfa_node_add_child(sizeof_rp,  binary_op);
	dfa_node_add_child(sizeof_rp,  comma);
	dfa_node_add_child(sizeof_rp,  semicolon);

	// (expr)
	dfa_node_add_child(lp,         identity);
	dfa_node_add_child(lp,         number);
	dfa_node_add_child(lp,         unary_op);
	dfa_node_add_child(lp,         _sizeof);
	dfa_node_add_child(lp,         lp);

	dfa_node_add_child(identity,   rp);
	dfa_node_add_child(number,     rp);
	dfa_node_add_child(rp,         rp);

	dfa_node_add_child(rp,         binary_op);
	dfa_node_add_child(identity,   binary_op);
	dfa_node_add_child(number,     binary_op);

	// type cast, like: (type*)var
	dfa_node_add_child(lp,         type_entry);
	dfa_node_add_child(base_type,  rp_cast);
	dfa_node_add_child(star,       rp_cast);
	dfa_node_add_child(identity,   rp_cast);

	dfa_node_add_child(rp_cast,    identity);
	dfa_node_add_child(rp_cast,    number);
	dfa_node_add_child(rp_cast,    unary_op);
	dfa_node_add_child(rp_cast,    _sizeof);
	dfa_node_add_child(rp_cast,    lp);

	// identity() means function call, implement in dfa_call.c
	dfa_node_add_child(identity,   call_lp);
	dfa_node_add_child(call_rp,    rp);
	dfa_node_add_child(call_rp,    binary_op);
	dfa_node_add_child(call_rp,    comma);
	dfa_node_add_child(call_rp,    semicolon);

	// array index, a[1 + 2], a[]
	// [] is a special binary op,
	// should be added before normal binary op such as '+'
	dfa_node_add_child(identity,   ls);
	dfa_node_add_child(ls,         expr);
	dfa_node_add_child(expr,       rs);
	dfa_node_add_child(ls,         rs);
	dfa_node_add_child(rs,         ls);
	dfa_node_add_child(rs,         binary_op);

	dfa_node_add_child(rs,         unary_post);
	dfa_node_add_child(rs,         rp);
	dfa_node_add_child(identity,   unary_post);

	// recursive unary_op, like: !*p
	dfa_node_add_child(unary_op,   unary_op);
	dfa_node_add_child(unary_op,   number);
	dfa_node_add_child(unary_op,   identity);
	dfa_node_add_child(unary_op,   expr);

	dfa_node_add_child(binary_op,  unary_op);
	dfa_node_add_child(binary_op,  number);
	dfa_node_add_child(binary_op,  identity);

	// create class object
	dfa_node_add_child(binary_op,  create);
	dfa_node_add_child(binary_op,  expr);

	dfa_node_add_child(unary_post, rp);
	dfa_node_add_child(unary_post, rs);
	dfa_node_add_child(unary_post, binary_op);
	dfa_node_add_child(unary_post, comma);
	dfa_node_add_child(unary_post, semicolon);

	dfa_node_add_child(rp,         comma);
	dfa_node_add_child(number,     comma);
	dfa_node_add_child(identity,   comma);
	dfa_node_add_child(rs,         comma);
	dfa_node_add_child(comma,      expr);

	dfa_node_add_child(rp,         semicolon);
	dfa_node_add_child(number,     semicolon);
	dfa_node_add_child(identity,   semicolon);
	dfa_node_add_child(rs,         semicolon);

	return 0;
}

dfa_module_t dfa_module_expr =
{
	.name        = "expr",

	.init_module = _dfa_init_module_expr,
	.init_syntax = _dfa_init_syntax_expr,

	.fini_module = _dfa_fini_module_expr,
};
