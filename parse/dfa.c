#include"dfa.h"
#include"lex_word.h"
#include<unistd.h>

extern dfa_ops_t   dfa_ops_parse;

static dfa_ops_t*  dfa_ops_array[] = 
{
	&dfa_ops_parse,

	NULL,
};

static int _dfa_node_parse_word(dfa_t* dfa, dfa_node_t* node, vector_t* words, void* data);

void dfa_del_hook_by_name(dfa_hook_t** pp, const char* name)
{
	while (*pp) {
		dfa_hook_t* hook = *pp;

		if (!strcmp(name, hook->node->name)) {
			*pp = hook->next;
			free(hook);
			hook = NULL;
			continue;
		}

		pp = &hook->next;
	}
}

void dfa_del_hook(dfa_hook_t** pp, dfa_hook_t* sentinel)
{
	while (*pp && *pp != sentinel)
		pp = &(*pp)->next;

	if (*pp) {
		*pp = sentinel->next;
		free(sentinel);
		sentinel = NULL;
	}
}

void dfa_clear_hooks(dfa_hook_t** pp, dfa_hook_t* sentinel)
{
	while (*pp != sentinel) {
		dfa_hook_t* h = *pp;

		*pp = h->next;

		free(h);
		h = NULL;
	}
}

dfa_hook_t* dfa_find_hook(dfa_t* dfa, dfa_hook_t** pp, void* word)
{
	while (*pp) {
		dfa_hook_t* h = *pp;

		if (!h->node || !h->node->is) {
			logd("delete invalid hook: %p\n", h);

			*pp = h->next;
			free(h);
			h = NULL;
			continue;
		}

		if (h->node->is(dfa, word)) {
			return h;
		}
		pp = &h->next;
	}

	return NULL;
}

dfa_node_t* dfa_node_alloc(const char* name, dfa_is_pt is, dfa_action_pt action)
{
	if (!name || !is) {
		return NULL;
	}

	dfa_node_t* node = calloc(1, sizeof(dfa_node_t));
	if (!node) {
		return NULL;
	}

	node->name = strdup(name);
	if (!node->name) {
		free(node);
		node = NULL;
		return NULL;
	}

	node->module_index = -1;
	node->is           = is;
	node->action       = action;
	node->refs         = 1;
	return node;
}

void dfa_node_free(dfa_node_t* node)
{
	if (!node)
		return;

	if (--node->refs > 0)
		return;

	assert(0 == node->refs);

	if (node->childs) {
		vector_clear(node->childs, (void (*)(void*) )dfa_node_free);
		vector_free(node->childs);
		node->childs = NULL;
	}

	free(node->name);
	free(node);
	node = NULL;
}

int dfa_node_add_child(dfa_node_t* parent, dfa_node_t* child)
{
	if (!parent || !child) {
		loge("\n");
		return -1;
	}

	dfa_node_t* node;
	int i;

	if (parent->childs) {

		for (i = 0; i < parent->childs->size; i++) {
			node      = parent->childs->data[i];

			if (!strcmp(child->name, node->name)) {
				logi("repeated: child: %s, parent: %s\n", child->name, parent->name);
				return DFA_REPEATED;
			}
		}

	} else {
		parent->childs = vector_alloc();
		if (!parent->childs)
			return -ENOMEM;
	}

	int ret = vector_add(parent->childs, child);
	if (ret < 0)
		return ret;

	child->refs++;
	return 0;
}

int dfa_open(dfa_t** pdfa, const char* name, void* priv)
{
	if (!pdfa || !name) {
		loge("\n");
		return -1;
	}

	dfa_ops_t* ops = NULL;

	int i;
	for (i = 0; dfa_ops_array[i]; i++) {
		ops =   dfa_ops_array[i];

		if (!strcmp(name, ops->name))
			break;
		ops = NULL;
	}

	if (!ops) {
		loge("\n");
		return -1;
	}

	dfa_t* dfa = calloc(1, sizeof(dfa_t));
	if (!dfa) {
		loge("\n");
		return -1;
	}

	dfa->nodes    = vector_alloc();
	dfa->syntaxes = vector_alloc();

	dfa->priv = priv;
	dfa->ops  = ops;

	*pdfa = dfa;
	return 0;
}

void dfa_close(dfa_t* dfa)
{
	if (!dfa)
		return;

	if (dfa->nodes) {
		vector_clear(dfa->nodes, (void (*)(void*) )dfa_node_free);
		vector_free(dfa->nodes);
		dfa->nodes = NULL;
	}

	if (dfa->syntaxes) {
		vector_free(dfa->syntaxes);
		dfa->syntaxes = NULL;
	}

	free(dfa);
	dfa = NULL;
}

int dfa_add_node(dfa_t* dfa, dfa_node_t* node)
{
	if (!dfa || !node)
		return -EINVAL;

	if (!dfa->nodes) {
		dfa->nodes = vector_alloc();
		if (!dfa->nodes)
			return -ENOMEM;
	}

	return vector_add(dfa->nodes, node);
}

dfa_node_t* dfa_find_node(dfa_t* dfa, const char* name)
{
	if (!dfa || !name)
		return NULL;

	if (!dfa->nodes)
		return NULL;

	dfa_node_t* node;
	int i;

	for (i = 0; i < dfa->nodes->size; i++) {
		node      = dfa->nodes->data[i];

		if (!strcmp(name, node->name))
			return node;
	}

	return NULL;
}

