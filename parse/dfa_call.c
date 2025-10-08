#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_call;

// 用于保存函数调用解析时的上下文数据
typedef struct {

	int              nb_lps;// 已匹配的左括号数量 (Left Parentheses)
	int              nb_rps;// 已匹配的右括号数量 (Right Parentheses)

	node_t*      func;// 函数节点（被调用的函数/函数指针）
	node_t*      call;// 调用节点 (OP_CALL 类型)
	vector_t*    argv;// 参数列表（实参表达式）

	expr_t*      parent_expr; // 调用之前的表达式（保存调用嵌套时的上层表达式）

} dfa_call_data_t;

// 处理左括号 ( 的 状态钩子，用于处理多层括号嵌套或参数列表的进入
// 这里处理 函数调用参数解析时嵌套括号的逻辑
// 每次遇到 （ nb_lps++
// 同时注册一个 后置钩子，保证括号解析的递归性
static int _call_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	// DFA 解析数据
	dfa_data_t*       d  = data;
	// call 模块的栈
	stack_t*      s  = d->module_datas[dfa_module_call.index];
	// 获取当前函数调用上下文
	dfa_call_data_t*  cd = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;// 没有调用上下文，错误
	}

	cd->nb_lps++;// 增加左括号数量

	// 注册一个钩子：在处理 "call_lp_stat" 节点后执行 (递归处理)
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	// 继续解析下一个单词
	return DFA_NEXT_WORD;
}

