#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_enum;

/*解释：

enum_module_data_t 结构体用于存储 enum 解析时的状态信息：

current_enum 存储当前正在解析的枚举类型。

current_v 存储当前的变量（在代码中并未实际使用）。

hook 存储相关的钩子，用于执行后置动作。

vars 存储枚举类型中的所有变量。

nb_lbs 和 nb_rbs 分别表示当前解析的左大括号 { 和右大括号 } 的数量。
*/
typedef struct {
	lex_word_t*  current_enum;// 当前枚举类型（指向 lex_word_t 类型）
	variable_t*  current_v;// 当前变量（未在代码中使用）

	dfa_hook_t*  hook;// 钩子（用于后续动作）

	vector_t*    vars;// 存储枚举中的变量列表

	int              nb_lbs;// 左大括号数量
	int              nb_rbs;// 右大括号数量

} enum_module_data_t;

// 处理遇到 `type` 关键字时的动作
/*
解释：

该函数在遇到 type 关键字时执行，表示要定义一个新的类型。

如果已经存在枚举类型，则返回错误（防止重复定义）。

使用 block_find_type 查找是否已经定义了该类型，如果没有，则使用 type_alloc 分配一个新的类型。

新类型将被加入作用域，并标记为枚举类型。

将当前的枚举类型存储在 md->current_enum 中，以便后续处理。
*/
static int _enum_action_type(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];

	// 如果已经定义了枚举类型，则返回错误
	if (md->current_enum) {
		loge("\n");
		return DFA_ERROR;
	}

	// 查找指定的类型
	type_t* t = block_find_type(parse->ast->root_block, w->text->data);
	if (!t) {
		// 如果类型不存在，则分配一个新的类型
		t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		// 增加结构体计数并将类型推入作用域
		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scope_push_type(parse->ast->root_block->scope, t);
	}

	// 将当前枚举类型标记为正在解析的类型
	md->current_enum = w;

	return DFA_NEXT_WORD;
}

// 处理遇到左大括号 '{' 时的动作
/*解释：

该函数处理左大括号 {，用于开始枚举类型的定义。

如果遇到超过一个左大括号，则返回错误。

如果枚举类型已经定义，查找该类型；如果未定义，则创建一个新的匿名类型。

在每次遇到左大括号时，增加 nb_lbs 计数，表示解析开始进入一个新的枚举元素定义部分。

*/
static int _enum_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	lex_word_t*      w     = words->data[words->size - 1];
	type_t*          t     = NULL;

	// 如果左大括号数量已经大于 1，表示语法错误
	if (md->nb_lbs > 0) {
		loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 如果已经定义了枚举类型，则查找该类型
	if (md->current_enum) {

		t = block_find_type(parse->ast->root_block, md->current_enum->text->data);
		if (!t) {
			loge("type '%s' not found\n", md->current_enum->text->data);
			return DFA_ERROR;
		}
	} else {
		// 如果没有枚举类型，则分配一个匿名类型
		t = type_alloc(w, "anonymous", STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("type alloc failed\n");
			return DFA_ERROR;
		}

		// 增加结构体计数并将匿名类型推入作用域
		parse->ast->nb_structs++;
		t->node.enum_flag = 1;
		scope_push_type(parse->ast->root_block->scope, t);

		md->current_enum = w;// 标记当前枚举类型
	}

	// 增加左大括号数量
	md->nb_lbs++;

	return DFA_NEXT_WORD;
}

