#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_create;

// ============================================
//   create 模块数据结构定义
// ============================================
/*
这个结构体 create_module_data_t 保存了该模块的状态。

它对应 “create 语法” 的当前解析上下文，例如 create(SomeType)。

nb_lps / nb_rps 用于平衡括号匹配。

create 指向正在构建的 AST 节点。

parent_expr 是解析上下文的父表达式，用于在嵌套表达式中恢复。
*/
typedef struct {

	int              nb_lps;// 已读取的左括号 '(' 数量
	int              nb_rps;// 已读取的右括号 ')' 数量

	node_t*      create;// 当前 "create" 节点（AST 节点）

	expr_t*      parent_expr;// 外层表达式的指针（如果当前 create 嵌套在表达式中）

} create_module_data_t;

// 判断当前词是否为 "create" 关键字
static int _create_is_create(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_CREATE == w->type;// 匹配 token 类型
}

// 当匹配到 '(' （左括号）时触发的动作
/*
该函数记录出现的左括号数量。

DFA_PUSH_HOOK 表示在当前状态处理完后要执行某个特定的后置动作，用于多层括号处理。

create_lp_stat 是一个内部 DFA 状态节点名称。
*/
static int _create_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*           d  = data;
	stack_t*          s  = d->module_datas[dfa_module_create.index];
	create_module_data_t* md = d->module_datas[dfa_module_create.index];

	md->nb_lps++;// 左括号计数 +1

	// 给当前 DFA 状态节点安装一个 “后置钩子”，
	// 当本状态解析完毕后执行回调。
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

// 当匹配到 "create" 关键字时执行的动作
/*
当解析到 create 关键字时，会创建一个 AST 节点 OP_CREATE。

后续会继续解析 create 后面的部分（如类型、括号、参数等）。

例如解析 create MyClass(...) 的第一步。
*/
static int _create_action_create(dfa_t* dfa, vector_t* words, void* data)
{
	// 全局解析上下文
	parse_t*          parse = dfa->priv;
	// 当前 DFA 运行数据
	dfa_data_t*           d     = data;
	// 当前词
	lex_word_t*       w     = words->data[words->size - 1];
	
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	// 确保当前模块的 create 节点为空（不允许重复）
	CHECK_ERROR(md->create, DFA_ERROR, "\n");

	// 为 "create" 分配一个 AST 节点，类型为 OP_CREATE
	md->create = node_alloc(w, OP_CREATE, NULL);
	if (!md->create)
		return DFA_ERROR;

	// 初始化模块内部状态
	md->nb_lps      = 0;
	md->nb_rps      = 0;
	md->parent_expr = d->expr;// 保存当前表达式上下文

	return DFA_NEXT_WORD;
}

// 当匹配到类型名（identity，即标识符）时执行的动作
/*
如果 create 后面不是括号（即不是函数调用形式），
则直接将 create 节点加入当前表达式中。

如果后面确实有括号，则下一步将继续解析参数表达式。

通过 pop_word / push_word 实现词流的“回退”机制。

这一部分相当于语法的“前瞻 lookahead”。

生成的 AST 节点结构为：
Block
 └── OP_CREATE
      └── Type(MyType)
*/
static int _create_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	type_t* t  = NULL;
	type_t* pt = NULL;

	// 在 AST 类型系统中查找该标识符对应的类型
	if (ast_find_type(&t, parse->ast, w->text->data) < 0)
		return DFA_ERROR;

	if (!t) {
		loge("type '%s' not found\n", w->text->data);
		return DFA_ERROR;
	}

	// 查找 function pointer 类型（函数指针类型）
	if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;
	assert(pt);

	// 为该类型创建变量（此变量为常量字面值）
	variable_t* var = VAR_ALLOC_BY_TYPE(w, pt, 1, 1, NULL);
	CHECK_ERROR(!var, DFA_ERROR, "var '%s' alloc failed\n", w->text->data);
	// 表示此变量是常量
	var->const_literal_flag = 1;

	// 根据变量创建 AST 节点
	node_t* node = node_alloc(NULL, var->type, var);
	CHECK_ERROR(!node, DFA_ERROR, "node alloc failed\n");

	// 把这个节点加入到当前 create 节点的子节点中
	int ret = node_add_child(md->create, node);
	CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");

	/* 上面出的是 create TypeName 部分

	它会去 AST 的类型表中查找 TypeName 对应的类型对象 t。

	然后构造一个表示这个类型的子节点，挂到 create 节点下。

	例如 create MyType 会生成：
	OP_CREATE
 	 └── MyType
	*/

	// 取下一个词（看看是不是 '('）
	w = dfa->ops->pop_word(dfa);

	// 如果不是左括号 '('，说明 create 没有参数，如 create MyType;
	if (LEX_WORD_LP != w->type) {

		if (d->expr) {
			// 将 create 节点加入当前表达式
			ret = expr_add_node(d->expr, md->create);
			CHECK_ERROR(ret < 0, DFA_ERROR, "expr add child failed\n");
		} else
			d->expr = md->create;// 否则当前表达式就是 create 本身

		// 清空 create 节点状态，结束该语句的构建
		md->create = NULL;
	}
	// 将当前词重新放回 DFA 输入流（回退）
	dfa->ops->push_word(dfa, w);

	return DFA_NEXT_WORD;
}

