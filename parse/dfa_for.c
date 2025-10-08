#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_for;

// dfa_for_data_t 结构体用于存储与 for 循环相关的数据
typedef struct {
	int              nb_lps;// 记录 '(' 的数量
	int              nb_rps;// 记录 ')' 的数量

	block_t*     parent_block;// 父级块（代表语法树中的一个代码块）
	node_t*      parent_node;// 父节点（表示语法树中的一个节点）

	node_t*      _for;// 当前处理的 for 节点

	int              nb_semicolons;// 记录分号的数量，用于解析 for 循环的三个表达式部分
	vector_t*    init_exprs;// 初始化部分的表达式列表
	expr_t*      cond_expr;// 条件表达式
	vector_t*    update_exprs;// 更新部分的表达式列表

} dfa_for_data_t;

// _for_is_end：用于判断 'for' 循环是否结束
// 该函数在循环结束时被调用，返回 1 表示结束
static int _for_is_end(dfa_t* dfa, void* word)
{
	return 1;// 假设总是结束
}

// _for_action_for：处理 for 循环的开始动作
// 主要是创建 for 循环节点并将其添加到语法树
static int _for_action_for(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器
	parse_t*     parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*      d     = data;
	// 获取当前的词法单元
	lex_word_t*  w     = words->data[words->size - 1];

	dfa_for_data_t*  fd    = NULL;
	// 获取当前模块的数据栈
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	// 创建新的 for 节点
	node_t*     _for   = node_alloc(w, OP_FOR, NULL);

	if (!_for)
		return -ENOMEM;// 分配内存失败

	// 为 for 数据分配内存
	fd = calloc(1, sizeof(dfa_for_data_t));
	if (!fd)
		return -ENOMEM;// 分配内存失败

	// 将 for 节点添加到父节点或当前块中
	if (d->current_node)
		node_add_child(d->current_node, _for);
	else
		node_add_child((node_t*)parse->ast->current_block, _for);

	fd->parent_block = parse->ast->current_block;// 设置父块
	fd->parent_node  = d->current_node;// 设置父节点
	fd->_for         = _for;// 设置当前 for 节点
	d->current_node  = _for;// 更新当前节点为 for 节点

	// 将 for 数据压入栈中
	stack_push(s, fd);
	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// _for_action_lp：处理左括号 '(' 出现的情况
// 主要是标记进入 for 循环的表达式解析阶段
static int _for_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;// 获取解析器
	dfa_data_t*       d     = data;// 获取 DFA 数据
	lex_word_t*   w     = words->data[words->size - 1]; // 获取当前的词法单元
	stack_t*      s     = d->module_datas[dfa_module_for.index];// 获取当前模块的数据栈
	dfa_for_data_t*   fd    = stack_top(s);// 获取栈顶的 for 数据

	assert(!d->expr);// 确保没有当前表达式
	d->expr_local_flag = 1;// 设置局部表达式标记

	// 推送 for 循环的钩子节点，用于后续操作
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_rp"),        DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// _for_action_lp_stat：处理 for 循环中的左括号内的语句部分
static int _for_action_lp_stat(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取 DFA 数据
	dfa_data_t*      d  = data;
	// 获取当前模块的数据栈
	stack_t*     s  = d->module_datas[dfa_module_for.index];
	// 获取栈顶的 for 数据
	dfa_for_data_t*  fd = stack_top(s);
	// 推送钩子节点
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"), DFA_HOOK_POST);

	fd->nb_lps++;// 增加左括号计数
 	// 继续处理下一个词
	return DFA_NEXT_WORD;
}

// _for_action_comma：处理逗号 ',' 的情况
// 根据逗号的位置将表达式分别放入初始化、条件和更新部分
static int _for_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取解析器
	parse_t*     parse = dfa->priv;
	// 获取 DFA 数据
	dfa_data_t*      d     = data;
 	// 获取当前的词法单元
	lex_word_t*  w     = words->data[words->size - 1];
	// 获取当前模块的数据栈
	stack_t*     s     = d->module_datas[dfa_module_for.index];
	// 获取栈顶的 for 数据
	dfa_for_data_t*  fd    = stack_top(s);
	// 如果没有表达式，出错
	if (!d->expr) {
		loge("need expr before ',' in file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 根据 semicolon 的数量处理不同部分的表达式
	if (0 == fd->nb_semicolons) {
		if (!fd->init_exprs)
			fd->init_exprs = vector_alloc();// 初始化表达式列表

		vector_add(fd->init_exprs, d->expr); // 添加初始化表达式
		d->expr = NULL;// 清空当前表达式

	} else if (1 == fd->nb_semicolons) {
		fd->cond_expr = d->expr;// 设置条件表达式
		d->expr = NULL;// 清空当前表达式

	} else if (2 == fd->nb_semicolons) {
		if (!fd->update_exprs)
			fd->update_exprs = vector_alloc(); // 更新表达式列表

		vector_add(fd->update_exprs, d->expr);// 添加更新表达式
		d->expr = NULL;// 清空当前表达式
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;// 分号过多，出错
	}

	// 推送钩子节点
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),   DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"), DFA_HOOK_POST);

	return DFA_NEXT_WORD;// 继续处理下一个词
}

// _for_action_semicolon：处理分号 ';' 的情况
// 更新 for 循环的表达式
static int _for_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	if (!data)
		return DFA_ERROR;

	parse_t*     parse = dfa->priv;// 获取解析器
	dfa_data_t*      d     = data;// 获取 DFA 数据
	lex_word_t*  w     = words->data[words->size - 1];// 获取当前的词法单元
	stack_t*     s     = d->module_datas[dfa_module_for.index];// 获取当前模块的数据栈
	dfa_for_data_t*  fd    = stack_top(s);// 获取栈顶的 for 数据

	// 处理不同阶段的表达式
	if (0 == fd->nb_semicolons) {
		if (d->expr) {
			if (!fd->init_exprs)
				fd->init_exprs = vector_alloc();// 初始化表达式列表

			vector_add(fd->init_exprs, d->expr); // 添加初始化表达式
			d->expr = NULL;// 清空当前表达式
		}
	} else if (1 == fd->nb_semicolons) {
		if (d->expr) {
			fd->cond_expr = d->expr;// 设置条件表达式
			d->expr = NULL;// 清空当前表达式
		}
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;// 分号过多，出错
	}

	fd->nb_semicolons++;// 增加分号计数

	// 推送钩子节点
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

	return DFA_SWITCH_TO;// 切换到下一个状态
}

