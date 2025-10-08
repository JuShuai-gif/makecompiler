#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

/*
这段代码的作用时 把 async 语法注册进 DFA 系统：
1、定义节点
	async 关键字节点
	; 分号节点
2、建立语法关系
	语法规则：async -> expr -> ;
3、注册模块
	将 async 模块交给 DFA 统一管理
*/

extern dfa_module_t dfa_module_async;

// 判断某个词是不是 async 关键字
// 谓词函数，用于DFA在解析时判断一个词是不是 async 关键字
static int _async_is_async(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	// 如果当前词的类型是关键字 async，就返回真
	return LEX_WORD_KEY_ASYNC == w->type;
}

// 当 DFA 识别到 async 关键字时触发的动作
static int _async_action_async(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;// 解析上下文（语法树等）
	dfa_data_t*       d     = data;// DFA 状态机的数据
	lex_word_t*   w     = words->data[words->size - 1];// 当前匹配到的 async 词

	// 如果当前已经有表达式在处理中，说明语法错误
	if (d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	// 记录当前的 async 关键字
	d->current_async_w = w;

	// 设置局部表达式标志，表示正在处理 async 表达式
	d->expr_local_flag = 1;

	// 注册一个钩子（hook），等待遇到 "async_semicolon" 节点时触发
	DFA_PUSH_HOOK(dfa_find_node(dfa, "async_semicolon"), DFA_HOOK_POST);

	// 返回，让 DFA 继续读取下一个词
	return DFA_NEXT_WORD;
}

// 当 DFA 遇到分号时执行的 async 收尾逻辑
static int _async_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;// 解析上下文
	dfa_data_t*   d     = data;// DFA 数据
	expr_t*   e     = d->expr;// 当前表达式

	// 如果没有正在处理的表达式，说明语法错误
	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	// 逐层剥开嵌套的 OP_EXPR 表达式，直到找到最核心的节点
	while (OP_EXPR == e->type) {

		assert(e->nodes && 1 == e->nb_nodes);

		e = e->nodes[0];
	}

	// 要求 async 后面必须是一个函数调用（OP_CALL）
	if (OP_CALL != e->type) {
		loge("\n");
		return DFA_ERROR;
	}

	// 断开父节点与 e 的连接
	e->parent->nodes[0] = NULL;

	// 释放原始表达式树，并清空状态
	expr_free(d->expr);
	d->expr = NULL;
	d->expr_local_flag = 0;

	// ===============================
    // 构造 async 包装函数调用
    // ===============================
	type_t*     t   = NULL;
	function_t* f   = NULL;
	variable_t* v   = NULL;
	node_t*     pf  = NULL;
	node_t*     fmt = NULL;

	// 找到函数指针类型
	if (ast_find_type_type(&t, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;

	// 找到名为 "async" 的函数定义
	if (ast_find_function(&f, parse->ast, "async") < 0)
		return DFA_ERROR;
	if (!f) {
		loge("\n");
		return DFA_ERROR;
	}

	// 创建一个变量 v，类型是函数指针，值为 async 函数
	v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		loge("\n");
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;

	// 创建节点 pf，用于表示 async 函数指针
	pf = node_alloc(d->current_async_w, v->type, v);
	if (!pf) {
		loge("\n");
		return DFA_ERROR;
	}

	// 准备一个字符串类型变量，表示 async 调用格式化参数（这里是空字符串）
	if (ast_find_type_type(&t, parse->ast, VAR_CHAR) < 0)
		return DFA_ERROR;

	v = VAR_ALLOC_BY_TYPE(d->current_async_w, t, 1, 1, NULL);
	if (!v) {
		loge("\n");
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->data.s = string_cstr("");

	// 创建一个字符串节点 fmt（空字符串）
	fmt = node_alloc(d->current_async_w, v->type, v);
	if (!fmt) {
		loge("\n");
		return DFA_ERROR;
	}

	// 把 async 函数指针节点和空字符串节点加到调用表达式 e 的子节点中
	node_add_child(e, pf);
	node_add_child(e, fmt);

	// 调整参数顺序：
    // e->nodes[0] = pf (async 函数)
    // e->nodes[1] = 原来的第一个参数
    // e->nodes[2] = fmt (空字符串)
	int i;
	for (i = e->nb_nodes - 3; i >= 0; i--)
		e->nodes[i + 2] = e->nodes[i];

	e->nodes[0] = pf;
	e->nodes[1] = e->nodes[2];
	e->nodes[2] = fmt;

	// 把 async 调用表达式 e 挂到当前语法树节点上
	if (d->current_node)
		node_add_child(d->current_node, e);
	else
		node_add_child((node_t*)parse->ast->current_block, e);

	// 清空状态
	d->current_async_w = NULL;

	return DFA_OK;
}

// 初始化 async 模块的节点定义（定义有哪些 token/动作）
static int _dfa_init_module_async(dfa_t* dfa)
{
	// 定义一个 DFA 节点：async_semicolon
    // 条件：dfa_is_semicolon（判断是不是分号）
    // 动作：_async_action_semicolon（遇到分号时触发）
	DFA_MODULE_NODE(dfa, async, semicolon, dfa_is_semicolon, _async_action_semicolon);
	
	// 定义一个 DFA 节点：async_async
    // 条件：_async_is_async（判断是不是 async 关键字）
    // 动作：_async_action_async（遇到 async 关键字时触发）
	DFA_MODULE_NODE(dfa, async, async,     _async_is_async,      _async_action_async);

	return DFA_OK;
}

// 定义 async 模块的语法关系（把节点连成一棵语法子树）
static int _dfa_init_syntax_async(dfa_t* dfa)
{
	// 取出上一步定义的几个 DFA 节点
	DFA_GET_MODULE_NODE(dfa,  async,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa,  async,   async,     async);
	DFA_GET_MODULE_NODE(dfa,  expr,    entry,     expr);

	// 构造语法树结构：
    // async 节点 -> expr 节点 -> semicolon 节点
	dfa_node_add_child(async, expr);
	dfa_node_add_child(expr,  semicolon);

	return 0;
}

// 定义 async 模块，注册到 DFA 系统
dfa_module_t dfa_module_async =
{
	.name        = "async",// 模块名称
	.init_module = _dfa_init_module_async,// 初始化模块节点
	.init_syntax = _dfa_init_syntax_async,// 初始化语法关系
};
