#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t  dfa_module_return;

static int _return_action_return(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse   = dfa->priv;
	dfa_data_t*      d       = data;
	lex_word_t*  w       = words->data[words->size - 1];
	node_t*      _return = NULL;

	if (d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	_return = node_alloc(w, OP_RETURN, NULL);
	if (!_return) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _return);
	else
		node_add_child((node_t*)parse->ast->current_block, _return);

	d->current_return  = _return;
	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_comma"),     DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _return_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (d->expr) {
		node_add_child(d->current_return, d->expr);
		d->expr = NULL;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_comma"),     DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _return_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (d->expr) {
		node_add_child(d->current_return, d->expr);
		d->expr = NULL;
	}

	d->expr_local_flag = 0;

	if (d->current_return->nb_nodes > 4) {
		loge("return values must NOT more than 4!\n");
		return DFA_ERROR;
	}

	d->current_return  = NULL;

	return DFA_OK;
}

static int _dfa_init_module_return(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, return, semicolon, dfa_is_semicolon, _return_action_semicolon);
	DFA_MODULE_NODE(dfa, return, comma,     dfa_is_comma,     _return_action_comma);
	DFA_MODULE_NODE(dfa, return, _return,   dfa_is_return,    _return_action_return);

	return DFA_OK;
}

static int _dfa_init_syntax_return(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, return,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, return,   comma,     comma);
	DFA_GET_MODULE_NODE(dfa, return,   _return,   _return);
	DFA_GET_MODULE_NODE(dfa, expr,     entry,     expr);

	dfa_node_add_child(_return,    semicolon);
	dfa_node_add_child(_return,    expr);
	dfa_node_add_child(expr,       comma);
	dfa_node_add_child(comma,      expr);
	dfa_node_add_child(expr,       semicolon);

	return 0;
}

dfa_module_t dfa_module_return =
{
	.name        = "return",
	.init_module = _dfa_init_module_return,
	.init_syntax = _dfa_init_syntax_return,
};
