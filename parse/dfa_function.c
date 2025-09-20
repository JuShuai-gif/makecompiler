#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_function;

typedef struct {

	block_t*     parent_block;

} dfa_fun_data_t;

int _function_add_function(dfa_t* dfa, dfa_data_t* d)
{
	if (d->current_identities->size < 2) {
		loge("d->current_identities->size: %d\n", d->current_identities->size);
		return DFA_ERROR;
	}

	parse_t*    parse = dfa->priv;
	ast_t*      ast   = parse->ast;
	dfa_identity_t* id    = stack_pop(d->current_identities);
	dfa_fun_data_t* fd    = d->module_datas[dfa_module_function.index];

	function_t* f;
	variable_t* v;
	block_t*    b;

	if (!id || !id->identity) {
		loge("function identity not found\n");
		return DFA_ERROR;
	}

	b = ast->current_block;
	while (b) {
		if (b->node.type >= STRUCT)
			break;
		b = (block_t*)b->node.parent;
	}

	f = function_alloc(id->identity);
	if (!f)
		return DFA_ERROR;
	f->member_flag = !!b;

	free(id);
	id = NULL;

	logi("function: %s,line:%d, member_flag: %d\n", f->node.w->text->data, f->node.w->line, f->member_flag);

	int void_flag = 0;

	while (d->current_identities->size > 0) {

		id = stack_pop(d->current_identities);

		if (!id || !id->type || !id->type_w) {
			loge("function return value type NOT found\n");
			return DFA_ERROR;
		}

		if (VAR_VOID == id->type->type && 0 == id->nb_pointers)
			void_flag = 1;

		f->extern_flag |= id->extern_flag;
		f->static_flag |= id->static_flag;
		f->inline_flag |= id->inline_flag;

		if (f->extern_flag && (f->static_flag || f->inline_flag)) {
			loge("'extern' function can't be 'static' or 'inline'\n");
			return DFA_ERROR;
		}

		v  = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, NULL);
		free(id);
		id = NULL;

		if (!v) {
			function_free(f);
			return DFA_ERROR;
		}

		if (vector_add(f->rets, v) < 0) {
			variable_free(v);
			function_free(f);
			return DFA_ERROR;
		}
	}

	assert(f->rets->size > 0);

	if (void_flag && 1 != f->rets->size) {
		loge("void function must have no other return value\n");
		return DFA_ERROR;
	}

	f->void_flag = void_flag;

	if (f->rets->size > 4) {
		loge("function return values must NOT more than 4!\n");
		return DFA_ERROR;
	}

	int i;
	int j;
	for (i = 0; i < f->rets->size / 2;  i++) {
		j  =        f->rets->size - 1 - i;

		XCHG(f->rets->data[i], f->rets->data[j]);
	}

	scope_push_function(ast->current_block->scope, f);

	node_add_child((node_t*)ast->current_block, (node_t*)f);

	fd ->parent_block  = ast->current_block;
	ast->current_block = (block_t*)f;

	d->current_function = f;

	return DFA_NEXT_WORD;
}

int _function_add_arg(dfa_t* dfa, dfa_data_t* d)
{
	dfa_identity_t* t = NULL;
	dfa_identity_t* v = NULL;

	switch (d->current_identities->size) {
		case 0:
			break;
		case 1:
			t = stack_pop(d->current_identities);
			assert(t && t->type);
			break;
		case 2:
			v = stack_pop(d->current_identities);
			t = stack_pop(d->current_identities);
			assert(t && t->type);
			assert(v && v->identity);
			break;
		default:
			loge("\n");
			return DFA_ERROR;
			break;
	};

	if (t && t->type) {
		variable_t* arg = NULL;
		lex_word_t* w   = NULL;

		if (v && v->identity)
			w = v->identity;

		if (VAR_VOID == t->type->type && 0 == t->nb_pointers) {
			loge("\n");
			return DFA_ERROR;
		}

		if (!d->current_var) {
			arg = VAR_ALLOC_BY_TYPE(w, t->type, t->const_flag, t->nb_pointers, t->func_ptr);
			if (!arg)
				return DFA_ERROR;

			scope_push_var(d->current_function->scope, arg);
		} else {
			arg = d->current_var;

			if (arg->nb_dimentions > 0) {
				arg->nb_pointers += arg->nb_dimentions;
				arg->nb_dimentions = 0;
			}

			if (arg->dimentions) {
				free(arg->dimentions);
				arg->dimentions = NULL;
			}

			arg->const_literal_flag = 0;

			d->current_var = NULL;
		}

		logi("d->argc: %d, arg->nb_pointers: %d, arg->nb_dimentions: %d\n",
				d->argc, arg->nb_pointers, arg->nb_dimentions);

		vector_add(d->current_function->argv, arg);

		arg->refs++;
		arg->arg_flag   = 1;
		arg->local_flag = 1;

		if (v)
			free(v);
		free(t);

		d->argc++;
	}

	return DFA_NEXT_WORD;
}

static int _function_action_vargs(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->current_function->vargs_flag = 1;

	return DFA_NEXT_WORD;
}

static int _function_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	if (_function_add_arg(dfa, d) < 0) {
		loge("function add arg failed\n");
		return DFA_ERROR;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_comma"), DFA_HOOK_PRE);

	return DFA_NEXT_WORD;
}

