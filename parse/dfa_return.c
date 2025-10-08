#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t  dfa_module_return;

/* ============================
 * return 模块
 * 负责解析 return 语句及其后跟的表达式列表（用逗号分隔）并把它们构造成 AST 节点
 * 语义要点：
 *  - 在遇到 return 关键字时创建一个 OP_RETURN 节点（node_t）
 *  - 后续解析到的表达式作为该节点的子节点（node_add_child）
 *  - 使用 d->expr_local_flag 表示接下来构造的 d->expr 的生命周期由模块管理
 *  - 在遇到逗号时把当前表达式入到 return 节点并等待下一个表达式
 *  - 在遇到分号时把最后的表达式加入并完成 return 节点，检查返回值数量上限（<=4）
 * ============================ */

/* 处理 "return" 关键字被识别时的动作：
 * - 创建 OP_RETURN 节点并挂到 AST 的合适位置（current_node 或 current_block）
 * - 设置 d->current_return 指向该节点（后续表达式会作为其子节点）
 * - 设置 d->expr_local_flag = 1，表示模块负责管理随后产生的表达式对象（避免被外部立即释放）
 * - 注册对分号和逗号的 POST 钩子（当解析到这些 token 时会触发相应动作）
 */
static int _return_action_return(dfa_t* dfa, vector_t* words, void* data)
{
	// DFA 私有上下文指向 parse_t
	parse_t*     parse   = dfa->priv;
	// 公共解析状态
	dfa_data_t*      d       = data;
	// 最近的词：return 关键字 token（用于位置/错误报告）
	lex_word_t*  w       = words->data[words->size - 1];
	
	node_t*      _return = NULL;


	/* 在进入 return 分支时，保证当前没有未消化的 d->expr（表达式状态应为空） */
	if (d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	/* 分配一个 OP_RETURN 节点，node_alloc 接受位置 token/w/OP_RETURN 等 */
	_return = node_alloc(w, OP_RETURN, NULL);
	if (!_return) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	/* 将 return 节点挂到 AST 上：
	 * - 如果 d->current_node 非空（说明当前在某种语句 / 复合节点下），把 return 作为它的子节点
	 * - 否则把 return 挂到 parse->ast->current_block（当前 Block 的顶层）
	 */
	if (d->current_node)
		node_add_child(d->current_node, _return);
	else
		node_add_child((node_t*)parse->ast->current_block, _return);

	/* 保存当前的 return 节点，以便后续的表达式被加入为其子节点 */
	d->current_return  = _return;

	/* 标记：接下来解析到的 d->expr 的生命周期归本模块控制（模块负责清理或把 expr 交给 AST） */
	d->expr_local_flag = 1;
	
	/* 注册两个 POST 钩子：
	 * - "return_semicolon"：当分号被解析到时（表示 return 语句结束），会触发 _return_action_semicolon
	 * - "return_comma"：当逗号被解析到时，会触发 _return_action_comma（加入一个返回表达式并等待下一个）
	 *
	 * POST 意味着在识别该节点动作之后再触发钩子（具体语义取决 DFA 框架）
	 */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_comma"),     DFA_HOOK_POST);

	return DFA_NEXT_WORD;/* 继续读取下一个词 */
}


/* 当在 return 表达式列表中遇到逗号 ',' 时的动作：
 * - 如果当前 d->expr 不为空（说明刚解析出一个表达式），把它加入到当前 return 节点作为子节点
 * - 清空 d->expr（所有权已转移到 AST 节点）
 * - 重新注册 return_comma 的 POST 钩子（以便继续捕捉下一个逗号/表达式）
 * - 返回 DFA_SWITCH_TO：通常表示状态机切换（比如准备解析下一个 expr）
 *
 * 注意：此处不立即把新的表达式节点挂到 AST 之外；仅把当前解析好的 expr 交给 return 节点。
 */
static int _return_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (d->expr) {
		/* 把 expr 作为 return 的子节点，并接管该 expr（d->expr 置 NULL） */
		node_add_child(d->current_return, d->expr);
		d->expr = NULL;
	}

	/* 再次注册钩子以便下一个逗号或表达式会被捕获（POST）*/
	DFA_PUSH_HOOK(dfa_find_node(dfa, "return_comma"),     DFA_HOOK_POST);

	/* SWITCH_TO 通常表明要切换到另一个解析子状态（例如立即进入 expr 节点解析）*/
	return DFA_SWITCH_TO;
}


/* 当在 return 后遇到分号 ';'（语句结束）时的动作：
 * - 如果 d->expr 存在，把它加入 return 节点并清空 d->expr
 * - 清除 expr_local_flag（模块不再负责 expr 生命周期）
 * - 检查 return 节点包含的子节点数量，限制最多 4 个返回值（node.nb_nodes > 4 视为错误）
 * - 清空 d->current_return 指针并返回 DFA_OK（表示语法节点成功结束）
 *
 * 说明关于 node.nb_nodes 的计数语义：取决 node_add_child 如何计算 children，
 * 这里把超过 4 个子节点视为错误（该语言/实现不允许更多的同时返回值）。
 */
static int _return_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	if (d->expr) {
		/* 把最后一个 expr 放入 return 节点 */
		node_add_child(d->current_return, d->expr);
		d->expr = NULL;
	}

	/* 恢复 expr 生命周期标识：模块不再以本地方式维护 expr */
	d->expr_local_flag = 0;

	/* 限制返回值个数（实现策略）*/
	if (d->current_return->nb_nodes > 4) {
		loge("return values must NOT more than 4!\n");
		return DFA_ERROR;
	}

	/* 完成 return 处理，清除当前记录 */
	d->current_return  = NULL;

	return DFA_OK;
}

/* 模块初始化：注册三个节点及其行动函数
 * - semicolon (识别 ;，绑定 _return_action_semicolon)
 * - comma     (识别 ,，绑定 _return_action_comma)
 * - _return   (识别 return 关键字，绑定 _return_action_return)
 */
static int _dfa_init_module_return(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, return, semicolon, dfa_is_semicolon, _return_action_semicolon);
	DFA_MODULE_NODE(dfa, return, comma,     dfa_is_comma,     _return_action_comma);
	DFA_MODULE_NODE(dfa, return, _return,   dfa_is_return,    _return_action_return);

	return DFA_OK;
}


/* 语法初始化：将模块内节点与 expr 节点连接，形成合法的 return 句法子图
 *
 * 语法图描述（伪 BNF）：
 *   _return -> semicolon           // return ;
 *   _return -> expr                // return <expr...>
 *   expr -> comma -> expr          // expr, expr, ...
 *   expr -> semicolon              // expr ;
 *
 * 上述连接确保可以解析类似：
 *   return;
 *   return a;
 *   return a, b, c;
 */
static int _dfa_init_syntax_return(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, return,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, return,   comma,     comma);
	DFA_GET_MODULE_NODE(dfa, return,   _return,   _return);
	DFA_GET_MODULE_NODE(dfa, expr,     entry,     expr);

	dfa_node_add_child(_return,    semicolon);
	dfa_node_add_child(_return,    expr);
	dfa_node_add_child(expr,       comma);
	dfa_node_add_child(comma,      expr);
	dfa_node_add_child(expr,       semicolon);

	return 0;
}

/* 导出模块描述符：供全局 dfa_modules 注册与初始化 */
dfa_module_t dfa_module_return =
{
	.name        = "return",
	.init_module = _dfa_init_module_return,
	.init_syntax = _dfa_init_syntax_return,
};
