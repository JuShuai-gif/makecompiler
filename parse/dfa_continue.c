#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_continue;

// ===============================
//  continue 关键字 DFA 模块实现
// ===============================

// 判断当前词是否为 continue 关键字
static int _continue_is_continue(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;// 将输入的通用指针转为词法单元类型

	// 判断词法类型是否是 "continue" 关键字
	return LEX_WORD_KEY_CONTINUE == w->type;
}

/*
 * 当 DFA 匹配到 "continue" 关键字时执行的动作
 * words：当前 DFA 已经识别到的词序列
 * data：用于语法解析阶段的上下文数据 (dfa_data_t)
 */
static int _continue_action_continue(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取语法解析器对象
	parse_t*     parse  = dfa->priv;
	// 获取 DFA 执行时的数据上下文
	dfa_data_t*      d      = data;
	// 当前的词（最后一个词）
	lex_word_t*  w      = words->data[words->size - 1];
	// 创建一个抽象语法树节点，类型为 OP_CONTINUE
	node_t*       _continue = node_alloc(w, OP_CONTINUE, NULL);
	if (!_continue) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	/*
	 * 将新建的 continue 节点挂接到语法树上。
	 * 如果当前语法节点存在（例如在循环语句内部），
	 * 则作为子节点添加；
	 * 否则挂到当前语法块（current_block）上。
	 */
	if (d->current_node)
		node_add_child(d->current_node, _continue);
	else
		node_add_child((node_t*)parse->ast->current_block, _continue);

	// 继续读取下一个词
	return DFA_NEXT_WORD;
}

/*
 * 当 DFA 匹配到分号时执行的动作。
 * 由于 continue 语句的结束是分号，这里无需再生成新节点，直接返回 OK。
 */
static int _continue_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;
}

/*
 * 模块初始化函数：注册 continue 模块内的节点定义与行为
 * 这里定义两个节点：
 *   - "continue" 节点：匹配 continue 关键字
 *   - "semicolon" 节点：匹配分号
 */
static int _dfa_init_module_continue(dfa_t* dfa)
{
	// 定义 semicolon 节点：用于检测分号
	DFA_MODULE_NODE(dfa, continue, semicolon, dfa_is_semicolon,  _continue_action_semicolon);
	// 定义 continue 节点：用于检测 continue 关键字
	DFA_MODULE_NODE(dfa, continue, _continue, _continue_is_continue, _continue_action_continue);

	return DFA_OK;
}

/*
 * 语法初始化函数：描述 continue 模块的语法结构。
 * continue 语法形式为：
 *     continue ;
 * 即 continue 节点后必须跟一个分号节点。
 */
static int _dfa_init_syntax_continue(dfa_t* dfa)
{
	// 从模块中获取 continue 节点与 semicolon 节点
	DFA_GET_MODULE_NODE(dfa, continue,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, continue,   _continue, _continue);

	// 语法规则：continue → semicolon
	dfa_node_add_child(_continue, semicolon);
	return 0;
}

/*
 * 定义 DFA 模块结构体。
 * 该结构体会被注册到 DFA 框架中。
 */
dfa_module_t dfa_module_continue =
{
	.name        = "continue",// 模块名
	.init_module = _dfa_init_module_continue,// 模块节点初始化
	.init_syntax = _dfa_init_syntax_continue,// 语法结构初始化
};
