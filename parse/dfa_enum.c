#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_enum;

typedef struct {
	lex_word_t*  current_enum;
	variable_t*  current_v;

	dfa_hook_t*  hook;

	vector_t*    vars;

	int              nb_lbs;
	int              nb_rbs;

} enum_module_data_t;

static int _enum_action_type(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];

	if (md->current_enum) {
		loge("\n");
		return DFA_ERROR;
	}

	type_t* t = block_find_type(parse->ast->root_block, w->text->data);
	if (!t) {
		t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scope_push_type(parse->ast->root_block->scope, t);
	}

	md->current_enum = w;

	return DFA_NEXT_WORD;
}

static int _enum_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];
	type_t*          t     = NULL;

	if (md->nb_lbs > 0) {
		loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	if (md->current_enum) {

		t = block_find_type(parse->ast->root_block, md->current_enum->text->data);
		if (!t) {
			loge("type '%s' not found\n", md->current_enum->text->data);
			return DFA_ERROR;
		}
	} else {
		t = type_alloc(w, "anonymous", STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scope_push_type(parse->ast->root_block->scope, t);

		md->current_enum = w;
	}

	md->nb_lbs++;

	return DFA_NEXT_WORD;
}

static int _enum_action_var(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];

	variable_t*      v0    = NULL;
	variable_t*      v     = NULL;
	type_t*          t     = NULL;

	if (!md->current_enum) {
		loge("\n");
		return DFA_ERROR;
	}

	if (ast_find_type_type(&t, parse->ast, VAR_INT) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	v = block_find_variable(parse->ast->root_block, w->text->data);
	if (v) {
		loge("repeated declared enum var '%s', 1st in file: %s, line: %d\n", w->text->data, v->w->file->data, v->w->line);
		return DFA_ERROR;
	}

	v = VAR_ALLOC_BY_TYPE(w, t, 1, 0, NULL);
	if (!v) {
		loge("var alloc failed\n");
		return DFA_ERROR;
	}

	v->const_literal_flag = 1;

	if (md->vars->size > 0) {
		v0          = md->vars->data[md->vars->size - 1];
		v->data.i64 = v0->data.i64 + 1;
	} else
		v->data.i64 = 0;

	if (vector_add(md->vars, v) < 0) {
		loge("var add failed\n");
		return DFA_ERROR;
	}

	scope_push_var(parse->ast->root_block->scope, v);

	md->current_v = v;

	return DFA_NEXT_WORD;
}

static int _enum_action_assign(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];

	if (!md->current_v) {
		loge("no enum var before '=' in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	assert(!d->expr);
	d->expr_local_flag++;

	md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "enum_comma"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _enum_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];
	variable_t*      r     = NULL;

	if (!md->current_v) {
		loge("enum var before ',' not found, in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");
			return DFA_ERROR;
		}

		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;
		}

		md->current_v->data.i64 = r->data.i64;

		variable_free(r);
		r = NULL;

		expr_free(d->expr);
		d->expr = NULL;
		d->expr_local_flag--;
	}

	logi("enum var: '%s', value: %ld\n", md->current_v->w->text->data, md->current_v->data.i64);

	md->current_v = NULL;

	return DFA_SWITCH_TO;
}

static int _enum_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];
	variable_t*      r     = NULL;

	if (md->nb_lbs > 1) {
		loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	md->nb_rbs++;

	assert(md->nb_rbs == md->nb_lbs);

	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;

		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");
			return DFA_ERROR;
		}

		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;
		}

		md->current_v->data.i64 = r->data.i64;

		variable_free(r);
		r = NULL;

		expr_free(d->expr);
		d->expr = NULL;
		d->expr_local_flag--;
	}

	if (md->hook) {
		dfa_del_hook(&(dfa->hooks[DFA_HOOK_POST]), md->hook);
		md->hook = NULL;
	}

	md->nb_lbs = 0;
	md->nb_rbs = 0;

	md->current_enum = NULL;
	md->current_v    = NULL;

	vector_clear(md->vars, NULL);

	return DFA_NEXT_WORD;
}

static int _enum_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];

	if (0 != md->nb_rbs || 0 != md->nb_lbs) {
		loge("'{' and '}' not same in enum, file: %s, line: %d\n", w->file->data, w->line);

		md->nb_rbs = 0;
		md->nb_lbs = 0;
		return DFA_ERROR;
	}

	if (md->current_v) {
		loge("enum var '%s' should be followed by ',' or '}', file: %s, line: %d\n",
				md->current_v->w->text->data,
				md->current_v->w->file->data,
				md->current_v->w->line);

		md->current_v = NULL;
		return DFA_ERROR;
	}

	return DFA_OK;
}

static int _dfa_init_module_enum(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, enum, _enum,     dfa_is_enum,      NULL);

	DFA_MODULE_NODE(dfa, enum, type,      dfa_is_identity,  _enum_action_type);

	DFA_MODULE_NODE(dfa, enum, lb,        dfa_is_lb,        _enum_action_lb);
	DFA_MODULE_NODE(dfa, enum, rb,        dfa_is_rb,        _enum_action_rb);
	DFA_MODULE_NODE(dfa, enum, semicolon, dfa_is_semicolon, _enum_action_semicolon);

	DFA_MODULE_NODE(dfa, enum, var,       dfa_is_identity,  _enum_action_var);
	DFA_MODULE_NODE(dfa, enum, assign,    dfa_is_assign,    _enum_action_assign);
	DFA_MODULE_NODE(dfa, enum, comma,     dfa_is_comma,     _enum_action_comma);

	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	assert(!md);

	md = calloc(1, sizeof(enum_module_data_t));
	if (!md) {
		loge("\n");
		return DFA_ERROR;
	}

	md->vars = vector_alloc();
	if (!md->vars) {
		loge("\n");
		free(md);
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_enum.index] = md;

	return DFA_OK;
}

static int _dfa_fini_module_enum(dfa_t* dfa)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_enum.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_enum(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, enum,  _enum,    _enum);
	DFA_GET_MODULE_NODE(dfa, enum,  type,      type);
	DFA_GET_MODULE_NODE(dfa, enum,  lb,        lb);
	DFA_GET_MODULE_NODE(dfa, enum,  rb,        rb);
	DFA_GET_MODULE_NODE(dfa, enum,  semicolon, semicolon);

	DFA_GET_MODULE_NODE(dfa, enum,  assign,    assign);
	DFA_GET_MODULE_NODE(dfa, enum,  comma,     comma);
	DFA_GET_MODULE_NODE(dfa, enum,  var,       var);
	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);

	vector_add(dfa->syntaxes,  _enum);

	// enum start
	dfa_node_add_child(_enum,   type);
	dfa_node_add_child(type,    semicolon);
	dfa_node_add_child(type,    lb);

	// anonymous enum
	dfa_node_add_child(_enum,   lb);

	// empty enum
	dfa_node_add_child(lb,      rb);

	// const member var
	dfa_node_add_child(lb,      var);
	dfa_node_add_child(var,     comma);
	dfa_node_add_child(var,     assign);
	dfa_node_add_child(assign,  expr);
	dfa_node_add_child(expr,    comma);
	dfa_node_add_child(comma,   var);

	dfa_node_add_child(var,     rb);
	dfa_node_add_child(expr,    rb);

	// end
	dfa_node_add_child(rb,      semicolon);

	return 0;
}

dfa_module_t dfa_module_enum = 
{
	.name        = "enum",
	.init_module = _dfa_init_module_enum,
	.init_syntax = _dfa_init_syntax_enum,

	.fini_module = _dfa_fini_module_enum,
};
