#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_if;

/* ----------------------------------------------------------
 * if 模块的数据结构：保存单个 if / else-if 的解析上下文
 * ---------------------------------------------------------- */
typedef struct {
	int              nb_lps;        // 在当前 if 条件解析中遇到的左括号 '(' 的计数（用于支持嵌套括号）
	int              nb_rps;        // 对应的右括号 ')' 的计数

	block_t*     parent_block;     // 创建 if 时的 AST 当前 block（解析完成时要恢复到它）
	node_t*      parent_node;      // 创建 if 前的 d->current_node（解析完成时要恢复到它）

	node_t*      _if;              // 指向为这个 if 分配的 AST 节点（OP_IF）

	lex_word_t*  prev_else;        // 当产生嵌套的 "else if" 时，用于链接来自外层的 else token（上一层的 else token）
	lex_word_t*  next_else;        // 当遇到 else 时，暂存当前 else token（稍后在闭合时处理）
} dfa_if_data_t;

/* ----------------------------------------------------------
 * 条件判断函数：用于判断某个 token 是否表示 if 模块的 'end' 条件
 * 这个实现是：当遇到的 token 不是 'else' 时，表示“结束”。
 * 注意：具体是被哪个 DFA 节点调用取决于 DFA 框架（见下面 init 时的绑定）。
 * ---------------------------------------------------------- */
static int _if_is_end(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	/* 返回非零表示“不是 else” -> 视为结束条件 */
	return LEX_WORD_KEY_ELSE != w->type;
}

/* ----------------------------------------------------------
 * 当识别到 'if' 关键字时的动作
 * 1. 分配一个新的 AST if 节点（node_alloc）
 * 2. 分配并初始化一个 dfa_if_data_t（if 上下文），并 push 到模块的栈上
 * 3. 将新的 if 节点挂到当前 AST 上（d->current_node 或 current_block）
 * 4. 设置 d->current_node = 新 if 节点，使接下来解析的子节点（condition、block）挂到它下
 * 返回 DFA_NEXT_WORD 表示继续解析下一个词
 * ---------------------------------------------------------- */
static int _if_action_if(dfa_t* dfa, vector_t* words, void* data)
{
	// dfa 的私有上下文，里面含 parse / ast 等
	parse_t*     parse = dfa->priv;
	// 模块级的解析数据结构
	dfa_data_t*      d     = data;
	// 最近的词（即 'if' 关键字）
	lex_word_t*  w     = words->data[words->size - 1];
	// if 模块使用的栈
	stack_t*     s     = d->module_datas[dfa_module_if.index];
	// 查看栈顶(可能为 NULL，如果当前没有未闭合的 if)
	dfa_if_data_t*   ifd   = stack_top(s);
	// 分配 AST 的 if 节点（w 作为节点位置/元信息）
	node_t*     _if    = node_alloc(w, OP_IF, NULL);

	if (!_if) {
		loge("\n");
		return DFA_ERROR;
	}

	/* 为 if 模块上下文分配结构体 ifd2 */
	dfa_if_data_t* ifd2 = calloc(1, sizeof(dfa_if_data_t));
	if (!ifd2) {
		loge("\n");
		return DFA_ERROR;
	}

	/* 把 if 节点挂到当前 AST 上：
	 * - 如果存在 d->current_node，则作为它的子节点
	 * - 否则，作为 parse->ast->current_block 的子节点（即顶层 block）
	 */
	if (d->current_node)
		node_add_child(d->current_node, _if);
	else
		node_add_child((node_t*)parse->ast->current_block, _if);

	/* 初始化 ifd2 的上下文信息，记录创建时的 AST 环境（用于完成 if 后恢复） */
	ifd2->parent_block = parse->ast->current_block;
	ifd2->parent_node  = d->current_node;
	ifd2->_if          = _if;

	/* 如果栈顶已有未闭合的 if 并且它记录了 next_else，
	 * 说明当前 new-if 是一个 “else if”（外层提供了 else token），
	 * 因此把外层的 next_else 记录到内层 ifd2->prev_else，
	 * 这样在完成时可以关联该 else token。
	 */
	if (ifd && ifd->next_else)
		ifd2->prev_else = ifd->next_else;

	/* 把新的 if 上下文推入模块栈 */
	stack_push(s, ifd2);

	/* 更新解析器的当前节点：接下来生成的子节点应挂到新创建的 if 节点下 */
	d->current_node = _if;

	return DFA_NEXT_WORD;
}


