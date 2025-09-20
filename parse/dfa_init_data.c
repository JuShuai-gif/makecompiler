#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t  dfa_module_init_data;

int array_init (ast_t* ast, lex_word_t* w, variable_t* var, vector_t* init_exprs);
int struct_init(ast_t* ast, lex_word_t* w, variable_t* var, vector_t* init_exprs);

int _expr_add_var(parse_t* parse, dfa_data_t* d);

typedef struct {
	lex_word_t*  assign;
	vector_t*    init_exprs;

	dfa_index_t*     current_index;
	int              current_n;
	int              current_dim;

	int              nb_lbs;
	int              nb_rbs;
} init_module_data_t;

static int _do_data_init(dfa_t* dfa, vector_t* words, dfa_data_t* d)
{
	parse_t*        parse = dfa->priv;
	variable_t*     var   = d->current_var;
	lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];
	dfa_init_expr_t*    ie;

	int ret = -1;
	int i   = 0;

	if (d->current_var->nb_dimentions > 0)
		ret = array_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	else if (d->current_var->type >=  STRUCT)
		ret = struct_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	if (ret < 0)
		goto error;

	for (i = 0; i < md->init_exprs->size; i++) {
		ie =        md->init_exprs->data[i];

		if (d->current_var->global_flag) {

			ret = expr_calculate(parse->ast, ie->expr, NULL);
			if (ret < 0)
				goto error;

			expr_free(ie->expr);
			ie->expr = NULL;

		} else {
			ret = node_add_child((node_t*)parse->ast->current_block, ie->expr);
			if (ret < 0)
				goto error;
			ie->expr = NULL;
		}

		free(ie);
		ie = NULL;
	}

error:
	for (; i < md->init_exprs->size; i++) {
		ie =   md->init_exprs->data[i];

		expr_free(ie->expr);
		free(ie);
		ie = NULL;
	}

	md->assign = NULL;

	vector_free(md->init_exprs);
	md->init_exprs = NULL;

	free(md->current_index);
	md->current_index = NULL;

	md->current_dim = -1;
	md->nb_lbs      = 0;
	md->nb_rbs      = 0;

	return ret;
}

static int _add_data_init_expr(dfa_t* dfa, vector_t* words, dfa_data_t* d)
{
	init_module_data_t* md = d->module_datas[dfa_module_init_data.index];
	dfa_init_expr_t*    ie;

	assert(!d->expr->parent);
	assert(md->current_dim >= 0);

	size_t N =  sizeof(dfa_index_t) * md->current_n;

	ie = malloc(sizeof(dfa_init_expr_t) + N);
	if (!ie)
		return -ENOMEM;

	ie->expr = d->expr;
	d ->expr = NULL;
	ie->n    = md->current_n;

	memcpy(ie->index, md->current_index, N);

	int ret = vector_add(md->init_exprs, ie);
	if (ret < 0)
		return ret;

	return DFA_OK;
}

static int _data_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim < 0) {
		logi("md->current_dim: %d\n", md->current_dim);
		return DFA_NEXT_SYNTAX;
	}

	if (d->expr) {
		if (_add_data_init_expr(dfa, words, d) < 0)
			return DFA_ERROR;
	}

	md->current_index[md->current_dim].w = NULL;
	md->current_index[md->current_dim].i++;

	intptr_t i = md->current_index[md->current_dim].i;

	logi("\033[31m md->current_dim[%d]: %ld\033[0m\n", md->current_dim, i);

	DFA_PUSH_HOOK(dfa_find_node(dfa, "init_data_comma"), DFA_HOOK_POST);

	return DFA_SWITCH_TO;
}

static int _data_action_entry(dfa_t* dfa, vector_t* words, void* data)
{
	assert(words->size >= 2);

	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 2];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	assert(LEX_WORD_ASSIGN == w->type);

	md->assign = w;
	md->nb_lbs = 0;
	md->nb_rbs = 0;

	assert(!md->init_exprs);
	assert(!md->current_index);

	md->init_exprs = vector_alloc();
	if (!md->init_exprs)
		return -ENOMEM;

	md->current_dim = -1;
	md->current_n   = 0;

	d->expr_local_flag = 1;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "init_data_comma"), DFA_HOOK_POST);

	return DFA_CONTINUE;
}

static int _data_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	d->expr = NULL;
	md->current_dim++;
	md->nb_lbs++;

	if (md->current_dim >= md->current_n) {

		void* p = realloc(md->current_index, sizeof(dfa_index_t) * (md->current_dim + 1));
		if (!p)
			return -ENOMEM;
		md->current_index = p;
		md->current_n     = md->current_dim + 1;
	}

	int i;
	for (i = md->current_dim; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	return DFA_NEXT_WORD;
}

static int _data_action_member(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];
	variable_t*     v;
	type_t*         t;

	if (md->current_dim >= md->current_n) {
		loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	assert(d->current_var);

	t = NULL;
	ast_find_type_type(&t, parse->ast, d->current_var->type);
	if (!t->scope) {
		loge("base type '%s' has no member var '%s', file: %s, line: %d\n",
				t->name->data, w->text->data, w->file->data, w->line);
		return DFA_ERROR;
	}

	v = scope_find_variable(t->scope, w->text->data);
	if (!v) {
		loge("member var '%s' NOT found in struct '%s', file: %s, line: %d\n",
				w->text->data, t->name->data, w->file->data, w->line);
		return DFA_ERROR;
	}

	md->current_index[md->current_dim].w = w;

	return DFA_NEXT_WORD;
}

