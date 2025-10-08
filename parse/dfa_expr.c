#include"dfa.h"
#include"dfa_util.h"
#include"utils_stack.h"
#include"parse.h"

extern dfa_module_t dfa_module_expr;

typedef struct {
	stack_t*      ls_exprs;
	stack_t*      lp_exprs;
	block_t*      parent_block;
	variable_t*   current_var;
	type_t*       current_struct;

} expr_module_data_t;

int _type_find_type(dfa_t* dfa, dfa_identity_t* id);

// 判断是否是表达式
static int _expr_is_expr(dfa_t* dfa, void* word)
{
	// 获取当前词
	lex_word_t* w = word;

	// 如果是分号、运算符、常量或者标识符，认为是有效的表达式
	if (LEX_WORD_SEMICOLON == w->type
			|| lex_is_operator(w)
			|| lex_is_const(w)
			|| lex_is_identity(w))
		return 1;// 返回 1 表示是有效的表达式
	return 0;// 否则返回 0
}

// 判断是否是数字
static int _expr_is_number(dfa_t* dfa, void* word)
{
	// 获取当前词
	lex_word_t* w = word;
	// 判断词是否为常量(即数字)
	return lex_is_const(w);
}

// 判断是否是单目运算符
/*
解释：

该函数检查一个词是否是单目运算符。单目运算符包括 ++、--、!、~ 等操作符。如果该词是单目运算符，返回 1，否则返回 0
*/
static int _expr_is_unary_op(dfa_t* dfa, void* word)
{
	// 获取当前词
	lex_word_t* w = word;

	// 如果是左移、右移、左括号、右括号或者 sizeof，返回 0，不是单目运算符
	if (LEX_WORD_LS == w->type
			|| LEX_WORD_RS == w->type
			|| LEX_WORD_LP == w->type
			|| LEX_WORD_RP == w->type
			|| LEX_WORD_KEY_SIZEOF == w->type)
		return 0;
	// 如果是单目运算符，返回 1
	operator_t* op = find_base_operator(w->text->data, 1);
	if (op)
		return 1;
	return 0;
}

// 判断是否是后缀单目运算符
/*
解释：

该函数检查一个词是否是后缀的单目运算符，如 ++ 或 --。如果是，返回 1，否则返回 0。
*/
static int _expr_is_unary_post(dfa_t* dfa, void* word)
{
	// 获取当前词
	lex_word_t* w = word;

	// 判断当前词是否为后缀自增或自减运算符
	return LEX_WORD_INC == w->type
		|| LEX_WORD_DEC == w->type;
}

// 判断是否是双目运算符
/*
解释：

该函数检查一个词是否是双目运算符。常见的双目运算符如 +、-、*、/ 等。如果是双目运算符，返回 1，否则返回 0。
*/
static int _expr_is_binary_op(dfa_t* dfa, void* word)
{
	// 获取当前词
	lex_word_t* w = word;

	// 如果是左移、右移、左括号、右括号，则不是双目运算符
	if (LEX_WORD_LS == w->type
			|| LEX_WORD_RS == w->type
			|| LEX_WORD_LP == w->type
			|| LEX_WORD_RP == w->type)
		return 0;

	// 如果是双目运算符，返回 1
	operator_t* op = find_base_operator(w->text->data, 2);
	if (op)
		return 1;
	return 0;
}