/* ----------------------------------------------------------
 * 当识别到 'else' 关键字时的动作
 * 1. 检查是否存在对应的未闭合 if（ifd），否则报错
 * 2. 检查连续两个 else 的非法情况
 * 3. 断言 if 节点此时已经有 condition 与 then-block（nb_nodes==2）
 * 4. 记录当前 else token 到 ifd->next_else，以便在后续处理 else-block 或 else-if 时使用
 * 5. 将解析上下文的 current_node 设置回 if 节点（便于 else-after-if 的处理）
 * ---------------------------------------------------------- */
static int _if_action_else(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*   ifd   = stack_top(s);

	// 必须存在一个未闭合的 if
	if (!ifd) {
		loge("no 'if' before 'else' in line: %d\n", w->line);
		return DFA_ERROR;
	}

	// 连续两个 else (在同一个 if 上重复出现 else) 是错误的
	if (ifd->next_else) {
		loge("continuous 2 'else', 1st line: %d, 2nd line: %d\n", ifd->next_else->line, w->line);
		return DFA_ERROR;
	}

	/* 调试输出并检查：期待 if 节点已有两个子节点（通常是 condition + then-block） */
	logd("ifd->_if->nb_nodes: %d, line: %d\n", ifd->_if->nb_nodes, ifd->_if->w->line);
	assert(2 == ifd->_if->nb_nodes);

	/* 记录当前 else token 到 ifd，用于后续把 else block 或 else-if 与该 token 关联 */
	ifd->next_else = w;

	/* 确保当前没有悬挂的表达式（e.g. d->expr 应为 NULL） */
	assert(!d->expr);

	/* 恢复 current_node 到 if 节点，让后续的 block（或 _if（用于 else if））挂到这里 */
	d->current_node = ifd->_if;

	return DFA_NEXT_WORD;
}

/* ----------------------------------------------------------
 * 遇到 '(' 的动作：开始或进入条件表达式解析
 * 处理包括：
 * - 清理可能残留的 d->expr
 * - 标记 expr_local_flag，表示接下来构造的 expr 属于本 if 的本地表达式
 * - 通过钩子（DFA_PUSH_HOOK）安排对 "if_rp" 和 "if_lp_stat" 节点的后处理，
 *   以支持嵌套括号和在条件表达式内继续解析。
 * ---------------------------------------------------------- */
static int _if_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	/* 如果 d->expr 已经存在（上次未清理），先释放并置空 */
	if (d->expr) {
		expr_free(d->expr);
		d->expr = NULL;
	}

	/* 标记为局部表达式（可能用于 expr 生命周期管理） */
	d->expr_local_flag = 1;

	/* 安排钩子：当未来出现 if_rp（右括号）或 if_lp_stat（额外的 '('）时，会触发相应节点的后处理
	 * 注：这里假定 DFA_PUSH_HOOK 将节点挂到某个待触发列表（POST 表示在节点处理后触发）
	 */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	logd("ifd->nb_lps: %d, ifd->nb_rps: %d\n", ifd->nb_lps, ifd->nb_rps);

	return DFA_NEXT_WORD;
}


/* ----------------------------------------------------------
 * 遇到 '(' 且处于 if_lp_stat 节点时的动作（多次 '(' 的处理）
 * 该函数会递增 nb_lps 并继续压入 lp_stat 的后处理钩子，支持嵌套 '(' 的计数与处理。
 * ---------------------------------------------------------- */
static int _if_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	/* 左括号计数加 1 */
	ifd->nb_lps++;

	/* 继续关注是否会有更多 '('（嵌套），在解析完当前 '(' 后仍然注册后处理钩子 */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

/* ----------------------------------------------------------
 * 遇到 ')' 时的动作（if 条件表达式右括号）
 * 1. 检查 d->expr（条件表达式）存在
 * 2. nb_rps++，并与 nb_lps 做对比：如果所有左括号都被匹配（nb_rps == nb_lps），
 *    则把表达式作为 if 节点的第一个子节点加入（assert 之前该 if 节点未含子节点）。
 * 3. 清理表达式状态并把 if_end 结束钩子入列，然后返回 DFA_SWITCH_TO（切换状态）
 * 4. 如果不是最终右括号（仍有未匹配的 '('），则继续注册 if_rp 和 if_lp_stat 的后处理钩子
 * ---------------------------------------------------------- */
