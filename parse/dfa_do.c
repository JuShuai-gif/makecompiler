#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_do;

// 数据结构定义
typedef struct {
	int              nb_lps;// 左括号 '(' 数量
	int              nb_rps;// 右括号 ')' 数量

	block_t*     parent_block;// 当前 block 的父 block
	node_t*      parent_node;// 当前节点的父节点

	node_t*      _do;// "do" 节点

} dfa_do_data_t;

// 当匹配到 "do" 关键字时执行的动作
/*
解释：

当解析到 do 关键字时，会创建一个 _do 节点，并分配相应的模块状态数据 dfa_do_data_t。

_do 节点被添加到当前节点（或者当前块）下，并更新 dfa_data_t 中的状态。

将 dfa_do_data_t 压入栈中，以便后续处理。

最后设置了钩子，表示在解析完当前 do 后会执行一些后置动作。
*/
static int _do_action_do(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_do.index];
	block_t*     b     = NULL;
	node_t*     _do    = node_alloc(w, OP_DO, NULL);

	if (!_do) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	// 分配并初始化 "do" 模块的状态数据
	dfa_do_data_t* dd = calloc(1, sizeof(dfa_do_data_t));
	if (!dd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	// 将 "_do" 节点添加到当前节点或当前 block 中
	if (d->current_node)
		node_add_child(d->current_node, _do);
	else
		node_add_child((node_t*)parse->ast->current_block, _do);

	// 更新模块状态数据
	dd->_do          = _do;
	dd->parent_block = parse->ast->current_block;
	dd->parent_node  = d->current_node;
	d->current_node  = _do;

	// 将模块状态数据压栈
	stack_push(s, dd);

	// 设置 "do__while" 钩子，表示该节点解析完成后需要执行某些后置操作
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do__while"),  DFA_HOOK_END);

	return DFA_NEXT_WORD;
}

// 处理 while 关键字的动作函数 当遇到 "while" 关键字时执行的动作
/*
解释：

当解析到 while 关键字时，调用该函数。

如果当前词是 while，则切换到处理 while 语法的 DFA 模块。
*/
static int _do_action_while(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = dfa->ops->pop_word(dfa);

	// 如果当前词不是 "while"，返回错误
	if (LEX_WORD_KEY_WHILE != w->type)
		return DFA_ERROR;

	// 切换到处理 "while" 语法的 DFA 模块
	return DFA_SWITCH_TO;
}

// 当遇到左括号 '(' 时执行的动作
/*
当遇到左括号 ( 时，表示开始一个新的表达式块（例如括号中的条件表达式）。

设置了后置钩子，处理右括号 ) 和左括号内的语句。
*/
static int _do_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	assert(!d->expr);// 确保当前没有正在处理的表达式
	d->expr_local_flag = 1;// 标记为局部表达式

	// 设置钩子，处理右括号和左括号内部语句
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

// 处理左括号中的语句
/*
解释：

该函数处理括号中的语句部分，每当遇到左括号时，增加左括号的计数（nb_lps）。

设置了钩子，以便在后续处理中执行操作。
*/
static int _do_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*     d  = data;
	stack_t*    s  = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*  dd = stack_top(s);

	// 设置钩子
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	dd->nb_lps++;// 记录左括号数量

	return DFA_NEXT_WORD;
}

// 当遇到右括号 ')' 时执行的动作
/*
解释：

当遇到右括号 ) 时，检查左右括号的数量是否匹配。如果匹配，表示当前 do-while 循环的条件表达式已经完成，将其添加到 _do 节点。

然后切换到下一个模块继续处理。
*/
static int _do_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	stack_t*     s     = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*   dd    = stack_top(s);

	// 如果没有表达式，返回错误
	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	dd->nb_rps++;// 记录右括号数量

	// 如果左括号和右括号数量相等，则完成当前 do-while 循环的表达式构建
	if (dd->nb_rps == dd->nb_lps) {

		assert(1 == dd->_do->nb_nodes);// 确保节点数量正确

		// 将表达式作为子节点添加到 _do 节点下
		node_add_child(dd->_do, d->expr);
		d->expr = NULL;// 清空当前表达式

		d->expr_local_flag = 0; // 重置局部表达式标志

		// 切换到下一个模块进行处理
		return DFA_SWITCH_TO;
	}

	// 设置后置钩子继续处理
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "do_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

