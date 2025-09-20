#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_for;

typedef struct {
	int              nb_lps;
	int              nb_rps;

	block_t*     parent_block;
	node_t*      parent_node;

	node_t*      _for;

	int              nb_semicolons;
	vector_t*    init_exprs;
	expr_t*      cond_expr;
	vector_t*    update_exprs;

} dfa_for_data_t;


static int _for_is_end(dfa_t* dfa, void* word)
{
	return 1;
}

static int _for_action_for(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_for_data_t*  fd    = NULL;
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	node_t*     _for   = node_alloc(w, OP_FOR, NULL);

	if (!_for)
		return -ENOMEM;

	fd = calloc(1, sizeof(dfa_for_data_t));
	if (!fd)
		return -ENOMEM;

	if (d->current_node)
		node_add_child(d->current_node, _for);
	else
		node_add_child((node_t*)parse->ast->current_block, _for);

	fd->parent_block = parse->ast->current_block;
	fd->parent_node  = d->current_node;
	fd->_for         = _for;
	d->current_node  = _for;

	stack_push(s, fd);

	return DFA_NEXT_WORD;
}

static int _for_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*   fd    = stack_top(s);

	assert(!d->expr);
	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_rp"),        DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _for_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*      d  = data;
	stack_t*     s  = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd = stack_top(s);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"), DFA_HOOK_POST);

	fd->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _for_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = stack_top(s);

	if (!d->expr) {
		loge("need expr before ',' in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	if (0 == fd->nb_semicolons) {
		if (!fd->init_exprs)
			fd->init_exprs = vector_alloc();

		vector_add(fd->init_exprs, d->expr);
		d->expr = NULL;

	} else if (1 == fd->nb_semicolons) {
		fd->cond_expr = d->expr;
		d->expr = NULL;

	} else if (2 == fd->nb_semicolons) {
		if (!fd->update_exprs)
			fd->update_exprs = vector_alloc();

		vector_add(fd->update_exprs, d->expr);
		d->expr = NULL;
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _for_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	if (!data)
		return DFA_ERROR;

	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = stack_top(s);

	if (0 == fd->nb_semicolons) {
		if (d->expr) {
			if (!fd->init_exprs)
				fd->init_exprs = vector_alloc();

			vector_add(fd->init_exprs, d->expr);
			d->expr = NULL;
		}
	} else if (1 == fd->nb_semicolons) {
		if (d->expr) {
			fd->cond_expr = d->expr;
			d->expr = NULL;
		}
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	fd->nb_semicolons++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _for_add_expr_vector(dfa_for_data_t* fd, vector_t* vec)
{
	if (!vec) {
		node_add_child(fd->_for, NULL);
		return DFA_OK;
	}

	if (0 == vec->size) {
		node_add_child(fd->_for, NULL);

		vector_free(vec);
		vec = NULL;
		return DFA_OK;
	}

	node_t* parent = fd->_for;
	if (vec->size > 1) {

		block_t* b = block_alloc_cstr("for");

		node_add_child(fd->_for, (node_t*)b);
		parent = (node_t*)b;
	}

	int i;
	for (i = 0; i < vec->size; i++) {

		expr_t* e = vec->data[i];

		node_add_child(parent, e);
	}

	vector_free(vec);
	vec = NULL;
	return DFA_OK;
}

static int _for_add_exprs(dfa_for_data_t* fd)
{
	_for_add_expr_vector(fd, fd->init_exprs);
	fd->init_exprs = NULL;

	node_add_child(fd->_for, fd->cond_expr);
	fd->cond_expr = NULL;

	_for_add_expr_vector(fd, fd->update_exprs);
	fd->update_exprs = NULL;

	return DFA_OK;
}

static int _for_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = stack_top(s);

	fd->nb_rps++;

	if (fd->nb_rps < fd->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_rp"),        DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}

	if (2 == fd->nb_semicolons) {
		if (!fd->update_exprs)
			fd->update_exprs = vector_alloc();

		vector_add(fd->update_exprs, d->expr);
		d->expr = NULL;
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	_for_add_exprs(fd);
	d->expr_local_flag = 0;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_end"), DFA_HOOK_END);

	return DFA_SWITCH_TO;
}

static int _for_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	dfa_for_data_t*  fd    = stack_pop(s);

	if (3 == fd->_for->nb_nodes)
		node_add_child(fd->_for, NULL);

	assert(parse->ast->current_block == fd->parent_block);

	d->current_node = fd->parent_node;

	logi("for: %d, fd: %p, s->size: %d\n", fd->_for->w->line, fd, s->size);

	free(fd);
	fd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

static int _dfa_init_module_for(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, for, semicolon, dfa_is_semicolon,  _for_action_semicolon);
	DFA_MODULE_NODE(dfa, for, comma,     dfa_is_comma,      _for_action_comma);
	DFA_MODULE_NODE(dfa, for, end,       _for_is_end,           _for_action_end);

	DFA_MODULE_NODE(dfa, for, lp,        dfa_is_lp,         _for_action_lp);
	DFA_MODULE_NODE(dfa, for, lp_stat,   dfa_is_lp,         _for_action_lp_stat);
	DFA_MODULE_NODE(dfa, for, rp,        dfa_is_rp,         _for_action_rp);

	DFA_MODULE_NODE(dfa, for, _for,      dfa_is_for,        _for_action_for);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_for.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_for.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_for(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_for.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_for.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_for(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, for,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, for,   comma,     comma);
	DFA_GET_MODULE_NODE(dfa, for,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, for,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, for,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, for,   _for,    _for);
	DFA_GET_MODULE_NODE(dfa, for,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// for start
	vector_add(dfa->syntaxes,  _for);

	// condition expr
	dfa_node_add_child(_for,      lp);
	dfa_node_add_child(lp,        semicolon);
	dfa_node_add_child(semicolon, semicolon);
	dfa_node_add_child(semicolon, rp);

	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      semicolon);
	dfa_node_add_child(semicolon, expr);
	dfa_node_add_child(expr,      rp);

	// for body block
	dfa_node_add_child(rp,     block);

	return 0;
}

dfa_module_t dfa_module_for =
{
	.name        = "for",

	.init_module = _dfa_init_module_for,
	.init_syntax = _dfa_init_syntax_for,

	.fini_module = _dfa_fini_module_for,
};
