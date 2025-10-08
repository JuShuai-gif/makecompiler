#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_container;

// ==========================================================
// 容器模块的运行时数据结构
// 用于记录括号层级、表达式上下文、父节点等信息
// ==========================================================
typedef struct {

	int              nb_lps;// 已解析的左括号 '(' 数量
	int              nb_rps;// 已解析的右括号 ')' 数量

	node_t*      container;// 当前正在构建的 container 节点（例如函数调用或初始化列表）

	expr_t*      parent_expr;// 当前 container 所属的父表达式（进入 container 前的表达式）

} dfa_container_data_t;

// ==========================================================
// 处理 '(' 出现时的状态行为：记录层级信息
// ==========================================================
static int _container_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*           d  = data;
	// 容器模块的数据栈
	stack_t*          s  = d->module_datas[dfa_module_container.index];
	// 获取栈顶的容器数据
	dfa_container_data_t* cd = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	// 括号层数 +1
	cd->nb_lps++;

	// 注册一个 HOOK：当 DFA 解析到下一个 container_lp_stat 时，再触发一次
	// （这是一种递归机制，用于正确处理嵌套括号）
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;// 继续解析下一个词
}

// ==========================================================
// 解析到一个新的 “容器起始符号”（如 '(' 或 '{'）时触发
// 创建一个新的 container 节点并入栈，开始新的作用域
// ==========================================================
static int _container_action_container(dfa_t* dfa, vector_t* words, void* data)
{
	// 解析全局上下文
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	// 当前词（触发词）
	lex_word_t*       w     = words->data[words->size - 1];
	// 容器数据栈
	stack_t*          s     = d->module_datas[dfa_module_container.index];

	// 为当前容器分配运行时数据结构
	dfa_container_data_t* cd    = calloc(1, sizeof(dfa_container_data_t));
	if (!cd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	// 创建一个 container 节点（AST 层的节点）
	node_t* container = node_alloc(w, OP_CONTAINER, NULL);
	if (!container) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);// 打印当前表达式上下文指针

	// 记录上下文
	cd->container   = container;
	cd->parent_expr = d->expr;// 保存父表达式
	d->expr         = NULL;// 当前表达式清空
	d->expr_local_flag++;// 进入局部表达式作用域
	d->nb_containers++;// 全局容器计数 +1

	// 入栈保存当前 container 状态（用于支持嵌套 container）
	stack_push(s, cd);

	return DFA_NEXT_WORD;
}

// ==========================================================
// 处理 container 左括号 '(' 行为
// 为右括号、逗号和嵌套左括号注册 HOOK
// ==========================================================
static int _container_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	// 设置 HOOK，确保在遇到这些符号时会自动回调对应动作函数
	// ')'
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),      DFA_HOOK_POST);
	// ','
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);
	// '(' (递归)
	DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);
	// 继续解析
	return DFA_NEXT_WORD;
}