static int _if_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d   = data;
	stack_t*    s   = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*  ifd = stack_top(s);

	/* 必须存在一个表达式（条件表达式）*/
	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	// 右括号计数
	ifd->nb_rps++;

	// 如果右括号数等于左括号数，说明条件表达式整体已闭合
	if (ifd->nb_rps == ifd->nb_lps) {

		// 在将表达式加入到 if 节点之前，期望 if 节点当前没有子节点（此处用于保证节点结构的顺序）
		assert(0 == ifd->_if->nb_nodes);

		// 把表达式节点作为 if 的第一个子节点（通常是 condition 表达式）
		node_add_child(ifd->_if, d->expr);
		d->expr = NULL;// 已经把 expr 交给 AST，清理本地引用

		d->expr_local_flag = 0;// 恢复表达式局部标志

		// 注册 if_end 的结束钩子：表示当 if 子语法解析结束时需要调用的动作
		DFA_PUSH_HOOK(dfa_find_node(dfa, "if_end"), DFA_HOOK_END);
		return DFA_SWITCH_TO;// 表示要切换解析状态（具体含义依 DFA 框架）
	}

	// 如果条件还未完全闭合（还有未匹配的 '('），继续监听 if_rp / if_lp_stat
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "if_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

/* ----------------------------------------------------------
 * _is_end: 一个内部的 lookahead 帮助函数 —— 向前看一个 token 判断是否为 ELSE
 * 它通过 pop_word/push_word 完成 peek 操作（因为 DFA 接口没有显式的 peek）。
 * 返回 true 表示“下一个 token 不是 else”。
 * ---------------------------------------------------------- */
static int _is_end(dfa_t* dfa)
{
	lex_word_t* w   = dfa->ops->pop_word(dfa);
	int             ret = LEX_WORD_KEY_ELSE != w->type;

	// 把刚才 pop 出来的词推回原位置，等于 peek
	dfa->ops->push_word(dfa, w);

	return ret;
}

/* ----------------------------------------------------------
 * if 模块的 end 节点动作：在 if 的子语法（condition + then-block + 可选 else/else-if）解析完毕后触发
 * 主要职责：
 * 1. 看看后面是不是 else（通过 _is_end 函数判断）
 * 2. 如果后面是 else：做相应的处理（可能是 else-if 链接或注册额外的钩子）
 * 3. 如果后面不是 else：说明本 if 已经完全解析，弹出模块栈，恢复 d->current_node，
 *    释放 ifd（上下文），返回 DFA_OK
 *
 * 该函数包含对 if 节点 child 数量的若干断言（用来保证 AST 形状如期望）
 * ---------------------------------------------------------- */
static int _if_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse     = dfa->priv;
	dfa_data_t*      d         = data;
	stack_t*     s         = d->module_datas[dfa_module_if.index];
	dfa_if_data_t*   ifd       = stack_top(s);
	// 可能用来和 next_else 关联（当前代码中未显式使用）
	lex_word_t*  prev_else = ifd->prev_else;

	// 如果下一个 token 不是结束（即存在 else）
	if (!_is_end(dfa)) {

		/* 如果 if 节点已经有 3 个子节点（例如：cond, then-block, else-block），
		 * 则处理逻辑按作者实现的语义做不同处理：
		 *   - 如果这是栈底（s->size == 1），直接返回 DFA_NEXT_WORD（不弹出栈）；
		 *   - 否则把 if_end 再次加入 END 钩子并返回 DFA_NEXT_WORD。
		 *
		 * 这段逻辑的细节依赖于整个 DFA 实现：它可能在处理 "else if" 链或多重 else 的场景中
		 * 保持栈项（ifd）直到所有关联的 else 都被处理完毕。
		 */
		if (3 == ifd->_if->nb_nodes) {
			if (1 == s->size)
				return DFA_NEXT_WORD;
		} else {
			DFA_PUSH_HOOK(dfa_find_node(dfa, "if_end"), DFA_HOOK_END);
			return DFA_NEXT_WORD;
		}
	}

	/* 到这里说明没有 else，当前 if 完全结束。
	 * 检查 AST 的 block 环境未被意外修改（严谨性检查）
	 */
	assert(parse->ast->current_block == ifd->parent_block);

	// 弹出模块栈：当前 if 上下文完成
	stack_pop(s);

	// 恢复解析器的 current_node 到创建 if 之前的节点（parent_node）
	d->current_node = ifd->parent_node;

	logi("if: %d, ifd: %p, s->size: %d\n", ifd->_if->w->line, ifd, s->size);

	// 释放 ifd 内存（注意：ifd->_if 对应的 AST 节点由 AST 管理，不在此处 free）
	free(ifd);
	ifd = NULL;

	// 保守检查：栈大小不能为负（断言只是调试用）
	assert(s->size >= 0);

	return DFA_OK;
}

