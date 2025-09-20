#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_type;

static int _type_is__struct(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CLASS  == w->type
		|| LEX_WORD_KEY_STRUCT == w->type;
}

static int _type_action_const(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->const_flag = 1;

	return DFA_NEXT_WORD;
}

static int _type_action_extern(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->extern_flag = 1;

	return DFA_NEXT_WORD;
}

static int _type_action_static(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->static_flag = 1;

	return DFA_NEXT_WORD;
}

static int _type_action_inline(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->inline_flag = 1;

	return DFA_NEXT_WORD;
}

static int _type_action_base_type(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_data_t*      d     = data;
	stack_t*     s     = d->current_identities;
	dfa_identity_t*  id    = calloc(1, sizeof(dfa_identity_t));

	if (!id)
		return DFA_ERROR;

	id->type = block_find_type(parse->ast->current_block, w->text->data);
	if (!id->type) {
		loge("can't find type '%s'\n", w->text->data);

		free(id);
		return DFA_ERROR;
	}

	if (stack_push(s, id) < 0) {
		free(id);
		return DFA_ERROR;
	}

	id->type_w      = w;

	id->const_flag  = d->const_flag;
	id->static_flag = d->static_flag;
	id->extern_flag = d->extern_flag;
	id->inline_flag = d->inline_flag;

	d ->const_flag  = 0;
	d ->static_flag = 0;
	d ->extern_flag = 0;
	d ->inline_flag = 0;

	return DFA_NEXT_WORD;
}

static function_t* _type_find_function(block_t* b, const char* name)
{
	while (b) {
		if (!b->node.file_flag && !b->node.root_flag) {
			b = (block_t*)b->node.parent;
			continue;
		}

		assert(b->scope);

		function_t* f = scope_find_function(b->scope, name);
		if (f)
			return f;

		b = (block_t*)b->node.parent;
	}

	return NULL;
}

int _type_find_type(dfa_t* dfa, dfa_identity_t* id)
{
	parse_t* parse = dfa->priv;

	if (!id->identity)
		return 0;

	id->type = block_find_type(parse->ast->current_block, id->identity->text->data);
	if (!id->type) {

		int ret = ast_find_global_type(&id->type, parse->ast, id->identity->text->data);
		if (ret < 0) {
			loge("find global function error\n");
			return DFA_ERROR;
		}

		if (!id->type) {
			id->type = block_find_type_type(parse->ast->current_block, FUNCTION_PTR);

			if (!id->type) {
				loge("function ptr not support\n");
				return DFA_ERROR;
			}
		}

		if (FUNCTION_PTR == id->type->type) {

			id->func_ptr = _type_find_function(parse->ast->current_block, id->identity->text->data);

			if (!id->func_ptr) {
				loge("can't find funcptr type '%s'\n", id->identity->text->data);
				return DFA_ERROR;
			}
		}
	}

	id->type_w   = id->identity;
	id->identity = NULL;
	return 0;
}

static int _type_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_data_t*      d     = data;
	stack_t*     s     = d->current_identities;
	dfa_identity_t*  id    = NULL;

	if (s->size > 0) {
		id = stack_top(s);

		int ret = _type_find_type(dfa, id);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	id  = calloc(1, sizeof(dfa_identity_t));
	if (!id)
		return DFA_ERROR;

	if (stack_push(s, id) < 0) {
		free(id);
		return DFA_ERROR;
	}
	id->identity = w;

	return DFA_NEXT_WORD;
}

static int _type_action_star(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d  = data;
	dfa_identity_t*   id = stack_top(d->current_identities);

	assert(id);

	if (!id->type) {
		int ret = _type_find_type(dfa, id);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	id->nb_pointers++;

	return DFA_NEXT_WORD;
}

static int _type_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*      d  = data;
	dfa_identity_t*  id = stack_top(d->current_identities);

	assert(id);

	if (!id->type) {
		int ret = _type_find_type(dfa, id);
		if (ret < 0) {
			loge("\n");
			return ret;
		}
	}

	return DFA_NEXT_WORD;
}