// ==========================================================
// 处理 container 内部的逗号 ',' 行为
// 用于分隔容器元素，例如：f(a, b, c) 或 {1, 2, 3}
// ==========================================================
static int _container_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	lex_word_t*       w     = words->data[words->size - 1];
	stack_t*          s     = d->module_datas[dfa_module_container.index];
	// 当前容器上下文
	dfa_container_data_t* cd    = stack_top(s);

	if (!cd)
		return DFA_NEXT_SYNTAX;// 没有容器上下文，则跳过

	// ========== 第一种情况：容器当前还没有任何子节点 ==========	
	if (0 == cd->container->nb_nodes) {
		if (!d->expr) {
			loge("\n");
			return DFA_ERROR;
		}
		// 将当前表达式节点添加为 container 的第一个子节点
		node_add_child(cd->container, d->expr);
		d->expr = NULL;
		d->expr_local_flag--;

		// 注册 HOOK，用于处理下一个逗号（连续分隔）
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_PRE);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);

	} else if (1 == cd->container->nb_nodes) {// ========== 第二种情况：容器已有一个子节点（如模板参数类型） ==========

		variable_t* v;
		dfa_identity_t* id;
		node_t*     node;

		// 从当前身份栈中取出一个标识符（例如类型名或变量名）
		id = stack_pop(d->current_identities);
		assert(id);

		// 若此标识符尚未解析类型，则尝试从 AST 中查找
		if (!id->type) {
			if (ast_find_type(&id->type, parse->ast, id->identity->text->data) < 0) {
				free(id);
				return DFA_ERROR;
			}

			if (!id->type) {
				loge("can't find type '%s'\n", w->text->data);
				free(id);
				return DFA_ERROR;
			}

			// 若类型不是 struct/class，则报错
			if (id->type->type < STRUCT) {
				loge("'%s' is not a class or struct\n", w->text->data);
				free(id);
				return DFA_ERROR;
			}

			// 将标识符转移为类型引用
			id->type_w   = id->identity;
			id->identity = NULL;
		}

		// 创建变量实例（基于解析出的类型）
		v = VAR_ALLOC_BY_TYPE(id->type_w, id->type, 0, 1, NULL);
		if (!v) {
			loge("\n");
			return DFA_ERROR;
		}

		// 创建变量节点并添加到 container 下
		node = node_alloc(NULL, v->type, v);
		if (!node) {
			loge("\n");
			return DFA_ERROR;
		}

		node_add_child(cd->container, node);

		loge("\n");

		// 注册 HOOK：准备解析右括号（可能是结束）
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),   DFA_HOOK_PRE);

		free(id);
		id = NULL;
	} else {
		// 超出预期情况（例如多余逗号）
		loge("\n");
		return DFA_ERROR;
	}

	// 通知 DFA 切换状态（通常继续回到 container）
	loge("\n");
	return DFA_SWITCH_TO;
}

// ============================================================================
// 该函数是 "container" 模块中处理右括号 ")" 的动作函数。
// 通常与 _container_action_lp（左括号）和 _container_action_comma（逗号）
// 共同组成容器语法（如函数参数列表、数组初始化列表等）的解析逻辑。
// ============================================================================
static int _container_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
 	// 当前解析上下文对象 parse_t
	parse_t*          parse = dfa->priv;
	// 当前 DFA 的运行时数据
	dfa_data_t*           d     = data;
	// 当前处理的词法单元
	lex_word_t*       w     = words->data[words->size - 1];
	// container 模块的栈
	stack_t*          s     = d->module_datas[dfa_module_container.index];
	// 取出当前 container 数据上下文（即当前括号层）
	dfa_container_data_t* cd    = stack_top(s);

	// 如果当前正在处理变长参数（va_arg），则跳过本 container 的语法处理
	if (d->current_va_arg)
		return DFA_NEXT_SYNTAX;

	// 如果当前 container 数据为空，说明栈状态错误
	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	// 若当前容器内节点数量 >= 3，说明已形成合法语法单元，可结束当前容器
	if (cd->container->nb_nodes >= 3) {
		stack_pop(s);
		free(cd);
		cd = NULL;
		return DFA_NEXT_WORD;// 继续解析下一个词
	}

	cd->nb_rps++;// 记录出现一个右括号“)”（Right Parenthesis）

	logd("cd->nb_lps: %d, cd->nb_rps: %d\n", cd->nb_lps, cd->nb_rps);

	// 如果右括号数量仍小于左括号数量，说明还未闭合，需要继续解析容器内部
	if (cd->nb_rps < cd->nb_lps) {

		// 推入三个语法钩子，用于继续匹配下一个层级
		// 分别是右括号、逗号、左括号状态节点
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "container_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}

	// 否则（nb_rps == nb_lps）表示容器括号完全闭合
	assert(cd->nb_rps == cd->nb_lps);

	// 以下逻辑用于在 AST 中挂接容器节点对应的子节点（即括号内容）
	variable_t* v;
	dfa_identity_t* id;
	node_t*     node;
	type_t*     t;

	// 从当前 identity 栈中弹出标识符信息
	id = stack_pop(d->current_identities);
	assert(id && id->identity);

	// 在 AST 类型系统中查找该标识符对应的类型定义
	t = NULL;
	if (ast_find_type_type(&t, parse->ast, cd->container->nodes[1]->type) < 0)
		return DFA_ERROR;
	assert(t);

	// 在类型作用域内查找该标识符对应的变量定义
	v = scope_find_variable(t->scope, id->identity->text->data);
	assert(v);

	// 创建一个新的 AST 节点，绑定该变量
	node = node_alloc(NULL, v->type, v);
	if (!node) {
		loge("\n");
		return DFA_ERROR;
	}

	// 将此节点作为子节点添加到容器节点中
	node_add_child(cd->container, node);

	logi("cd->container->nb_nodes: %d\n", cd->container->nb_nodes);

	// 如果存在父表达式节点，则把该容器作为子节点挂入父表达式
	if (cd->parent_expr) {
		if (expr_add_node(cd->parent_expr, cd->container) < 0) {
			loge("\n");
			return DFA_ERROR;
		}
		d->expr = cd->parent_expr;
	} else
		d->expr = cd->container;// 否则将该容器设为当前表达式

	// 容器数量减一（闭合一个括号层）
	d->nb_containers--;

	logi("d->expr: %p, d->expr_local_flag: %d, d->nb_containers: %d\n", d->expr, d->expr_local_flag, d->nb_containers);

	// 返回继续处理下一个词
	return DFA_NEXT_WORD;
}

