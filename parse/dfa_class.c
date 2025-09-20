#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_class;

typedef struct {
	lex_word_t*  current_identity;

	block_t*     parent_block;

	type_t*      current_class;

	int              nb_lbs;
	int              nb_rbs;

} class_module_data_t;

static int _class_is_class(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CLASS  == w->type
		|| LEX_WORD_KEY_STRUCT == w->type;
}

static int _class_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];
	lex_word_t*       w     = words->data[words->size - 1];

	if (md->current_identity) {
		loge("\n");
		return DFA_ERROR;
	}

	type_t* t = block_find_type(parse->ast->current_block, w->text->data);
	if (!t) {
		t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("\n");
			return DFA_ERROR;
		}

		parse->ast->nb_structs++;
		t->node.class_flag = 1;
		scope_push_type(parse->ast->current_block->scope, t);
		node_add_child((node_t*)parse->ast->current_block, (node_t*)t);
	}

	md->current_identity = w;
	md->parent_block     = parse->ast->current_block;
	DFA_PUSH_HOOK(dfa_find_node(dfa, "class_end"), DFA_HOOK_END);

	logi("\033[31m t: %p, t->type: %d\033[0m\n", t, t->type);

	return DFA_NEXT_WORD;
}

static int _class_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];
	lex_word_t*       w     = words->data[words->size - 1];

	if (!md->current_identity) {
		loge("\n");
		return DFA_ERROR;
	}

	type_t* t = block_find_type(parse->ast->current_block, md->current_identity->text->data);
	if (!t) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!t->scope)
		t->scope = scope_alloc(w, "class");

	md->parent_block  = parse->ast->current_block;
	md->current_class = t;
	md->nb_lbs++;

	parse->ast->current_block = (block_t*)t;

	return DFA_NEXT_WORD;
}

static int _class_calculate_size(dfa_t* dfa, type_t* s)
{
	variable_t* v;

	int bits = 0;
	int size = 0;
	int i;
	int j;

	for (i = 0; i < s->scope->vars->size; i++) {
		v  =        s->scope->vars->data[i];

		assert(v->size >= 0);

		switch (v->size) {
			case 1:
				v->offset = size;
				break;
			case 2:
				v->offset = (size + 1) & ~0x1;
				break;
			case 3:
			case 4:
				v->offset = (size + 3) & ~0x3;
				break;
			default:
				v->offset = (size + 7) & ~0x7;
				break;
		};

		if (v->nb_dimentions > 0) {
			v->capacity = 1;

			for (j = 0; j < v->nb_dimentions; j++) {

				if (v->dimentions[j].num < 0) {
					loge("number of %d-dimention for array '%s' is less than 0, size: %d, file: %s, line: %d\n",
							j, v->w->text->data, v->dimentions[j].num, v->w->file->data, v->w->line);
					return DFA_ERROR;
				}

				v->capacity *= v->dimentions[j].num;
			}

			size = v->offset + v->size * v->capacity;
			bits = size << 3;
		} else {
			if (v->bit_size > 0) {
				int align = v->size << 3;
				int used  = bits & (align - 1);
				int rest  = align - used;

				if (rest < v->bit_size) {
					bits += rest;
					used  = 0;
					rest  = align;
				}

				v->offset     = (bits >> 3) & ~(v->size - 1);
				v->bit_offset =  used;

				logd("bits: %d, align: %d, used: %d, rest: %d, v->offset: %d\n", bits, align, used, rest, v->offset);

				bits = (v->offset << 3) + v->bit_offset + v->bit_size;
			} else
				bits = (v->offset + v->size) << 3;

			if (size < v->offset + v->size)
				size = v->offset + v->size;
		}

		logi("class '%s', member: '%s', member_flag: %d, offset: %d, size: %d, v->dim: %d, v->capacity: %d, bit offset: %d, bit size: %d\n",
				s->name->data, v->w->text->data, v->member_flag, v->offset, v->size, v->nb_dimentions, v->capacity, v->bit_offset, v->bit_size);
	}

	switch (size) {
		case 1:
			s->size = size;
			break;
		case 2:
			s->size = (size + 1) & ~0x1;
			break;
		case 3:
		case 4:
			s->size = (size + 3) & ~0x3;
			break;
		default:
			s->size = (size + 7) & ~0x7;
			break;
	};

	s->node.define_flag = 1;

	logi("class '%s', s->size: %d, size: %d\n", s->name->data, s->size, size);
	return 0;
}

static int _class_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	if (_class_calculate_size(dfa, md->current_class) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	md->nb_rbs++;

	return DFA_NEXT_WORD;
}

static int _class_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

static int _class_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	if (md->nb_rbs == md->nb_lbs) {

		parse->ast->current_block = md->parent_block;

		md->current_identity = NULL;
		md->current_class    = NULL;
		md->parent_block     = NULL;
		md->nb_lbs           = 0;
		md->nb_rbs           = 0;

		return DFA_OK;
	}

	DFA_PUSH_HOOK(dfa_find_node(dfa, "class_end"), DFA_HOOK_END);

	return DFA_SWITCH_TO;
}

static int _dfa_init_module_class(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, class, _class,    _class_is_class,      NULL);

	DFA_MODULE_NODE(dfa, class, identity,  dfa_is_identity,  _class_action_identity);

	DFA_MODULE_NODE(dfa, class, lb,        dfa_is_lb,        _class_action_lb);
	DFA_MODULE_NODE(dfa, class, rb,        dfa_is_rb,        _class_action_rb);

	DFA_MODULE_NODE(dfa, class, semicolon, dfa_is_semicolon, _class_action_semicolon);
	DFA_MODULE_NODE(dfa, class, end,       dfa_is_entry,     _class_action_end);

	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	assert(!md);

	md = calloc(1, sizeof(class_module_data_t));
	if (!md)
		return DFA_ERROR;

	d->module_datas[dfa_module_class.index] = md;

	return DFA_OK;
}

static int _dfa_fini_module_class(dfa_t* dfa)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_class.index] = NULL;
	}

	return DFA_OK;
}

static int _dfa_init_syntax_class(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, class,  _class,    _class);
	DFA_GET_MODULE_NODE(dfa, class,  identity,  identity);
	DFA_GET_MODULE_NODE(dfa, class,  lb,        lb);
	DFA_GET_MODULE_NODE(dfa, class,  rb,        rb);
	DFA_GET_MODULE_NODE(dfa, class,  semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, class,  end,       end);

	DFA_GET_MODULE_NODE(dfa, type,   entry,     member);
	DFA_GET_MODULE_NODE(dfa, union,  _union,    _union);

	vector_add(dfa->syntaxes,     _class);

	// class start
	dfa_node_add_child(_class,    identity);

	dfa_node_add_child(identity,  semicolon);
	dfa_node_add_child(identity,  lb);

	dfa_node_add_child(lb,        rb);

	dfa_node_add_child(lb,        _union);
	dfa_node_add_child(lb,        member);
	dfa_node_add_child(member,    rb);
	dfa_node_add_child(rb,        semicolon);

	dfa_node_add_child(end,       _union);
	dfa_node_add_child(end,       member);
	dfa_node_add_child(end,       rb);

	return 0;
}

dfa_module_t dfa_module_class = 
{
	.name        = "class",
	.init_module = _dfa_init_module_class,
	.init_syntax = _dfa_init_syntax_class,

	.fini_module = _dfa_fini_module_class,
};
