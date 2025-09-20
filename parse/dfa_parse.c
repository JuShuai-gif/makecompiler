#include"dfa.h"
#include"parse.h"

extern dfa_module_t  dfa_module_macro;
extern dfa_module_t  dfa_module_include;

extern dfa_module_t  dfa_module_identity;

extern dfa_module_t  dfa_module_expr;
extern dfa_module_t  dfa_module_create;
extern dfa_module_t  dfa_module_call;
extern dfa_module_t  dfa_module_sizeof;
extern dfa_module_t  dfa_module_container;
extern dfa_module_t  dfa_module_init_data;
extern dfa_module_t  dfa_module_va_arg;

extern dfa_module_t  dfa_module_enum;
extern dfa_module_t  dfa_module_union;
extern dfa_module_t  dfa_module_class;

extern dfa_module_t  dfa_module_type;

extern dfa_module_t  dfa_module_var;

extern dfa_module_t  dfa_module_function;
extern dfa_module_t  dfa_module_operator;

extern dfa_module_t  dfa_module_if;
extern dfa_module_t  dfa_module_while;
extern dfa_module_t  dfa_module_do;
extern dfa_module_t  dfa_module_for;
extern dfa_module_t  dfa_module_switch;


extern dfa_module_t  dfa_module_break;
extern dfa_module_t  dfa_module_continue;
extern dfa_module_t  dfa_module_return;
extern dfa_module_t  dfa_module_goto;
extern dfa_module_t  dfa_module_label;
extern dfa_module_t  dfa_module_async;

extern dfa_module_t  dfa_module_block;

dfa_module_t* dfa_modules[] =
{
	&dfa_module_macro,
	&dfa_module_include,

	&dfa_module_identity,

	&dfa_module_expr,
	&dfa_module_create,
	&dfa_module_call,
	&dfa_module_sizeof,
	&dfa_module_container,
	&dfa_module_init_data,
	&dfa_module_va_arg,

	&dfa_module_enum,
	&dfa_module_union,
	&dfa_module_class,

	&dfa_module_type,

	&dfa_module_var,

	&dfa_module_function,
	&dfa_module_operator,

	&dfa_module_if,
	&dfa_module_while,
	&dfa_module_do,
	&dfa_module_for,
	&dfa_module_switch,

#if 1
	&dfa_module_break,
	&dfa_module_continue,
	&dfa_module_goto,
	&dfa_module_return,
	&dfa_module_label,
	&dfa_module_async,
#endif
	&dfa_module_block,
};

int parse_dfa_init(parse_t* parse)
{
	if (dfa_open(&parse->dfa, "parse", parse) < 0) {
		loge("\n");
		return -1;
	}

	int nb_modules  = sizeof(dfa_modules) / sizeof(dfa_modules[0]);

	parse->dfa_data = calloc(1, sizeof(dfa_data_t));
	if (!parse->dfa_data) {
		loge("\n");
		return -1;
	}

	parse->dfa_data->module_datas = calloc(nb_modules, sizeof(void*));
	if (!parse->dfa_data->module_datas) {
		loge("\n");
		return -1;
	}

	parse->dfa_data->current_identities = stack_alloc();
	if (!parse->dfa_data->current_identities) {
		loge("\n");
		return -1;
	}

	int i;
	for (i = 0; i < nb_modules; i++) {

		dfa_module_t* m = dfa_modules[i];

		if (!m)
			continue;

		m->index = i;

		if (!m->init_module)
			continue;

		if (m->init_module(parse->dfa) < 0) {
			loge("init module: %s\n", m->name);
			return -1;
		}
	}

	for (i = 0; i < nb_modules; i++) {

		dfa_module_t* m = dfa_modules[i];

		if (!m || !m->init_syntax)
			continue;

		if (m->init_syntax(parse->dfa) < 0) {
			loge("init syntax: %s\n", m->name);
			return -1;
		}
	}

	return 0;
}

static void* dfa_pop_word(dfa_t* dfa)
{
	parse_t* parse = dfa->priv;

	lex_word_t* w = NULL;
	lex_pop_word(parse->lex, &w);
	return w;
}

static int dfa_push_word(dfa_t* dfa, void* word)
{
	parse_t* parse = dfa->priv;

	lex_word_t* w = word;
	lex_push_word(parse->lex, w);
	return 0;
}

static void dfa_free_word(void* word)
{
	lex_word_t* w = word;
	lex_word_free(w);
}

dfa_ops_t dfa_ops_parse = 
{
	.name      = "parse",

	.pop_word  = dfa_pop_word,
	.push_word = dfa_push_word,
	.free_word = dfa_free_word,
};
