#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_label;

// 当解析到 ':'（冒号）时，被调用：把之前解析到的标识符转换为标签
static int _label_action_colon(dfa_t* dfa, vector_t* words, void* data)
{
	// DFA 的私有上下文，包含 parse/ast 等
	parse_t*      parse  = dfa->priv;
	// 传入的 DFA 模块公共数据
	dfa_data_t*       d      = data;
	// 取当前身份栈顶（最近解析到的标识符）
	dfa_identity_t*   id     = stack_top(d->current_identities);

	// 必须存在身份信息（即此前确实解析到了一个标识符并 push 到栈中）
	if (!id || !id->identity) {
		loge("\n");// 在实际工程中建议打印更多上下文（如行号/词）以便定位
		return DFA_ERROR;// 语法错误：冒号前没有标识符
	}

	// 创建 label 结构，通常包含名字/位置等元数据
	label_t* l = label_alloc(id->identity);
	// 把 label 包装成 AST 的节点形式，以便挂到 AST 上
	node_t*  n = node_alloc_label(l);

	// 从身份栈弹出已使用的 id（不再需要）
	stack_pop(d->current_identities);
	free(id);// 释放 dfa_identity_t 的内存（注意：id->identity 的生命由 label/AST 拥有或复制）
	id = NULL;

	// 把 label 节点插入到当前 AST block 下（成为该 block 的子节点）
	node_add_child((node_t*)parse->ast->current_block, n);

	// 把 label 注册到当前 block 的 scope 中，便于 later lookup（例如 goto）
	scope_push_label(parse->ast->current_block->scope, l);

	// 表示该语法分支成功结束
	return DFA_OK;
}

// 注册模块节点：label 节点用 dfa_is_colon 识别，触发 _label_action_colon
static int _dfa_init_module_label(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, label, label,  dfa_is_colon, _label_action_colon);

	return DFA_OK;
}

// 建立语法连接：identity -> label （即当看到标识符后，若后面是冒号，就会进入 label 处理）
static int _dfa_init_syntax_label(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, label,    label,    label);
	DFA_GET_MODULE_NODE(dfa, identity, identity, identity);

	dfa_node_add_child(identity, label);
	return 0;
}

// 导出模块描述符，供 DFA 框架注册
dfa_module_t dfa_module_label =
{
	.name        = "label",
	.init_module = _dfa_init_module_label,
	.init_syntax = _dfa_init_syntax_label,
};
