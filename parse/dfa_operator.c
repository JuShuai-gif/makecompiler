#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_operator;

typedef struct {

	block_t*     parent_block;

	lex_word_t*  word_op;

} dfa_op_data_t;

static int _operator_is_key(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_OPERATOR == w->type;
}

static int _operator_is_op(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return (LEX_WORD_PLUS <= w->type && LEX_WORD_GE >= w->type);
}

int _operator_add_operator(dfa_t* dfa, dfa_data_t* d)
{
	parse_t*     parse = dfa->priv;
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];
	dfa_identity_t*  id;
	function_t*  f;
	variable_t*  v;

	if (!opd->word_op) {
		loge("\n");
		return DFA_ERROR;
	}

	f = function_alloc(opd->word_op);
	if (!f) {
		loge("operator overloading function alloc failed\n");
		return DFA_ERROR;
	}

	logi("operator: %s,line:%d,pos:%d\n", f->node.w->text->data, f->node.w->line, f->node.w->pos);

	while (d->current_identities->size > 0) {

		id = stack_pop(d->current_identities);

		if (!id || !id->type || !id->type_w) {
			loge("function return value type NOT found\n");
			return DFA_ERROR;
		}

		f->static_flag |= id->static_flag;
		f->inline_flag |= id->inline_flag;

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

	int i;
	int j;
	for (i = 0; i < f->rets->size / 2;  i++) {
		j  =        f->rets->size - 1 - i;

		XCHG(f->rets->data[i], f->rets->data[j]);
	}

	opd->word_op = NULL;

	d->current_function = f;

	return DFA_NEXT_WORD;
}

int _operator_add_arg(dfa_t* dfa, dfa_data_t* d)
{
	variable_t* arg = NULL;

	if (d->current_identities->size > 2) {
		loge("operator parse args error\n");
		return DFA_ERROR;
	}

	if (2 == d->current_identities->size) {

		dfa_identity_t* id1 = stack_pop(d->current_identities);
		dfa_identity_t* id0 = stack_pop(d->current_identities);

		if (!id0 || !id0->type) {
			loge("operator parse arg type error\n");
			return DFA_ERROR;
		}

		if (!id1 || !id1->identity) {
			loge("operator parse arg name error\n");
			return DFA_ERROR;
		}

		arg = VAR_ALLOC_BY_TYPE(id1->identity, id0->type, id0->const_flag, id0->nb_pointers, id0->func_ptr);
		if (!arg) {
			loge("arg var alloc failed\n");
			return DFA_ERROR;
		}

		vector_add(d->current_function->argv, arg);
		scope_push_var(d->current_function->scope, arg);
		arg->refs++;
		arg->arg_flag   = 1;
		arg->local_flag = 1;

		logi("d->current_function->argv->size: %d, %p\n", d->current_function->argv->size, d->current_function);

		free(id0);
		free(id1);

		d->argc++;
	}

	return DFA_NEXT_WORD;
}

static int _operator_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	if (_operator_add_arg(dfa, d) < 0) {
		loge("add arg failed\n");
		return DFA_ERROR;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_comma"), DFA_HOOK_PRE);

	return DFA_NEXT_WORD;
}

static int _operator_action_op(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	opd->word_op = words->data[words->size - 1];

	return DFA_NEXT_WORD;
}

static int _operator_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	assert(!d->current_node);

	if (_operator_add_operator(dfa, d) < 0) {
		loge("add operator failed\n");
		return DFA_ERROR;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_rp"), DFA_HOOK_PRE);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_comma"), DFA_HOOK_PRE);

	d->argc = 0;
	d->nb_lps++;

	return DFA_NEXT_WORD;
}

