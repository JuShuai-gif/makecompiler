#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_block;

typedef struct {

	int              nb_lbs;
	int              nb_rbs;

	block_t*     parent_block;

	node_t*      parent_node;

} dfa_block_data_t;

static int _block_action_entry(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse     = dfa->priv;
	dfa_data_t* d         = data;
	stack_t*      s         = d->module_datas[dfa_module_block.index];
	dfa_block_data_t* bd        = stack_top(s);

	if (!bd) {
		dfa_block_data_t* bd = calloc(1, sizeof(dfa_block_data_t));
		assert(bd);

		bd->parent_block = parse->ast->current_block;
		bd->parent_node  = d->current_node;

		DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

		stack_push(s, bd);

		logi("new bd: %p, s->size: %d\n", bd, s->size);
	} else
		logi("new bd: %p, s->size: %d\n", bd, s->size);

	return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}

static int _block_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse     = dfa->priv;
	dfa_data_t*       d         = data;
	stack_t*      s         = d->module_datas[dfa_module_block.index];
	dfa_block_data_t* bd        = stack_top(s);

	if (bd->nb_rbs < bd->nb_lbs) {
		logi("end bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
				bd, bd->nb_lbs, bd->nb_rbs, s->size);

		DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

		return DFA_SWITCH_TO;
	}

	assert(bd->nb_lbs       == bd->nb_rbs);
	assert(bd->parent_block == parse->ast->current_block);
	assert(bd->parent_node  == d->current_node);

	stack_pop(s);

	logi("end bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
			bd, bd->nb_lbs, bd->nb_rbs, s->size);

	free(bd);
	bd = NULL;

	return DFA_OK;
}

static int _block_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_block.index];

	dfa_block_data_t* bd = calloc(1, sizeof(dfa_block_data_t));
	assert(bd);

	block_t* b = block_alloc(w);
	assert(b);

	if (d->current_node)
		node_add_child(d->current_node, (node_t*)b);
	else
		node_add_child((node_t*)parse->ast->current_block, (node_t*)b);

	bd->parent_block = parse->ast->current_block;
	bd->parent_node  = d->current_node;

	parse->ast->current_block = b;
	d->current_node = NULL;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

	bd->nb_lbs++;
	stack_push(s, bd);

	logi("new bd: %p, s->size: %d\n", bd, s->size);

	return DFA_NEXT_WORD;
}

static int _block_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_block.index];
	dfa_block_data_t* bd    = stack_top(s);

	bd->nb_rbs++;

	logi("bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
			bd, bd->nb_lbs, bd->nb_rbs, s->size);

	assert(bd->nb_lbs == bd->nb_rbs);

	parse->ast->current_block = bd->parent_block;
	d->current_node = bd->parent_node;

	return DFA_OK;
}

static int _dfa_init_module_block(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, block, entry, dfa_is_entry, _block_action_entry);
	DFA_MODULE_NODE(dfa, block, end,   dfa_is_entry, _block_action_end);
	DFA_MODULE_NODE(dfa, block, lb,    dfa_is_lb,    _block_action_lb);
	DFA_MODULE_NODE(dfa, block, rb,    dfa_is_rb,    _block_action_rb);

	DFA_GET_MODULE_NODE(dfa, block,     entry,     entry);
	DFA_GET_MODULE_NODE(dfa, block,     end,       end);
	DFA_GET_MODULE_NODE(dfa, block,     lb,        lb);
	DFA_GET_MODULE_NODE(dfa, block,     rb,        rb);

	DFA_GET_MODULE_NODE(dfa, expr,      entry,     expr);
	DFA_GET_MODULE_NODE(dfa, type,      entry,     type);

	DFA_GET_MODULE_NODE(dfa, macro,     hash,      macro);

	DFA_GET_MODULE_NODE(dfa, if,       _if,       _if);
	DFA_GET_MODULE_NODE(dfa, while,    _while,    _while);
	DFA_GET_MODULE_NODE(dfa, do,       _do,       _do);
	DFA_GET_MODULE_NODE(dfa, for,      _for,      _for);

	DFA_GET_MODULE_NODE(dfa, switch,   _switch,   _switch);
	DFA_GET_MODULE_NODE(dfa, switch,   _case,     _case);
	DFA_GET_MODULE_NODE(dfa, switch,   _default,  _default);

	DFA_GET_MODULE_NODE(dfa, break,    _break,    _break);
	DFA_GET_MODULE_NODE(dfa, continue, _continue, _continue);
	DFA_GET_MODULE_NODE(dfa, return,   _return,   _return);
	DFA_GET_MODULE_NODE(dfa, goto,     _goto,     _goto);
	DFA_GET_MODULE_NODE(dfa, label,    label,     label);
	DFA_GET_MODULE_NODE(dfa, async,    async,     async);

	DFA_GET_MODULE_NODE(dfa, va_arg,   start,     va_start);
	DFA_GET_MODULE_NODE(dfa, va_arg,   end,       va_end);

	// block could includes these statements
	dfa_node_add_child(entry, lb);
	dfa_node_add_child(entry, rb);

	dfa_node_add_child(entry, va_start);
	dfa_node_add_child(entry, va_end);
	dfa_node_add_child(entry, expr);
	dfa_node_add_child(entry, type);

	dfa_node_add_child(entry, macro);

	dfa_node_add_child(entry, _if);
	dfa_node_add_child(entry, _while);
	dfa_node_add_child(entry, _do);
	dfa_node_add_child(entry, _for);
	dfa_node_add_child(entry, _switch);
	dfa_node_add_child(entry, _case);
	dfa_node_add_child(entry, _default);

	dfa_node_add_child(entry, _break);
	dfa_node_add_child(entry, _continue);
	dfa_node_add_child(entry, _return);
	dfa_node_add_child(entry, _goto);
	dfa_node_add_child(entry, label);
	dfa_node_add_child(entry, async);


	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = parse->dfa_data;
	stack_t*      s     = d->module_datas[dfa_module_block.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("error: \n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_block.index] = s;
	return DFA_OK;
}

static int _dfa_fini_module_block(dfa_t* dfa)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = parse->dfa_data;
	stack_t*      s     = d->module_datas[dfa_module_block.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_block.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_block(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, block,     entry,     entry);
	DFA_GET_MODULE_NODE(dfa, block,     end,       end);

	dfa_node_add_child(entry, end);
	dfa_node_add_child(end,   entry);

	int i;
	for (i = 0; i < entry->childs->size; i++) {
		dfa_node_t* n = entry->childs->data[i];

		logd("n->name: %s\n", n->name);
	}

	return 0;
}

dfa_module_t dfa_module_block =
{
	.name        = "block",
	.init_module = _dfa_init_module_block,
	.init_syntax = _dfa_init_syntax_block,

	.fini_module = _dfa_fini_module_block,
};