// _for_add_expr_vector：将表达式向量添加到 for 循环节点
// 主要是将初始化、条件、更新表达式列表添加到 for 节点
static int _for_add_expr_vector(dfa_for_data_t* fd, vector_t* vec)
{
	if (!vec) {
		node_add_child(fd->_for, NULL);// 空表达式，添加空节点
		return DFA_OK;
	}

	if (0 == vec->size) {
		node_add_child(fd->_for, NULL);// 空向量，添加空节点

		vector_free(vec);// 释放向量内存
		vec = NULL;
		return DFA_OK;
	}

	node_t* parent = fd->_for;
	if (vec->size > 1) {

		block_t* b = block_alloc_cstr("for");// 创建新的代码块节点

		node_add_child(fd->_for, (node_t*)b);
		parent = (node_t*)b;
	}

	int i;
	for (i = 0; i < vec->size; i++) {

		expr_t* e = vec->data[i];// 获取表达式

		node_add_child(parent, e);// 将表达式添加到父节点
	}

	vector_free(vec);// 释放向量内存
	vec = NULL;
	return DFA_OK;
}

// _for_add_exprs 函数将所有的 for 循环表达式添加到相应的节点中
static int _for_add_exprs(dfa_for_data_t* fd)
{
	// 将初始化表达式列表添加到 for 节点
	_for_add_expr_vector(fd, fd->init_exprs);
	fd->init_exprs = NULL;// 清空初始化表达式列表

	// 将条件表达式添加到 for 节点
	node_add_child(fd->_for, fd->cond_expr);
	fd->cond_expr = NULL;// 清空条件表达式

	// 将更新表达式列表添加到 for 节点
	_for_add_expr_vector(fd, fd->update_exprs);
	fd->update_exprs = NULL;// 清空更新表达式列表

	// 返回成功标志
	return DFA_OK;
}