/*
当检测到 函数调用左括号 ( 时触发，主要负责：
1、确定调用的是函数还是函数指针

2、创建对应的 AST 节点 (OP_CALL)

3、建立函数调用的上下文数据 (dfa_call_data_t) 并压栈
*/
static int _call_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	// 当前解析器
	parse_t*      parse     = dfa->priv;
	// DFA 数据
	dfa_data_t*       d         = data;
	// 当前 （ 词
	lex_word_t*   w1        = words->data[words->size - 1];
	// call 模块的栈
	stack_t*      s         = d->module_datas[dfa_module_call.index];
	// 函数定义
	function_t*   f         = NULL;
	// 调用上下文
	dfa_call_data_t*  cd        = NULL;

	// 函数指针变量
	variable_t*   var_pf    = NULL;
	// 函数指针节点
	node_t*       node_pf   = NULL;
	// FUNCTION_PTR 类型
	type_t*       pt        = NULL;

	// 调用节点
	node_t*       node_call = NULL;
	// 查找 OP_CALL 运算符
	operator_t*   op        = find_base_operator_by_type(OP_CALL);

	// 找到 FUNCTION_PTR 类型定义
	if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
		return DFA_ERROR;

	assert(pt);
	assert(op);

	// 获取当前标识符 (函数名 或 变量名)
	dfa_identity_t* id = stack_top(d->current_identities);
	if (id && id->identity) {

		// 先尝试在 AST 里查找函数定义
		int ret = ast_find_function(&f, parse->ast, id->identity->text->data);
		if (ret < 0)
			return DFA_ERROR;

		if (f) {
			// 找到函数，分配函数指针变量
			logd("f: %p, %s\n", f, f->node.w->text->data);

			var_pf = VAR_ALLOC_BY_TYPE(id->identity, pt, 1, 1, f);
			if (!var_pf) {
				loge("var alloc error\n");
				return DFA_ERROR;
			}

			// 标记为常量字面量
			var_pf->const_flag = 1;
			var_pf->const_literal_flag = 1;

		} else {
			// 没找到函数，则尝试查找变量（函数指针）
			ret = ast_find_variable(&var_pf, parse->ast, id->identity->text->data);
			if (ret < 0)
				return DFA_ERROR;

			if (!var_pf) {
				loge("funcptr var '%s' not found\n", id->identity->text->data);
				return DFA_ERROR;
			}

			// 检查变量是否真的是函数指针
			if (FUNCTION_PTR != var_pf->type || !var_pf->func_ptr) {
				loge("invalid function ptr\n");
				return DFA_ERROR;
			}
		}

		// 为函数指针生成 AST 节点
		node_pf = node_alloc(NULL, var_pf->type, var_pf);
		if (!node_pf) {
			loge("node alloc failed\n");
			return DFA_ERROR;
		}

		// 弹出 identity，避免重复使用
		stack_pop(d->current_identities);
		free(id);
		id = NULL;
	} else {
		// 出现 f()() 这种情况，要求 f() 的返回值是函数指针
		loge("\n");
		return DFA_ERROR;
	}

	// 新建 OP_CALL 节点
	node_call = node_alloc(w1, OP_CALL, NULL);
	if (!node_call) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}
	node_call->op = op;// 设置为调用运算符

	// 创建函数调用上下文
	cd = calloc(1, sizeof(dfa_call_data_t));
	if (!cd) {
		loge("dfa data alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);

	// 保存调用上下文
	cd->func           = node_pf;// 被调用函数节点
	cd->call           = node_call;// 调用节点
	cd->parent_expr    = d->expr;// 保存调用前的表达式
	d->expr            = NULL;// 清空当前表达式，准备解析函数调用
	d->expr_local_flag++;// 表示进入局部表达式解析状态

	// 将调用上下文压栈
	stack_push(s, cd);

	// 注册钩子：右括号、逗号、左括号继续处理
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;// 继续解析
}

// ============================
//  函数调用模块（call module）
//  负责解析语法中类似 func(a, b, c) 这种调用表达式
//  包括括号匹配、参数列表处理、表达式拼接等逻辑
// ============================

// -----------------------------------------
// 处理右括号 ')' 的语法动作
// -----------------------------------------
static int _call_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	if (words->size < 2) {// 没有足够的词汇上下文
		loge("\n");
		return DFA_ERROR;
	}

	// 全局解析器上下文
	parse_t*      parse = dfa->priv;
	// 当前 DFA 的运行数据(状态机上下文)
	dfa_data_t*       d     = data;
	// 当前词
	lex_word_t*   w     = words->data[words->size - 1];
	// 取出 "call" 模块的栈
	stack_t*      s     = d->module_datas[dfa_module_call.index];
	// 当前函数调用数据帧
	dfa_call_data_t*  cd    = stack_top(s);

	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	// 遇到一个右括号，计数 +1
	cd->nb_rps++;

	logd("cd->nb_lps: %d, cd->nb_rps: %d\n", cd->nb_lps, cd->nb_rps);

	// 括号未完全匹配，说明还有内层函数调用未闭合
	if (cd->nb_rps < cd->nb_lps) {

		// 继续监听后续的右括号、逗号、左括号等语法节点
		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

		logd("d->expr: %p\n", d->expr);
		return DFA_NEXT_WORD;// 继续读取下一个词
	}
	assert(cd->nb_rps == cd->nb_lps);// 括号数量匹配完成

	stack_pop(s);// 当前函数调用数据帧解析结束，从栈中弹出

	// ===== 组装 AST（表达式树）结构 =====

	// 如果有父表达式（例如嵌套调用）
	if (cd->parent_expr) {
		expr_add_node(cd->parent_expr, cd->func);
		expr_add_node(cd->parent_expr, cd->call);
	} else {
		// 没有父表达式，则将函数节点作为 call 节点的子节点
		node_add_child(cd->call, cd->func);
	}

	// 将参数列表节点全部挂接到 call 节点下
	if (cd->argv) {
		int i;
		for (i = 0; i < cd->argv->size; i++)
			node_add_child(cd->call, cd->argv->data[i]);

		vector_free(cd->argv);
		cd->argv = NULL;
	}

	// 处理最后一个参数表达式
	if (d->expr) {
		node_add_child(cd->call, d->expr);
		d->expr = NULL;
	}

	// 恢复当前表达式上下文
	if (cd->parent_expr)
		d->expr = cd->parent_expr;
	else
		d->expr = cd->call;

	d->expr_local_flag--;

	logd("d->expr: %p\n", d->expr);

	// 清理调用帧
	free(cd);
	cd = NULL;

	return DFA_NEXT_WORD;
}