static int _operator_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];
	function_t*  f     = NULL;

	d->nb_rps++;

	if (d->nb_rps < d->nb_lps) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_rp"), DFA_HOOK_PRE);
		return DFA_NEXT_WORD;
	}

	if (_operator_add_arg(dfa, d) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	if (parse->ast->current_block->node.type >= STRUCT) {

		type_t* t = (type_t*)parse->ast->current_block;

		if (!t->node.class_flag) {
			loge("only class has operator overloading\n");
			return DFA_ERROR;
		}

		assert(t->scope);

		f = scope_find_same_function(t->scope, d->current_function);

	} else {
		loge("only class has operator overloading\n");
		return DFA_ERROR;
	}

	if (f) {
		if (!f->node.define_flag) {
			int i;

			for (i = 0; i < f->argv->size; i++) {
				variable_t* v0 = f->argv->data[i];
				variable_t* v1 = d->current_function->argv->data[i];

				if (v1->w) {
					if (v0->w)
						lex_word_free(v0->w);
					v0->w = lex_word_clone(v1->w);
				}
			}

			function_free(d->current_function);
			d->current_function = f;
		} else {
			lex_word_t* w = dfa->ops->pop_word(dfa);

			if (LEX_WORD_SEMICOLON != w->type) {

				loge("repeated define operator '%s', first in line: %d, second in line: %d\n",
						f->node.w->text->data, f->node.w->line, d->current_function->node.w->line); 

				dfa->ops->push_word(dfa, w);
				return DFA_ERROR;
			}

			dfa->ops->push_word(dfa, w);
		}
	} else {
		operator_t* op = find_base_operator(d->current_function->node.w->text->data, d->current_function->argv->size);

		if (!op || !op->signature) {
			loge("operator: '%s', nb_operands: %d\n",
					d->current_function->node.w->text->data, d->current_function->argv->size);
			return DFA_ERROR;
		}

		d->current_function->op_type = op->type;

		scope_push_operator(parse->ast->current_block->scope, d->current_function);
		node_add_child((node_t*)parse->ast->current_block, (node_t*)d->current_function);
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_end"), DFA_HOOK_END);

	opd->parent_block = parse->ast->current_block;
	parse->ast->current_block = (block_t*)d->current_function;

	return DFA_NEXT_WORD;
}

static int _operator_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];

	parse->ast->current_block  = (block_t*)(opd->parent_block);

	if (d->current_function->node.nb_nodes > 0)
		d->current_function->node.define_flag = 1;

	opd->parent_block = NULL;

	d->current_function = NULL;
	d->argc   = 0;
	d->nb_lps = 0;
	d->nb_rps = 0;

	logi("\n");
	return DFA_OK;
}

static int _dfa_init_module_operator(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, operator, comma,  dfa_is_comma, _operator_action_comma);
	DFA_MODULE_NODE(dfa, operator, end,    dfa_is_entry, _operator_action_end);

	DFA_MODULE_NODE(dfa, operator, lp,     dfa_is_lp,    _operator_action_lp);
	DFA_MODULE_NODE(dfa, operator, rp,     dfa_is_rp,    _operator_action_rp);

	DFA_MODULE_NODE(dfa, operator, ls,     dfa_is_ls,    _operator_action_op);
	DFA_MODULE_NODE(dfa, operator, rs,     dfa_is_rs,    NULL);

	DFA_MODULE_NODE(dfa, operator, key,    _operator_is_key, NULL);
	DFA_MODULE_NODE(dfa, operator, op,     _operator_is_op,  _operator_action_op);

	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = parse->dfa_data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	assert(!opd);

	opd = calloc(1, sizeof(dfa_op_data_t));
	if (!opd) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_operator.index] = opd;

	return DFA_OK;
}

static int _dfa_fini_module_operator(dfa_t* dfa)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = parse->dfa_data;
	dfa_op_data_t*  opd    = d->module_datas[dfa_module_operator.index];

	if (opd) {
		free(opd);
		opd = NULL;
		d->module_datas[dfa_module_operator.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_operator(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, operator, comma,     comma);
	DFA_GET_MODULE_NODE(dfa, operator, end,       end);

	DFA_GET_MODULE_NODE(dfa, operator, lp,        lp);
	DFA_GET_MODULE_NODE(dfa, operator, rp,        rp);

	DFA_GET_MODULE_NODE(dfa, operator, ls,        ls);
	DFA_GET_MODULE_NODE(dfa, operator, rs,        rs);

	DFA_GET_MODULE_NODE(dfa, operator, key,       key);
	DFA_GET_MODULE_NODE(dfa, operator, op,        op);

	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, type,     identity,  identity);
	DFA_GET_MODULE_NODE(dfa, block,    entry,     block);

	// operator start
	dfa_node_add_child(base_type, key);
	dfa_node_add_child(type_name, key);
	dfa_node_add_child(star,      key);

	dfa_node_add_child(key,       op);

	dfa_node_add_child(key,       ls);
	dfa_node_add_child(ls,        rs);

	dfa_node_add_child(op,        lp);
	dfa_node_add_child(rs,        lp);

	// operator args
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

	dfa_node_add_child(comma,     base_type);
	dfa_node_add_child(comma,     type_name);

	// operator body
	dfa_node_add_child(rp,        block);

	return 0;
}

dfa_module_t dfa_module_operator =
{
	.name        = "operator",

	.init_module = _dfa_init_module_operator,
	.init_syntax = _dfa_init_syntax_operator,

	.fini_module = _dfa_fini_module_operator,
};
