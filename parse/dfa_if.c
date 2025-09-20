#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_if;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	block_t*     parent_block;
	node_t*      parent_node;

	node_t*      _if;

	lex_word_t*  prev_else;
	lex_word_t*  next_else;

} dfa_if_data_t;


static int _if_is_end(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_ELSE != w->type;
}

static int _if_action_if(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*   ifd   = stack_top(s);
	node_t*     _if    = node_alloc(w, OP_IF, NULL);

	if (!_if) {
		loge("\n");
		return DFA_ERROR;
	}

	dfa_if_data_t* ifd2 = calloc(1, sizeof(dfa_if_data_t));
	if (!ifd2) {
		loge("\n");
		return DFA_ERROR;
	}

	if (d->current_node)
		node_add_child(d->current_node, _if);
	else
		node_add_child((node_t*)parse->ast->current_block, _if);

	ifd2->parent_block = parse->ast->current_block;
	ifd2->parent_node  = d->current_node;
	ifd2->_if          = _if;

	if (ifd && ifd->next_else)
		ifd2->prev_else = ifd->next_else;

	stack_push(s, ifd2);

	d->current_node = _if;

	return DFA_NEXT_WORD;
}

static int _if_action_else(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*   ifd   = stack_top(s);

	if (!ifd) {
		loge("no 'if' before 'else' in line: %d\n", w->line);
		return DFA_ERROR;
	}

	if (ifd->next_else) {
		loge("continuous 2 'else', 1st line: %d, 2nd line: %d\n", ifd->next_else->line, w->line);
		return DFA_ERROR;
	}

	logd("ifd->_if->nb_nodes: %d, line: %d\n", ifd->_if->nb_nodes, ifd->_if->w->line);
	assert(2 == ifd->_if->nb_nodes);

	ifd->next_else = w;

	assert(!d->expr);
	d->current_node = ifd->_if;

	return DFA_NEXT_WORD;
}

static int _if_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	if (d->expr) {
		expr_free(d->expr);
		d->expr = NULL;
	}
	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	logd("ifd->nb_lps: %d, ifd->nb_rps: %d\n", ifd->nb_lps, ifd->nb_rps);

	return DFA_NEXT_WORD;
}

static int _if_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	ifd->nb_lps++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _if_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	ifd->nb_rps++;

	if (ifd->nb_rps == ifd->nb_lps) {

		assert(0 == ifd->_if->nb_nodes);

		node_add_child(ifd->_if, d->expr);
		d->expr = NULL;

		d->expr_local_flag = 0;

		DFA_PUSH_HOOK(dfa_find_node(dfa, "if_end"), DFA_HOOK_END);
		return DFA_SWITCH_TO;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _is_end(dfa_t* dfa)
{
	lex_word_t* w   = dfa->ops->pop_word(dfa);
	int             ret = LEX_WORD_KEY_ELSE != w->type;

	dfa->ops->push_word(dfa, w);

	return ret;
}

static int _if_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse     = dfa->priv;
	dfa_data_t*      d         = data;
	stack_t*     s         = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*   ifd       = stack_top(s);
	lex_word_t*  prev_else = ifd->prev_else;

	if (!_is_end(dfa)) {

		if (3 == ifd->_if->nb_nodes) {
			if (1 == s->size)
				return DFA_NEXT_WORD;
		} else {
			DFA_PUSH_HOOK(dfa_find_node(dfa, "if_end"), DFA_HOOK_END);
			return DFA_NEXT_WORD;
		}
	}

	assert(parse->ast->current_block == ifd->parent_block);

	stack_pop(s);

	d->current_node = ifd->parent_node;

	logi("if: %d, ifd: %p, s->size: %d\n", ifd->_if->w->line, ifd, s->size);

	free(ifd);
	ifd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

static int _dfa_init_module_if(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, if, end,       _if_is_end,       _if_action_end);

	DFA_MODULE_NODE(dfa, if, lp,        dfa_is_lp,    _if_action_lp);
	DFA_MODULE_NODE(dfa, if, rp,        dfa_is_rp,    _if_action_rp);
	DFA_MODULE_NODE(dfa, if, lp_stat,   dfa_is_lp,    _if_action_lp_stat);

	DFA_MODULE_NODE(dfa, if, _if,       dfa_is_if,    _if_action_if);
	DFA_MODULE_NODE(dfa, if, _else,     dfa_is_else,  _if_action_else);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_if.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_if.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_if(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_if.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_if.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_if(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, if,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, if,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, if,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, if,   _if,       _if);
	DFA_GET_MODULE_NODE(dfa, if,   _else,     _else);
	DFA_GET_MODULE_NODE(dfa, if,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// if start
	vector_add(dfa->syntaxes,  _if);

	// condition expr
	dfa_node_add_child(_if,    lp);
	dfa_node_add_child(lp,     expr);
	dfa_node_add_child(expr,   rp);

	// if body block
	dfa_node_add_child(rp,     block);

	// recursive else if block
	dfa_node_add_child(block,  _else);
	dfa_node_add_child(_else,  _if);

	// last else block
	dfa_node_add_child(_else,  block);

	return 0;
}

dfa_module_t dfa_module_if =
{
	.name        = "if",

	.init_module = _dfa_init_module_if,
	.init_syntax = _dfa_init_syntax_if,

	.fini_module = _dfa_fini_module_if,
};