// 将变量添加到表达式中
int _expr_add_var(parse_t* parse, dfa_data_t* d)
{
	// 获取表达式模块的数据
	expr_module_data_t* md   = d->module_datas[dfa_module_expr.index];
	// 变量指针
	variable_t*     var  = NULL;
	// 节点指针
	node_t*         node = NULL;
	// 类型指针
	type_t*         pt   = NULL;
	// 函数指针
	function_t*     f    = NULL;
	// 弹出当前标识符
	dfa_identity_t*     id   = stack_pop(d->current_identities);
	
	lex_word_t*     w;

	// 确保 id 存在并且包含标识符
	assert(id && id->identity);
	// 获取标识符的词
	w = id->identity;

	// 查找变量，如果找到则返回
	if (ast_find_variable(&var, parse->ast, w->text->data) < 0)
		return DFA_ERROR;

	// 如果没有找到该变量，可能是一个函数
	if (!var) {
		logw("var '%s' not found, maybe it's a function\n", w->text->data);

		// 查找是否有该函数的类型（指针类型）
		if (ast_find_type_type(&pt, parse->ast, FUNCTION_PTR) < 0)
			return DFA_ERROR;
		assert(pt);

		// 查找函数定义
		if (ast_find_function(&f, parse->ast, w->text->data) < 0)
			return DFA_ERROR;

		if (!f) {
			loge("function '%s' not found\n", w->text->data);
			return DFA_ERROR;
		}

		// 如果找到函数，将其作为变量添加
		var = VAR_ALLOC_BY_TYPE(id->identity, pt, 1, 1, f);
		if (!var)
			return -ENOMEM;

		// 设置常量标志
		var->const_literal_flag = 1;
	}

	// 创建一个新节点，表示该变量
	logd("var: %s, member_flag: %d, line: %d\n", var->w->text->data, var->member_flag, var->w->line);
	
	node = node_alloc(w, var->type, var);
	if (!node)
		return -ENOMEM;

	// 如果没有当前表达式，创建一个新的表达式
	if (!d->expr) {
		d->expr = expr_alloc();
		if (!d->expr)
			return -ENOMEM;
	}

	logd("d->expr: %p, node: %p\n", d->expr, node);

	// 将节点添加到表达式中
	if (expr_add_node(d->expr, node) < 0) {
		loge("add var node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 如果变量类型是结构体，查找当前结构体
	if (var->type >= STRUCT) {

		int ret = ast_find_type_type(&md->current_struct, parse->ast, var->type);
		if (ret < 0)
			return DFA_ERROR;
		assert(md->current_struct);
	}
	// 更新当前变量
	md->current_var = var;

	// 清理 id
	free(id);
	id = NULL;
	// 返回成功
	return DFA_OK;
}

// 初始化表达式
/*
解释：

这个函数初始化一个新的表达式对象（如果尚未创建），并返回控制流程继续处理表达式或跳到下一个词。
*/
static int _expr_action_expr(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	// 如果当前表达式不存在，创建一个新的表达式对象
	if (!d->expr) {
		d->expr = expr_alloc();
		if (!d->expr) {
			loge("expr alloc failed\n");
			return DFA_ERROR;// 如果分配失败，返回错误
		}
	}

	logd("d->expr: %p\n", d->expr);

	// 如果词组不为空，继续解析，否则进入下一个词
	return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}

// 处理数字常量
static int _expr_action_number(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	logd("w: %s\n", w->text->data);

	int type;
	int nb_pointers = 0;

	// 根据词的类型来确定常量的类型和指针数量
	switch (w->type) {
		case LEX_WORD_CONST_CHAR:
			type = VAR_U32;
			break;

		case LEX_WORD_CONST_STRING:
			type = VAR_CHAR;
			nb_pointers = 1;
			// 特殊处理 "__func__" 关键字
			if (!strcmp(w->text->data, "__func__")) {

				function_t* f = (function_t*)parse->ast->current_block;

				while (f && FUNCTION != f->node.type)
					f = (function_t*) f->node.parent;

				if (!f) {
					loge("line: %d, '__func__' isn't in a function\n", w->line);
					return DFA_ERROR;
				}

				// 获取当前函数的签名
				if (function_signature(parse->ast, f) < 0)
					return DFA_ERROR;
				// 只保留前两位（"__"）
				w->text->data[2] = '\0';
				w->text->len     = 2;

				// 拼接函数签名
				int ret = string_cat(w->text, f->signature);
				if (ret < 0)
					return DFA_ERROR;

				ret = string_cat_cstr_len(w->text, "__", 2);
				if (ret < 0)
					return DFA_ERROR;

				// 克隆函数签名
				w->data.s = string_clone(f->signature);
				if (!w->data.s)
					return DFA_ERROR;
			}
			break;

		case LEX_WORD_CONST_INT:
			type = VAR_INT;
			break;
		case LEX_WORD_CONST_U32:
			type = VAR_U32;
			break;

		case LEX_WORD_CONST_FLOAT:
			type = VAR_FLOAT;
			break;

		case LEX_WORD_CONST_DOUBLE:
			type = VAR_DOUBLE;
			break;

		case LEX_WORD_CONST_I64:
			type = VAR_I64;
			break;

		case LEX_WORD_CONST_U64:
			type = VAR_U64;
			break;

		default:
			loge("unknown number type\n");
			return DFA_ERROR;
	};

	// 查找常量类型
	type_t* t = block_find_type_type(parse->ast->current_block, type);
	if (!t) {
		loge("\n");
		return DFA_ERROR;
	}

	// 创建变量并分配内存
	variable_t* var = VAR_ALLOC_BY_TYPE(w, t, 1, nb_pointers, NULL);
	if (!var) {
		loge("var '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 创建节点并将其添加到表达式中
	node_t* n = node_alloc(w, var->type, var);
	if (!n) {
		loge("var node '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 将变量节点添加到表达式中
	if (expr_add_node(d->expr, n) < 0) {
		loge("add var node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 处理完毕，继续解析下一个词
	return DFA_NEXT_WORD;
}

// 处理操作符
/*
解释：

该函数用于处理操作符。首先，查找操作符（例如 +、- 等），然后创建相应的操作符节点，并将其添加到当前的表达式中。
*/
static int _expr_action_op(dfa_t* dfa, vector_t* words, void* data, int nb_operands)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];

	operator_t*  op;
	node_t*      node;

	// 查找操作符
	op = find_base_operator(w->text->data, nb_operands);
	if (!op) {
		loge("find op '%s' error, nb_operands: %d\n", w->text->data, nb_operands);
		return DFA_ERROR;
	}

	// 创建节点
	node = node_alloc(w, op->type, NULL);
	if (!node) {
		loge("op node '%s' alloc failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 将操作符节点添加到表达式中
	if (expr_add_node(d->expr, node) < 0) {
		loge("add op node '%s' to expr failed\n", w->text->data);
		return DFA_ERROR;
	}

	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// 处理单目运算符
/*
解释：

该函数处理单目运算符（如 ++、--）。它调用了 _expr_action_op，并传递 1 表示该操作符是单目运算符。
*/
static int _expr_action_unary_op(dfa_t* dfa, vector_t* words, void* data)
{
	logd("\n");
	// 单目运算符只需要一个操作数
	return _expr_action_op(dfa, words, data, 1);
}

// 处理双目运算符
/*
解释：

该函数处理双目运算符（如 +、-、*、/ 等）。它首先处理一些特定的操作符，
如解引用符号 *，然后判断是否需要访问结构体成员，如果需要，切换到结构体
的块。最后，调用 _expr_action_op 处理双目运算符。
*/
static int _expr_action_binary_op(dfa_t* dfa, vector_t* words, void* data)
{
	logd("\n");

	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 1];
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id    = stack_top(d->current_identities);
	variable_t*     v;

	// 处理解引用操作符（*）
	if (LEX_WORD_STAR == w->type) {

		if (id && id->identity) {

			v = block_find_variable(parse->ast->current_block, id->identity->text->data);
			if (!v) {
				logw("'%s' not var\n", id->identity->text->data);

				// 清除当前表达式
				if (d->expr) {
					expr_free(d->expr);
					d->expr = NULL;
				}
				return DFA_NEXT_SYNTAX;
			}
		}
	}

	// 如果是变量，添加到表达式中
	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0)
			return DFA_ERROR;
	}

	// 处理成员访问操作符（箭头或点）
	if (LEX_WORD_ARROW == w->type || LEX_WORD_DOT == w->type) {
		assert(md->current_struct);

		if (!md->parent_block)
			md->parent_block = parse->ast->current_block;
		// 切换到结构体所在的块
		parse->ast->current_block = (block_t*)md->current_struct;

	} else if (md->parent_block) {
		// 恢复之前的块
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}
	// 双目运算符需要两个操作数
	return _expr_action_op(dfa, words, data, 2);
}

// 处理左括号
/*
解释：

该函数处理左括号 (，当遇到左括号时，创建一个新的表达式，并将当前表达式压入栈中，以便后续处理。还会恢复之前的块信息。
*/
static int _expr_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];

	// 创建新的表达式对象
	expr_t* e = expr_alloc();
	if (!e) {
		loge("\n");
		return DFA_ERROR;
	}

	logi("d->expr: %p, e: %p\n", d->expr, e);

	// 将当前表达式压入栈中，并设置当前表达式为新创建的表达式
	stack_push(md->lp_exprs, d->expr);
	d->expr = e;

	// 如果存在父块，恢复父块
	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block = NULL;
	}

	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// 处理右括号 ) 并执行类型转换
/*
解释：

	目的：处理右括号 ) 时，如果是类型转换表达式，将类型转换节点添加到当前表达式中。

流程：

	检查当前标识符是否有效，如果无效则返回错误。

	如果没有找到类型，则调用 _type_find_type 查找类型。

	如果没有类型或类型不合法，则返回错误。

	如果当前是 sizeof 或 va_arg，跳过处理。

	分配变量并创建节点，将类型转换节点加入到表达式中。
*/
static int _expr_action_rp_cast(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	expr_module_data_t* md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id    = stack_top(d->current_identities);

	// 检查当前是否有有效的标识符
	if (!id) {
		loge("\n");
		return DFA_ERROR;
	}

	// 如果当前标识符没有类型，则查找类型
	if (!id->type) {
		if (_type_find_type(dfa, id) < 0) {
			loge("\n");
			return DFA_ERROR;
		}
	}

	// 如果没有找到类型或类型无效，则报错
	if (!id->type || !id->type_w) {
		loge("\n");
		return DFA_ERROR;
	}

	// 如果当前存在 sizeof 操作符，跳过
	if (d->nb_sizeofs > 0) {
		logw("DFA_NEXT_SYNTAX\n");

		// 清空表达式
		if (d->expr) {
			expr_free(d->expr);
			d->expr = NULL;
		}

		return DFA_NEXT_SYNTAX;
	}

	// 如果当前是 va_arg，跳过
	if (d->current_va_arg) {
		logw("DFA_NEXT_SYNTAX\n");
		return DFA_NEXT_SYNTAX;
	}

	variable_t* var       = NULL;
	node_t*     node_var  = NULL;
	node_t*     node_cast = NULL;

	// 根据类型信息为变量分配内存
	var = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, id->func_ptr);
	if (!var) {
		loge("var alloc failed\n");
		return DFA_ERROR;
	}

	// 为变量节点分配内存
	node_var = node_alloc(NULL, var->type, var);
	if (!node_var) {
		loge("var node alloc failed\n");
		return DFA_ERROR;
	}

	// 为类型转换操作符创建节点
	node_cast = node_alloc(id->type_w, OP_TYPE_CAST, NULL);
	if (!node_cast) {
		loge("cast node alloc failed\n");
		return DFA_ERROR;
	}
	node_add_child(node_cast, node_var);

	// 通过左括号时，将表达式压栈
	expr_t* e = stack_pop(md->lp_exprs);

	logd("type cast: d->expr: %p, d->expr->parent: %p, e: %p\n", d->expr, d->expr->parent, e);

	assert(e);

	// 将类型转换节点添加到表达式中
	expr_add_node(e, node_cast);
	d->expr = e;

	// 释放标识符，并继续处理下一个词
	stack_pop(d->current_identities);
	free(id);
	return DFA_NEXT_WORD;
}

// 处理右括号 ) 和变量/函数解析
/*
解释：

	目的：处理右括号 )，用于解析函数或变量，添加表达式节点并恢复父表达式。

流程：

	检查当前标识符是否有效。

	查找变量或函数。

	如果找到了变量或函数，则将其添加到当前表达式中。

恢复父块并将当前表达式节点返回到父表达式中。
*/
static int _expr_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	// 处理右括号 ) 和变量/函数解析
	if (id && id->identity) {

		variable_t* v = NULL;
		function_t* f = NULL;
		
		if (ast_find_variable(&v, parse->ast, id->identity->text->data) < 0)
			return DFA_ERROR;

		// 如果找不到函数，跳过
		if (!v) {
			logw("'%s' not var\n", id->identity->text->data);

			if (ast_find_function(&f, parse->ast, id->identity->text->data) < 0)
				return DFA_ERROR;

			if (!f) {
				logw("'%s' not function\n", id->identity->text->data);
				return DFA_NEXT_SYNTAX;
			}
		}
		// 将变量添加到当前表达式
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	// 恢复父块
	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	// 处理括号内的表达式
	expr_t* parent = stack_pop(md->lp_exprs);

	logd("d->expr: %p, d->expr->parent: %p, lp: %p\n\n", d->expr, d->expr->parent, parent);

	// 将当前表达式添加到父表达式
	if (parent) {
		expr_add_node(parent, d->expr);
		d->expr = parent;
	}

	return DFA_NEXT_WORD;
}

// 处理后置递增/递减 ++ 或 --
/*
解释：

	目的：处理后置递增（++）或递减（--）操作符，将其添加到表达式中。

流程：

	检查标识符是否有效，如果有效，将其添加到表达式中。

	根据操作符创建相应的后置递增或递减节点。

	将节点添加到当前表达式中。
*/
static int _expr_action_unary_post(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_data_t*      d     = data;
	node_t*      n     = NULL;
	dfa_identity_t*  id    = stack_top(d->current_identities);

	// 如果标识符有效，将其添加到表达式
	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	// 根据操作符类型创建相应的节点
	if (LEX_WORD_INC == w->type)
		n = node_alloc(w, OP_INC_POST, NULL);

	else if (LEX_WORD_DEC == w->type)
		n = node_alloc(w, OP_DEC_POST, NULL);
	else {
		loge("\n");
		return DFA_ERROR;
	}

	// 如果节点创建失败，返回错误
	if (!n) {
		loge("node alloc error\n");
		return DFA_ERROR;
	}

	// 将节点添加到表达式中
	expr_add_node(d->expr, n);

	logd("n: %p, expr: %p, parent: %p\n", n, d->expr, d->expr->parent);

	return DFA_NEXT_WORD;
}

// 处理左方括号
/*
解释：

	目的：处理左方括号 [，表示数组下标操作，创建相应的表达式节点。

流程：

	检查标识符是否有效。

	查找并创建数组下标操作符节点。

	将当前表达式压入栈，准备处理下标表达式。
*/
static int _expr_action_ls(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	lex_word_t*      w     = words->data[words->size - 1];
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	// 如果标识符有效，将其添加到表达式中
	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	// 恢复父块
	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block = NULL;
	}

	// 查找数组下标操作符
	operator_t* op = find_base_operator_by_type(OP_ARRAY_INDEX);
	assert(op);

	// 创建节点并分配内存
	node_t* n = node_alloc(w, op->type, NULL);
	if (!n) {
		loge("node alloc error\n");
		return DFA_ERROR;
	}
	n->op = op;

	// 将节点添加到当前表达式中
	expr_add_node(d->expr, n);

	// 将当前表达式压入栈中，准备处理下标表达式
	stack_push(md->ls_exprs, d->expr);

	// 创建新的表达式用于处理下标
	expr_t* index = expr_alloc();
	if (!index) {
		loge("index expr alloc error\n");
		return DFA_ERROR;
	}

	d->expr = index;

	return DFA_NEXT_WORD;
}

// 处理右方括号
/*
解释：

	目的：处理右方括号 ]，表示结束数组下标操作，将表达式恢复到左方括号之前的状态。

流程：

	检查标识符是否有效并将其添加到表达式中。

	恢复父块。

	弹出栈中存储的左方括号表达式，并将当前表达式添加到其中。
*/
static int _expr_action_rs(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*      id    = stack_top(d->current_identities);

	// 如果标识符有效，将其添加到表达式
	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0) {
			loge("expr add var error\n");
			return DFA_ERROR;
		}
	}

	// 恢复父块
	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	// 从栈中弹出左方括号对应的表达式
	expr_t* ls_parent = stack_pop(md->ls_exprs);

	// 确保表达式存在，回溯到父表达式
	assert (d->expr);
	while (d->expr->parent)
		d->expr = d->expr->parent;

	// 将当前表达式添加到左方括号表达式
	if (ls_parent) {
		expr_add_node(ls_parent, d->expr);
		d->expr = ls_parent;
	}

	return DFA_NEXT_WORD;
}

