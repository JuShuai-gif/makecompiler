#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_sizeof;

typedef struct {

	int              nb_lps;
	int              nb_rps;

	node_t*      _sizeof;

	expr_t*      parent_expr;

} dfa_sizeof_data_t;

static int _sizeof_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*         d  = data;
	stack_t*        s  = d->module_datas[dfa_module_sizeof.index];
	dfa_sizeof_data_t*  sd = stack_top(s);

	if (!sd) {
		loge("\n");
		return DFA_ERROR;
	}

	sd->nb_lps++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _sizeof_action_sizeof(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_sizeof.index];

	dfa_sizeof_data_t* sd    = calloc(1, sizeof(dfa_sizeof_data_t));
	if (!sd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	node_t* _sizeof = node_alloc(w, OP_SIZEOF, NULL);
	if (!_sizeof) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);

	sd->_sizeof     = _sizeof;
	sd->parent_expr = d->expr;
	d->expr         = NULL;
	d->expr_local_flag++;
	d->nb_sizeofs++;

	stack_push(s, sd);

	return DFA_NEXT_WORD;
}

static int _sizeof_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _sizeof_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	lex_word_t*    w     = words->data[words->size - 1];
	stack_t*       s     = d->module_datas[dfa_module_sizeof.index];
	dfa_sizeof_data_t* sd    = stack_top(s);

	if (d->current_va_arg)
		return DFA_NEXT_SYNTAX;

	if (!sd) {
		loge("\n");
		return DFA_ERROR;
	}

	sd->nb_rps++;

	logd("sd->nb_lps: %d, sd->nb_rps: %d\n", sd->nb_lps, sd->nb_rps);

	if (sd->nb_rps < sd->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}
	assert(sd->nb_rps == sd->nb_lps);

	if (d->expr) {
		node_add_child(sd->_sizeof, d->expr);
		d->expr = NULL;

	} else if (d->current_identities->size > 0) {

		variable_t* v;
		dfa_identity_t* id;
		node_t*     n;
		expr_t*     e;
		type_t*     t;

		id = stack_pop(d->current_identities);
		assert(id && id->type);

		if (id->nb_pointers > 0) {

			t = block_find_type_type(parse->ast->current_block, VAR_INTPTR);
			assert(t);

			v = VAR_ALLOC_BY_TYPE(sd->_sizeof->w, t, 1, 0, NULL);
			if (!v) {
				loge("\n");
				return DFA_ERROR;
			}
			v->data.i = t->size;

			n = node_alloc(NULL, VAR_INTPTR, v);
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			node_free(sd->_sizeof);
			sd->_sizeof = n;
		} else {
			v = VAR_ALLOC_BY_TYPE(sd->_sizeof->w, id->type, 1, 0, NULL);
			if (!v) {
				loge("\n");
				return DFA_ERROR;
			}

			n = node_alloc(NULL, v->type, v);
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			e = expr_alloc();
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			expr_add_node(e, n);
			node_add_child(sd->_sizeof, e);
		}

		free(id);
		id = NULL;
	} else {
		loge("\n");
		return DFA_ERROR;
	}

	stack_pop(s);

	if (sd->parent_expr) {
		expr_add_node(sd->parent_expr, sd->_sizeof);
		d->expr = sd->parent_expr;
	} else
		d->expr = sd->_sizeof;

	d->expr_local_flag--;
	d->nb_sizeofs--;

	logi("d->expr: %p, d->nb_sizeofs: %d\n", d->expr, d->nb_sizeofs);

	free(sd);
	sd = NULL;

	return DFA_NEXT_WORD;
}

static int _dfa_init_module_sizeof(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, sizeof, _sizeof,  dfa_is_sizeof, _sizeof_action_sizeof);
	DFA_MODULE_NODE(dfa, sizeof, lp,       dfa_is_lp,     _sizeof_action_lp);
	DFA_MODULE_NODE(dfa, sizeof, rp,       dfa_is_rp,     _sizeof_action_rp);
	DFA_MODULE_NODE(dfa, sizeof, lp_stat,  dfa_is_lp,     _sizeof_action_lp_stat);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_sizeof.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_sizeof.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_sizeof(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_sizeof.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_sizeof.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_sizeof(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, sizeof,   _sizeof,   _sizeof);
	DFA_GET_MODULE_NODE(dfa, sizeof,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, sizeof,   rp,        rp);

	DFA_GET_MODULE_NODE(dfa, expr,     entry,     expr);

	DFA_GET_MODULE_NODE(dfa, type,     entry,     type);
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  identity);

	dfa_node_add_child(_sizeof,   lp);
	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      rp);

	dfa_node_add_child(lp,        type);
	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(identity,  rp);
	dfa_node_add_child(star,      rp);

	return 0;
}

dfa_module_t dfa_module_sizeof =
{
	.name        = "sizeof",
	.init_module = _dfa_init_module_sizeof,
	.init_syntax = _dfa_init_syntax_sizeof,

	.fini_module = _dfa_fini_module_sizeof,
};
