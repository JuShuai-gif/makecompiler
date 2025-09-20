#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_var;

int _expr_multi_rets(expr_t* e);

int _check_recursive(type_t* parent, type_t* child, lex_word_t* w)
{
	if (child->type == parent->type) {
	
		loge("recursive define '%s' type member var '%s' in type '%s', line: %d\n",
				child->name->data, w->text->data, parent->name->data, w->line);

		return DFA_ERROR;
	}

	if (child->scope) {
		assert(child->type >= STRUCT);

		variable_t* v      = NULL;
		type_t*     type_v = NULL;
		int i;

		for (i = 0; i < child->scope->vars->size; i++) {

			v = child->scope->vars->data[i];
			if (v->nb_pointers > 0 || v->type < STRUCT)
				continue;

			type_v = block_find_type_type((block_t*)child, v->type);
			assert(type_v);

			if (_check_recursive(parent, type_v, v->w) < 0)
				return DFA_ERROR;
		}
	}

	return DFA_OK;
}

static int _var_add_var(dfa_t* dfa, dfa_data_t* d)
{
	parse_t*     parse = dfa->priv;
	dfa_identity_t*  id    = stack_top(d->current_identities);

	if (id && id->identity) {

		variable_t* v = scope_find_variable(parse->ast->current_block->scope, id->identity->text->data);
		if (v) {
			loge("repeated declare var '%s', line: %d\n", id->identity->text->data, id->identity->line);
			return DFA_ERROR;
		}

		assert(d->current_identities->size >= 2);

		dfa_identity_t* id0 = d->current_identities->data[0];
		assert(id0 && id0->type);

		block_t* b = parse->ast->current_block;
		while (b) {
			if (b->node.type >= STRUCT || FUNCTION == b->node.type)
				break;
			b = (block_t*)b->node.parent;
		}

		uint32_t global_flag;
		uint32_t local_flag;
		uint32_t member_flag;

		if (!b) {
			local_flag  = 0;
			global_flag = 1;
			member_flag = 0;

		} else if (FUNCTION == b->node.type) {
			local_flag  = 1;
			global_flag = 0;
			member_flag = 0;

		} else if (b->node.type >= STRUCT) {
			local_flag  = 0;
			global_flag = 0;
			member_flag = 1;

			if (0 == id0->nb_pointers && id0->type->type >= STRUCT) {
				// if not pointer var, check if define recursive struct/union/class var

				if (_check_recursive((type_t*)b, id0->type, id->identity) < 0) {

					loge("recursive define when define var '%s', line: %d\n",
							id->identity->text->data, id->identity->line);
					return DFA_ERROR;
				}
			}
		}

		if (FUNCTION_PTR == id0->type->type
				&& (!id0->func_ptr || 0 == id0->nb_pointers)) {
			loge("invalid func ptr\n");
			return DFA_ERROR;
		}

		if (id0->extern_flag) {
			if (!global_flag) {
				loge("extern var must be global.\n");
				return DFA_ERROR;
			}

			variable_t* v = block_find_variable(parse->ast->current_block, id->identity->text->data);
			if (v) {
				loge("extern var already declared, line: %d\n", v->w->line);
				return DFA_ERROR;
			}
		}

		if (VAR_VOID == id0->type->type && 0 == id0->nb_pointers) {
			loge("void var must be a pointer, like void*\n");
			return DFA_ERROR;
		}

		v = VAR_ALLOC_BY_TYPE(id->identity, id0->type, id0->const_flag, id0->nb_pointers, id0->func_ptr);
		if (!v) {
			loge("alloc var failed\n");
			return DFA_ERROR;
		}
		v->local_flag  = local_flag;
		v->global_flag = global_flag;
		v->member_flag = member_flag;

		v->static_flag = id0->static_flag;
		v->extern_flag = id0->extern_flag;

		logi("type: %d, nb_pointers: %d,nb_dimentions: %d, var: %s,line:%d,pos:%d, local: %d, global: %d, member: %d, extern: %d, static: %d\n\n",
				v->type, v->nb_pointers, v->nb_dimentions,
				v->w->text->data, v->w->line, v->w->pos,
				v->local_flag,  v->global_flag, v->member_flag,
				v->extern_flag, v->static_flag);

		scope_push_var(parse->ast->current_block->scope, v);

		d->current_var   = v;
		d->current_var_w = id->identity;
		id0->nb_pointers = 0;
		id0->const_flag  = 0;
		id0->static_flag = 0;
		id0->extern_flag = 0;

		stack_pop(d->current_identities);
		free(id);
		id = NULL;
	}

	return 0;
}