// 处理多重返回值(多返回值函数)
/*
解释：

	目的：此函数处理表达式中的函数调用或对象创建操作，确保多返回值的情况得到处理。

流程：

	检查赋值语句的类型，确保是 OP_ASSIGN。

	查找赋值语句的右操作数是否为函数调用或对象创建。

	根据函数返回值的个数，判断是否需要处理多返回值。

	创建一个新的块（multi_rets）以保存多返回值，并添加合适的节点。

	更新父节点并返回。
*/
int _expr_multi_rets(expr_t* e)
{
	// 检查表达式的第一个节点是否为赋值操作
	if (OP_ASSIGN != e->nodes[0]->type)
		return 0;

	node_t*  parent = e->parent;
	node_t*  assign = e->nodes[0];
	node_t*  call   = assign->nodes[1];

	// 如果调用链是表达式类型，继续向下查找
	while (call) {
		if (OP_EXPR == call->type)
			call = call->nodes[0];
		else
			break;
	}

	int nb_rets;

	// 如果没有找到调用节点，返回0
	if (!call)
		return 0;

	// 如果调用是函数调用类型
	if (OP_CALL == call->type) {

		node_t* n_pf = call->nodes[0];

		// 如果是指针类型，则向下追溯到指针类型的右边节点
		if (OP_POINTER == n_pf->type) {
			assert(2       == n_pf->nb_nodes);

			n_pf = n_pf->nodes[1];
		}

		// 获取函数指针并检查返回值数量
		variable_t* v_pf = _operand_get(n_pf);
		function_t* f    = v_pf->func_ptr;

		// 如果函数的返回值个数小于或等于1，则返回0
		if (f->rets->size <= 1)
			return 0;

		// 获取返回值数量
		nb_rets = f->rets->size;

	} else if (OP_CREATE == call->type)
		nb_rets = 2;// 如果是对象创建操作，返回值为2
	else
		return 0;// 如果是其他类型，不处理

	assert(call->nb_nodes > 0);

	node_t*  ret;
	block_t* b;

	int i;
	int j;
	int k;

	// 为多返回值创建一个新的块
	b = block_alloc_cstr("multi_rets");
	if (!b)
		return -ENOMEM;

	// 向块中添加节点，直到块的节点数达到要求的返回值数量
	for (i  = parent->nb_nodes - 2; i >= 0; i--) {
		ret = parent->nodes[i];

		// 如果遇到分号标记，则停止处理
		if (ret->semi_flag)
			break;

		// 如果块的节点数已达到目标数量，则停止
		if (b->node.nb_nodes >= nb_rets - 1)
			break;

		// 添加节点到块中
		node_add_child((node_t*)b, ret);
		parent->nodes[i] = NULL;
	}

	// 反转块中的节点
	j = 0;
	k = b->node.nb_nodes - 1;
	while (j < k) {
		XCHG(b->node.nodes[j], b->node.nodes[k]);
		j++;
		k--;
	}

	// 将赋值语句的左操作数添加到块中
	node_add_child((node_t*)b, assign->nodes[0]);
	assign->nodes[0] = (node_t*)b;
	b->node.parent   = assign;

	// 更新父节点中的节点信息
	i++;
	assert(i >= 0);

	// 更新父节点的节点数量
	parent->nodes[i] = e;
	parent->nb_nodes = i + 1;

	logd("parent->nb_nodes: %d\n", parent->nb_nodes);

	return 0;
}