static int _function_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	assert(!d->current_node);

	d->current_var = NULL;

	if (_function_add_function(dfa, d) < 0)
		return DFA_ERROR;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_rp"),    DFA_HOOK_PRE);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_comma"), DFA_HOOK_PRE);

	d->argc = 0;
	d->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _function_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];
	function_t*  f     = d->current_function;
	function_t*  fprev = NULL;

	d->nb_rps++;

	if (d->nb_rps < d->nb_lps) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "function_rp"), DFA_HOOK_PRE);
		return DFA_NEXT_WORD;
	}

	if (_function_add_arg(dfa, d) < 0) {
		loge("function add arg failed\n");
		return DFA_ERROR;
	}

	list_del(&f->list);
	node_del_child((node_t*)fd->parent_block, (node_t*)f);

	if (fd->parent_block->node.type >= STRUCT) {

		type_t* t = (type_t*)fd->parent_block;

		if (!t->node.class_flag) {
			loge("only class has member function\n");
			return DFA_ERROR;
		}

		assert(t->scope);

		if (!strcmp(f->node.w->text->data, "__init")) {

			fprev = scope_find_same_function(t->scope, f);

		} else if (!strcmp(f->node.w->text->data, "__release")) {

			fprev = scope_find_function(t->scope, f->node.w->text->data);

			if (fprev && !function_same(fprev, f)) {
				loge("function '%s' can't be overloaded, repeated declare first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line);
				return DFA_ERROR;
			}
		} else {
			loge("class member function must be '__init()' or '__release()', file: %s, line: %d\n", f->node.w->file->data, f->node.w->line);
			return DFA_ERROR;
		}
	} else {
		block_t* b = fd->parent_block;

		if (!b->node.root_flag && !b->node.file_flag) {
			loge("function should be defined in file, global, or class\n");
			return DFA_ERROR;
		}

		assert(b->scope);

		if (f->static_flag)
			fprev = scope_find_function(b->scope, f->node.w->text->data);
		else {
			int ret = ast_find_global_function(&fprev, parse->ast, f->node.w->text->data);
			if (ret < 0)
				return ret;
		}

		if (fprev && !function_same(fprev, f)) {

			loge("repeated declare function '%s', first in line: %d, second in line: %d, function overloading only can do in class\n",
					f->node.w->text->data, fprev->node.w->line, f->node.w->line);
			return DFA_ERROR;
		}
	}

	if (fprev) {
		if (!fprev->node.define_flag) {
			int i;
			variable_t* v0;
			variable_t* v1;

			for (i = 0; i < fprev->argv->size; i++) {
				v0 =        fprev->argv->data[i];
				v1 =        f    ->argv->data[i];

				if (v1->w)
					XCHG(v0->w, v1->w);
			}

			function_free(f);
			d->current_function = fprev;

		} else {
			lex_word_t* w = dfa->ops->pop_word(dfa);

			if (LEX_WORD_SEMICOLON != w->type) {

				loge("repeated define function '%s', first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line); 

				dfa->ops->push_word(dfa, w);
				return DFA_ERROR;
			}

			dfa->ops->push_word(dfa, w);
		}
	} else {
		scope_push_function(fd->parent_block->scope, f);

		node_add_child((node_t*)fd->parent_block, (node_t*)f);
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_end"), DFA_HOOK_END);

	parse->ast->current_block = (block_t*)d->current_function;

	return DFA_NEXT_WORD;
}

static int _function_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	parse->ast->current_block = (block_t*)(fd->parent_block);

	if (d->current_function->node.nb_nodes > 0)
		d->current_function->node.define_flag = 1;

	fd->parent_block = NULL;

	d->current_function = NULL;
	d->argc   = 0;
	d->nb_lps = 0;
	d->nb_rps = 0;

	return DFA_OK;
}

static int _dfa_init_module_function(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, function, comma,  dfa_is_comma, _function_action_comma);
	DFA_MODULE_NODE(dfa, function, vargs,  dfa_is_vargs, _function_action_vargs);
	DFA_MODULE_NODE(dfa, function, end,    dfa_is_entry, _function_action_end);

	DFA_MODULE_NODE(dfa, function, lp,     dfa_is_lp,    _function_action_lp);
	DFA_MODULE_NODE(dfa, function, rp,     dfa_is_rp,    _function_action_rp);

	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = parse->dfa_data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	assert(!fd);

	fd = calloc(1, sizeof(dfa_fun_data_t));
	if (!fd) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_function.index] = fd;

	return DFA_OK;
}

static int _dfa_fini_module_function(dfa_t* dfa)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = parse->dfa_data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	if (fd) {
		free(fd);
		fd = NULL;
		d->module_datas[dfa_module_function.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_function(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, function, comma,     comma);
	DFA_GET_MODULE_NODE(dfa, function, vargs,     vargs);

	DFA_GET_MODULE_NODE(dfa, function, lp,        lp);
	DFA_GET_MODULE_NODE(dfa, function, rp,        rp);

	DFA_GET_MODULE_NODE(dfa, type,     _const,    _const);
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, type,     identity,  identity);
	DFA_GET_MODULE_NODE(dfa, block,    entry,     block);

	// function start
	dfa_node_add_child(identity,  lp);

	// function args
	dfa_node_add_child(lp,        _const);
	dfa_node_add_child(lp,        base_type);
	dfa_node_add_child(lp,        type_name);
	dfa_node_add_child(lp,        rp);

	dfa_node_add_child(identity,  comma);
	dfa_node_add_child(identity,  rp);

	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(type_name, comma);
	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(type_name, rp);
	dfa_node_add_child(star,      comma);
	dfa_node_add_child(star,      rp);

	dfa_node_add_child(comma,     _const);
	dfa_node_add_child(comma,     base_type);
	dfa_node_add_child(comma,     type_name);
	dfa_node_add_child(comma,     vargs);
	dfa_node_add_child(vargs,     rp);

	// function body
	dfa_node_add_child(rp,        block);

	return 0;
}

dfa_module_t dfa_module_function =
{
	.name        = "function",

	.init_module = _dfa_init_module_function,
	.init_syntax = _dfa_init_syntax_function,

	.fini_module = _dfa_fini_module_function,
};