// _for_action_rp 函数处理右括号 ')' 的动作
// 它标志着 for 循环表达式部分的结束，进行一些检查并转移到下一个状态
static int _for_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;// 获取解析器
	dfa_data_t*      d     = data;// 获取 DFA 数据
	lex_word_t*  w     = words->data[words->size - 1];// 获取当前的词法单元
	stack_t*     s     = d->module_datas[dfa_module_for.index];// 获取 for 模块的数据栈
	dfa_for_data_t*  fd    = stack_top(s);// 获取栈顶的 for 数据

	fd->nb_rps++; // 增加右括号的计数

	// 如果右括号的数量小于左括号，表示还没结束，继续处理下一个词
	if (fd->nb_rps < fd->nb_lps) {

		// 推送钩子节点，用于处理循环的各个部分
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_rp"),        DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_semicolon"), DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_comma"),     DFA_HOOK_POST);
		DFA_PUSH_HOOK(dfa_find_node(dfa, "for_lp_stat"),   DFA_HOOK_POST);

		return DFA_NEXT_WORD;// 继续处理下一个词
	}

	// 如果是第二个分号之后，处理更新表达式
	if (2 == fd->nb_semicolons) {
		if (!fd->update_exprs)
			fd->update_exprs = vector_alloc();// 如果没有更新表达式列表，则创建一个

		vector_add(fd->update_exprs, d->expr);// 将表达式添加到更新列表
		d->expr = NULL;// 清空当前表达式
	} else {
		loge("too many ';' in for, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;// 错误：分号过多
	}

	// 将表达式添加到 for 节点中
	_for_add_exprs(fd);
	d->expr_local_flag = 0;// 重置局部表达式标志

	// 推送钩子节点，表示 for 循环结束
	DFA_PUSH_HOOK(dfa_find_node(dfa, "for_end"), DFA_HOOK_END);

	// 切换到下一个状态
	return DFA_SWITCH_TO;
}

// _for_action_end 函数处理 for 循环结束的动作
// 它从栈中弹出 for 数据，并更新语法树结构
static int _for_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;// 获取解析器
	dfa_data_t*      d     = data;// 获取 DFA 数据
	stack_t*     s     = d->module_datas[dfa_module_for.index];// 获取 for 模块的数据栈
	dfa_for_data_t*  fd    = stack_pop(s);// 从栈中弹出 for 数据

	// 如果 for 循环节点只有 3 个子节点，表示语法树中的 for 节点为空，需要添加一个空节点
	if (3 == fd->_for->nb_nodes)
		node_add_child(fd->_for, NULL);

	// 确保当前块与父块匹配
	assert(parse->ast->current_block == fd->parent_block);

	// 恢复父节点为当前节点
	d->current_node = fd->parent_node;

	// 输出调试信息
	logi("for: %d, fd: %p, s->size: %d\n", fd->_for->w->line, fd, s->size);

	// 释放 for 数据结构
	free(fd);
	fd = NULL;

	// 确保栈大小大于等于 0
	assert(s->size >= 0);

	// 返回成功标志
	return DFA_OK;
}