// 结束表达式的处理
/*
解释：

	目的：结束当前表达式的处理，检查是否有返回值、局部变量等，并进行必要的清理。

流程：

	检查标识符并添加变量。

	恢复父块。

	如果表达式为空，则释放表达式。

	如果表达式有效且没有局部标志，则将其添加到父节点。

	处理多重返回值的情况。
*/
static int _expr_fini_expr(parse_t* parse, dfa_data_t* d, int semi_flag)
{
	expr_module_data_t* md = d->module_datas[dfa_module_expr.index];
	dfa_identity_t*     id = stack_top(d->current_identities);

	// 如果当前标识符有效，添加变量到表达式中
	if (id && id->identity) {
		if (_expr_add_var(parse, d) < 0)
			return DFA_ERROR;
	}

	// 恢复父块
	if (md->parent_block) {
		parse->ast->current_block = md->parent_block;
		md->parent_block          = NULL;
	}

	logd("d->expr: %p\n", d->expr);

	// 检查并处理表达式的节点
	if (d->expr) {
		while (d->expr->parent)
			d->expr = d->expr->parent;

		// 如果表达式没有节点，释放表达式
		if (0 == d->expr->nb_nodes) {

			expr_free(d->expr);
			d->expr = NULL;

		} else if (!d->expr_local_flag) {
			// 如果表达式没有局部标志，添加到父节点
			node_t* parent;

			if (d->current_node)
				parent = d->current_node;
			else
				parent = (node_t*)parse->ast->current_block;

			node_add_child(parent, d->expr);

			logd("d->expr->parent->type: %d\n", d->expr->parent->type);
			// 处理多重返回值
			if (_expr_multi_rets(d->expr) < 0) {
				loge("\n");
				return DFA_ERROR;
			}

			// 设置表达式的分号标志
			d->expr->semi_flag = semi_flag;
			d->expr = NULL;
		}

		logd("d->expr: %p, d->expr_local_flag: %d\n", d->expr, d->expr_local_flag);
	}

	return DFA_OK;
}