// -----------------------------------------
// 处理逗号 ',' 的语法动作
// 用于分隔参数
// -----------------------------------------
static int _call_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	if (words->size < 2) {
		printf("%s(),%d, error: \n", __func__, __LINE__);
		return DFA_ERROR;
	}

	parse_t*      parse = dfa->priv;
	dfa_data_t*       d     = data;
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_call.index];

	dfa_call_data_t*  cd    = stack_top(s);
	if (!cd) {
		loge("\n");
		return DFA_ERROR;
	}

	if (!d->expr) {// 逗号前必须有一个表达式（参数）
		loge("\n");
		return DFA_ERROR;
	}

	// 初始化参数向量
	if (!cd->argv)
		cd->argv = vector_alloc();

	// 把当前参数表达式加入参数列表
	vector_add(cd->argv, d->expr);
	// 清空当前表达式，为下一个参数做准备
	d->expr = NULL;

	// 再次监听后续可能的 “,” 和 “(” ，形成递归处理
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "call_lp_stat"), DFA_HOOK_POST);

	// 切换状态以继续解析参数
	return DFA_SWITCH_TO;
}


// -----------------------------------------
// 初始化 call 模块（在整个 DFA 注册阶段调用）
// -----------------------------------------
static int _dfa_init_module_call(dfa_t* dfa)
{
	// 定义 “函数调用” 相关的语法节点及对应动作
	DFA_MODULE_NODE(dfa, call, lp,       dfa_is_lp,    _call_action_lp);
	DFA_MODULE_NODE(dfa, call, rp,       dfa_is_rp,    _call_action_rp);

	DFA_MODULE_NODE(dfa, call, lp_stat,  dfa_is_lp,    _call_action_lp_stat);

	DFA_MODULE_NODE(dfa, call, comma,    dfa_is_comma, _call_action_comma);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_call.index];

	assert(!s);

	// 每个模块一个独立栈，用于管理嵌套函数调用
	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_call.index] = s;

	return DFA_OK;
}


// -----------------------------------------
// 模块销毁函数（释放资源）
// -----------------------------------------
static int _dfa_fini_module_call(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_call.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_call.index] = NULL;
	}

	return DFA_OK;
}

// -----------------------------------------
// 定义函数调用语法结构（即：语法图的构造）
// -----------------------------------------
static int _dfa_init_syntax_call(dfa_t* dfa)
{
	// 从 DFA 获取模块节点
	DFA_GET_MODULE_NODE(dfa, call,   lp,       lp);
	DFA_GET_MODULE_NODE(dfa, call,   rp,       rp);
	DFA_GET_MODULE_NODE(dfa, call,   comma,    comma);

	DFA_GET_MODULE_NODE(dfa, expr,   entry,    expr);

	DFA_GET_MODULE_NODE(dfa, create, create,   create);
	DFA_GET_MODULE_NODE(dfa, create, identity, create_id);
	DFA_GET_MODULE_NODE(dfa, create, rp,       create_rp);

	// 无参数调用： func()
	dfa_node_add_child(lp,       rp);

	// 有参数调用的语法图规则
	// arg 可以是创建对象语句或一般表达式

	// 例如 list.push(new A);
	dfa_node_add_child(lp,        create);
	dfa_node_add_child(create_id, comma);
	dfa_node_add_child(create_id, rp);
	dfa_node_add_child(create_rp, comma);
	dfa_node_add_child(create_rp, rp);
	dfa_node_add_child(comma,     create);

	// 普通表达式参数：func(a, b, c)
	dfa_node_add_child(lp,       expr);
	dfa_node_add_child(expr,     comma);
	dfa_node_add_child(comma,    expr);
	dfa_node_add_child(expr,     rp);

	return 0;
}

// -----------------------------------------
// call 模块注册表定义
// -----------------------------------------
dfa_module_t dfa_module_call =
{
	.name        = "call",// 模块名
	.init_module = _dfa_init_module_call,// 模块初始化(分配栈、注册节点)
	.init_syntax = _dfa_init_syntax_call,// 构造语法图

	.fini_module = _dfa_fini_module_call,// 模块销毁
};
