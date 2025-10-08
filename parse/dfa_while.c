#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_while;

// 用于在 DFA 解析 while 循环时存储局部状态，包括括号计数和 AST 节点信息
typedef struct {
	int              nb_lps;        // 左括号 '(' 的计数，用于嵌套表达式匹配
	int              nb_rps;        // 右括号 ')' 的计数，用于嵌套表达式匹配

	block_t*     parent_block;     // while 所在的父块（block）
	node_t*      parent_node;      // while 所在的父节点（当前节点的上级节点）

	node_t*      _while;           // AST 中表示 while 语句的节点

} dfa_while_data_t;

// 用于 DFA 判断 while 模块是否结束，这里直接返回 1，表示结束条件恒成立
static int _while_is_end(dfa_t* dfa, void* word)
{
	return 1;// while 语句总是有明确结束，直接返回结束条件
}

// 该函数处理 while 关键字，创建 AST 节点，并为当前 while 语句在 DFA 中保存局部状态
static int _while_action_while(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;// 获取解析器上下文
	dfa_data_t*       d     = data;// DFA 模块数据
	lex_word_t*   w     = words->data[words->size - 1];// 当前词（while关键字）
	stack_t*      s     = d->module_datas[dfa_module_while.index]; // while 模块堆栈
	node_t*      _while = node_alloc(w, OP_WHILE, NULL);// 创建 AST 节点表示 while

	if (!_while) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	dfa_while_data_t* wd = calloc(1, sizeof(dfa_while_data_t));// 分配模块数据
	if (!wd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	// 添加到 AST 当前节点或块中
	if (d->current_node)
		node_add_child(d->current_node, _while);
	else
		node_add_child((node_t*)parse->ast->current_block, _while);

	// 初始化模块数据
	wd->_while       = _while;
	wd->parent_block = parse->ast->current_block;
	wd->parent_node  = d->current_node;
	d->current_node  = _while;// 当前节点切换到 while 节点

	stack_push(s, wd);// 将模块数据入栈

	return DFA_NEXT_WORD;// 解析下一个词
}

// 当解析到左括号 '(' 时，开始解析 while 条件表达式，并设置 DFA 钩子以处理括号匹配
static int _while_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_top(s);// 获取栈顶 while 状态
	
	// 当前没有正在解析的表达式
	assert(!d->expr);
	// 标记当前表达式为局部表达式
	d->expr_local_flag = 1;

	// 注册 DFA 钩子，解析 ')' 右括号和 '(' 内表达式
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

// 用于处理嵌套表达式的左括号，保证表达式括号匹配
static int _while_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*       d  = data;
	stack_t*      s  = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd = stack_top(s);

	// 持续注册钩子以处理嵌套表达式 '('
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	// 左括号计数增加
	wd->nb_lps++;

	return DFA_NEXT_WORD;
}

// 该函数用于处理右括号 ')'，将条件表达式绑定到 AST while 节点，并管理括号嵌套计数
static int _while_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_top(s);

	if (!d->expr) {// 如果没有表达式，报错
		loge("\n");
		return DFA_ERROR;
	}

	wd->nb_rps++;// 右括号计数增加

	if (wd->nb_rps == wd->nb_lps) { // 括号匹配完成

		assert(0 == wd->_while->nb_nodes);

		node_add_child(wd->_while, d->expr);// 将表达式添加到 while 节点
		d->expr = NULL;

		d->expr_local_flag = 0;

		DFA_PUSH_HOOK(dfa_find_node(dfa, "while_end"), DFA_HOOK_END);// 注册 while 结束钩子

		return DFA_SWITCH_TO;// 切换到下一个状态
	}

	// 括号未匹配完成，继续解析
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "while_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

// 用于 while 语句结束时恢复上下文，并清理堆栈数据
static int _while_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	stack_t*      s     = d->module_datas[dfa_module_while.index];
	dfa_while_data_t* wd    = stack_pop(s);// 弹出模块数据

	assert(parse->ast->current_block == wd->parent_block);

	// 恢复解析上下文
	d->current_node = wd->parent_node;

	logi("while: %d, wd: %p, s->size: %d\n", wd->_while->w->line, wd, s->size);

	free(wd);// 释放模块数据
	wd = NULL;

	assert(s->size >= 0);

	return DFA_OK;
}

// 初始化模块，创建堆栈用于存储 while 模块的临时状态
static int _dfa_init_module_while(dfa_t* dfa)
{
	// 注册模块节点及动作函数
	DFA_MODULE_NODE(dfa, while, end,       _while_is_end,    _while_action_end);

	DFA_MODULE_NODE(dfa, while, lp,        dfa_is_lp,    _while_action_lp);
	DFA_MODULE_NODE(dfa, while, rp,        dfa_is_rp,    _while_action_rp);
	DFA_MODULE_NODE(dfa, while, lp_stat,   dfa_is_lp,    _while_action_lp_stat);

	DFA_MODULE_NODE(dfa, while, _while,    dfa_is_while, _while_action_while);

	// 初始化堆栈用于存储模块数据
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_while.index];

	assert(!s);// 确保之前未分配

	s = stack_alloc();// 分配堆栈
	if (!s) {
		logi("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_while.index] = s;

	return DFA_OK;
}

// 清理模块资源，释放堆栈
static int _dfa_fini_module_while(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_while.index];

	if (s) {
		stack_free(s);// 释放堆栈
		s = NULL;
		d->module_datas[dfa_module_while.index] = NULL;
	}

	return DFA_OK;
}

// 构建 DFA 状态节点之间的语法关系，确保 while 语法顺序正确：while -> '(' -> expr -> ')' -> block
static int _dfa_init_syntax_while(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, while,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, while,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, while,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, while,   _while,    _while);
	DFA_GET_MODULE_NODE(dfa, while,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// 将 while 模块添加到 DFA 语法列表
	vector_add(dfa->syntaxes,  _while);

	// 构建语法树关系
	dfa_node_add_child(_while, lp);// while -> '('
	dfa_node_add_child(lp,     expr);// '(' -> 条件表达式
	dfa_node_add_child(expr,   rp);// expr -> ')'

	// while body
	dfa_node_add_child(rp,     block);// ')' -> while body

	return 0;
}

// 整体封装了 while 循环 DFA 模块，包含初始化、语法节点关系以及资源释放
dfa_module_t dfa_module_while =
{
	.name        = "while",

	.init_module = _dfa_init_module_while,// 初始化模块
	.init_syntax = _dfa_init_syntax_while,// 初始化语法节点

	.fini_module = _dfa_fini_module_while,// 清理模块
};
