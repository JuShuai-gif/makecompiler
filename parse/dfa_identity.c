#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_identity;

static int _identity_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t*  w = words->data[words->size - 1];
	dfa_data_t*      d = data;
	stack_t*     s = d->current_identities;

	logd("w: '%s'\n", w->text->data);

	dfa_identity_t* id  = calloc(1, sizeof(dfa_identity_t));
	if (!id)
		return DFA_ERROR;

	if (stack_push(s, id) < 0) {
		free(id);
		return DFA_ERROR;
	}

	id->identity = w;
	id->const_flag = d->const_flag;
	d ->const_flag = 0;

	return DFA_NEXT_WORD;
}

static int _dfa_init_module_identity(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, identity, identity,  dfa_is_identity,  _identity_action_identity);

	return DFA_OK;
}

static int _dfa_init_syntax_identity(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, identity, identity,  identity);

	vector_add(dfa->syntaxes, identity);

	return DFA_OK;
}

dfa_module_t dfa_module_identity = 
{
	.name        = "identity",
	.init_module = _dfa_init_module_identity,
	.init_syntax = _dfa_init_syntax_identity,
};