static int _error_action_member(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = words->data[words->size - 1];

	loge("member '%s' should be an var in struct, file: %s, line: %d\n",
			w->text->data, w->file->data, w->line);
	return DFA_ERROR;
}

static int _error_action_index(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = words->data[words->size - 1];

	loge("array index '%s' should be an integer, file: %s, line: %d\n",
			w->text->data, w->file->data, w->line);
	return DFA_ERROR;
}

static int _data_action_index(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim >= md->current_n) {
		loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	md->current_index[md->current_dim].i = w->data.u64;

	return DFA_NEXT_WORD;
}

static int _data_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (d->expr) {
		dfa_identity_t* id = stack_top(d->current_identities);

		if (id && id->identity) {
			if (_expr_add_var(parse, d) < 0)
				return DFA_ERROR;
		}

		if (_add_data_init_expr(dfa, words, d) < 0)
			return DFA_ERROR;
	}

	md->nb_rbs++;
	md->current_dim--;

	int i;
	for (i = md->current_dim + 1; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	if (md->nb_rbs == md->nb_lbs) {
		d->expr_local_flag = 0;

		if (_do_data_init(dfa, words, d) < 0)
			return DFA_ERROR;

		dfa_del_hook_by_name(&(dfa->hooks[DFA_HOOK_POST]), "init_data_comma");
	}

	return DFA_NEXT_WORD;
}

static int _dfa_init_module_init_data(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, init_data, entry,  dfa_is_lb,             _data_action_entry);
	DFA_MODULE_NODE(dfa, init_data, comma,  dfa_is_comma,          _data_action_comma);
	DFA_MODULE_NODE(dfa, init_data, lb,     dfa_is_lb,             _data_action_lb);
	DFA_MODULE_NODE(dfa, init_data, rb,     dfa_is_rb,             _data_action_rb);

	DFA_MODULE_NODE(dfa, init_data, ls,     dfa_is_ls,             dfa_action_next);
	DFA_MODULE_NODE(dfa, init_data, rs,     dfa_is_rs,             dfa_action_next);

	DFA_MODULE_NODE(dfa, init_data, dot,    dfa_is_dot,            dfa_action_next);
	DFA_MODULE_NODE(dfa, init_data, member, dfa_is_identity,       _data_action_member);
	DFA_MODULE_NODE(dfa, init_data, index,  dfa_is_const_integer,  _data_action_index);
	DFA_MODULE_NODE(dfa, init_data, assign, dfa_is_assign,         dfa_action_next);

	DFA_MODULE_NODE(dfa, init_data, merr,   dfa_is_entry,          _error_action_member);
	DFA_MODULE_NODE(dfa, init_data, ierr,   dfa_is_entry,          _error_action_index);

	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = parse->dfa_data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	assert(!md);

	md = calloc(1, sizeof(init_module_data_t));
	if (!md)
		return -ENOMEM;

	d->module_datas[dfa_module_init_data.index] = md;

	return DFA_OK;
}

static int _dfa_init_syntax_init_data(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, init_data, entry,  entry);
	DFA_GET_MODULE_NODE(dfa, init_data, comma,  comma);
	DFA_GET_MODULE_NODE(dfa, init_data, lb,     lb);
	DFA_GET_MODULE_NODE(dfa, init_data, rb,     rb);

	DFA_GET_MODULE_NODE(dfa, init_data, ls,     ls);
	DFA_GET_MODULE_NODE(dfa, init_data, rs,     rs);

	DFA_GET_MODULE_NODE(dfa, init_data, dot,    dot);
	DFA_GET_MODULE_NODE(dfa, init_data, member, member);
	DFA_GET_MODULE_NODE(dfa, init_data, index,  index);
	DFA_GET_MODULE_NODE(dfa, init_data, assign, assign);

	DFA_GET_MODULE_NODE(dfa, init_data, merr,   merr);
	DFA_GET_MODULE_NODE(dfa, init_data, ierr,   ierr);

	DFA_GET_MODULE_NODE(dfa, expr,      entry,  expr);

	// empty init, use 0 to fill the data
	dfa_node_add_child(entry,     lb);
	dfa_node_add_child(lb,        rb);

	// multi-dimention data init
	dfa_node_add_child(lb,        lb);
	dfa_node_add_child(rb,        rb);
	dfa_node_add_child(rb,        comma);
	dfa_node_add_child(comma,     lb);

	// init expr for member of data
	dfa_node_add_child(lb,        dot);
	dfa_node_add_child(lb,        ls);
	dfa_node_add_child(comma,     dot);
	dfa_node_add_child(comma,     ls);

	dfa_node_add_child(lb,        expr);
	dfa_node_add_child(expr,      comma);
	dfa_node_add_child(comma,     expr);
	dfa_node_add_child(expr,      rb);

	dfa_node_add_child(dot,       member);
	dfa_node_add_child(member,    assign);
	dfa_node_add_child(assign,    expr);

	dfa_node_add_child(ls,        index);
	dfa_node_add_child(index,     rs);
	dfa_node_add_child(rs,        ls);
	dfa_node_add_child(rs,        assign);

	dfa_node_add_child(dot,       merr);
	dfa_node_add_child(ls,        ierr);
	return 0;
}

dfa_module_t dfa_module_init_data =
{
	.name        = "init_data",
	.init_module = _dfa_init_module_init_data,
	.init_syntax = _dfa_init_syntax_init_data,
};