// _dfa_init_module_for 函数初始化 for 模块，设置相关的 DFA 节点
static int _dfa_init_module_for(dfa_t* dfa)
{
	// 定义 for 循环模块的各个节点及其动作函数
	DFA_MODULE_NODE(dfa, for, semicolon, dfa_is_semicolon,  _for_action_semicolon);
	DFA_MODULE_NODE(dfa, for, comma,     dfa_is_comma,      _for_action_comma);
	DFA_MODULE_NODE(dfa, for, end,       _for_is_end,           _for_action_end);

	DFA_MODULE_NODE(dfa, for, lp,        dfa_is_lp,         _for_action_lp);
	DFA_MODULE_NODE(dfa, for, lp_stat,   dfa_is_lp,         _for_action_lp_stat);
	DFA_MODULE_NODE(dfa, for, rp,        dfa_is_rp,         _for_action_rp);

	DFA_MODULE_NODE(dfa, for, _for,      dfa_is_for,        _for_action_for);

	parse_t*  parse = dfa->priv;// 获取解析器
	dfa_data_t*   d     = parse->dfa_data;// 获取 DFA 数据
	stack_t*  s     = d->module_datas[dfa_module_for.index];// 获取 for 模块的数据栈

	// 确保栈为空
	assert(!s);

	// 创建一个新的栈
	s = stack_alloc();
	if (!s) {
		loge("\n");
		return DFA_ERROR;// 分配栈失败，返回错误
	}

	// 将新的栈分配给 for 模块
	d->module_datas[dfa_module_for.index] = s;

	// 返回成功标志
	return DFA_OK;
}

// _dfa_fini_module_for 函数清理 for 模块的资源
static int _dfa_fini_module_for(dfa_t* dfa)
{
	parse_t*  parse = dfa->priv;// 获取解析器
	dfa_data_t*   d     = parse->dfa_data;// 获取 DFA 数据
	stack_t*  s     = d->module_datas[dfa_module_for.index];// 获取 for 模块的数据栈

	// 如果栈存在，释放栈
	if (s) {
		stack_free(s);// 释放栈内存
		s = NULL;
		d->module_datas[dfa_module_for.index] = NULL;// 清空栈指针
	}

	return DFA_OK;// 返回成功标志
}

// _dfa_init_syntax_for 函数初始化 for 循环的语法结构
static int _dfa_init_syntax_for(dfa_t* dfa)
{
	// 获取相关的模块节点
	DFA_GET_MODULE_NODE(dfa, for,   semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, for,   comma,     comma);
	DFA_GET_MODULE_NODE(dfa, for,   lp,        lp);
	DFA_GET_MODULE_NODE(dfa, for,   lp_stat,   lp_stat);
	DFA_GET_MODULE_NODE(dfa, for,   rp,        rp);
	DFA_GET_MODULE_NODE(dfa, for,   _for,    _for);
	DFA_GET_MODULE_NODE(dfa, for,   end,       end);

	DFA_GET_MODULE_NODE(dfa, expr,  entry,     expr);
	DFA_GET_MODULE_NODE(dfa, block, entry,     block);

	// 定义 for 循环的开始部分
	vector_add(dfa->syntaxes,  _for);

	// 定义 for 循环的条件表达式部分
	dfa_node_add_child(_for,      lp);
	dfa_node_add_child(lp,        semicolon);
	dfa_node_add_child(semicolon, semicolon);
	dfa_node_add_child(semicolon, rp);

	dfa_node_add_child(lp,        expr);
	dfa_node_add_child(expr,      semicolon);
	dfa_node_add_child(semicolon, expr);
	dfa_node_add_child(expr,      rp);

	// 定义 for 循环体
	dfa_node_add_child(rp,     block);

	return 0;
}

// for 循环 DFA 模块结构
dfa_module_t dfa_module_for =
{
	.name        = "for",// 模块名称

	.init_module = _dfa_init_module_for, // 初始化模块函数
	.init_syntax = _dfa_init_syntax_for,// 初始化语法函数

	.fini_module = _dfa_fini_module_for,// 清理模块函数
};