// 处理左括号(lp)
// 当遇到左括号 '(' 时执行的动作
static int _create_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;// 获取解析器
	dfa_data_t*           d     = data;// 获取 DFA 数据上下文
	lex_word_t*       w     = words->data[words->size - 1];// 当前词
	// 获取 create 模块的数据
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	// 调试日志：打印当前的表达式上下文
	logd("d->expr: %p\n", d->expr);

	// 清空当前表达式
	d->expr = NULL;
	// 标记局部表达式
	d->expr_local_flag++;

	// 添加钩子，后续匹配到右括号、逗号或左括号时会触发这些钩子
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// 当遇到右括号 ')' 时执行的动作
static int _create_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器
	parse_t*          parse = dfa->priv;
	// 获取 DFA 数据上下文
	dfa_data_t*           d     = data;
	// 当前词
	lex_word_t*       w     = words->data[words->size - 1];
	// 获取 "create" 模块的数据
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	md->nb_rps++;// 右括号计数增加

	// 调试日志：打印当前的左括号和右括号的数量
	logd("md->nb_lps: %d, md->nb_rps: %d\n", md->nb_lps, md->nb_rps);

	// 如果右括号数量小于左括号数量，继续处理括号
	if (md->nb_rps < md->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}

	// 确保左括号数量与右括号数量相等
	assert(md->nb_rps == md->nb_lps);

	// 如果当前有表达式，则将其作为子节点添加到 "create" 节点中
	if (d->expr) {
		int ret = node_add_child(md->create, d->expr);
		d->expr = NULL;// 清空当前表达式
		CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");
	}

	// 将当前表达式设置为父表达式，并恢复局部表达式标志
	d->expr = md->parent_expr;
	d->expr_local_flag--;

	// 如果当前表达式不为空，将 "create" 节点添加到父表达式中
	if (d->expr) {
		int ret = expr_add_node(d->expr, md->create);
		CHECK_ERROR(ret < 0, DFA_ERROR, "expr add child failed\n");
	} else
		d->expr = md->create;// 否则直接将其作为表达式

	md->create = NULL;// 清空 "create" 节点

	// 调试日志：打印当前的表达式
	logi("d->expr: %p\n", d->expr);

	return DFA_NEXT_WORD;
}

// 当遇到逗号 ',' 时执行的动作
static int _create_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器
	parse_t*          parse = dfa->priv;
	// 获取 DFA 数据上下文
	dfa_data_t*           d     = data;
	// 当前词
	lex_word_t*       w     = words->data[words->size - 1];
	// 获取 "create" 模块的数据
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	// 检查当前是否有表达式，如果没有，抛出错误
	CHECK_ERROR(!d->expr, DFA_ERROR, "\n");

	// 将当前表达式作为子节点添加到 "create" 节点
	int ret = node_add_child(md->create, d->expr);
	// 清空当前表达式
	d->expr = NULL;
	CHECK_ERROR(ret < 0, DFA_ERROR, "node add child failed\n");

	// 设置后置钩子，用于处理逗号和左括号
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "create_lp_stat"), DFA_HOOK_POST);

	// 切换到其他 DFA 模块进行处理
	return DFA_SWITCH_TO;
}

// 初始化 "create" 模块的节点和状态
static int _dfa_init_module_create(dfa_t* dfa)
{
	// 注册不同的 DFA 节点和对应的动作函数
	DFA_MODULE_NODE(dfa, create, create,    _create_is_create,    _create_action_create);

	DFA_MODULE_NODE(dfa, create, identity,  dfa_is_identity,  _create_action_identity);

	DFA_MODULE_NODE(dfa, create, lp,        dfa_is_lp,        _create_action_lp);
	DFA_MODULE_NODE(dfa, create, rp,        dfa_is_rp,        _create_action_rp);

	DFA_MODULE_NODE(dfa, create, lp_stat,   dfa_is_lp,        _create_action_lp_stat);

	DFA_MODULE_NODE(dfa, create, comma,     dfa_is_comma,     _create_action_comma);

	// 为模块分配内存并初始化
	parse_t*       parse = dfa->priv;
	dfa_data_t*  d     = parse->dfa_data;
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	assert(!md);// 确保模块数据未初始化

	md = calloc(1, sizeof(create_module_data_t));
	if (!md) {
		loge("\n");
		return DFA_ERROR;
	}

	// 保存模块数据
	d->module_datas[dfa_module_create.index] = md;

	return DFA_OK;
}

// 清理 "create" 模块
static int _dfa_fini_module_create(dfa_t* dfa)
{
	parse_t*          parse = dfa->priv; // 获取解析器
	dfa_data_t*           d     = parse->dfa_data;
	create_module_data_t* md    = d->module_datas[dfa_module_create.index];

	// 释放分配的内存
	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_create.index] = NULL;
	}

	return DFA_OK;
}

// 初始化 "create" 模块的语法结构
static int _dfa_init_syntax_create(dfa_t* dfa)
{
	// 获取已注册的 DFA 节点
	DFA_GET_MODULE_NODE(dfa, create,  create,    create);
	DFA_GET_MODULE_NODE(dfa, create,  identity,  identity);
	DFA_GET_MODULE_NODE(dfa, create,  lp,        lp);
	DFA_GET_MODULE_NODE(dfa, create,  rp,        rp);
	DFA_GET_MODULE_NODE(dfa, create,  comma,     comma);

	DFA_GET_MODULE_NODE(dfa, expr,    entry,     expr);

	// 配置节点的父子关系，构建语法树
	dfa_node_add_child(create,   identity);
	dfa_node_add_child(identity, lp);

	dfa_node_add_child(lp,       rp);

	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     comma);
	dfa_node_add_child(comma,    expr);
	dfa_node_add_child(expr,     rp);

	return 0;
}

// 声明 "create" 模块，包含初始化和清理函数
dfa_module_t dfa_module_create =
{
	.name        = "create",
	.init_module = _dfa_init_module_create,
	.init_syntax = _dfa_init_syntax_create,

	.fini_module = _dfa_fini_module_create,
};