// 处理枚举变量的定义
static int _enum_action_var(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*         parse = dfa->priv;// 获取解析器的上下文
	dfa_data_t*          d     = data;// 获取 DFA 数据
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	// 获取当前词
	lex_word_t*      w     = words->data[words->size - 1];

	variable_t*      v0    = NULL;// 用于存储先前的枚举变量
	variable_t*      v     = NULL; // 当前定义的枚举变量
	type_t*          t     = NULL;// 当前变量的类型

	// 检查是否存在有效的枚举类型
	if (!md->current_enum) {
		loge("\n");
		return DFA_ERROR;// 没有找到当前枚举类型，返回错误
	}

	// 查找 `int` 类型
	if (ast_find_type_type(&t, parse->ast, VAR_INT) < 0) {
		loge("\n");
		return DFA_ERROR;// 如果未找到 `int` 类型，则返回错误
	}

	// 查找该枚举变量是否已经定义
	v = block_find_variable(parse->ast->root_block, w->text->data);
	if (v) {
		// 如果变量已定义，报错并返回错误
		loge("repeated declared enum var '%s', 1st in file: %s, line: %d\n", w->text->data, v->w->file->data, v->w->line);
		return DFA_ERROR;
	}

	// 分配一个新的变量
	v = VAR_ALLOC_BY_TYPE(w, t, 1, 0, NULL);
	if (!v) {
		loge("var alloc failed\n");
		return DFA_ERROR;// 如果变量分配失败，返回错误
	}

	// 标记为常量
	v->const_literal_flag = 1;

	// 如果已有其他枚举变量，递增当前枚举值
	if (md->vars->size > 0) {
		v0          = md->vars->data[md->vars->size - 1];// 获取先前的枚举变量
		v->data.i64 = v0->data.i64 + 1;// 当前变量值等于上一个变量值加一
	} else
		v->data.i64 = 0;// 如果没有先前的变量，初始化为 0

	// 将该变量添加到枚举变量列表
	if (vector_add(md->vars, v) < 0) {
		loge("var add failed\n");
		return DFA_ERROR;// 如果添加失败，返回错误
	}

	// 将变量推入作用域
	scope_push_var(parse->ast->root_block->scope, v);

	// 设置当前变量为已定义的枚举变量
	md->current_v = v;

	// 返回继续解析
	return DFA_NEXT_WORD;
}

// 处理枚举变量赋值的动作
/*
解释：

该函数处理枚举变量的赋值操作。确保枚举变量已经定义，并且设置钩子来处理后续的赋值表达式。
*/
static int _enum_action_assign(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器的上下文
	parse_t*         parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*          d     = data;
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	// 获取当前词
	lex_word_t*      w     = words->data[words->size - 1];

	if (!md->current_v) {
		loge("no enum var before '=' in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR; // 如果没有找到当前枚举变量，则返回错误
	}

	// 确保没有现有表达式
	assert(!d->expr);
	// 增加局部表达式标志
	d->expr_local_flag++;

	// 设置钩子，推入枚举处理钩子
	md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "enum_comma"), DFA_HOOK_POST);

	// 返回继续解析
	return DFA_NEXT_WORD;
}

// 处理枚举变量的逗号 , 分隔符
/*
解释：

该函数处理 , 分隔符，表示枚举项的定义结束。它会计算枚举变量的表达式，确保赋值的是常量，并将值赋给当前枚举变量。
*/
static int _enum_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器的上下文
	parse_t*         parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*          d     = data;
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	// 获取当前词
	lex_word_t*      w     = words->data[words->size - 1];
	// 用于存储计算结果的变量
	variable_t*      r     = NULL;

	if (!md->current_v) {
		loge("enum var before ',' not found, in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;// 如果没有找到枚举变量，则返回错误
	}

	// 如果存在表达式，计算并赋值给枚举变量
	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent;// 跳转到根节点

		// 计算表达式的值
		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");
			return DFA_ERROR;// 计算失败，返回错误
		}

		// 确保赋值的结果是常量
		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;// 不是常量赋值，返回错误
		}

		// 将计算结果赋值给枚举变量
		md->current_v->data.i64 = r->data.i64;

		// 释放计算结果
		variable_free(r);
		r = NULL;

		// 释放表达式
		expr_free(d->expr);
		d->expr = NULL;
		// 减少表达式标志
		d->expr_local_flag--;
	}

	logi("enum var: '%s', value: %ld\n", md->current_v->w->text->data, md->current_v->data.i64);

	// 重置当前枚举变量
	md->current_v = NULL;

	// 切换到下一个状态
	return DFA_SWITCH_TO;
}