static int _var_init_expr(dfa_t* dfa, dfa_data_t* d, vector_t* words, int semi_flag)
{
	parse_t*    parse = dfa->priv;
	variable_t* r     = NULL;
	lex_word_t* w     = words->data[words->size - 1];

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	assert(d->current_var);

	d->expr_local_flag--;

	if (d->current_var->global_flag
			|| (d->current_var->const_flag && 0 == d->current_var->nb_pointers + d->current_var->nb_dimentions)) {

		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("\n");

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("number of array should be constant, file: %s, line: %d\n", w->file->data, w->line);

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		expr_free(d->expr);

	} else {
		assert(d->expr->nb_nodes > 0);

		node_add_child((node_t*)parse->ast->current_block, (node_t*)d->expr);

		logd("d->expr->parent->type: %d\n", d->expr->parent->type);

		if (_expr_multi_rets(d->expr) < 0) {
			loge("\n");
			return DFA_ERROR;
		}

		d->expr->semi_flag = semi_flag;
	}

	d->expr = NULL;
	return 0;
}

static int _var_add_vla(ast_t* ast, variable_t* vla)
{
	function_t* f   = NULL;
	expr_t*     e   = NULL;
	expr_t*     e2  = NULL;
	node_t*     mul = NULL;

	if (ast_find_function(&f, ast, "printf") < 0 || !f) {
		loge("printf() NOT found, which used to print error message when the variable length of array '%s' <= 0, file: %s, line: %d\n",
				vla->w->text->data, vla->w->file->data, vla->w->line);
		return DFA_ERROR;
	}

	int size = vla->data_size;
	int i;

	for (i = 0; i < vla->nb_dimentions; i++) {

		if (vla->dimentions[i].num > 0) {
			size *= vla->dimentions[i].num;
			continue;
		}

		if (0 == vla->dimentions[i].num) {
			loge("\n");

			expr_free(e);
			return DFA_ERROR;
		}

		if (!vla->dimentions[i].vla) {
			loge("\n");

			expr_free(e);
			return DFA_ERROR;
		}

		if (!e) {
			e = expr_clone(vla->dimentions[i].vla);
			if (!e)
				return -ENOMEM;
			continue;
		}

		e2 = expr_clone(vla->dimentions[i].vla);
		if (!e2) {
			expr_free(e);
			return -ENOMEM;
		}

		mul = node_alloc(vla->w, OP_MUL, NULL);
		if (!mul) {
			expr_free(e2);
			expr_free(e);
			return -ENOMEM;
		}

		int ret = expr_add_node(e, mul);
		if (ret < 0) {
			expr_free(mul);
			expr_free(e2);
			expr_free(e);
			return ret;
		}

		ret = expr_add_node(e, e2);
		if (ret < 0) {
			expr_free(e2);
			expr_free(e);
			return ret;
		}
	}

	assert(e);

	variable_t* v;
	type_t*     t;
	node_t*     node;

	if (size > 1) {
		mul = node_alloc(vla->w, OP_MUL, NULL);
		if (!mul) {
			expr_free(e);
			return -ENOMEM;
		}

		int ret = expr_add_node(e, mul);
		if (ret < 0) {
			expr_free(mul);
			expr_free(e);
			return ret;
		}

		t = block_find_type_type(ast->current_block, VAR_INT);
		v = VAR_ALLOC_BY_TYPE(vla->w, t, 1, 0, NULL);
		if (!v) {
			expr_free(e);
			return DFA_ERROR;
		}
		v->data.i64    = size;
		v->global_flag = 1;
		v->const_literal_flag = 1;

		node = node_alloc(NULL, v->type, v);
		variable_free(v);
		v = NULL;
		if (!node) {
			expr_free(e);
			return DFA_ERROR;
		}

		ret = expr_add_node(e, node);
		if (ret < 0) {
			node_free(node);
			expr_free(e);
			return ret;
		}
	}

	node_t*  assign;
	node_t*  len;
	node_t*  alloc;

	// len = e
	assign = node_alloc(vla->w, OP_ASSIGN, NULL);
	if (!assign) {
		expr_free(e);
		return DFA_ERROR;
	}

	node_add_child(assign, e->nodes[0]);
	e->nodes[0]    = assign;
	assign->parent = e;

	node_add_child((node_t*)ast->current_block, e);
	e = NULL;

	t = block_find_type_type(ast->current_block, VAR_INT);
	v = VAR_ALLOC_BY_TYPE(vla->w, t, 0, 0, NULL);
	if (!v)
		return DFA_ERROR;
	v->tmp_flag = 1;

	len = node_alloc(NULL, v->type, v);
	if (!len) {
		variable_free(v);
		return DFA_ERROR;
	}

	node_add_child(assign, len);
	XCHG(assign->nodes[0], assign->nodes[1]);

	// vla_alloc(vla, len, printf, msg)
	len = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!len)
		return DFA_ERROR;

	alloc = node_alloc(vla->w, OP_VLA_ALLOC, NULL);
	if (!alloc) {
		node_free(len);
		return -ENOMEM;
	}

	// vla node
	node = node_alloc(NULL, vla->type, vla);
	if (!node) {
		node_free(len);
		node_free(alloc);
		return DFA_ERROR;
	}

	node_add_child(alloc, node);
	node_add_child(alloc, len);
	node = NULL;
	len  = NULL;

	// printf() node
	t = block_find_type_type(ast->current_block, FUNCTION_PTR);
	v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		node_free(alloc);
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;

	node = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!node) {
		node_free(alloc);
		return DFA_ERROR;
	}

	node_add_child(alloc, node);
	node = NULL;

	// msg
	char msg[1024];
	snprintf(msg, sizeof(msg) - 1, "\033[31merror:\033[0m variable length '%%d' of array '%s' not more than 0, file: %s, line: %d\n",
			vla->w->text->data, vla->w->file->data, vla->w->line);

	t = block_find_type_type(ast->current_block, VAR_CHAR);
	v = VAR_ALLOC_BY_TYPE(vla->w, t, 1, 1, NULL);
	if (!v) {
		node_free(alloc);
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->global_flag = 1;

	v->data.s = string_cstr(msg);
	if (!v->data.s) {
		node_free(alloc);
		variable_free(v);
		return -ENOMEM;
	}

	node = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!node) {
		node_free(alloc);
		return DFA_ERROR;
	}
	node_add_child(alloc, node);
	node = NULL;

	node_add_child((node_t*)ast->current_block, alloc);
	return 0;
}

