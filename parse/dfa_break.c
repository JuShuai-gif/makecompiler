#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_break;

static int _break_is_break(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_BREAK == w->type;
}

static int _break_action_break(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	lex_word_t*  w      = words->data[words->size - 1];

	node_t*       _break = node_alloc(w, OP_BREAK, NULL);
	if (!_break) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _break);
	else
		node_add_child((node_t*)parse->ast->current_block, _break);

	return DFA_NEXT_WORD;
}

static int _break_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

static int _dfa_init_module_break(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, break, semicolon, dfa_is_semicolon, _break_action_semicolon);
	DFA_MODULE_NODE(dfa, break, _break,    _break_is_break,      _break_action_break);

	return DFA_OK;
}

static int _dfa_init_syntax_break(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, break,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, break,   _break,       _break);

	dfa_node_add_child(_break, semicolon);
	return 0;
}

dfa_module_t dfa_module_break =
{
	.name        = "break",
	.init_module = _dfa_init_module_break,
	.init_syntax = _dfa_init_syntax_break,
};
