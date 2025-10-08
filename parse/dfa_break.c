#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_break;

/*
这个模块定义了 break 语句的 DFA 解析逻辑：

1、词法识别
	_break_is_break 判断一个单词是否为 break。

2、语法动作
	_break_action_break 创建 break 的 AST 节点，并挂到当前代码块或节点下。

	_break_action_semicolon 确保 break 后必须跟一个分号。

3、模块初始化
	_dfa_init_module_break 注册了 break 和 semicolon 两个节点，以及对应的动作函数。

4、语法规则
	_dfa_init_syntax_break 定义 break -> semicolon 的规则，保证语法正确性。

5、整体模块
	dfa_module_break 把这些规则封装成一个模块，方便整个 DFA 框架调用。
*/

// 判断当前词是否是关键字 "break"
static int _break_is_break(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	// 如果词类型是 "break" 关键字，返回真
	return LEX_WORD_KEY_BREAK == w->type;
}

// 当解析到 "break" 关键字时执行的动作
static int _break_action_break(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse  = dfa->priv;// 当前语法解析器
	dfa_data_t*      d      = data;// DFA 上下文数据
	lex_word_t*  w      = words->data[words->size - 1]; // 当前处理的词（最后一个）

	// 为 "break" 关键字新建一个语法树节点
	node_t*       _break = node_alloc(w, OP_BREAK, NULL);
	if (!_break) {
		loge("node alloc failed\n");
		return DFA_ERROR;// 节点分配失败
	}

	// 将 “break” 节点挂到当前 AST 结构中
	if (d->current_node)
		node_add_child(d->current_node, _break);// 挂到当前节点下
	else
		node_add_child((node_t*)parse->ast->current_block, _break);// 挂到当前代码

	return DFA_NEXT_WORD;// 继续读取下一个词
}

// 当遇到分号时的动作（break 语句后必须有分号）
static int _break_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	// 分号不需要特殊处理，直接返回 OK
	return DFA_OK;
}

// 初始化 break 模块（注册 DFA 状态机的动作节点）
static int _dfa_init_module_break(dfa_t* dfa)
{
	// 定义 break 模块中的两个节点：
	// 1. "semicolon" 节点：匹配分号，执行 _break_action_semicolon
	DFA_MODULE_NODE(dfa, break, semicolon, dfa_is_semicolon, _break_action_semicolon);
	// 2. "_break" 节点：匹配 "break" 关键字，执行 _break_action_break
	DFA_MODULE_NODE(dfa, break, _break,    _break_is_break,      _break_action_break);

	return DFA_OK;
}


// 初始化 break 模块的语法规则（定义节点之间的连接关系）
static int _dfa_init_syntax_break(dfa_t* dfa)
{
	// 获取模块内的两个节点指针
	DFA_GET_MODULE_NODE(dfa, break,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, break,   _break,       _break);

	// 语法规则：break -> semicolon
	// 即：break 语句必须以分号结尾
	dfa_node_add_child(_break, semicolon);
	return 0;
}


// 定义 break 模块
dfa_module_t dfa_module_break =
{
	.name        = "break",// 模块名字
	.init_module = _dfa_init_module_break,// 模块初始化函数(注册节点)
	.init_syntax = _dfa_init_syntax_break,// 模块语法初始化函数(建立语法关系)
};