static int _var_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	if (d->current_var) {
		variable_size(d->current_var);

		if (d->current_var->vla_flag) {

			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return DFA_ERROR;
		}
	}

	if (d->expr_local_flag > 0 && _var_init_expr(dfa, d, words, 0) < 0)
		return DFA_ERROR;

	return DFA_SWITCH_TO;
}

static int _var_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_identity_t*  id    = NULL;

	d->var_semicolon_flag = 0;

	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	id = stack_pop(d->current_identities);
	assert(id && id->type);
	free(id);
	id = NULL;

	if (d->current_var) {
		variable_size(d->current_var);

		if (d->current_var->vla_flag) {

			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return DFA_ERROR;
		}
	}

	if (d->expr_local_flag > 0) {

		if (_var_init_expr(dfa, d, words, 1) < 0)
			return DFA_ERROR;

	} else if (d->expr) {
		expr_free(d->expr);
		d->expr = NULL;
	}

	node_t* b = (node_t*)parse->ast->current_block;

	if (b->nb_nodes > 0)
		b->nodes[b->nb_nodes - 1]->semi_flag = 1;

	return DFA_OK;
}

static int _var_action_assign(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	lex_word_t*  w = words->data[words->size - 1];

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	if (d->current_var->extern_flag) {
		loge("extern var '%s' can't be inited here, line: %d\n",
				d->current_var->w->text->data, w->line);
		return DFA_ERROR;
	}

	if (d->current_var->vla_flag) {

		if (_var_add_vla(parse->ast, d->current_var) < 0)
			return DFA_ERROR;
	}

	if (d->current_var->nb_dimentions > 0) {
		logi("var array '%s' init, nb_dimentions: %d\n",
				d->current_var->w->text->data, d->current_var->nb_dimentions);
		return DFA_NEXT_WORD;
	}

	operator_t* op = find_base_operator_by_type(OP_ASSIGN);
	node_t*     n0 = node_alloc(w, op->type, NULL);
	n0->op = op;

	node_t*     n1 = node_alloc(d->current_var_w, d->current_var->type, d->current_var);
	expr_t*     e  = expr_alloc();

	node_add_child(n0, n1);
	expr_add_node(e, n0);

	d->expr         = e;
	d->expr_local_flag++;

	if (!d->var_semicolon_flag) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "var_semicolon"), DFA_HOOK_POST);
		d->var_semicolon_flag = 1;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "var_comma"), DFA_HOOK_POST);

	logd("d->expr: %p\n", d->expr);

	return DFA_NEXT_WORD;
}

static int _var_action_colon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;
}

static int _var_action_bits(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	lex_word_t* w     = words->data[words->size - 1];

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!d->current_var->member_flag) {
		loge("bits var '%s' must be a member of struct, file: %s, line: %d\n",
				d->current_var->w->text->data, d->current_var->w->file->data, d->current_var->w->line);
		return DFA_ERROR;
	}

	d->current_var->bit_size = w->data.u32;
	return DFA_NEXT_WORD;
}

