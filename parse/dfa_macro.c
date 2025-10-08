#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_macro;

// 当 DFA 遇到 '#if' 时的处理函数（内联、性能敏感所以用 inline）
static inline int _macro_action_if(dfa_t* dfa, vector_t* words, void* data)
{
	//  w 是当前处理的 '#if' 词（用于错误报告时显示文件/行等）
	lex_word_t* w  = words->data[words->size - 1];

	// 从输入流中 pop 出紧随 '#if' 之后的那个词，期望是常量整型表达式 token 
	lex_word_t* w1 = dfa->ops->pop_word(dfa);
	lex_word_t* w2;
	lex_word_t* w3;

	if (!w1)
		return DFA_ERROR;

	// 条件必须为常量整型，否则报错（这是该实现的限制）
	if (!lex_is_const_integer(w1)) {
		loge("the condition after '#if' must be a const integer, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 读取条件值（取 32 位无符号或有符号位域，代码用 u32）
	int flag = w1->data.u32;

	// 已经取到条件值，释放这个词（不再需要保留它）
	lex_word_free(w1);
	w1 = NULL;

	/* 丢弃 '#if <cond>' 行剩下的 token 直到换行（LF）
     * 目的：把指令行的余下 token（例如注释或多余空格）消费干净。
     */
	while (1) {
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			return DFA_ERROR;

		int type = w1->type;

		// 先释放，因为暂时不保留该行的 token
		lex_word_free(w1);
		w1 = NULL;

		if (LEX_WORD_EOF == type) {
			loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			return DFA_ERROR;
		}

		// 当遇到换行，跳出（已经消费完 '#if' 指令行）
		if (LEX_WORD_LF == type)
			break;
	}

	// h 是一个链表头（用于收集要保留并最终回推的 token），初始为空
	lex_word_t* h = NULL;

	// n_if 用来记录嵌套的 #if 层数；当前最外层已经是 1 
	int n_if = 1;

	// 从这里开始读取 '#if' 之后的所有 token，直到匹配的最外层 '#endif'
	while (1) {
		// 每次从输入流取下一个词（拥有其所有权）
		w1 = dfa->ops->pop_word(dfa);
		if (!w1)
			goto error;
		
		// 初始化链表指针（w1 可能被连成小链条，见后文）
		w1->next = NULL;

		// 遇到文件结束：报错 
		if (LEX_WORD_EOF == w1->type) {
			loge("'#endif' NOT found for '#if' in file: %s, line: %d\n", w->file->data, w->line);
			lex_word_free(w1);
			goto error;
		}

		// 如果当前 token 是 '#'（预处理指令开头），需要特别处理后面的指令名字
		if (LEX_WORD_HASH == w1->type) {
			// 取得 '#' 后面的 token（指令名，如 if/else/endif 等）
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

			// 防护：层计数不能小于 1（否则说明指令流不匹配）
			if (n_if < 1) {
				loge("extra '#%s' without an '#if' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
				lex_word_free(w2);
				lex_word_free(w1);
				goto error;
			}

			// 处理 #else / #endif —— 这两种指令行后面应跟一个换行（LF）
			if (LEX_WORD_KEY_ELSE == w2->type || LEX_WORD_KEY_ENDIF == w2->type) {
				// 读取这一行剩下的 token，期望是 LF（换行结束）
				w3 = dfa->ops->pop_word(dfa);
				if (!w3) {
					lex_word_free(w2);
					lex_word_free(w1);
					goto error;
				}
				w3->next = NULL;

				// 如果不是换行，报错（指令必须以换行结束）
				if (LEX_WORD_LF != w3->type) {
					loge("'\\n' NOT found after '#%s' in file: %s, line: %d\n", w2->text->data, w2->file->data, w2->line);
					lex_word_free(w3);
					lex_word_free(w2);
					lex_word_free(w1);
					goto error;
				}

				/* 如果是 #else 并且当前是最外层 if（n_if==1），则翻转 flag（开/关块）
                 * 注意：这个实现里遇到外层 #else 时直接翻转 flag 并跳到下一个 token（继续扫描）。
                 *      并且它 **释放** w3(#line LF)，w2(#else)，w1（此前的 token）。
                 */
				if (LEX_WORD_KEY_ELSE == w2->type) {
					if (1 == n_if) {
						flag = !flag;
						lex_word_free(w3);
						lex_word_free(w2);
						lex_word_free(w1);
						continue;// 继续循环，不把这些指令放入保留链表 
					}
				} else {
					// 遇到 #endif：减少嵌套计数，若回到 0 表示匹配结束，退出主循环 
					if (0 == --n_if) {
						lex_word_free(w3);
						lex_word_free(w2);
						lex_word_free(w1);
						break;
					}
				}

				/* 如果代码执行到这里，说明 #else/#endif 虽不是直接作用于最外层，
                 * 但如果当前 flag 为真，我们需要把 LF 挂到 w2（即保留整条指令行）
                 * 否则释放该行的 LF（不保留该行）
                 */
				if (flag)
					w2->next = w3;
				else
					lex_word_free(w3);
				w3 = NULL;

			} else if (LEX_WORD_KEY_IF == w2->type) {
				// 遇到嵌套的 #if，增加层级计数
				n_if++;
			}

			// 如果 flag（表示当前是哪一侧被 "激活"），决定是否把 w2（整个指令名 token）加入到 w1 的链中
			if (flag)
				w1->next = w2;
			else
				lex_word_free(w2);
			w2 = NULL;
		}

		/* 
         * 到这里 w1 代表一段要么被保留、要么被释放的 token 链： 
         * - 若 flag == true：把 w1 这段（可能包含 # 指令链：w1->w2->w3）插入 h（头插法，注意会反转顺序）
         * - 若 flag == false：释放 w1（及其后接的链）
         */
		if (flag) {
			// 把当前链 w1 头插到 h 上（这会把读到的 token 链按逆序链接到 h）
			while (w1) {
				w2 = w1->next;
				w1->next = h;
				h  = w1;
				w1 = w2;
			}
		} else
			lex_word_free(w1);// 释放整段链（包含前面可能加入的 w2/w3 等）
		w1 = NULL;
	}

	/* 循环退出（匹配到最外层 #endif），h 中保存的是按 **逆序** 链接的 token。
     * 下面把 h 中的元素逐个 push 回 DFA 的输入流（push_word），
     * push 的顺序要恢复回原来的文本顺序，因此直接遍历 h（h 存的顺序恰好是原序列的正序）
     *
     * 注意：h 的构建是头插法把每次读到的 token 插到 h 前面——
     *       结合 push 的顺序，最终能保证被保留的 token 以原来的先后顺序被 push 回去。
     */
	while (h) {
		w = h;
		logd("'%s' file: %s, line: %d\n", w->text->data, w->file->data, w->line);
		h = w->next;
		dfa->ops->push_word(dfa, w);// 把 token 放回解析流（将按需要重新被解析）
	}

	return DFA_OK;

// 出错清理：释放 h 链上尚未处理的 token 
error:
	while (h) {
		w = h;
		h = w->next;
		lex_word_free(w);
	}
	return DFA_ERROR;
}

/* 注册模块节点：hash（#）与 if 指令 */
static int _dfa_init_module_macro(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, macro, hash, dfa_is_hash, dfa_action_next);
	DFA_MODULE_NODE(dfa, macro, _if,  dfa_is_if,   _macro_action_if);

	return DFA_OK;
}

/* 语法初始化：把 hash 节点加入初始 syntaxes 并连接到 if */
static int _dfa_init_syntax_macro(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa,  macro, hash, hash);
	DFA_GET_MODULE_NODE(dfa,  macro, _if,  _if);

	vector_add(dfa->syntaxes, hash);

	dfa_node_add_child(hash,  _if);
	return 0;
}

/* 导出模块描述符 */
dfa_module_t dfa_module_macro =
{
	.name        = "macro",
	.init_module = _dfa_init_module_macro,
	.init_syntax = _dfa_init_syntax_macro,
};