// 处理逗号操作符 ,
/*
解释：

	目的：处理逗号（,）操作符。

流程：

	调用 _expr_fini_expr 完成当前表达式的处理。

	返回下一词的处理。
*/
static int _expr_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	// 完成当前表达式的处理
	if (_expr_fini_expr(parse, d, 0) < 0)
		return DFA_ERROR;

	return DFA_NEXT_WORD;
}

// 处理分号操作符 ;
/*
解释：

	目的：处理分号（;）操作符，表示表达式的结束。

流程：

	调用 _expr_fini_expr 完成当前表达式的处理，并设置分号标志。
*/
static int _expr_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	// 完成当前表达式的处理
	if (_expr_fini_expr(parse, d, 1) < 0)
		return DFA_ERROR;

	return DFA_OK;
}

// 初始化表达式模块
static int _dfa_init_module_expr(dfa_t* dfa)
{
	// 使用 DFA 宏来定义各个状态节点及其对应的动作函数
	DFA_MODULE_NODE(dfa, expr, entry,      _expr_is_expr,        _expr_action_expr);
	DFA_MODULE_NODE(dfa, expr, number,     _expr_is_number,      _expr_action_number);
	DFA_MODULE_NODE(dfa, expr, unary_op,   _expr_is_unary_op,    _expr_action_unary_op);
	DFA_MODULE_NODE(dfa, expr, binary_op,  _expr_is_binary_op,   _expr_action_binary_op);
	DFA_MODULE_NODE(dfa, expr, unary_post, _expr_is_unary_post,  _expr_action_unary_post);

	DFA_MODULE_NODE(dfa, expr, lp,         dfa_is_lp,        _expr_action_lp);
	DFA_MODULE_NODE(dfa, expr, rp,         dfa_is_rp,        _expr_action_rp);
	DFA_MODULE_NODE(dfa, expr, rp_cast,    dfa_is_rp,        _expr_action_rp_cast);

	DFA_MODULE_NODE(dfa, expr, ls,         dfa_is_ls,        _expr_action_ls);
	DFA_MODULE_NODE(dfa, expr, rs,         dfa_is_rs,        _expr_action_rs);

	DFA_MODULE_NODE(dfa, expr, comma,      dfa_is_comma,     _expr_action_comma);
	DFA_MODULE_NODE(dfa, expr, semicolon,  dfa_is_semicolon, _expr_action_semicolon);

	// 从 DFA 中获取解析数据结构
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	// 确保模块数据未被初始化
	assert(!md);

	// 为表达式模块分配内存
	md = calloc(1, sizeof(expr_module_data_t));
	if (!md) {
		loge("expr_module_data_t alloc error\n");
		return DFA_ERROR;
	}

	// 为表达式模块中的栈分配内存
	md->ls_exprs = stack_alloc();
	if (!md->ls_exprs)
		goto _ls_exprs;

	// 将表达式模块数据存入 DFA 数据
	md->lp_exprs = stack_alloc();
	if (!md->lp_exprs)
		goto _lp_exprs;

	d->module_datas[dfa_module_expr.index] = md;

	return DFA_OK;

_lp_exprs:
	stack_free(md->ls_exprs);
_ls_exprs:
	loge("\n");

	// 如果有内存分配错误，释放已分配的资源
	free(md);
	md = NULL;
	return DFA_ERROR;
}

