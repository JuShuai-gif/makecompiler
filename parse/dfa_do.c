#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_do;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	block_t*     parent_block;
	node_t*      parent_node;

	node_t*      _do;

} dfa_do_data_t;


static int _do_action_do(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_do.index];
	block_t*     b     = NULL;
	node_t*     _do    = node_alloc(w, OP_DO, NULL);

	if (!_do) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	dfa_do_data_t* dd = calloc(1, sizeof(dfa_do_data_t));
	if (!dd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _do);
	else
		node_add_child((node_t*)parse->ast->current_block, _do);

	dd->_do          = _do;
	dd->parent_block = parse->ast->current_block;
	dd->parent_node  = d->current_node;
	d->current_node  = _do;

	stack_push(s, dd);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "do__while"),  DFA_HOOK_END);

	return DFA_NEXT_WORD;
}

static int _do_action_while(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = dfa->ops->pop_word(dfa);

	if (LEX_WORD_KEY_WHILE != w->type)
		return DFA_ERROR;

	return DFA_SWITCH_TO;
}

static int _do_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	assert(!d->expr);
	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _do_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d  = data;
	stack_t*    s  = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*  dd = stack_top(s);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	dd->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _do_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*   dd    = stack_top(s);

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	dd->nb_rps++;

	if (dd->nb_rps == dd->nb_lps) {

		assert(1 == dd->_do->nb_nodes);

		node_add_child(dd->_do, d->expr);
		d->expr = NULL;

		d->expr_local_flag = 0;

		return DFA_SWITCH_TO;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _do_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	stack_t*    s     = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*  dd    = stack_pop(s);

	assert(parse->ast->current_block == dd->parent_block);

	d->current_node = dd->parent_node;

	logi("\033[31m do: %d, dd: %p, s->size: %d\033[0m\n", dd->_do->w->line, dd, s->size);

	free(dd);
	dd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

static int _dfa_init_module_do(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, do, semicolon, dfa_is_semicolon, _do_action_semicolon);

	DFA_MODULE_NODE(dfa, do, lp,        dfa_is_lp,        _do_action_lp);
	DFA_MODULE_NODE(dfa, do, rp,        dfa_is_rp,        _do_action_rp);
	DFA_MODULE_NODE(dfa, do, lp_stat,   dfa_is_lp,        _do_action_lp_stat);

	DFA_MODULE_NODE(dfa, do, _do,       dfa_is_do,        _do_action_do);
	DFA_MODULE_NODE(dfa, do, _while,    dfa_is_while,     _do_action_while);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_do.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		logi("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_do.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_do(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_do.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_do.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_do(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, do,   lp,         lp);
	DFA_GET_MODULE_NODE(dfa, do,   rp,         rp);
	DFA_GET_MODULE_NODE(dfa, do,   lp_stat,    lp_stat);
	DFA_GET_MODULE_NODE(dfa, do,  _do,        _do);
	DFA_GET_MODULE_NODE(dfa, do,  _while,     _while);
	DFA_GET_MODULE_NODE(dfa, do,   semicolon,  semicolon);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// do start
	vector_add(dfa->syntaxes,   _do);

	dfa_node_add_child(_do,      block);
	dfa_node_add_child(block,   _while);

	dfa_node_add_child(_while,   lp);
	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     rp);
	dfa_node_add_child(rp,       semicolon);

	return 0;
}

dfa_module_t dfa_module_do =
{
	.name        = "do",

	.init_module = _dfa_init_module_do,
	.init_syntax = _dfa_init_syntax_do,

	.fini_module = _dfa_fini_module_do,
};