static int _var_action_ls(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	assert(!d->expr);
	variable_add_array_dimention(d->current_var, -1, NULL);
	d->current_var->const_literal_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "var_rs"), DFA_HOOK_POST);

	d->nb_lss++;

	return DFA_NEXT_WORD;
}

static int _var_action_rs(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	variable_t* r     = NULL;
	lex_word_t* w     = words->data[words->size - 1];

	d->nb_rss++;

	logd("d->expr: %p\n", d->expr);

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		assert(d->current_var->dim_index < d->current_var->nb_dimentions);

		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {

			if (!d->current_var->local_flag) {
				loge("variable length array '%s' must in local scope, file: %s, line: %d\n",
						d->current_var->w->text->data, w->file->data, w->line);

				variable_free(r);
				r = NULL;

				expr_free(d->expr);
				d->expr = NULL;
				return DFA_ERROR;
			}

			logw("define variable length array, file: %s, line: %d\n", w->file->data, w->line);

			d->current_var->dimentions[d->current_var->dim_index].vla = d->expr;
			d->current_var->vla_flag = 1;
			d->expr = NULL;
		} else {
			d->current_var->dimentions[d->current_var->dim_index].num = r->data.i;

			logi("dimentions: %d, size: %d\n",
					d->current_var->dim_index, d->current_var->dimentions[d->current_var->dim_index].num);

			expr_free(d->expr);
			d->expr = NULL;
		}

		d->current_var->dim_index++;

		variable_free(r);
		r = NULL;
	}

	return DFA_SWITCH_TO;
}

static int _dfa_init_module_var(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, var, comma,     dfa_is_comma,         _var_action_comma);
	DFA_MODULE_NODE(dfa, var, semicolon, dfa_is_semicolon,     _var_action_semicolon);

	DFA_MODULE_NODE(dfa, var, ls,        dfa_is_ls,            _var_action_ls);
	DFA_MODULE_NODE(dfa, var, rs,        dfa_is_rs,            _var_action_rs);

	DFA_MODULE_NODE(dfa, var, assign,    dfa_is_assign,        _var_action_assign);

	DFA_MODULE_NODE(dfa, var, colon,     dfa_is_colon,         _var_action_colon);
	DFA_MODULE_NODE(dfa, var, bits,      dfa_is_const_integer, _var_action_bits);

	return DFA_OK;
}

static int _dfa_init_syntax_var(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, var,    comma,     comma);
	DFA_GET_MODULE_NODE(dfa, var,    semicolon, semicolon);

	DFA_GET_MODULE_NODE(dfa, var,    ls,        ls);
	DFA_GET_MODULE_NODE(dfa, var,    rs,        rs);
	DFA_GET_MODULE_NODE(dfa, var,    assign,    assign);

	DFA_GET_MODULE_NODE(dfa, var,    colon,     colon);
	DFA_GET_MODULE_NODE(dfa, var,    bits,      bits);

	DFA_GET_MODULE_NODE(dfa, type,   star,      star);
	DFA_GET_MODULE_NODE(dfa, type,   identity,  identity);

	DFA_GET_MODULE_NODE(dfa, expr,   entry,     expr);

	DFA_GET_MODULE_NODE(dfa, init_data, entry,  init_data);
	DFA_GET_MODULE_NODE(dfa, init_data, rb,     init_rb);


	dfa_node_add_child(identity,  comma);
	dfa_node_add_child(comma,     star);
	dfa_node_add_child(comma,     identity);

	// array var
	dfa_node_add_child(identity,  ls);
	dfa_node_add_child(ls,        rs);
	dfa_node_add_child(ls,        expr);
	dfa_node_add_child(expr,      rs);
	dfa_node_add_child(rs,        ls);
	dfa_node_add_child(rs,        comma);
	dfa_node_add_child(rs,        semicolon);

	// bits
	dfa_node_add_child(identity,  colon);
	dfa_node_add_child(colon,     bits);
	dfa_node_add_child(bits,      semicolon);

	// var init
	dfa_node_add_child(rs,        assign);
	dfa_node_add_child(identity,  assign);

	// normal var init
	dfa_node_add_child(assign,    expr);
	dfa_node_add_child(expr,      comma);
	dfa_node_add_child(expr,      semicolon);

	// struct or array init
	dfa_node_add_child(assign,    init_data);
	dfa_node_add_child(init_rb,   comma);
	dfa_node_add_child(init_rb,   semicolon);

	dfa_node_add_child(identity,  semicolon);
	return 0;
}

dfa_module_t dfa_module_var = 
{
	.name        = "var",
	.init_module = _dfa_init_module_var,
	.init_syntax = _dfa_init_syntax_var,
};
