#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_include;

static int _include_action_include(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	lex_word_t*  w      = words->data[words->size - 1];

	return DFA_NEXT_WORD;
}

static int _include_action_path(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;
	dfa_data_t*      d      = data;
	lex_word_t*  w      = words->data[words->size - 1];
	lex_t*       lex    = parse->lex;
	block_t*     cur    = parse->ast->current_block;

	assert(w->data.s);
	logd("include '%s', line %d\n", w->data.s->data, w->line);

	parse->lex = NULL;
	parse->ast->current_block = parse->ast->root_block;

	int ret = parse_file(parse, w->data.s->data);
	if (ret < 0) {
		loge("parse file '%s' failed, 'include' line: %d\n", w->data.s->data, w->line);
		goto error;
	}

	if (parse->lex != lex && parse->lex->macros) { // copy macros

		if (!lex->macros) {
			lex->macros = vector_clone(parse->lex->macros);

			if (!lex->macros) {
				ret = -ENOMEM;
				goto error;
			}
		} else {
			ret = vector_cat(lex->macros, parse->lex->macros);
			if (ret < 0)
				goto error;
		}

		macro_t* m;
		int i;
		for (i = 0; i < parse->lex->macros->size; i++) {
			m  =        parse->lex->macros->data[i];
			m->refs++;
		}
	}

	ret = DFA_NEXT_WORD;
error:
	parse->lex = lex;
	parse->ast->current_block = cur;
	return ret;
}

static int _include_action_LF(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

static int _dfa_init_module_include(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, include, include,   dfa_is_include,      _include_action_include);
	DFA_MODULE_NODE(dfa, include, path,      dfa_is_const_string, _include_action_path);
	DFA_MODULE_NODE(dfa, include, LF,        dfa_is_LF,           _include_action_LF);

	return DFA_OK;
}

static int _dfa_init_syntax_include(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, include, include, include);
	DFA_GET_MODULE_NODE(dfa, include, path,    path);
	DFA_GET_MODULE_NODE(dfa, include, LF,      LF);

	DFA_GET_MODULE_NODE(dfa, macro,   hash,    hash);

	dfa_node_add_child(hash,     include);
	dfa_node_add_child(include,  path);
	dfa_node_add_child(path,     LF);

	return 0;
}

dfa_module_t dfa_module_include =
{
	.name        = "include",
	.init_module = _dfa_init_module_include,
	.init_syntax = _dfa_init_syntax_include,
};
