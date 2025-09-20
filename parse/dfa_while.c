#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_while;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	block_t*     parent_block;
	node_t*      parent_node;

	node_t*      _while;

} dfa_while_data_t;


static int _while_is_end(dfa_t* dfa, void* word)
{
	return 1;
}

static int _while_action_while(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	node_t*      _while = node_alloc(w, OP_WHILE, NULL);

	if (!_while) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	dfa_while_data_t* wd = calloc(1, sizeof(dfa_while_data_t));
	if (!wd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _while);
	else
		node_add_child((node_t*)parse->ast->current_block, _while);

	wd->_while       = _while;
	wd->parent_block = parse->ast->current_block;
	wd->parent_node  = d->current_node;
	d->current_node  = _while;

	stack_push(s, wd);

	return DFA_NEXT_WORD;
}

static int _while_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_top(s);

	assert(!d->expr);
	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _while_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*       d  = data;
	stack_t*      s  = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd = stack_top(s);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	wd->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _while_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_top(s);

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	wd->nb_rps++;

	if (wd->nb_rps == wd->nb_lps) {

		assert(0 == wd->_while->nb_nodes);

		node_add_child(wd->_while, d->expr);
		d->expr = NULL;

		d->expr_local_flag = 0;

		DFA_PUSH_HOOK(dfa_find_node(dfa, "while_end"), DFA_HOOK_END);

		return DFA_SWITCH_TO;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _while_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_pop(s);

	assert(parse->ast->current_block == wd->parent_block);

	d->current_node = wd->parent_node;

	logi("while: %d, wd: %p, s->size: %d\n", wd->_while->w->line, wd, s->size);

	free(wd);
	wd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

static int _dfa_init_module_while(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, while, end,       _while_is_end,    _while_action_end);

	DFA_MODULE_NODE(dfa, while, lp,        dfa_is_lp,    _while_action_lp);
	DFA_MODULE_NODE(dfa, while, rp,        dfa_is_rp,    _while_action_rp);
	DFA_MODULE_NODE(dfa, while, lp_stat,   dfa_is_lp,    _while_action_lp_stat);

	DFA_MODULE_NODE(dfa, while, _while,    dfa_is_while, _while_action_while);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_while.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		logi("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_while.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_while(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_while.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_while.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_while(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, while,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, while,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, while,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, while,   _while,    _while);
	DFA_GET_MODULE_NODE(dfa, while,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// while start
	vector_add(dfa->syntaxes,  _while);

	// condition expr
	dfa_node_add_child(_while, lp);
	dfa_node_add_child(lp,     expr);
	dfa_node_add_child(expr,   rp);

	// while body
	dfa_node_add_child(rp,     block);

	return 0;
}

dfa_module_t dfa_module_while =
{
	.name        = "while",

	.init_module = _dfa_init_module_while,
	.init_syntax = _dfa_init_syntax_while,

	.fini_module = _dfa_fini_module_while,
};