static int _dfa_init_module_type(dfa_t* dfa)
{
	DFA_MODULE_ENTRY(dfa, type);

	DFA_MODULE_NODE(dfa, type, _struct,   _type_is__struct,     NULL);

	DFA_MODULE_NODE(dfa, type, _const,    dfa_is_const,     _type_action_const);
	DFA_MODULE_NODE(dfa, type, _static,   dfa_is_static,    _type_action_static);
	DFA_MODULE_NODE(dfa, type, _extern,   dfa_is_extern,    _type_action_extern);
	DFA_MODULE_NODE(dfa, type, _inline,   dfa_is_inline,    _type_action_inline);

	DFA_MODULE_NODE(dfa, type, base_type, dfa_is_base_type, _type_action_base_type);
	DFA_MODULE_NODE(dfa, type, identity,  dfa_is_identity,  _type_action_identity);
	DFA_MODULE_NODE(dfa, type, star,      dfa_is_star,      _type_action_star);
	DFA_MODULE_NODE(dfa, type, comma,     dfa_is_comma,     _type_action_comma);

	return DFA_OK;
}

static int _dfa_init_syntax_type(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, type,     entry,     entry);

	DFA_GET_MODULE_NODE(dfa, type,     _const,    _const);
	DFA_GET_MODULE_NODE(dfa, type,     _static,   _static);
	DFA_GET_MODULE_NODE(dfa, type,     _extern,   _extern);
	DFA_GET_MODULE_NODE(dfa, type,     _inline,   _inline);

	DFA_GET_MODULE_NODE(dfa, type,     _struct,   _struct);
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, type,     identity,  var_name);
	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, type,     comma,     comma);

	DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);


	vector_add(dfa->syntaxes,     entry);

	dfa_node_add_child(entry,     _static);
	dfa_node_add_child(entry,     _extern);
	dfa_node_add_child(entry,     _const);
	dfa_node_add_child(entry,     _inline);

	dfa_node_add_child(entry,     _struct);
	dfa_node_add_child(entry,     base_type);
	dfa_node_add_child(entry,     type_name);

	dfa_node_add_child(_static,   _struct);
	dfa_node_add_child(_static,   base_type);
	dfa_node_add_child(_static,   type_name);

	dfa_node_add_child(_extern,   _struct);
	dfa_node_add_child(_extern,   base_type);
	dfa_node_add_child(_extern,   type_name);

	dfa_node_add_child(_const,    _struct);
	dfa_node_add_child(_const,    base_type);
	dfa_node_add_child(_const,    type_name);

	dfa_node_add_child(_inline,   _struct);
	dfa_node_add_child(_inline,   base_type);
	dfa_node_add_child(_inline,   type_name);

	dfa_node_add_child(_static,   _inline);
	dfa_node_add_child(_static,   _const);
	dfa_node_add_child(_extern,   _const);
	dfa_node_add_child(_inline,   _const);

	dfa_node_add_child(_struct,   type_name);

	// multi-pointer
	dfa_node_add_child(star,      star);
	dfa_node_add_child(star,      var_name);

	dfa_node_add_child(base_type, star);
	dfa_node_add_child(type_name, star);

	dfa_node_add_child(base_type, var_name);
	dfa_node_add_child(type_name, var_name);

	// multi-return-value function
	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(star,      comma);
	dfa_node_add_child(comma,     _struct);
	dfa_node_add_child(comma,     base_type);
	dfa_node_add_child(comma,     type_name);

	int i;
	for (i = 0; i < base_type->childs->size; i++) {
		dfa_node_t* n = base_type->childs->data[i];

		logd("n->name: %s\n", n->name);
	}

	return DFA_OK;
}

dfa_module_t dfa_module_type = 
{
	.name        = "type",
	.init_module = _dfa_init_module_type,
	.init_syntax = _dfa_init_syntax_type,
};
