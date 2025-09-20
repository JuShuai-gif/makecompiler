#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_union;

typedef struct {
	lex_word_t*  current_identity;

	block_t*     parent_block;

	type_t*      current_union;

	dfa_hook_t*  hook;

	int              nb_lbs;
	int              nb_rbs;

} union_module_data_t;

static int _union_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];
	lex_word_t*       w     = words->data[words->size - 1];

	if (md->current_identity) {
		loge("\n");
		return DFA_ERROR;
	}

	type_t* t = block_find_type(parse->ast->current_block, w->text->data);
	if (!t) {
		t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.union_flag = 1;
		scope_push_type(parse->ast->current_block->scope, t);
		node_add_child((node_t*)parse->ast->current_block, (node_t*)t);
	}

	md->current_identity = w;
	md->parent_block     = parse->ast->current_block;

	return DFA_NEXT_WORD;
}

static int _union_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];
	lex_word_t*       w     = words->data[words->size - 1];
	type_t*           t     = NULL;

	if (md->current_identity) {

		t = block_find_type(parse->ast->current_block, md->current_identity->text->data);
		if (!t) {
			loge("type '%s' not found\n", md->current_identity->text->data);
			return DFA_ERROR;
		}
	} else {
		t = type_alloc(w, "anonymous", STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.union_flag = 1;
		scope_push_type(parse->ast->current_block->scope, t);
		node_add_child((node_t*)parse->ast->current_block, (node_t*)t);
	}

	if (!t->scope)
		t->scope = scope_alloc(w, "union");

	md->parent_block  = parse->ast->current_block;
	md->current_union = t;
	md->nb_lbs++;

	parse->ast->current_block = (block_t*)t;

	md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "union_semicolon"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _union_calculate_size(dfa_t* dfa, type_t* s)
{
	variable_t* v;

	int max_size = 0;
	int i;
	int j;

	for (i = 0; i < s->scope->vars->size; i++) {
		v  =        s->scope->vars->data[i];

		assert(v->size >= 0);

		int size = 0;

		if (v->nb_dimentions > 0) {
			v->capacity = 1;

			for (j = 0; j < v->nb_dimentions; j++) {

				if (v->dimentions[j].num < 0) {
					loge("number of %d-dimention for array '%s' is less than 0, number: %d, file: %s, line: %d\n",
							j, v->w->text->data, v->dimentions[j].num, v->w->file->data, v->w->line);
					return DFA_ERROR;
				}

				if (0 == v->dimentions[j].num && j < v->nb_dimentions - 1) {

					loge("only the number of array's last dimention can be 0, array '%s', dimention: %d, file: %s, line: %d\n",
							v->w->text->data, j, v->w->file->data, v->w->line);
					return DFA_ERROR;
				}

				v->capacity *= v->dimentions[j].num;
			}

			size = v->size * v->capacity;
		} else
			size = v->size;

		if (max_size < size)
			max_size = size;

		logi("union '%s', member: '%s', size: %d, v->dim: %d, v->capacity: %d\n",
				s->name->data, v->w->text->data, v->size, v->nb_dimentions, v->capacity);
	}
	s->size = max_size;

	logi("union '%s', size: %d\n", s->name->data, s->size);
	return 0;
}

static int _union_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];

	if (_union_calculate_size(dfa, md->current_union) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	md->nb_rbs++;

	assert(md->nb_rbs == md->nb_lbs);

	parse->ast->current_block = md->parent_block;
	md->parent_block = NULL;

	return DFA_NEXT_WORD;
}

static int _union_action_var(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];
	lex_word_t*       w     = words->data[words->size - 1];

	if (!md->current_union) {
		loge("\n");
		return DFA_ERROR;
	}

	variable_t* var = variable_alloc(w, md->current_union);
	if (!var) {
		loge("var alloc failed\n");
		return DFA_ERROR;
	}

	scope_push_var(parse->ast->current_block->scope, var);

	logi("union var: '%s', type: %d, size: %d\n", w->text->data, var->type, var->size);

	return DFA_NEXT_WORD;
}

static int _union_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];

	if (md->nb_rbs == md->nb_lbs) {
		logi("DFA_OK\n");

//		parse->ast->current_block = md->parent_block;
//		md->parent_block     = NULL;

		md->current_identity = NULL;
		md->current_union    = NULL;
		md->nb_lbs           = 0;
		md->nb_rbs           = 0;

		dfa_del_hook(&(dfa->hooks[DFA_HOOK_POST]), md->hook);
		md->hook             = NULL;

		return DFA_OK;
	}

	md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "union_semicolon"), DFA_HOOK_POST);

	logi("\n");
	return DFA_SWITCH_TO;
}

static int _dfa_init_module_union(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, union, _union,    dfa_is_union,     NULL);

	DFA_MODULE_NODE(dfa, union, identity,  dfa_is_identity,  _union_action_identity);

	DFA_MODULE_NODE(dfa, union, lb,        dfa_is_lb,        _union_action_lb);
	DFA_MODULE_NODE(dfa, union, rb,        dfa_is_rb,        _union_action_rb);
	DFA_MODULE_NODE(dfa, union, semicolon, dfa_is_semicolon, _union_action_semicolon);

	DFA_MODULE_NODE(dfa, union, var,       dfa_is_identity,  _union_action_var);

	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];

	assert(!md);

	md = calloc(1, sizeof(union_module_data_t));
	if (!md) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_union.index] = md;

	return DFA_OK;
}

static int _dfa_fini_module_union(dfa_t* dfa)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	union_module_data_t*  md    = d->module_datas[dfa_module_union.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_union.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_union(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, union,  _union,    _union);
	DFA_GET_MODULE_NODE(dfa, union,  identity,  identity);
	DFA_GET_MODULE_NODE(dfa, union,  lb,        lb);
	DFA_GET_MODULE_NODE(dfa, union,  rb,        rb);
	DFA_GET_MODULE_NODE(dfa, union,  semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, union,  var,       var);

	DFA_GET_MODULE_NODE(dfa, type, entry,    member);

	vector_add(dfa->syntaxes,     _union);

	// union start
	dfa_node_add_child(_union,    identity);

	dfa_node_add_child(identity,  semicolon);
	dfa_node_add_child(identity,  lb);

	// anonymous union, will be only used in struct or class to define vars shared the same memory
	dfa_node_add_child(_union,    lb);

	// empty union
	dfa_node_add_child(lb,        rb);

	// member var
	dfa_node_add_child(lb,        member);
	dfa_node_add_child(member,    semicolon);
	dfa_node_add_child(semicolon, rb);
	dfa_node_add_child(semicolon, member);

	// end
	dfa_node_add_child(rb,        var);
	dfa_node_add_child(var,       semicolon);
	dfa_node_add_child(rb,        semicolon);

	return 0;
}

dfa_module_t dfa_module_union = 
{
	.name        = "union",
	.init_module = _dfa_init_module_union,
	.init_syntax = _dfa_init_syntax_union,

	.fini_module = _dfa_fini_module_union,
};