// 清理表达式模块
static int _dfa_fini_module_expr(dfa_t* dfa)
{
	// 从 DFA 中获取解析数据结构
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	expr_module_data_t*  md    = d->module_datas[dfa_module_expr.index];

	// 如果表达式模块已初始化，清理模块资源
	if (md) {
		if (md->ls_exprs)
			stack_free(md->ls_exprs);

		if (md->lp_exprs)
			stack_free(md->lp_exprs);

		// 释放模块内存
		free(md);
		md = NULL;
		d->module_datas[dfa_module_expr.index] = NULL;
	}

	return DFA_OK;
}

// 初始化表达式语法树
static int _dfa_init_syntax_expr(dfa_t* dfa)
{
	// 获取各个节点的语法结构
	DFA_GET_MODULE_NODE(dfa, expr,     entry,       expr);
	DFA_GET_MODULE_NODE(dfa, expr,     number,      number);
	DFA_GET_MODULE_NODE(dfa, expr,     unary_op,    unary_op);
	DFA_GET_MODULE_NODE(dfa, expr,     binary_op,   binary_op);
	DFA_GET_MODULE_NODE(dfa, expr,     unary_post,  unary_post);

	DFA_GET_MODULE_NODE(dfa, expr,     lp,          lp);
	DFA_GET_MODULE_NODE(dfa, expr,     rp,          rp);
	DFA_GET_MODULE_NODE(dfa, expr,     rp_cast,     rp_cast);

	DFA_GET_MODULE_NODE(dfa, expr,     ls,          ls);
	DFA_GET_MODULE_NODE(dfa, expr,     rs,          rs);

	DFA_GET_MODULE_NODE(dfa, expr,     comma,       comma);
	DFA_GET_MODULE_NODE(dfa, expr,     semicolon,   semicolon);

	DFA_GET_MODULE_NODE(dfa, identity, identity,    identity);
	DFA_GET_MODULE_NODE(dfa, call,     lp,          call_lp);
	DFA_GET_MODULE_NODE(dfa, call,     rp,          call_rp);

	DFA_GET_MODULE_NODE(dfa, sizeof,   _sizeof,     _sizeof);
	DFA_GET_MODULE_NODE(dfa, sizeof,   rp,          sizeof_rp);

	DFA_GET_MODULE_NODE(dfa, create,   create,      create);
	DFA_GET_MODULE_NODE(dfa, create,   identity,    create_id);
	DFA_GET_MODULE_NODE(dfa, create,   rp,          create_rp);

	DFA_GET_MODULE_NODE(dfa, type,     entry,       type_entry);
	DFA_GET_MODULE_NODE(dfa, type,     base_type,   base_type);
	DFA_GET_MODULE_NODE(dfa, type,     star,        star);

	DFA_GET_MODULE_NODE(dfa, va_arg,   arg,         va_arg);
	DFA_GET_MODULE_NODE(dfa, va_arg,   rp,          va_rp);

	DFA_GET_MODULE_NODE(dfa, container, container,  container);
	DFA_GET_MODULE_NODE(dfa, container, rp,         container_rp);

	// 将表达式模块添加到语法树中
	vector_add(dfa->syntaxes, expr);

	// expr start with number, identity, an unary_op, '(',
	// like: a = b, *p = 1, (a + b)
	// number start may be only useful in return statement.
	// 为表达式起始节点添加子节点
	dfa_node_add_child(expr,       number);
	dfa_node_add_child(expr,       identity);
	dfa_node_add_child(expr,       unary_op);
	dfa_node_add_child(expr,       unary_post);
	dfa_node_add_child(expr,       lp);
	dfa_node_add_child(expr,       semicolon);

	// container(ptr, type, member)
	// 处理数组索引、类型转换、函数调用等
	dfa_node_add_child(expr,         container);
	dfa_node_add_child(container_rp, rp);
	dfa_node_add_child(container_rp, binary_op);
	dfa_node_add_child(container_rp, comma);
	dfa_node_add_child(container_rp, semicolon);

	// create class object
	// 处理类型创建
	dfa_node_add_child(expr,       create);
	dfa_node_add_child(create_id,  semicolon);
	dfa_node_add_child(create_rp,  semicolon);

	// va_arg(ap, type)
	// 处理 va_arg 和 sizeof 等
	dfa_node_add_child(expr,       va_arg);
	dfa_node_add_child(va_rp,      rp);
	dfa_node_add_child(va_rp,      binary_op);
	dfa_node_add_child(va_rp,      comma);
	dfa_node_add_child(va_rp,      semicolon);

	// sizeof()
	// 处理数组下标和类型转换
	dfa_node_add_child(expr,       _sizeof);
	dfa_node_add_child(sizeof_rp,  rp);
	dfa_node_add_child(sizeof_rp,  binary_op);
	dfa_node_add_child(sizeof_rp,  comma);
	dfa_node_add_child(sizeof_rp,  semicolon);

	// (expr)
	dfa_node_add_child(lp,         identity);
	dfa_node_add_child(lp,         number);
	dfa_node_add_child(lp,         unary_op);
	dfa_node_add_child(lp,         _sizeof);
	dfa_node_add_child(lp,         lp);

	dfa_node_add_child(identity,   rp);
	dfa_node_add_child(number,     rp);
	dfa_node_add_child(rp,         rp);

	dfa_node_add_child(rp,         binary_op);
	dfa_node_add_child(identity,   binary_op);
	dfa_node_add_child(number,     binary_op);

	// type cast, like: (type*)var
	dfa_node_add_child(lp,         type_entry);
	dfa_node_add_child(base_type,  rp_cast);
	dfa_node_add_child(star,       rp_cast);
	dfa_node_add_child(identity,   rp_cast);

	dfa_node_add_child(rp_cast,    identity);
	dfa_node_add_child(rp_cast,    number);
	dfa_node_add_child(rp_cast,    unary_op);
	dfa_node_add_child(rp_cast,    _sizeof);
	dfa_node_add_child(rp_cast,    lp);

	// identity() means function call, implement in dfa_call.c
	dfa_node_add_child(identity,   call_lp);
	dfa_node_add_child(call_rp,    rp);
	dfa_node_add_child(call_rp,    binary_op);
	dfa_node_add_child(call_rp,    comma);
	dfa_node_add_child(call_rp,    semicolon);

	// array index, a[1 + 2], a[]
	// [] is a special binary op,
	// should be added before normal binary op such as '+'
	dfa_node_add_child(identity,   ls);
	dfa_node_add_child(ls,         expr);
	dfa_node_add_child(expr,       rs);
	dfa_node_add_child(ls,         rs);
	dfa_node_add_child(rs,         ls);
	dfa_node_add_child(rs,         binary_op);

	dfa_node_add_child(rs,         unary_post);
	dfa_node_add_child(rs,         rp);
	dfa_node_add_child(identity,   unary_post);

	// recursive unary_op, like: !*p
	dfa_node_add_child(unary_op,   unary_op);
	dfa_node_add_child(unary_op,   number);
	dfa_node_add_child(unary_op,   identity);
	dfa_node_add_child(unary_op,   expr);

	dfa_node_add_child(binary_op,  unary_op);
	dfa_node_add_child(binary_op,  number);
	dfa_node_add_child(binary_op,  identity);

	// create class object
	dfa_node_add_child(binary_op,  create);
	dfa_node_add_child(binary_op,  expr);

	dfa_node_add_child(unary_post, rp);
	dfa_node_add_child(unary_post, rs);
	dfa_node_add_child(unary_post, binary_op);
	dfa_node_add_child(unary_post, comma);
	dfa_node_add_child(unary_post, semicolon);

	dfa_node_add_child(rp,         comma);
	dfa_node_add_child(number,     comma);
	dfa_node_add_child(identity,   comma);
	dfa_node_add_child(rs,         comma);
	dfa_node_add_child(comma,      expr);

	dfa_node_add_child(rp,         semicolon);
	dfa_node_add_child(number,     semicolon);
	dfa_node_add_child(identity,   semicolon);
	dfa_node_add_child(rs,         semicolon);

	return 0;
}

// 表达式模块结构
dfa_module_t dfa_module_expr =
{
	.name        = "expr",

	.init_module = _dfa_init_module_expr,
	.init_syntax = _dfa_init_syntax_expr,

	.fini_module = _dfa_fini_module_expr,
};