/* ----------------------------------------------------------
 * 模块初始化：注册模块内使用到的节点并在模块数据区分配栈
 * 这个函数通过若干宏定义模块节点：
 *   DFA_MODULE_NODE(dfa, if, end,       _if_is_end,       _if_action_end);
 *   DFA_MODULE_NODE(dfa, if, lp,        dfa_is_lp,    _if_action_lp);
 *   ...
 * 这些宏的具体实现不在这里，但可理解为把节点添加到 DFA 的内部节点表并绑定判断函数/动作函数。
 * 最后为本模块分配一个栈并保存到 d->module_datas。
 * ---------------------------------------------------------- */
static int _dfa_init_module_if(dfa_t* dfa)
{
	// 在模块内注册若干节点（名字、判断函数、动作函数）
	DFA_MODULE_NODE(dfa, if, end,       _if_is_end,       _if_action_end);

	DFA_MODULE_NODE(dfa, if, lp,        dfa_is_lp,    _if_action_lp);
	DFA_MODULE_NODE(dfa, if, rp,        dfa_is_rp,    _if_action_rp);
	DFA_MODULE_NODE(dfa, if, lp_stat,   dfa_is_lp,    _if_action_lp_stat);

	DFA_MODULE_NODE(dfa, if, _if,       dfa_is_if,    _if_action_if);
	DFA_MODULE_NODE(dfa, if, _else,     dfa_is_else,  _if_action_else);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_if.index];

	// 初始化时应为空
	assert(!s);

	// 分配用于保存 if 上下文的栈结构
	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_if.index] = s;

	return DFA_OK;
}

/* ----------------------------------------------------------
 * 模块反初始化：释放为该模块分配的栈
 * ---------------------------------------------------------- */
static int _dfa_fini_module_if(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_if.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_if.index] = NULL;
	}

	return DFA_OK;
}

/* ----------------------------------------------------------
 * 语法（syntax）初始化：把模块内各个节点连成语法树（或状态图）
 *
 * 逻辑关系（注释里用伪 BNF 表示）：
 *   if start:   _if
 *
 *   condition expr:
 *     _if -> lp -> expr -> rp
 *
 *   if body block:
 *     rp -> block
 *
 *   recursive else-if:
 *     block -> _else -> _if
 *
 *   last else (without 'if'):
 *     _else -> block
 *
 * 也就是把节点按照期望的语法顺序连接起来，供 DFA 在解析时按这些边跳转。
 * ---------------------------------------------------------- */
static int _dfa_init_syntax_if(dfa_t* dfa)
{
	/* 获取模块内之前注册的节点句柄（宏取得的是节点指针或 id）*/
	DFA_GET_MODULE_NODE(dfa, if,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, if,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, if,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, if,   _if,       _if);
	DFA_GET_MODULE_NODE(dfa, if,   _else,     _else);
	DFA_GET_MODULE_NODE(dfa, if,   end,       end);

	/* 还需用到其它模块的节点（expr, block） */
	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	/* if 起始节点注册到 dfa->syntaxes（表示这是一个可选的语法起点）*/
	vector_add(dfa->syntaxes,  _if);

	/* 下面把节点以预期的语法顺序连接起来（建立状态转移图） */

	// if start
	dfa_node_add_child(_if,    lp);
	dfa_node_add_child(lp,     expr);
	dfa_node_add_child(expr,   rp);

	// if body block
	dfa_node_add_child(rp,     block);

	// recursive else if block
	dfa_node_add_child(block,  _else);
	dfa_node_add_child(_else,  _if);

	// last else block
	dfa_node_add_child(_else,  block);

	return 0;
}

/* ----------------------------------------------------------
 * 导出的模块描述符（供外部注册）
 * ---------------------------------------------------------- */
dfa_module_t dfa_module_if =
{
	.name        = "if",

	.init_module = _dfa_init_module_if,
	.init_syntax = _dfa_init_syntax_if,

	.fini_module = _dfa_fini_module_if,
};
