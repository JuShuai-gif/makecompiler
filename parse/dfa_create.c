#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_create;

typedef struct {

	int              nb_lps;
	int              nb_rps;

	node_t*      create;

	expr_t*      parent_expr;

} create_module_data_t;

static int _create_is_create(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CREATE == w->type;
}

static int _create_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*           d  = data;
	stack_t*          s  = d->module_datas[dfa_module_create.index];
	create_module_data_t* md = d->module_datas[dfa_module_create.index];

	md->nb_lps++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _create_action_create(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	CHECK_ERROR(md->create, DFA_ERROR, "\n");

	md->create = node_alloc(w, OP_CREATE, NULL);
	if (!md->create)
		return DFA_ERROR;

	md->nb_lps      = 0;
	md->nb_rps      = 0;
	md->parent_expr = d->expr;

	return DFA_NEXT_WORD;
}

static int _create_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	type_t* t  = NULL;
	type_t* pt = NULL;

	if (ast_find_type(&t, parse->ast, w->text->data) < 0)
		return DFA_ERROR;

	if (!t) {
		loge("type '%s' not found\n", w->text->data);
		return DFA_ERROR;
	}

	if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;
	assert(pt);

	variable_t* var = VAR_ALLOC_BY_TYPE(w, pt, 1, 1, NULL);
	CHECK_ERROR(!var, DFA_ERROR, "var '%s' alloc failed\n", w->text->data);
	var->const_literal_flag = 1;

	node_t* node = node_alloc(NULL, var->type, var);
	CHECK_ERROR(!node, DFA_ERROR, "node alloc failed\n");

	int ret = node_add_child(md->create, node);
	CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");

	w = dfa->ops->pop_word(dfa);
	if (LEX_WORD_LP != w->type) {

		if (d->expr) {
			ret = expr_add_node(d->expr, md->create);
			CHECK_ERROR(ret < 0, DFA_ERROR, "expr add child failed\n");
		} else
			d->expr = md->create;

		md->create = NULL;
	}
	dfa->ops->push_word(dfa, w);

	return DFA_NEXT_WORD;
}

static int _create_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	logd("d->expr: %p\n", d->expr);

	d->expr = NULL;
	d->expr_local_flag++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _create_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	md->nb_rps++;

	logd("md->nb_lps: %d, md->nb_rps: %d\n", md->nb_lps, md->nb_rps);

	if (md->nb_rps < md->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}
	assert(md->nb_rps == md->nb_lps);

	if (d->expr) {
		int ret = node_add_child(md->create, d->expr);
		d->expr = NULL;
		CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");
	}

	d->expr = md->parent_expr;
	d->expr_local_flag--;

	if (d->expr) {
		int ret = expr_add_node(d->expr, md->create);
		CHECK_ERROR(ret < 0, DFA_ERROR, "expr add child failed\n");
	} else
		d->expr = md->create;

	md->create = NULL;

	logi("d->expr: %p\n", d->expr);

	return DFA_NEXT_WORD;
}

static int _create_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	CHECK_ERROR(!d->expr, DFA_ERROR, "\n");

	int ret = node_add_child(md->create, d->expr);
	d->expr = NULL;
	CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");

	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _dfa_init_module_create(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, create, create,    _create_is_create,    _create_action_create);

	DFA_MODULE_NODE(dfa, create, identity,  dfa_is_identity,  _create_action_identity);

	DFA_MODULE_NODE(dfa, create, lp,        dfa_is_lp,        _create_action_lp);
	DFA_MODULE_NODE(dfa, create, rp,        dfa_is_rp,        _create_action_rp);

	DFA_MODULE_NODE(dfa, create, lp_stat,   dfa_is_lp,        _create_action_lp_stat);

	DFA_MODULE_NODE(dfa, create, comma,     dfa_is_comma,     _create_action_comma);

	parse_t*       parse = dfa->priv;
	dfa_data_t*  d     = parse->dfa_data;
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	assert(!md);

	md = calloc(1, sizeof(create_module_data_t));
	if (!md) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_create.index] = md;

	return DFA_OK;
}

static int _dfa_fini_module_create(dfa_t* dfa)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_create.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_create(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, create,  create,    create);
	DFA_GET_MODULE_NODE(dfa, create,  identity,  identity);
	DFA_GET_MODULE_NODE(dfa, create,  lp,        lp);
	DFA_GET_MODULE_NODE(dfa, create,  rp,        rp);
	DFA_GET_MODULE_NODE(dfa, create,  comma,     comma);

	DFA_GET_MODULE_NODE(dfa, expr,    entry,     expr);

	dfa_node_add_child(create,   identity);
	dfa_node_add_child(identity, lp);

	dfa_node_add_child(lp,       rp);

	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     comma);
	dfa_node_add_child(comma,    expr);
	dfa_node_add_child(expr,     rp);

	return 0;
}

dfa_module_t dfa_module_create =
{
	.name        = "create",
	.init_module = _dfa_init_module_create,
	.init_syntax = _dfa_init_syntax_create,

	.fini_module = _dfa_fini_module_create,
};