// 处理右大括号 '}'，结束枚举类型的定义
/*
解释：

该函数处理分号 ;，用于确保枚举类型的定义已完成。它会检查左右大括号是否匹配，并确保枚举变量在结束时已正确处理。
*/
static int _enum_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器的上下文
	parse_t*         parse = dfa->priv;
 	// 获取 DFA 数据
	dfa_data_t*          d     = data;
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	// 获取当前词
	lex_word_t*      w     = words->data[words->size - 1];
	// 用于存储计算结果的变量
	variable_t*      r     = NULL;

	// 如果左大括号数量多于 1，报错
	if (md->nb_lbs > 1) {
		loge("too many '{' in enum, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 增加右大括号计数
	md->nb_rbs++;

	// 确保左右大括号匹配
	assert(md->nb_rbs == md->nb_lbs);

	// 处理表达式并赋值给枚举变量
	if (d->expr) {
		while(d->expr->parent)
			d->expr = d->expr->parent; // 跳转到根节点

		// 计算表达式
		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");
			return DFA_ERROR;// 计算失败，返回错误
		}

		// 确保赋值为常量
		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("enum var must be inited by constant, file: %s, line: %d\n", w->file->data, w->line);
			return -1;// 非常量赋值，返回错误
		}

		// 给枚举变量赋值
		md->current_v->data.i64 = r->data.i64;

		// 释放变量
		variable_free(r);
		r = NULL;

		// 释放表达式
		expr_free(d->expr);
		d->expr = NULL;
		// 减少局部标志
		d->expr_local_flag--;
	}

	// 清除钩子
	if (md->hook) {
		dfa_del_hook(&(dfa->hooks[DFA_HOOK_POST]), md->hook);
		md->hook = NULL;
	}

	// 重置状态
	md->nb_lbs = 0;
	md->nb_rbs = 0;

	// 清空枚举变量列表
	md->current_enum = NULL;
	md->current_v    = NULL;

	vector_clear(md->vars, NULL);

	// 返回继续解析
	return DFA_NEXT_WORD;
}

// 处理枚举定义结束时的分号 ';'
/*
解释：

该函数在遇到分号 ; 时检查是否存在枚举定义的错误。例如，检查左大括号 { 和右大括号 } 的数量是否匹配，检查枚举变量是否被正确赋值。
*/
static int _enum_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器上下文
	parse_t*         parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*          d     = data;
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];
	// 获取当前词
	lex_word_t*      w     = words->data[words->size - 1];

	// 检查是否左大括号数量和右大括号数量匹配
	if (0 != md->nb_rbs || 0 != md->nb_lbs) {
		loge("'{' and '}' not same in enum, file: %s, line: %d\n", w->file->data, w->line);

		md->nb_rbs = 0;// 重置右大括号计数
		md->nb_lbs = 0;// 重置左大括号计数
		return DFA_ERROR;// 返回错误
	}

	// 如果枚举变量未处理完，抛出错误
	if (md->current_v) {
		loge("enum var '%s' should be followed by ',' or '}', file: %s, line: %d\n",
				md->current_v->w->text->data,
				md->current_v->w->file->data,
				md->current_v->w->line);

		md->current_v = NULL;// 重置当前枚举变量
		return DFA_ERROR;// 返回错误
	}
 	// 返回成功
	return DFA_OK;
}

// 初始化枚举模块，设置解析节点和动作
/*
解释：

该函数初始化了枚举模块，包括定义枚举相关的解析节点及其动作（如处理 enum 类型、左大括号 {、右大括号 } 等）。
它还初始化了与枚举相关的数据结构（如枚举变量列表）。
*/
static int _dfa_init_module_enum(dfa_t* dfa)
{
	// 枚举类型节点
	DFA_MODULE_NODE(dfa, enum, _enum,     dfa_is_enum,      NULL);
	// 枚举类型的处理
	DFA_MODULE_NODE(dfa, enum, type,      dfa_is_identity,  _enum_action_type);
	// 左大括号
	DFA_MODULE_NODE(dfa, enum, lb,        dfa_is_lb,        _enum_action_lb);
	// 右大括号
	DFA_MODULE_NODE(dfa, enum, rb,        dfa_is_rb,        _enum_action_rb);
	// 分号
	DFA_MODULE_NODE(dfa, enum, semicolon, dfa_is_semicolon, _enum_action_semicolon);
	// 枚举变量定义
	DFA_MODULE_NODE(dfa, enum, var,       dfa_is_identity,  _enum_action_var);
	// 枚举赋值
	DFA_MODULE_NODE(dfa, enum, assign,    dfa_is_assign,    _enum_action_assign);
	// 枚举成员间逗号
	DFA_MODULE_NODE(dfa, enum, comma,     dfa_is_comma,     _enum_action_comma);

	// 初始化模块数据
	parse_t*         parse = dfa->priv;
	dfa_data_t*          d     = parse->dfa_data;
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	// 确保没有先前的模块数据
	assert(!md);

	// 分配内存初始化枚举模块数据
	md = calloc(1, sizeof(enum_module_data_t));
	if (!md) {
		loge("\n");
		return DFA_ERROR;// 分配失败，返回错误
	}

	// 初始化枚举变量列表
	md->vars = vector_alloc();
	if (!md->vars) {
		loge("\n");
		free(md);
		return DFA_ERROR;// 分配失败，返回错误
	}

	// 将模块数据存储到 DFA 中
	d->module_datas[dfa_module_enum.index] = md;

	// 返回成功
	return DFA_OK;
}

