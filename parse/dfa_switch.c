#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_switch;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	block_t*     parent_block;
	node_t*      parent_node;

	node_t*      _switch;
	node_t*      child;

} dfa_switch_data_t;

int _expr_add_var(parse_t* parse, dfa_data_t* d);


static int _switch_is_end(dfa_t* dfa, void* word)
{
	return 1;
}

static int _switch_action_switch(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	lex_word_t*  w      = words->data[words->size - 1];
	stack_t*     s      = d->module_datas[dfa_module_switch.index];
	node_t*     _switch = node_alloc(w, OP_SWITCH, NULL);

	if (!_switch)
		return -ENOMEM;

	dfa_switch_data_t* sd = calloc(1, sizeof(dfa_switch_data_t));
	if (!sd)
		return -ENOMEM;

	if (d->current_node)
		node_add_child(d->current_node, _switch);
	else
		node_add_child((node_t*)parse->ast->current_block, _switch);

	sd->_switch      = _switch;
	sd->parent_block = parse->ast->current_block;
	sd->parent_node  = d->current_node;
	d->current_node  = _switch;

	stack_push(s, sd);

	return DFA_NEXT_WORD;
}

static int _switch_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	assert(!d->expr);
	d->expr_local_flag++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _switch_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*        d  = data;
	stack_t*       s  = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd = stack_top(s);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

	sd->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _switch_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	stack_t*       s     = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd    = stack_top(s);

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	sd->nb_rps++;

	if (sd->nb_rps == sd->nb_lps) {

		assert(0 == sd->_switch->nb_nodes);

		node_add_child(sd->_switch, d->expr);
		d->expr = NULL;
		assert(--d->expr_local_flag >= 0);

		DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_end"), DFA_HOOK_END);

		return DFA_SWITCH_TO;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _switch_action_case(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	lex_word_t*    w     = words->data[words->size - 1];
	stack_t*       s     = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd    = stack_top(s);

	assert(!d->expr);
	d->expr_local_flag++;

	sd->child = node_alloc(w, OP_CASE, NULL);
	if (!sd->child)
		return DFA_ERROR;

	node_add_child((node_t*)parse->ast->current_block, sd->child);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_colon"), DFA_HOOK_PRE);

	return DFA_NEXT_WORD;
}

static int _switch_action_default(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	lex_word_t*    w     = words->data[words->size - 1];
	stack_t*       s     = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd    = stack_top(s);

	assert(!d->expr);

	sd->child = node_alloc(w, OP_DEFAULT, NULL);
	if (!sd->child)
		return DFA_ERROR;

	node_add_child((node_t*)parse->ast->current_block, sd->child);

	return DFA_NEXT_WORD;
}

static int _switch_action_colon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	stack_t*       s     = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd    = stack_top(s);

	if (OP_CASE == sd->child->type) {

		if (!d->expr) {
			loge("NOT found the expr for case\n");
			return DFA_ERROR;
		}

		dfa_identity_t* id = stack_top(d->current_identities);

		if (id && id->identity) {
			if (_expr_add_var(parse, d) < 0)
				return DFA_ERROR;
		}

		node_add_child(sd->child, d->expr);
		d->expr = NULL;
		assert(--d->expr_local_flag >= 0);

	} else {
		assert(OP_DEFAULT == sd->child->type);
		assert(!d->expr);
	}

	return DFA_OK;
}

static int _switch_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	stack_t*       s     = d->module_datas[dfa_module_switch.index];
	dfa_switch_data_t* sd    = stack_pop(s);

	assert(parse->ast->current_block == sd->parent_block);

	d->current_node = sd->parent_node;

	logi("switch: %d, sd: %p, s->size: %d\n", sd->_switch->w->line, sd, s->size);

	free(sd);
	sd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

static int _dfa_init_module_switch(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, switch, lp,        dfa_is_lp,      _switch_action_lp);
	DFA_MODULE_NODE(dfa, switch, rp,        dfa_is_rp,      _switch_action_rp);
	DFA_MODULE_NODE(dfa, switch, lp_stat,   dfa_is_lp,      _switch_action_lp_stat);
	DFA_MODULE_NODE(dfa, switch, colon,     dfa_is_colon,   _switch_action_colon);

	DFA_MODULE_NODE(dfa, switch, _switch,   dfa_is_switch,  _switch_action_switch);
	DFA_MODULE_NODE(dfa, switch, _case,     dfa_is_case,    _switch_action_case);
	DFA_MODULE_NODE(dfa, switch, _default,  dfa_is_default, _switch_action_default);
	DFA_MODULE_NODE(dfa, switch, end,       _switch_is_end,     _switch_action_end);

	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = parse->dfa_data;
	stack_t*      s     = d->module_datas[dfa_module_switch.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		logi("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_switch.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_switch(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_switch.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_switch.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_switch(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, switch,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, switch,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, switch,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, switch,   colon,     colon);

	DFA_GET_MODULE_NODE(dfa, switch,   _switch,   _switch);
	DFA_GET_MODULE_NODE(dfa, switch,   _case,     _case);
	DFA_GET_MODULE_NODE(dfa, switch,   _default,  _default);
	DFA_GET_MODULE_NODE(dfa, switch,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	dfa_node_add_child(_switch,  lp);
	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     rp);
	dfa_node_add_child(rp,       block);

	dfa_node_add_child(_case,    expr);
	dfa_node_add_child(expr,     colon);
	dfa_node_add_child(_default, colon);

	return 0;
}

dfa_module_t dfa_module_switch =
{
	.name        = "switch",

	.init_module = _dfa_init_module_switch,
	.init_syntax = _dfa_init_syntax_switch,

	.fini_module = _dfa_fini_module_switch,
};
