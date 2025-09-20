#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_continue;

static int _continue_is_continue(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CONTINUE == w->type;
}

static int _continue_action_continue(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	lex_word_t*  w      = words->data[words->size - 1];

	node_t*       _continue = node_alloc(w, OP_CONTINUE, NULL);
	if (!_continue) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _continue);
	else
		node_add_child((node_t*)parse->ast->current_block, _continue);

	return DFA_NEXT_WORD;
}

static int _continue_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

static int _dfa_init_module_continue(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, continue, semicolon, dfa_is_semicolon,  _continue_action_semicolon);
	DFA_MODULE_NODE(dfa, continue, _continue, _continue_is_continue, _continue_action_continue);

	return DFA_OK;
}

static int _dfa_init_syntax_continue(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, continue,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, continue,   _continue, _continue);

	dfa_node_add_child(_continue, semicolon);
	return 0;
}

dfa_module_t dfa_module_continue =
{
	.name        = "continue",
	.init_module = _dfa_init_module_continue,
	.init_syntax = _dfa_init_syntax_continue,
};
