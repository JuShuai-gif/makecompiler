#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_goto;

static int _goto_action_goto(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse  = dfa->priv;
	dfa_data_t*       d      = data;
	lex_word_t*   w      = words->data[words->size - 1];

	node_t*       _goto  = node_alloc(w, OP_GOTO, NULL);
	if (!_goto) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _goto);
	else
		node_add_child((node_t*)parse->ast->current_block, _goto);

	d->current_goto = _goto;

	return DFA_NEXT_WORD;
}

static int _goto_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse  = dfa->priv;
	dfa_data_t*       d      = data;
	lex_word_t*   w      = words->data[words->size - 1];

	label_t*      l      = label_alloc(w);
	node_t*       n      = node_alloc_label(l);

	node_add_child(d->current_goto, n);

	return DFA_NEXT_WORD;
}

static int _goto_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->current_goto = NULL;

	return DFA_OK;
}

static int _dfa_init_module_goto(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, goto, _goto,     dfa_is_goto,      _goto_action_goto);
	DFA_MODULE_NODE(dfa, goto, identity,  dfa_is_identity,  _goto_action_identity);
	DFA_MODULE_NODE(dfa, goto, semicolon, dfa_is_semicolon, _goto_action_semicolon);

	return DFA_OK;
}

static int _dfa_init_syntax_goto(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, goto,   _goto,      _goto);
	DFA_GET_MODULE_NODE(dfa, goto,   identity,  identity);
	DFA_GET_MODULE_NODE(dfa, goto,   semicolon, semicolon);

	dfa_node_add_child(_goto,    identity);
	dfa_node_add_child(identity, semicolon);

	return 0;
}

dfa_module_t dfa_module_goto =
{
	.name        = "goto",
	.init_module = _dfa_init_module_goto,
	.init_syntax = _dfa_init_syntax_goto,
};
