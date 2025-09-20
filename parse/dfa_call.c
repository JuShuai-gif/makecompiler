#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_call;

typedef struct {

	int              nb_lps;
	int              nb_rps;

	node_t*      func;
	node_t*      call;
	vector_t*    argv;

	expr_t*      parent_expr;

} dfa_call_data_t;

static int _call_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*       d  = data;
	stack_t*      s  = d->module_datas[dfa_module_call.index];
	dfa_call_data_t*  cd = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	cd->nb_lps++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}


static int _call_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse     = dfa->priv;
	dfa_data_t*       d         = data;
	lex_word_t*   w1        = words->data[words->size - 1];
	stack_t*      s         = d->module_datas[dfa_module_call.index];
	function_t*   f         = NULL;
	dfa_call_data_t*  cd        = NULL;

	variable_t*   var_pf    = NULL;
	node_t*       node_pf   = NULL;
	type_t*       pt        = NULL;

	node_t*       node_call = NULL;
	operator_t*   op        = find_base_operator_by_type(OP_CALL);

	if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;

	assert(pt);
	assert(op);

	dfa_identity_t* id = stack_top(d->current_identities);
	if (id && id->identity) {

		int ret = ast_find_function(&f, parse->ast, id->identity->text->data);
		if (ret < 0)
			return DFA_ERROR;

		if (f) {
			logd("f: %p, %s\n", f, f->node.w->text->data);

			var_pf = VAR_ALLOC_BY_TYPE(id->identity, pt, 1, 1, f);
			if (!var_pf) {
				loge("var alloc error\n");
				return DFA_ERROR;
			}

			var_pf->const_flag = 1;
			var_pf->const_literal_flag = 1;

		} else {
			ret = ast_find_variable(&var_pf, parse->ast, id->identity->text->data);
			if (ret < 0)
				return DFA_ERROR;

			if (!var_pf) {
				loge("funcptr var '%s' not found\n", id->identity->text->data);
				return DFA_ERROR;
			}

			if (FUNCTION_PTR != var_pf->type || !var_pf->func_ptr) {
				loge("invalid function ptr\n");
				return DFA_ERROR;
			}
		}

		node_pf = node_alloc(NULL, var_pf->type, var_pf);
		if (!node_pf) {
			loge("node alloc failed\n");
			return DFA_ERROR;
		}

		stack_pop(d->current_identities);
		free(id);
		id = NULL;
	} else {
		// f()(), function f should return a function pointer
		loge("\n");
		return DFA_ERROR;
	}

	node_call = node_alloc(w1, OP_CALL, NULL);
	if (!node_call) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}
	node_call->op = op;

	cd = calloc(1, sizeof(dfa_call_data_t));
	if (!cd) {
		loge("dfa data alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);

	cd->func           = node_pf;
	cd->call           = node_call;
	cd->parent_expr    = d->expr;
	d->expr            = NULL;
	d->expr_local_flag++;

	stack_push(s, cd);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _call_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	if (words->size < 2) {
		loge("\n");
		return DFA_ERROR;
	}

	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_call.index];
	dfa_call_data_t*  cd    = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	cd->nb_rps++;

	logd("cd->nb_lps: %d, cd->nb_rps: %d\n", cd->nb_lps, cd->nb_rps);

	if (cd->nb_rps < cd->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

		logd("d->expr: %p\n", d->expr);
		return DFA_NEXT_WORD;
	}
	assert(cd->nb_rps == cd->nb_lps);

	stack_pop(s);

	if (cd->parent_expr) {
		expr_add_node(cd->parent_expr, cd->func);
		expr_add_node(cd->parent_expr, cd->call);
	} else {
		node_add_child(cd->call, cd->func);
	}

	if (cd->argv) {
		int i;
		for (i = 0; i < cd->argv->size; i++)
			node_add_child(cd->call, cd->argv->data[i]);

		vector_free(cd->argv);
		cd->argv = NULL;
	}

	// the last arg
	if (d->expr) {
		node_add_child(cd->call, d->expr);
		d->expr = NULL;
	}

	if (cd->parent_expr)
		d->expr = cd->parent_expr;
	else
		d->expr = cd->call;

	d->expr_local_flag--;

	logd("d->expr: %p\n", d->expr);

	free(cd);
	cd = NULL;

	return DFA_NEXT_WORD;
}

static int _call_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	if (words->size < 2) {
		printf("%s(),%d, error: \n", __func__, __LINE__);
		return DFA_ERROR;
	}

	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_call.index];

	dfa_call_data_t*  cd    = stack_top(s);
	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!cd->argv)
		cd->argv = vector_alloc();

	vector_add(cd->argv, d->expr);
	d->expr = NULL;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _dfa_init_module_call(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, call, lp,       dfa_is_lp,    _call_action_lp);
	DFA_MODULE_NODE(dfa, call, rp,       dfa_is_rp,    _call_action_rp);

	DFA_MODULE_NODE(dfa, call, lp_stat,  dfa_is_lp,    _call_action_lp_stat);

	DFA_MODULE_NODE(dfa, call, comma,    dfa_is_comma, _call_action_comma);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_call.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_call.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_call(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_call.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_call.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_call(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, call,   lp,       lp);
	DFA_GET_MODULE_NODE(dfa, call,   rp,       rp);
	DFA_GET_MODULE_NODE(dfa, call,   comma,    comma);

	DFA_GET_MODULE_NODE(dfa, expr,   entry,    expr);

	DFA_GET_MODULE_NODE(dfa, create, create,   create);
	DFA_GET_MODULE_NODE(dfa, create, identity, create_id);
	DFA_GET_MODULE_NODE(dfa, create, rp,       create_rp);

	// no args
	dfa_node_add_child(lp,       rp);

	// have args

	// arg: create class object, such as: list.push(new A);
	dfa_node_add_child(lp,        create);
	dfa_node_add_child(create_id, comma);
	dfa_node_add_child(create_id, rp);
	dfa_node_add_child(create_rp, comma);
	dfa_node_add_child(create_rp, rp);
	dfa_node_add_child(comma,     create);

	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     comma);
	dfa_node_add_child(comma,    expr);
	dfa_node_add_child(expr,     rp);

	return 0;
}

dfa_module_t dfa_module_call =
{
	.name        = "call",
	.init_module = _dfa_init_module_call,
	.init_syntax = _dfa_init_syntax_call,

	.fini_module = _dfa_fini_module_call,
};