// 清理枚举模块，释放资源
/*
解释：

该函数负责清理枚举模块，释放相关的内存资源，并将模块数据指针置为 NULL。
*/
static int _dfa_fini_module_enum(dfa_t* dfa)
{
	// 获取解析器上下文
	parse_t*         parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*          d     = parse->dfa_data;
	// 获取枚举模块的数据
	enum_module_data_t*  md    = d->module_datas[dfa_module_enum.index];

	if (md) {
		free(md);// 释放枚举模块数据
		md = NULL;
		// 清空模块数据指针
		d->module_datas[dfa_module_enum.index] = NULL;
	}

	// 返回成功
	return DFA_OK;
}

// 初始化枚举语法，构建语法树
/*
解释：

该函数初始化了枚举类型的语法树。它通过将不同的语法节点连接在一起，
构建出枚举的语法结构，包括枚举类型、类型定义、成员变量、赋值等部分。
*/
static int _dfa_init_syntax_enum(dfa_t* dfa)
{
	// 获取枚举节点
	DFA_GET_MODULE_NODE(dfa, enum,  _enum,    _enum);
	// 获取类型节点
	DFA_GET_MODULE_NODE(dfa, enum,  type,      type);
	// 获取左大括号节点
	DFA_GET_MODULE_NODE(dfa, enum,  lb,        lb);
	// 获取右大括号节点
	DFA_GET_MODULE_NODE(dfa, enum,  rb,        rb);
	// 获取分号节点
	DFA_GET_MODULE_NODE(dfa, enum,  semicolon, semicolon);
	// 获取赋值节点
	DFA_GET_MODULE_NODE(dfa, enum,  assign,    assign);
	// 获取逗号节点
	DFA_GET_MODULE_NODE(dfa, enum,  comma,     comma);
	// 获取变量节点
	DFA_GET_MODULE_NODE(dfa, enum,  var,       var);
	// 获取表达式节点
	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);

	// 将语法树中的节点按顺序连接起来
	vector_add(dfa->syntaxes,  _enum);// 将枚举类型添加到语法列表

	// 定义枚举类型的语法树结构
	dfa_node_add_child(_enum,   type);
	dfa_node_add_child(type,    semicolon);
	dfa_node_add_child(type,    lb);

	// 定义匿名枚举类型的语法
	dfa_node_add_child(_enum,   lb);

	// 空枚举类型的语法
	dfa_node_add_child(lb,      rb);

	// 枚举成员变量的语法
	dfa_node_add_child(lb,      var);
	dfa_node_add_child(var,     comma);
	dfa_node_add_child(var,     assign);
	dfa_node_add_child(assign,  expr);
	dfa_node_add_child(expr,    comma);
	dfa_node_add_child(comma,   var);

	dfa_node_add_child(var,     rb);
	dfa_node_add_child(expr,    rb);

	// 枚举结束的语法
	dfa_node_add_child(rb,      semicolon);

	// 返回成功
	return 0;
}

// 定义枚举模块，包含初始化、清理等功能
dfa_module_t dfa_module_enum = 
{
	.name        = "enum", // 模块名称
	.init_module = _dfa_init_module_enum,// 初始化函数
	.init_syntax = _dfa_init_syntax_enum,// 初始化语法函数

	.fini_module = _dfa_fini_module_enum,// 清理函数
};
