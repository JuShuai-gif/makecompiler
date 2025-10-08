#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_sizeof;


/* ============================
 * sizeof 模块
 * 负责解析 sizeof 表达式：
 *   - sizeof(expr)
 *   - sizeof(type)
 * 构建 AST 节点并在必要时计算类型大小
 * 
 * 数据结构 dfa_sizeof_data_t:
 *   - nb_lps: 左括号 '(' 的计数
 *   - nb_rps: 右括号 ')' 的计数
 *   - _sizeof: 当前 sizeof 节点（node_t*）
 *   - parent_expr: 原始 d->expr 的父表达式，解析结束后会恢复到 d->expr
 * ============================ */

typedef struct {
	int              nb_lps;       // '(' 左括号计数
	int              nb_rps;       // ')' 右括号计数
	node_t*          _sizeof;      // 当前 sizeof AST 节点
	expr_t*          parent_expr;  // sizeof 外层表达式指针，用于恢复
} dfa_sizeof_data_t;

/* 处理 '(' 的动作（LP_STAT 钩子）：
 * - 统计左括号数量
 * - 注册 sizeof_lp_stat 钩子，用于嵌套括号的正确匹配
 */
static int _sizeof_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t*         d  = data;
	stack_t*        s  = d->module_datas[dfa_module_sizeof.index];
	dfa_sizeof_data_t*  sd = stack_top(s);// 获取当前 sizeof 状态

	if (!sd) {
		loge("\n");
		return DFA_ERROR;
	}

	sd->nb_lps++;// 左括号计数++


	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

/* 处理 sizeof 关键字：
 * - 创建 sizeof 节点 OP_SIZEOF
 * - 保存当前 d->expr 到 sd->parent_expr
 * - 清空 d->expr 并增加 expr_local_flag（表达式生命周期管理）
 * - 压入模块栈，便于后续匹配括号和嵌套 sizeof
 */
static int _sizeof_action_sizeof(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];// 当前 sizeof token
	stack_t*     s     = d->module_datas[dfa_module_sizeof.index];

	dfa_sizeof_data_t* sd    = calloc(1, sizeof(dfa_sizeof_data_t));
	if (!sd) {
		loge("module data alloc failed\n");
		return DFA_ERROR;
	}

	node_t* _sizeof = node_alloc(w, OP_SIZEOF, NULL);
	if (!_sizeof) {
		loge("node alloc failed\n");
		return DFA_ERROR;
	}

	logd("d->expr: %p\n", d->expr);

	// 保存原始 expr，用于解析结束后恢复
	sd->_sizeof     = _sizeof;
	sd->parent_expr = d->expr;
	d->expr         = NULL;
	d->expr_local_flag++;// 标记 expr 由模块管理
	d->nb_sizeofs++;// 当前正在解析 sizeof 数量++

	stack_push(s, sd);// 压入 sizeof 状态栈

	return DFA_NEXT_WORD;
}

/* 处理 '(' 的动作：
 * - 注册两个 POST 钩子：
 *   - sizeof_rp: 匹配右括号
 *   - sizeof_lp_stat: 匹配内部嵌套括号
 */
static int _sizeof_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_rp"),      DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;
}

/* 处理右括号 ')' 的动作：
 * - 如果遇到 VA_ARG，直接切换到下一语法
 * - 匹配左括号与右括号计数，处理嵌套 sizeof
 * - 将 expr 或 type/identity 转换为 AST 子节点
 * - 弹出模块栈，恢复父 expr
 */