static int _dfa_childs_parse_word(dfa_t* dfa, dfa_node_t** childs, int nb_childs, vector_t* words, void* data)
{
	assert(words->size > 0);

	int i;
	for (i = 0; i < nb_childs; i++) {

		dfa_node_t* child = childs[i];
		lex_word_t* w     = words->data[words->size - 1];

		logd("i: %d, nb_childs: %d, child: %s, w: %s\n", i, nb_childs, child->name, w->text->data);

		dfa_hook_t* hook = dfa_find_hook(dfa, &(dfa->hooks[DFA_HOOK_PRE]), w);
		if (hook) {
			// if pre hook is set, deliver the word to the proper hook node.
			if (hook->node != child)
				continue;

			logi("\033[32mpre hook: %s\033[0m\n", hook->node->name);

			// delete all hooks before it, and itself.
			dfa_clear_hooks(&(dfa->hooks[DFA_HOOK_PRE]), hook->next);
			hook = NULL;

		} else {
			assert(child->is);
			if (!child->is(dfa, w))
				continue;
		}

		int ret = _dfa_node_parse_word(dfa, child, words, data);

		if (DFA_OK == ret)
			return DFA_OK;

		else if (DFA_ERROR == ret)
			return DFA_ERROR;
	}

	logd("DFA_NEXT_SYNTAX\n\n");
	return DFA_NEXT_SYNTAX;
}

static int _dfa_node_parse_word(dfa_t* dfa, dfa_node_t* node, vector_t* words, void* data)
{
	int             ret = DFA_NEXT_WORD;
	lex_word_t* w   = words->data[words->size - 1];

	logi("\033[35m%s->action(), w: %s, position: %d,%d\033[0m\n", node->name, w->text->data, w->line, w->pos);

	if (node->action) {

		ret = node->action(dfa, words, data);
		if (ret < 0)
			return ret;

		if (DFA_NEXT_SYNTAX == ret)
			return DFA_NEXT_SYNTAX;

		if (DFA_SWITCH_TO == ret)
			ret = DFA_NEXT_WORD;
	}

	if (DFA_CONTINUE == ret)
		goto _continue;

#if 1
	dfa_hook_t* h = dfa->hooks[DFA_HOOK_POST];
	while (h) {
		logd("\033[32m post hook: %s\033[0m\n", h->node->name);
		h = h->next;
	}

	h = dfa->hooks[DFA_HOOK_END];
	while (h) {
		logd("\033[32m end hook: %s\033[0m\n", h->node->name);
		h = h->next;
	}
	printf("\n");
#endif

	dfa_hook_t* hook = dfa_find_hook(dfa, &(dfa->hooks[DFA_HOOK_POST]), w);
	if (hook) {
		dfa_node_t* hook_node = hook->node;

		dfa_clear_hooks(&(dfa->hooks[DFA_HOOK_POST]), hook->next);
		hook = NULL;

		logi("\033[32m post hook: %s->action()\033[0m\n", hook_node->name);

		if (hook_node != node && hook_node->action) {

			ret = hook_node->action(dfa, words, data);

			if (DFA_SWITCH_TO == ret) {
				logi("\033[32m post hook: switch to %s->%s\033[0m\n", node->name, hook_node->name);

				node = hook_node;
				ret = DFA_NEXT_WORD;
			}
		}
	}

	if (DFA_OK == ret) {

		dfa_hook_t** pp = &(dfa->hooks[DFA_HOOK_END]);

		while (*pp) {
			dfa_hook_t* hook = *pp;
			dfa_node_t* hook_node = hook->node;

			*pp = hook->next;
			free(hook);
			hook = NULL;

			logi("\033[34m end hook: %s->action()\033[0m\n", hook_node->name);

			if (!hook_node->action)
				continue;

			ret = hook_node->action(dfa, words, data);

			if (DFA_OK == ret)
				continue;

			if (DFA_SWITCH_TO == ret) {
				logi("\033[34m end hook: switch to %s->%s\033[0m\n\n", node->name, hook_node->name);

				node = hook_node;
				ret  = DFA_NEXT_WORD;
			}
			break;
		}
	}

	if (DFA_NEXT_WORD == ret) {

		lex_word_t* w = dfa->ops->pop_word(dfa);
		if (!w) {
			loge("DFA_ERROR\n");
			return DFA_ERROR;
		}

		vector_add(words, w);

		logd("pop w->type: %d, '%s', line: %d, pos: %d\n", w->type, w->text->data, w->line, w->pos);

	} else if (DFA_OK == ret) {
		logi("DFA_OK\n\n");
		return DFA_OK;

	} else if (DFA_CONTINUE == ret) {

	} else {
		logd("DFA: %d\n", ret);
		return DFA_ERROR;
	}

_continue:
	if (!node->childs || node->childs->size <= 0) {
		logi("DFA_NEXT_SYNTAX\n");
		return DFA_NEXT_SYNTAX;
	}

	ret = _dfa_childs_parse_word(dfa, (dfa_node_t**)node->childs->data, node->childs->size, words, data);
	return ret;
}

int dfa_parse_word(dfa_t* dfa, void* word, void* data)
{
	if (!dfa || !word)
		return -EINVAL;

	if (!dfa->syntaxes || dfa->syntaxes->size <= 0)
		return -EINVAL;

	if (!dfa->ops || !dfa->ops->pop_word)
		return -EINVAL;

	vector_t* words = vector_alloc();
	if (!words)
		return -ENOMEM;

	int ret = vector_add(words, word);
	if (ret < 0)
		return ret;

	ret = _dfa_childs_parse_word(dfa, (dfa_node_t**)dfa->syntaxes->data, dfa->syntaxes->size, words, data);

	if (DFA_OK != ret) {
		assert(words->size >= 1);

		lex_word_t* w = words->data[words->size - 1];

		loge("ret: %d, w->type: %d, '%s', line: %d\n\n", ret, w->type, w->text->data, w->line);

		ret = DFA_ERROR;
	}

	vector_clear(words, (void (*)(void*) )dfa->ops->free_word);
	vector_free(words);
	words = NULL;
	return ret;
}
