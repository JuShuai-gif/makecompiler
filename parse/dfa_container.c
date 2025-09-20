#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_container;

typedef struct {

	int              nb_lps;
	int              nb_rps;

	node_t*      container;

	expr_t*      parent_expr;

} dfa_container_data_t;

static int _container_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*           d  = data;
	stack_t*          s  = d->module_datas[dfa_module_container.index];
	dfa_container_data_t* cd = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	cd->nb_lps++;

	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _container_action_container(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	stack_t*          s     = d->module_datas[dfa_module_container.index];

	dfa_container_data_t* cd    = calloc(1, sizeof(dfa_container_data_t));
	if (!cd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	node_t* container = node_alloc(w, OP_CONTAINER, NULL);
	if (!container) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);

	cd->container   = container;
	cd->parent_expr = d->expr;
	d->expr         = NULL;
	d->expr_local_flag++;
	d->nb_containers++;

	stack_push(s, cd);

	return DFA_NEXT_WORD;
}

static int _container_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

static int _container_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	stack_t*          s     = d->module_datas[dfa_module_container.index];
	dfa_container_data_t* cd    = stack_top(s);

	if (!cd)
		return DFA_NEXT_SYNTAX;

	if (0 == cd->container->nb_nodes) {
		if (!d->expr) {
			loge("\n");
			return DFA_ERROR;
		}

		node_add_child(cd->container, d->expr);
		d->expr = NULL;
		d->expr_local_flag--;

		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_PRE);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);

	} else if (1 == cd->container->nb_nodes) {

		variable_t* v;
		dfa_identity_t* id;
		node_t*     node;

		id = stack_pop(d->current_identities);
		assert(id);

		if (!id->type) {
			if (ast_find_type(&id->type, parse->ast, id->identity->text->data) < 0) {
				free(id);
				return DFA_ERROR;
			}

			if (!id->type) {
				loge("can't find type '%s'\n", w->text->data);
				free(id);
				return DFA_ERROR;
			}

			if (id->type->type < STRUCT) {
				loge("'%s' is not a class or struct\n", w->text->data);
				free(id);
				return DFA_ERROR;
			}

			id->type_w   = id->identity;
			id->identity = NULL;
		}

		v = VAR_ALLOC_BY_TYPE(id->type_w, id->type, 0, 1, NULL);
		if (!v) {
			loge("\n");
			return DFA_ERROR;
		}

		node = node_alloc(NULL, v->type, v);
		if (!node) {
			loge("\n");
			return DFA_ERROR;
		}

		node_add_child(cd->container, node);

		loge("\n");
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),   DFA_HOOK_PRE);

		free(id);
		id = NULL;
	} else {
		loge("\n");
		return DFA_ERROR;
	}

	loge("\n");
	return DFA_SWITCH_TO;
}

static int _container_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	stack_t*          s     = d->module_datas[dfa_module_container.index];
	dfa_container_data_t* cd    = stack_top(s);

	if (d->current_va_arg)
		return DFA_NEXT_SYNTAX;

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	if (cd->container->nb_nodes >= 3) {
		stack_pop(s);
		free(cd);
		cd = NULL;
		return DFA_NEXT_WORD;
	}

	cd->nb_rps++;

	logd("cd->nb_lps: %d, cd->nb_rps: %d\n", cd->nb_lps, cd->nb_rps);

	if (cd->nb_rps < cd->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}
	assert(cd->nb_rps == cd->nb_lps);

	variable_t* v;
	dfa_identity_t* id;
	node_t*     node;
	type_t*     t;

	id = stack_pop(d->current_identities);
	assert(id && id->identity);

	t = NULL;
	if (ast_find_type_type(&t, parse->ast, cd->container->nodes[1]->type) < 0)
		return DFA_ERROR;
	assert(t);

	v = scope_find_variable(t->scope, id->identity->text->data);
	assert(v);

	node = node_alloc(NULL, v->type, v);
	if (!node) {
		loge("\n");
		return DFA_ERROR;
	}

	node_add_child(cd->container, node);

	logi("cd->container->nb_nodes: %d\n", cd->container->nb_nodes);

	if (cd->parent_expr) {
		if (expr_add_node(cd->parent_expr, cd->container) < 0) {
			loge("\n");
			return DFA_ERROR;
		}
		d->expr = cd->parent_expr;
	} else
		d->expr = cd->container;

	d->nb_containers--;

	logi("d->expr: %p, d->expr_local_flag: %d, d->nb_containers: %d\n", d->expr, d->expr_local_flag, d->nb_containers);

	return DFA_NEXT_WORD;
}

static int _dfa_init_module_container(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, container, container, dfa_is_container, _container_action_container);
	DFA_MODULE_NODE(dfa, container, lp,        dfa_is_lp,        _container_action_lp);
	DFA_MODULE_NODE(dfa, container, rp,        dfa_is_rp,        _container_action_rp);
	DFA_MODULE_NODE(dfa, container, lp_stat,   dfa_is_lp,        _container_action_lp_stat);
	DFA_MODULE_NODE(dfa, container, comma,     dfa_is_comma,     _container_action_comma);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_container.index];

	assert(!s);

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_container.index] = s;

	return DFA_OK;
}

static int _dfa_fini_module_container(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_container.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_container.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_container(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa,      container,   container, container);
	DFA_GET_MODULE_NODE(dfa,      container,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa,      container,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa,      container,   comma,     comma);

	DFA_GET_MODULE_NODE(dfa,      expr,        entry,     expr);

	DFA_GET_MODULE_NODE(dfa,      type,        entry,     type);
	DFA_GET_MODULE_NODE(dfa,      type,        base_type, base_type);
	DFA_GET_MODULE_NODE(dfa,      identity,    identity,  identity);

	dfa_node_add_child(container, lp);
	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      comma);

	dfa_node_add_child(comma,     type);
	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(identity,  comma);

	dfa_node_add_child(comma,     identity);
	dfa_node_add_child(identity,  rp);

	return 0;
}

dfa_module_t dfa_module_container =
{
	.name        = "container",
	.init_module = _dfa_init_module_container,
	.init_syntax = _dfa_init_syntax_container,

	.fini_module = _dfa_fini_module_container,
};
