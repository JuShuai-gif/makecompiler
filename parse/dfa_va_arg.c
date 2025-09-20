#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t  dfa_module_va_arg;

int _type_find_type(dfa_t* dfa, dfa_identity_t* id);

static int _va_arg_action_start(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	if (d->current_va_start
			|| d->current_va_arg
			|| d->current_va_end) {
		loge("recursive 'va_start' in file: %s, line %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	node_t* node = node_alloc(w, OP_VA_START, NULL);
	if (!node)
		return DFA_ERROR;

	node_add_child((node_t*)parse->ast->current_block, node);

	d->current_va_start = node;

	return DFA_NEXT_WORD;
}

static int _va_arg_action_arg(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	if (d->current_va_start
			|| d->current_va_arg
			|| d->current_va_end) {
		loge("recursive 'va_arg' in file: %s, line %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	node_t* node = node_alloc(w, OP_VA_ARG, NULL);
	if (!node)
		return DFA_ERROR;

	if (d->expr)
		expr_add_node(d->expr, node);
	else
		node_add_child((node_t*)parse->ast->current_block, node);

	d->current_va_arg = node;

	return DFA_NEXT_WORD;
}

static int _va_arg_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	if (d->current_va_start
			|| d->current_va_arg
			|| d->current_va_end) {
		loge("recursive 'va_end' in file: %s, line %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	node_t* node = node_alloc(w, OP_VA_END, NULL);
	if (!node)
		return DFA_ERROR;

	node_add_child((node_t*)parse->ast->current_block, node);

	d->current_va_end = node;

	return DFA_NEXT_WORD;
}

static int _va_arg_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	assert(d->current_va_start
			|| d->current_va_arg
			|| d->current_va_end);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_rp"),         DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_comma"),      DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _va_arg_action_ap(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	variable_t*  ap;
	type_t*      t;
	node_t*      node;

	ap = block_find_variable(parse->ast->current_block, w->text->data);
	if (!ap) {
		loge("va_list variable %s not found\n", w->text->data);
		return DFA_ERROR;
	}

	if (ast_find_type(&t, parse->ast, "va_list") < 0) {
		loge("type 'va_list' not found, line: %d\n", w->line);
		return DFA_ERROR;
	}
	assert(t);

	if (t->type != ap->type || 0 != ap->nb_dimentions) {
		loge("variable %s is not va_list type\n", w->text->data);
		return DFA_ERROR;
	}

	node = node_alloc(w, ap->type, ap);
	if (!node)
		return DFA_ERROR;

	if (d->current_va_start)
		node_add_child(d->current_va_start, node);

	else if (d->current_va_arg)
		node_add_child(d->current_va_arg, node);

	else if (d->current_va_end)
		node_add_child(d->current_va_end, node);
	else {
		loge("\n");
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;
}

static int _va_arg_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_comma"), DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _va_arg_action_fmt(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	function_t*  f;
	variable_t*  fmt;
	variable_t*  arg;
	node_t*      node;

	fmt = block_find_variable(parse->ast->current_block, w->text->data);
	if (!fmt) {
		loge("format string %s not found\n", w->text->data);
		return DFA_ERROR;
	}

	if (VAR_CHAR != fmt->type
			&& VAR_I8 != fmt->type
			&& VAR_U8 != fmt->type) {

		loge("format string %s is not 'char*' or 'int8*' or 'uint8*' type\n", w->text->data);
		return DFA_ERROR;
	}

	if (variable_nb_pointers(fmt) != 1) {
		loge("format string %s is not 'char*' or 'int8*' or 'uint8*' type\n", w->text->data);
		return DFA_ERROR;
	}

	f = (function_t*)parse->ast->current_block;

	while (f && FUNCTION != f->node.type)
		f = (function_t*) f->node.parent;

	if (!f) {
		loge("va_list format string %s not in a function\n", w->text->data);
		return DFA_ERROR;
	}

	if (!f->vargs_flag) {
		loge("function %s has no variable args\n", f->node.w->text->data);
		return DFA_ERROR;
	}

	if (!f->argv || f->argv->size <= 0) {
		loge("function %s with variable args should have one format string\n", f->node.w->text->data);
		return DFA_ERROR;
	}

	arg = f->argv->data[f->argv->size - 1];

	if (fmt != arg) {
		loge("format string %s is not the last arg of function %s\n", w->text->data, f->node.w->text->data);
		return DFA_ERROR;
	}

	node = node_alloc(w, fmt->type, fmt);
	if (!node)
		return DFA_ERROR;

	if (d->current_va_start)
		node_add_child(d->current_va_start, node);
	else {
		loge("\n");
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;
}

static int _va_arg_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	if (d->current_va_arg) {
		if (d->current_va_arg->nb_nodes != 1) {
			loge("\n");
			return DFA_ERROR;
		}

		dfa_identity_t* id = stack_pop(d->current_identities);
		if (!id) {
			loge("\n");
			return DFA_ERROR;
		}

		if (!id->type) {

			if (_type_find_type(dfa, id) < 0) {
				loge("\n");
				return DFA_ERROR;
			}
		}

		variable_t* v = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, id->func_ptr);
		if (!v)
			return DFA_ERROR;

		node_t* node = node_alloc(w, v->type, v);
		if (!node)
			return DFA_ERROR;

		node_add_child(d->current_va_arg, node);

		free(id);
		id = NULL;

		d->current_va_start = NULL;
		d->current_va_arg   = NULL;
		d->current_va_end   = NULL;

		return DFA_NEXT_WORD;
	}

	d->current_va_start = NULL;
	d->current_va_arg   = NULL;
	d->current_va_end   = NULL;

	return DFA_SWITCH_TO;
}

static int _va_arg_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

static int _dfa_init_module_va_arg(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, va_arg, lp,         dfa_is_lp,         _va_arg_action_lp);
	DFA_MODULE_NODE(dfa, va_arg, rp,         dfa_is_rp,         _va_arg_action_rp);

	DFA_MODULE_NODE(dfa, va_arg, start,      dfa_is_va_start,   _va_arg_action_start);
	DFA_MODULE_NODE(dfa, va_arg, arg,        dfa_is_va_arg,     _va_arg_action_arg);
	DFA_MODULE_NODE(dfa, va_arg, end,        dfa_is_va_end,     _va_arg_action_end);

	DFA_MODULE_NODE(dfa, va_arg, ap,         dfa_is_identity,   _va_arg_action_ap);
	DFA_MODULE_NODE(dfa, va_arg, fmt,        dfa_is_identity,   _va_arg_action_fmt);

	DFA_MODULE_NODE(dfa, va_arg, comma,      dfa_is_comma,      _va_arg_action_comma);
	DFA_MODULE_NODE(dfa, va_arg, semicolon,  dfa_is_semicolon,  _va_arg_action_semicolon);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;

	d->current_va_start = NULL;
	d->current_va_arg   = NULL;
	d->current_va_end   = NULL;

	return DFA_OK;
}

static int _dfa_init_syntax_va_arg(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa,   va_arg,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa,   va_arg,   rp,        rp);

	DFA_GET_MODULE_NODE(dfa,   va_arg,   start,     start);
	DFA_GET_MODULE_NODE(dfa,   va_arg,   arg,       arg);
	DFA_GET_MODULE_NODE(dfa,   va_arg,   end,       end);

	DFA_GET_MODULE_NODE(dfa,   va_arg,   ap,        ap);
	DFA_GET_MODULE_NODE(dfa,   va_arg,   fmt,       fmt);

	DFA_GET_MODULE_NODE(dfa,   va_arg,   comma,     comma);
	DFA_GET_MODULE_NODE(dfa,   va_arg,   semicolon, semicolon);

	DFA_GET_MODULE_NODE(dfa,   type,     entry,     type);
	DFA_GET_MODULE_NODE(dfa,   type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa,   type,     star,      star);
	DFA_GET_MODULE_NODE(dfa,   identity, identity,  identity);

	dfa_node_add_child(start,     lp);
	dfa_node_add_child(lp,        ap);
	dfa_node_add_child(ap,        comma);
	dfa_node_add_child(comma,     fmt);
	dfa_node_add_child(fmt,       rp);
	dfa_node_add_child(rp,        semicolon);

	dfa_node_add_child(end,       lp);
	dfa_node_add_child(ap,        rp);

	dfa_node_add_child(arg,       lp);
	dfa_node_add_child(ap,        type);

	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(identity,  rp);
	dfa_node_add_child(star,      rp);

	return 0;
}

dfa_module_t dfa_module_va_arg =
{
	.name        = "va_arg",
	.init_module = _dfa_init_module_va_arg,
	.init_syntax = _dfa_init_syntax_va_arg,
};
