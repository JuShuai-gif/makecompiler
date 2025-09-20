#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_label;

static int _label_action_colon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse  = dfa->priv;
	dfa_data_t*       d      = data;
	dfa_identity_t*   id     = stack_top(d->current_identities);

	if (!id || !id->identity) {
		loge("\n");
		return DFA_ERROR;
	}

	label_t* l = label_alloc(id->identity);
	node_t*  n = node_alloc_label(l);

	stack_pop(d->current_identities);
	free(id);
	id = NULL;

	node_add_child((node_t*)parse->ast->current_block, n);

	scope_push_label(parse->ast->current_block->scope, l);

	return DFA_OK;
}

static int _dfa_init_module_label(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, label, label,  dfa_is_colon, _label_action_colon);

	return DFA_OK;
}

static int _dfa_init_syntax_label(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, label,    label,    label);
	DFA_GET_MODULE_NODE(dfa, identity, identity, identity);

	dfa_node_add_child(identity, label);
	return 0;
}

dfa_module_t dfa_module_label =
{
	.name        = "label",
	.init_module = _dfa_init_module_label,
	.init_syntax = _dfa_init_syntax_label,
};
