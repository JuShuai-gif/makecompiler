#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_goto;

// _goto_action_goto：处理 `goto` 语句的词法分析动作
static int _goto_action_goto(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析上下文
	parse_t*      parse  = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*       d      = data;
	// 获取当前词（应为 `goto`）
	lex_word_t*   w      = words->data[words->size - 1];

	// 创建一个表示 `goto` 语句的节点
	node_t*       _goto  = node_alloc(w, OP_GOTO, NULL);// 为 `goto` 创建一个新的节点
	if (!_goto) {
		loge("node alloc failed\n");// 如果节点创建失败，打印错误并返回错误
		return DFA_ERROR;
	}

	// 将 `goto` 节点添加到当前节点或当前块的节点树中
	if (d->current_node)
		node_add_child(d->current_node, _goto);// 如果有当前节点，添加子节点
	else
		node_add_child((node_t*)parse->ast->current_block, _goto); // 如果没有当前节点，添加到当前块

	// 设置 `goto` 节点为当前节点
	d->current_goto = _goto;

	// 返回继续处理下一个词
	return DFA_NEXT_WORD;
}

// _goto_action_identity：处理标签，`goto` 后面跟随标签名
static int _goto_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse  = dfa->priv;// 获取解析上下文
	dfa_data_t*       d      = data;// 获取 DFA 数据
	lex_word_t*   w      = words->data[words->size - 1];// 获取当前词（应为标签）

	// 为标签创建标签节点
	label_t*      l      = label_alloc(w);// 为标签创建一个新的标签对象
	node_t*       n      = node_alloc_label(l);// 使用标签创建一个新的节点

	// 使用标签创建一个新的节点
	node_add_child(d->current_goto, n);

	// 返回继续处理下一个词
	return DFA_NEXT_WORD;
}

// _goto_action_semicolon：处理 `goto` 语句中的分号
static int _goto_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取 DFA 数据
	dfa_data_t* d = data;

	// 清除当前的 `goto` 节点，表示语句处理完毕
	d->current_goto = NULL;

	// 返回操作成功
	return DFA_OK;
}

// _dfa_init_module_goto：初始化 `goto` 模块，定义语法解析节点
static int _dfa_init_module_goto(dfa_t* dfa)
{
	// 定义各个语法节点及其处理动作
	DFA_MODULE_NODE(dfa, goto, _goto,     dfa_is_goto,      _goto_action_goto);
	DFA_MODULE_NODE(dfa, goto, identity,  dfa_is_identity,  _goto_action_identity);
	DFA_MODULE_NODE(dfa, goto, semicolon, dfa_is_semicolon, _goto_action_semicolon);

	// 返回操作成功
	return DFA_OK;
}

// _dfa_init_syntax_goto：初始化 `goto` 语句的语法结构
static int _dfa_init_syntax_goto(dfa_t* dfa)
{
	// 获取并定义各个语法节点
	DFA_GET_MODULE_NODE(dfa, goto,   _goto,      _goto);
	DFA_GET_MODULE_NODE(dfa, goto,   identity,  identity);
	DFA_GET_MODULE_NODE(dfa, goto,   semicolon, semicolon);

	// 定义 `goto` 语法结构
	dfa_node_add_child(_goto,    identity);// `goto` 后跟标签
	dfa_node_add_child(identity, semicolon); // 标签后跟分号

	// 返回成功
	return 0;
}

// dfa_module_goto：定义 `goto` 模块，包括模块名称、初始化函数、语法初始化函数
dfa_module_t dfa_module_goto =
{
	.name        = "goto",// 模块名称为 "goto"
	.init_module = _dfa_init_module_goto,// 初始化函数模块
	.init_syntax = _dfa_init_syntax_goto,// 初始化函数语法
};
