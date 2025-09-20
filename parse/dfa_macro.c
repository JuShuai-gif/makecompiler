#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_macro;

static inline int _macro_action_if(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w  = words->data[words->size - 1];
	lex_word_t* w1 = dfa->ops->pop_word(dfa);
	lex_word_t* w2;
	lex_word_t* w3;

	if (!w1)
		return DFA_ERROR;

	if (!lex_is_const_integer(w1)) {
		loge("the condition after '#if' must be a const integer, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	int flag = w1->data.u32;

	lex_word_free(w1);
	w1 = NULL;

	while (1) {
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			return DFA_ERROR;

		int type = w1->type;

		lex_word_free(w1);
		w1 = NULL;

		if (LEX_WORD_EOF == type) {
			loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			return DFA_ERROR;
		}

		if (LEX_WORD_LF == type)
			break;
	}

	lex_word_t* h = NULL;

	int n_if = 1;

	while (1) {
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			goto error;
		w1->next = NULL;

		if (LEX_WORD_EOF == w1->type) {
			loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			lex_word_free(w1);
			goto error;
		}

		if (LEX_WORD_HASH == w1->type) {
			w2 = dfa->ops->pop_word(dfa);
			if (!w2) {
				lex_word_free(w1);
				goto error;
			}
			w2->next = NULL;

			logd("'#%s' file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);

			if (LEX_WORD_EOF == w2->type) {
				loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
				lex_word_free(w2);
				lex_word_free(w1);
				goto error;
			}

			if (n_if < 1) {
				loge("extra '#%s' without an '#if' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
				lex_word_free(w2);
				lex_word_free(w1);
				goto error;
			}

			if (LEX_WORD_KEY_ELSE == w2->type || LEX_WORD_KEY_ENDIF == w2->type) {
				w3 = dfa->ops->pop_word(dfa);
				if (!w3) {
					lex_word_free(w2);
					lex_word_free(w1);
					goto error;
				}
				w3->next = NULL;

				if (LEX_WORD_LF != w3->type) {
					loge("'\\n' NOT found after '#%s' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
					lex_word_free(w3);
					lex_word_free(w2);
					lex_word_free(w1);
					goto error;
				}

				if (LEX_WORD_KEY_ELSE == w2->type) {
					if (1 == n_if) {
						flag = !flag;
						lex_word_free(w3);
						lex_word_free(w2);
						lex_word_free(w1);
						continue;
					}
				} else {
					if (0 == --n_if) {
						lex_word_free(w3);
						lex_word_free(w2);
						lex_word_free(w1);
						break;
					}
				}

				if (flag)
					w2->next = w3;
				else
					lex_word_free(w3);
				w3 = NULL;

			} else if (LEX_WORD_KEY_IF == w2->type) {
				n_if++;
			}

			if (flag)
				w1->next = w2;
			else
				lex_word_free(w2);
			w2 = NULL;
		}

		if (flag) {
			while (w1) {
				w2 = w1->next;
				w1->next = h;
				h  = w1;
				w1 = w2;
			}
		} else
			lex_word_free(w1);
		w1 = NULL;
	}

	while (h) {
		w = h;
		logd("'%s' file: %s, line: %d\n", w->text->data, w->file->data, w->line);
		h = w->next;
		dfa->ops->push_word(dfa, w);
	}

	return DFA_OK;

error:
	while (h) {
		w = h;
		h = w->next;
		lex_word_free(w);
	}
	return DFA_ERROR;
}

static int _dfa_init_module_macro(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, macro, hash, dfa_is_hash, dfa_action_next);
	DFA_MODULE_NODE(dfa, macro, _if,  dfa_is_if,   _macro_action_if);

	return DFA_OK;
}

static int _dfa_init_syntax_macro(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa,  macro, hash, hash);
	DFA_GET_MODULE_NODE(dfa,  macro, _if,  _if);

	vector_add(dfa->syntaxes, hash);

	dfa_node_add_child(hash,  _if);
	return 0;
}

dfa_module_t dfa_module_macro =
{
	.name        = "macro",
	.init_module = _dfa_init_module_macro,
	.init_syntax = _dfa_init_syntax_macro,
};
