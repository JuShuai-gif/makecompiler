#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_identity;

// _identity_action_identity：处理标识符，主要用于识别变量或类型名等标识符
static int _identity_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t*  w = words->data[words->size - 1];// 获取当前词（应为标识符）
	dfa_data_t*      d = data;// 获取 DFA 数据
	stack_t*     s = d->current_identities;// 获取当前标识符栈

	logd("w: '%s'\n", w->text->data);// 打印当前标识符的文本（调试信息）
    // 创建一个新的标识符数据结构
	dfa_identity_t* id  = calloc(1, sizeof(dfa_identity_t));
	if (!id)// 如果内存分配失败，返回错误
		return DFA_ERROR;

	// 将新的标识符数据结构压入当前标识符栈
	if (stack_push(s, id) < 0) { // 如果栈操作失败，释放内存并返回错误
		free(id);
		return DFA_ERROR;
	}

	// 设置标识符的数据
	id->identity = w;// 设置标识符文本（如变量名、类型名）
	id->const_flag = d->const_flag;// 继承常量标志
	d ->const_flag = 0;// 清除常量标志，以便下一个标识符处理

	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// _dfa_init_module_identity：初始化标识符模块，定义标识符的解析规则
static int _dfa_init_module_identity(dfa_t* dfa)
{
	// 定义模块节点：标识符（`identity`）的解析及其对应的动作（`_identity_action_identity`）
	DFA_MODULE_NODE(dfa, identity, identity,  dfa_is_identity,  _identity_action_identity);

	// 返回操作成功
	return DFA_OK;
}

// _dfa_init_syntax_identity：初始化标识符的语法结构，定义标识符的使用规则
static int _dfa_init_syntax_identity(dfa_t* dfa)
{
	// 获取并定义标识符模块的节点
	DFA_GET_MODULE_NODE(dfa, identity, identity,  identity);

	// 将标识符节点添加到语法规则中
	vector_add(dfa->syntaxes, identity);
 	// 返回操作成功
	return DFA_OK;
}


// dfa_module_identity：定义标识符模块，包含模块名称、初始化函数和语法初始化函数
dfa_module_t dfa_module_identity = 
{
    .name        = "identity",  // 模块名称为 "identity"
    .init_module = _dfa_init_module_identity,  // 初始化模块函数
    .init_syntax = _dfa_init_syntax_identity,  // 初始化语法函数
};