// 当遇到分号 ';' 时执行的动作
/*
解释：

当遇到分号 ; 时，表示当前 do-while 循环的语句块结束。此时会弹出栈中的状态数据，恢复父节点，并释放内存。
*/
static int _do_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	stack_t*    s     = d->module_datas[dfa_module_do.index];
	dfa_do_data_t*  dd    = stack_pop(s);// 弹出栈中的状态数据

	// 确保当前块匹配
	assert(parse->ast->current_block == dd->parent_block);

	// 恢复当前节点为父节点
	d->current_node = dd->parent_node;

	// 清理分配的内存
	logi("\033[31m do: %d, dd: %p, s->size: %d\033[0m\n", dd->_do->w->line, dd, s->size);

	free(dd);
	dd = NULL;

	assert(s->size >= 0); // 确保栈大小合理

	return DFA_OK;
}

// 初始化 "do" 模块
/*
解释：

该函数初始化了 do 模块，注册了相关的动作函数，并为模块分配了栈内存。
*/
static int _dfa_init_module_do(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, do, semicolon, dfa_is_semicolon, _do_action_semicolon);

	DFA_MODULE_NODE(dfa, do, lp,        dfa_is_lp,        _do_action_lp);
	DFA_MODULE_NODE(dfa, do, rp,        dfa_is_rp,        _do_action_rp);
	DFA_MODULE_NODE(dfa, do, lp_stat,   dfa_is_lp,        _do_action_lp_stat);

	DFA_MODULE_NODE(dfa, do, _do,       dfa_is_do,        _do_action_do);
	DFA_MODULE_NODE(dfa, do, _while,    dfa_is_while,     _do_action_while);

	// 初始化栈
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_do.index];

	// 确保栈未初始化
	assert(!s);

	s = stack_alloc();// 分配栈内存
	if (!s) {
		logi("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_do.index] = s;

	return DFA_OK;
}

// 清理 do 模块
/*
解释：

该函数清理了 do 模块的资源，释放了栈内存。
*/
static int _dfa_fini_module_do(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_do.index];

	if (s) {
		stack_free(s);// 释放栈内存
		s = NULL;
		d->module_datas[dfa_module_do.index] = NULL;
	}

	return DFA_OK;
}

// 初始化 do 模块的语法结构
/*
解释：

该函数配置了 do-while 循环的语法结构，定义了各个节点间的父子关系，确保解析的顺序和结构正确。
*/
static int _dfa_init_syntax_do(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, do,   lp,         lp);
	DFA_GET_MODULE_NODE(dfa, do,   rp,         rp);
	DFA_GET_MODULE_NODE(dfa, do,   lp_stat,    lp_stat);
	DFA_GET_MODULE_NODE(dfa, do,  _do,        _do);
	DFA_GET_MODULE_NODE(dfa, do,  _while,     _while);
	DFA_GET_MODULE_NODE(dfa, do,   semicolon,  semicolon);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// do 循环开始
	vector_add(dfa->syntaxes,   _do);

	// 设置节点的父子关系
	dfa_node_add_child(_do,      block);
	dfa_node_add_child(block,   _while);

	dfa_node_add_child(_while,   lp);
	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     rp);
	dfa_node_add_child(rp,       semicolon);

	return 0;
}

dfa_module_t dfa_module_do =
{
	.name        = "do",

	.init_module = _dfa_init_module_do,
	.init_syntax = _dfa_init_syntax_do,

	.fini_module = _dfa_fini_module_do,
};
