#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_async;

static int _async_is_async(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ASYNC == w->type;
}

static int _async_action_async(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];

	if (d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	d->current_async_w = w;

	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "async_semicolon"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _async_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;
	expr_t*   e     = d->expr;

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	while (OP_EXPR == e->type) {

		assert(e->nodes && 1 == e->nb_nodes);

		e = e->nodes[0];
	}

	if (OP_CALL != e->type) {
		loge("\n");
		return DFA_ERROR;
	}

	e->parent->nodes[0] = NULL;

	expr_free(d->expr);
	d->expr = NULL;
	d->expr_local_flag = 0;

	type_t*     t   = NULL;
	function_t* f   = NULL;
	variable_t* v   = NULL;
	node_t*     pf  = NULL;
	node_t*     fmt = NULL;

	if (ast_find_type_type(&t, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;

	if (ast_find_function(&f, parse->ast, "async") < 0)
		return DFA_ERROR;
	if (!f) {
		loge("\n");
		return DFA_ERROR;
	}

	v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		loge("\n");
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;

	pf = node_alloc(d->current_async_w, v->type, v);
	if (!pf) {
		loge("\n");
		return DFA_ERROR;
	}

	if (ast_find_type_type(&t, parse->ast, VAR_CHAR) < 0)
		return DFA_ERROR;

	v = VAR_ALLOC_BY_TYPE(d->current_async_w, t, 1, 1, NULL);
	if (!v) {
		loge("\n");
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->data.s = string_cstr("");

	fmt = node_alloc(d->current_async_w, v->type, v);
	if (!fmt) {
		loge("\n");
		return DFA_ERROR;
	}

	node_add_child(e, pf);
	node_add_child(e, fmt);

	int i;
	for (i = e->nb_nodes - 3; i >= 0; i--)
		e->nodes[i + 2] = e->nodes[i];

	e->nodes[0] = pf;
	e->nodes[1] = e->nodes[2];
	e->nodes[2] = fmt;

	if (d->current_node)
		node_add_child(d->current_node, e);
	else
		node_add_child((node_t*)parse->ast->current_block, e);

	d->current_async_w = NULL;

	return DFA_OK;
}

static int _dfa_init_module_async(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, async, semicolon, dfa_is_semicolon, _async_action_semicolon);
	DFA_MODULE_NODE(dfa, async, async,     _async_is_async,      _async_action_async);

	return DFA_OK;
}

static int _dfa_init_syntax_async(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa,  async,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa,  async,   async,     async);
	DFA_GET_MODULE_NODE(dfa,  expr,    entry,     expr);

	dfa_node_add_child(async, expr);
	dfa_node_add_child(expr,  semicolon);

	return 0;
}

dfa_module_t dfa_module_async =
{
	.name        = "async",
	.init_module = _dfa_init_module_async,
	.init_syntax = _dfa_init_syntax_async,
};