// ============================================================================
// 初始化 container 模块：注册 container 模块的 DFA 节点、分配模块栈。
// ============================================================================
static int _dfa_init_module_container(dfa_t* dfa)
{
	// 注册 container 模块的各个节点及其匹配动作
	DFA_MODULE_NODE(dfa, container, container, dfa_is_container, _container_action_container);
	DFA_MODULE_NODE(dfa, container, lp,        dfa_is_lp,        _container_action_lp);
	DFA_MODULE_NODE(dfa, container, rp,        dfa_is_rp,        _container_action_rp);
	DFA_MODULE_NODE(dfa, container, lp_stat,   dfa_is_lp,        _container_action_lp_stat);
	DFA_MODULE_NODE(dfa, container, comma,     dfa_is_comma,     _container_action_comma);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_container.index];

	assert(!s);// 确保还未初始化

	// 为 container 模块分配一个独立栈
	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	// 绑定到 dfa_data 的模块数据区
	d->module_datas[dfa_module_container.index] = s;

	return DFA_OK;
}


// ============================================================================
// 释放 container 模块资源（在 DFA 模块销毁时调用）。
// ============================================================================
static int _dfa_fini_module_container(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_container.index];

	// 若模块栈存在则释放
	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_container.index] = NULL;
	}

	return DFA_OK;
}

// ============================================================================
// 定义 container 模块的语法结构关系。
// 即构建 container 模块的 DFA 状态转移图。
// ============================================================================
static int _dfa_init_syntax_container(dfa_t* dfa)
{
	// 获取 container 模块节点
	DFA_GET_MODULE_NODE(dfa,      container,   container, container);
	DFA_GET_MODULE_NODE(dfa,      container,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa,      container,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa,      container,   comma,     comma);

	// 获取 expr、type、identity 模块节点（这些属于通用语法）
	DFA_GET_MODULE_NODE(dfa,      expr,        entry,     expr);

	DFA_GET_MODULE_NODE(dfa,      type,        entry,     type);
	DFA_GET_MODULE_NODE(dfa,      type,        base_type, base_type);
	DFA_GET_MODULE_NODE(dfa,      identity,    identity,  identity);

	// 定义状态转移关系（即语法树形态）
	//
	// container -> lp -> expr -> comma -> type/base_type/identity -> rp
	//
	// 用于描述一个括号表达式或函数参数序列的完整语法路径
	dfa_node_add_child(container, lp);
	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      comma);

	dfa_node_add_child(comma,     type);
	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(identity,  comma);

	dfa_node_add_child(comma,     identity);
	dfa_node_add_child(identity,  rp);

	return 0;
}


// ============================================================================
// container 模块的定义表（供 DFA 框架注册）
// ============================================================================
dfa_module_t dfa_module_container =
{
	.name        = "container",// 模块名
	.init_module = _dfa_init_module_container,// 模块初始化函数
	.init_syntax = _dfa_init_syntax_container,// 模块语法结构初始化函数

	.fini_module = _dfa_fini_module_container,// 模块释放函数
};