static int _sizeof_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*       parse = dfa->priv;
	dfa_data_t*        d     = data;
	lex_word_t*    w     = words->data[words->size - 1];
	stack_t*       s     = d->module_datas[dfa_module_sizeof.index];
	dfa_sizeof_data_t* sd    = stack_top(s);

	if (d->current_va_arg)// va_arg 情况特殊处理
		return DFA_NEXT_SYNTAX;

	if (!sd) {
		loge("\n");
		return DFA_ERROR;
	}

	sd->nb_rps++;// 右括号计数++

	logd("sd->nb_lps: %d, sd->nb_rps: %d\n", sd->nb_lps, sd->nb_rps);

	// 如果右括号数量未匹配左括号，继续等待
	if (sd->nb_rps < sd->nb_lps) {

		DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_rp"),      DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "sizeof_lp_stat"), DFA_HOOK_POST);

		return DFA_NEXT_WORD;
	}
	assert(sd->nb_rps == sd->nb_lps);// 括号完全匹配

	// 如果有 expr，加入到 sizeof 节点
	if (d->expr) {
		node_add_child(sd->_sizeof, d->expr);
		d->expr = NULL;

	} else if (d->current_identities->size > 0) {// 否则处理类型标识符

		variable_t* v;
		dfa_identity_t* id;
		node_t*     n;
		expr_t*     e;
		type_t*     t;

		id = stack_pop(d->current_identities);
		assert(id && id->type);

		if (id->nb_pointers > 0) {// 指针类型

			t = block_find_type_type(parse->ast->current_block, VAR_INTPTR);
			assert(t);

			v = VAR_ALLOC_BY_TYPE(sd->_sizeof->w, t, 1, 0, NULL);
			if (!v) {
				loge("\n");
				return DFA_ERROR;
			}
			v->data.i = t->size;

			n = node_alloc(NULL, VAR_INTPTR, v);
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			node_free(sd->_sizeof);
			sd->_sizeof = n;
		} else {// 非指针类型
			v = VAR_ALLOC_BY_TYPE(sd->_sizeof->w, id->type, 1, 0, NULL);
			if (!v) {
				loge("\n");
				return DFA_ERROR;
			}

			n = node_alloc(NULL, v->type, v);
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			e = expr_alloc();
			if (!n) {
				loge("\n");
				return DFA_ERROR;
			}

			expr_add_node(e, n);
			node_add_child(sd->_sizeof, e);
		}

		free(id);
		id = NULL;
	} else {
		loge("\n");
		return DFA_ERROR;
	}

	stack_pop(s);// 弹出模块状态栈

	// 恢复父 expr
	if (sd->parent_expr) {
		expr_add_node(sd->parent_expr, sd->_sizeof);
		d->expr = sd->parent_expr;
	} else
		d->expr = sd->_sizeof;

	d->expr_local_flag--;
	d->nb_sizeofs--;

	logi("d->expr: %p, d->nb_sizeofs: %d\n", d->expr, d->nb_sizeofs);

	free(sd);
	sd = NULL;

	return DFA_NEXT_WORD;
}

/* 初始化 sizeof 模块节点及栈 */
static int _dfa_init_module_sizeof(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, sizeof, _sizeof,  dfa_is_sizeof, _sizeof_action_sizeof);
	DFA_MODULE_NODE(dfa, sizeof, lp,       dfa_is_lp,     _sizeof_action_lp);
	DFA_MODULE_NODE(dfa, sizeof, rp,       dfa_is_rp,     _sizeof_action_rp);
	DFA_MODULE_NODE(dfa, sizeof, lp_stat,  dfa_is_lp,     _sizeof_action_lp_stat);

	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_sizeof.index];

	assert(!s);// 确保栈尚未创建

	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_sizeof.index] = s;

	return DFA_OK;
}

/* 清理 sizeof 模块 */
static int _dfa_fini_module_sizeof(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = parse->dfa_data;
	stack_t*  s     = d->module_datas[dfa_module_sizeof.index];

	if (s) {
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_sizeof.index] = NULL;
	}

	return DFA_OK;
}

/* sizeof 语法连接：
 * - _sizeof -> lp
 * - lp -> expr/rp/type
 * - expr -> rp
 * - 支持 sizeof(expr), sizeof(type), sizeof(base_type*), sizeof(identity)
 */
static int _dfa_init_syntax_sizeof(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, sizeof,   _sizeof,   _sizeof);
	DFA_GET_MODULE_NODE(dfa, sizeof,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, sizeof,   rp,        rp);

	DFA_GET_MODULE_NODE(dfa, expr,     entry,     expr);

	DFA_GET_MODULE_NODE(dfa, type,     entry,     type);
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  identity);

	dfa_node_add_child(_sizeof,   lp);
	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      rp);

	dfa_node_add_child(lp,        type);
	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(identity,  rp);
	dfa_node_add_child(star,      rp);

	return 0;
}

/* sizeof 模块描述符 */
dfa_module_t dfa_module_sizeof =
{
	.name        = "sizeof",
	.init_module = _dfa_init_module_sizeof,
	.init_syntax = _dfa_init_syntax_sizeof,

	.fini_module = _dfa_fini_module_sizeof,
};
