#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_function;

// dfa_fun_data_t 结构体，用于存储函数模块的数据，特别是父块信息
typedef struct {

	block_t*     parent_block;// 当前函数所属的父块（作用域）

} dfa_fun_data_t;

// _function_add_function：处理函数定义的过程
// 参数：dfa_t* dfa - DFA 结构体，dfa_data_t* d - DFA 数据结构
int _function_add_function(dfa_t* dfa, dfa_data_t* d)
{
	// 确保当前标识符的数量至少为 2，表示函数有返回类型和函数名
	if (d->current_identities->size < 2) {
		loge("d->current_identities->size: %d\n", d->current_identities->size);
		return DFA_ERROR;
	}

	parse_t*    parse = dfa->priv;// 获取解析器
	ast_t*      ast   = parse->ast;// 获取抽象语法树
	dfa_identity_t* id    = stack_pop(d->current_identities);// 弹出一个标识符（函数返回类型）
	dfa_fun_data_t* fd    = d->module_datas[dfa_module_function.index];// 获取函数模块的数据结构

	function_t* f;// 定义一个函数指针
	variable_t* v;// 定义一个变量指针
	block_t*    b;// 定义一个代码块指针

	// 如果没有找到标识符，或标识符为空，返回错误
	if (!id || !id->identity) {
		loge("function identity not found\n");// 错误：函数标识符未找到
		return DFA_ERROR;
	}

	// 查找函数所在的代码块（如果有的话）
	b = ast->current_block;
	while (b) {
		if (b->node.type >= STRUCT)// 如果找到了结构体，停止查找
			break;
		b = (block_t*)b->node.parent;// 向父块回溯
	}

	// 为函数分配内存，并初始化函数
	f = function_alloc(id->identity);

	// 分配失败，返回错误
	if (!f)
		return DFA_ERROR;
	
	// 设置函数是否为成员函数
	f->member_flag = !!b;

	// 释放标识符内存
	free(id);
	id = NULL;

	// 输出函数的相关信息
	logi("function: %s,line:%d, member_flag: %d\n", f->node.w->text->data, f->node.w->line, f->member_flag);

	// 标记函数是否为 void 类型
	int void_flag = 0;

	// 遍历当前标识符栈，处理函数的返回类型和修饰符
	while (d->current_identities->size > 0) {

		// 弹出返回值类型的标识符
		id = stack_pop(d->current_identities);

		// 如果返回值类型不存在，返回错误
		if (!id || !id->type || !id->type_w) {
			loge("function return value type NOT found\n");
			return DFA_ERROR;
		}

		// 如果返回值类型是 void 且没有指针，设置 void_flag 标志
		if (VAR_VOID == id->type->type && 0 == id->nb_pointers)
			void_flag = 1;

		// 处理函数的修饰符
		f->extern_flag |= id->extern_flag;
		f->static_flag |= id->static_flag;
		f->inline_flag |= id->inline_flag;

		// extern 和 static 或 inline 不可以同时存在
		if (f->extern_flag && (f->static_flag || f->inline_flag)) {
			loge("'extern' function can't be 'static' or 'inline'\n");
			return DFA_ERROR;
		}
 		// 根据标识符类型分配返回值变量，并将其添加到返回值列表中
		v  = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, NULL);
		free(id);// 释放标识符
		id = NULL;

		// 如果分配失败，释放函数并返回错误
		if (!v) {
			function_free(f);
			return DFA_ERROR;
		}
		// 将返回值变量添加到函数的返回值列表中
		if (vector_add(f->rets, v) < 0) {
			variable_free(v);
			function_free(f);
			return DFA_ERROR;
		}
	}
    // 检查函数的返回值数量是否符合要求
	assert(f->rets->size > 0);

	// 如果是 void 类型函数，返回值必须为 0
	if (void_flag && 1 != f->rets->size) {
		loge("void function must have no other return value\n");
		return DFA_ERROR;
	}

	f->void_flag = void_flag;// 设置函数的 void 标志

	// 返回值数量不能超过 4
	if (f->rets->size > 4) {
		loge("function return values must NOT more than 4!\n");
		return DFA_ERROR;
	}

	// 反转返回值列表（这通常是为了处理参数的顺序）
	int i;
	int j;
	for (i = 0; i < f->rets->size / 2;  i++) {
		j  =        f->rets->size - 1 - i;

		XCHG(f->rets->data[i], f->rets->data[j]);// 交换两个返回值
	}

	// 将函数加入当前作用域
	scope_push_function(ast->current_block->scope, f);

	// 将函数节点添加到当前代码块
	node_add_child((node_t*)ast->current_block, (node_t*)f);

	// 更新父块信息
	fd ->parent_block  = ast->current_block;
	ast->current_block = (block_t*)f;

	// 更新当前函数
	d->current_function = f;

	// 返回下一个词
	return DFA_NEXT_WORD;
}

// _function_add_arg：处理函数参数的添加
// 参数：dfa_t* dfa - DFA 结构体，dfa_data_t* d - DFA 数据结构
int _function_add_arg(dfa_t* dfa, dfa_data_t* d)
{
	dfa_identity_t* t = NULL;// 参数类型
	dfa_identity_t* v = NULL;// 参数变量

	// 根据当前标识符栈的大小，分别处理类型和变量
	switch (d->current_identities->size) {
		case 0:
			break;
		case 1:
			t = stack_pop(d->current_identities);
			assert(t && t->type);// 确保类型存在
			break;
		case 2:
			v = stack_pop(d->current_identities);
			t = stack_pop(d->current_identities);
			assert(t && t->type);// 确保类型存在
			assert(v && v->identity);// 确保变量标识符存在
			break;
		default:
			loge("\n");
			return DFA_ERROR;// 错误：标识符数量不匹配
			break;
	};

	// 如果有类型，继续处理
	if (t && t->type) {
		variable_t* arg = NULL;// 函数参数
		lex_word_t* w   = NULL;

		// 如果有变量标识符，获取其词法信息
		if (v && v->identity)
			w = v->identity;

		// 如果类型是 void 且没有指针，返回错误
		if (VAR_VOID == t->type->type && 0 == t->nb_pointers) {
			loge("\n");
			return DFA_ERROR;
		}

		// 如果没有当前变量，分配新的变量并将其加入作用域
		if (!d->current_var) {
			arg = VAR_ALLOC_BY_TYPE(w, t->type, t->const_flag, t->nb_pointers, t->func_ptr);
			if (!arg)
				return DFA_ERROR;

			scope_push_var(d->current_function->scope, arg);
		} else {
			// 如果有当前变量，处理该变量
			arg = d->current_var;

			if (arg->nb_dimentions > 0) {
				arg->nb_pointers += arg->nb_dimentions;
				arg->nb_dimentions = 0;
			}

			if (arg->dimentions) {
				free(arg->dimentions);// 释放维度信息
				arg->dimentions = NULL;
			}

			arg->const_literal_flag = 0;// 清除常量标记

			d->current_var = NULL;
		}

		// 输出调试信息
		logi("d->argc: %d, arg->nb_pointers: %d, arg->nb_dimentions: %d\n",
				d->argc, arg->nb_pointers, arg->nb_dimentions);

		// 将参数添加到函数参数列表中
		vector_add(d->current_function->argv, arg);

		// 更新参数引用次数，并设置相关标志
		arg->refs++;
		arg->arg_flag   = 1;
		arg->local_flag = 1;

		if (v)
			free(v); // 释放变量标识符
		free(t);// 释放类型标识符

		d->argc++;// 增加参数计数
	}
	// 返回下一个词
	return DFA_NEXT_WORD;
}

// _function_action_vargs：处理函数的可变参数
static int _function_action_vargs(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	d->current_function->vargs_flag = 1;// 设置当前函数的可变参数标志

	return DFA_NEXT_WORD;// 返回下一个词
}

// _function_action_comma：处理函数参数列表中的逗号分隔符
static int _function_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	// 调用 _function_add_arg 函数添加一个参数
	if (_function_add_arg(dfa, d) < 0) {
		loge("function add arg failed\n");// 如果添加参数失败，返回错误
		return DFA_ERROR;
	}
	// 推送钩子用于处理逗号
	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_comma"), DFA_HOOK_PRE);

	return DFA_NEXT_WORD;// 返回下一个词
}

// _function_action_lp：处理函数定义中的左括号，开始函数解析
static int _function_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	assert(!d->current_node);// 确保当前节点为空

	d->current_var = NULL;// 清除当前变量

	// 调用 _function_add_function 来添加函数
	if (_function_add_function(dfa, d) < 0)
		return DFA_ERROR;

	// 推送钩子用于处理右括号和逗号
	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_rp"),    DFA_HOOK_PRE);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_comma"), DFA_HOOK_PRE);

	d->argc = 0;// 清除参数计数器
	d->nb_lps++;// 增加左括号计数

	// 返回下一个词
	return DFA_NEXT_WORD;
}

// _function_action_rp：处理函数定义中的右括号，完成函数定义的解析
static int _function_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];
	function_t*  f     = d->current_function;
	function_t*  fprev = NULL;// 用于存储已存在的同名函数

	d->nb_rps++;// 增加右括号计数

	// 如果右括号数量小于左括号数量，继续等待
	if (d->nb_rps < d->nb_lps) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "function_rp"), DFA_HOOK_PRE);
		return DFA_NEXT_WORD;
	}

	// 添加一个参数（如果有）
	if (_function_add_arg(dfa, d) < 0) {
		loge("function add arg failed\n");
		return DFA_ERROR;
	}

	// 从父块中删除当前函数，并进行处理
	list_del(&f->list);
	node_del_child((node_t*)fd->parent_block, (node_t*)f);

	// 如果当前函数是类的成员函数，进行更多的检查
	if (fd->parent_block->node.type >= STRUCT) {

		type_t* t = (type_t*)fd->parent_block;

		// 如果不是类成员函数，返回错误
		if (!t->node.class_flag) {
			loge("only class has member function\n");
			return DFA_ERROR;
		}

		assert(t->scope);// 确保作用域存在

		// 如果是构造函数或析构函数，处理重载逻辑
		if (!strcmp(f->node.w->text->data, "__init")) {

			fprev = scope_find_same_function(t->scope, f);

		} else if (!strcmp(f->node.w->text->data, "__release")) {

			fprev = scope_find_function(t->scope, f->node.w->text->data);

			if (fprev && !function_same(fprev, f)) {
				loge("function '%s' can't be overloaded, repeated declare first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line);
				return DFA_ERROR;
			}
		} else {
			loge("class member function must be '__init()' or '__release()', file: %s, line: %d\n", f->node.w->file->data, f->node.w->line);
			return DFA_ERROR;
		}
	} else {
		// 如果是普通函数，检查是否符合定义规则
		block_t* b = fd->parent_block;

		// 检查函数定义是否在文件、全局或类中
		if (!b->node.root_flag && !b->node.file_flag) {
			loge("function should be defined in file, global, or class\n");
			return DFA_ERROR;
		}

		assert(b->scope);// 确保作用域存在

		// 如果是静态函数，查找同名函数
		if (f->static_flag)
			fprev = scope_find_function(b->scope, f->node.w->text->data);
		else {
			int ret = ast_find_global_function(&fprev, parse->ast, f->node.w->text->data);
			if (ret < 0)
				return ret;
		}

		// 检查是否存在同名函数并处理重定义的错误
		if (fprev && !function_same(fprev, f)) {

			loge("repeated declare function '%s', first in line: %d, second in line: %d, function overloading only can do in class\n",
					f->node.w->text->data, fprev->node.w->line, f->node.w->line);
			return DFA_ERROR;
		}
	}

	// 如果找到了已定义的同名函数，并且该函数尚未定义，进行函数重载处理
	if (fprev) {
		if (!fprev->node.define_flag) {
			int i;
			variable_t* v0;
			variable_t* v1;

			// 交换返回值列表中的变量（重载函数时需要调整）
			for (i = 0; i < fprev->argv->size; i++) {
				v0 =        fprev->argv->data[i];
				v1 =        f    ->argv->data[i];

				if (v1->w)
					XCHG(v0->w, v1->w);
			}

			// 释放当前函数，使用之前定义的函数
			function_free(f);
			d->current_function = fprev;

		} else {
			// 如果函数已经定义，则进行错误提示，并重新推回词法单元
			lex_word_t* w = dfa->ops->pop_word(dfa);

			if (LEX_WORD_SEMICOLON != w->type) {

				loge("repeated define function '%s', first in line: %d, second in line: %d\n",
						f->node.w->text->data, fprev->node.w->line, f->node.w->line); 

				dfa->ops->push_word(dfa, w);// 将词法单元推回栈中
				return DFA_ERROR;
			}
			// 推回词法单元
			dfa->ops->push_word(dfa, w);
		}
	} else {
		// 如果没有找到同名函数，则将当前函数加入作用域
		scope_push_function(fd->parent_block->scope, f);
		// 将函数节点添加到父块
		node_add_child((node_t*)fd->parent_block, (node_t*)f);
	}

	// 推送钩子，结束函数的定义
	DFA_PUSH_HOOK(dfa_find_node(dfa, "function_end"), DFA_HOOK_END);
	// 更新当前代码块为当前函数
	parse->ast->current_block = (block_t*)d->current_function;
	// 返回下一个词
	return DFA_NEXT_WORD;
}

// _function_action_end：处理函数定义的结束，完成函数的解析
static int _function_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取当前的解析上下文
	parse_t*     parse = dfa->priv;
	
	// 获取 DFA 数据
	dfa_data_t*      d     = data;

	// 获取当前词
	lex_word_t*  w     = words->data[words->size - 1];

	// 获取函数模块的数据
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	// 恢复到父块
	parse->ast->current_block = (block_t*)(fd->parent_block);

	// 如果当前函数的节点数量大于 0，则标记函数定义完成
	if (d->current_function->node.nb_nodes > 0)
		d->current_function->node.define_flag = 1;

	// 清理函数模块的数据
	fd->parent_block = NULL;

	d->current_function = NULL;// 清除当前函数
	d->argc   = 0;// 清除参数计数器
	d->nb_lps = 0;// 清除左括号计数
	d->nb_rps = 0;// 清除右括号计数

	return DFA_OK; // 返回 DFA_OK，表示操作成功
}

// _dfa_init_module_function：初始化函数模块，定义函数的语法处理节点
static int _dfa_init_module_function(dfa_t* dfa)
{
	// 定义函数模块中的处理节点
	DFA_MODULE_NODE(dfa, function, comma,  dfa_is_comma, _function_action_comma);
	DFA_MODULE_NODE(dfa, function, vargs,  dfa_is_vargs, _function_action_vargs);
	DFA_MODULE_NODE(dfa, function, end,    dfa_is_entry, _function_action_end);

	DFA_MODULE_NODE(dfa, function, lp,     dfa_is_lp,    _function_action_lp);
	DFA_MODULE_NODE(dfa, function, rp,     dfa_is_rp,    _function_action_rp);
	
	// 获取当前的解析上下文
	parse_t*     parse = dfa->priv;
	
	// 获取 DFA 数据
	dfa_data_t*      d     = parse->dfa_data;
	
	// 获取函数模块的数据
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	// 确保函数模块数据未被初始化
	assert(!fd);

	// 分配内存为函数模块数据
	fd = calloc(1, sizeof(dfa_fun_data_t));
	if (!fd) {
		loge("\n");
		return DFA_ERROR;// 如果分配失败，返回错误
	}

	// 将模块数据存入 DFA 数据中
	d->module_datas[dfa_module_function.index] = fd;

	// 返回 DFA_OK，表示初始化成功
	return DFA_OK;
}

// _dfa_fini_module_function：清理函数模块，释放资源
static int _dfa_fini_module_function(dfa_t* dfa)
{
	// 获取当前的解析上下文
	parse_t*     parse = dfa->priv;
	
	// 获取 DFA 数据
	dfa_data_t*      d     = parse->dfa_data;
	
	// 获取函数模块的数据
	dfa_fun_data_t*  fd    = d->module_datas[dfa_module_function.index];

	if (fd) {
		free(fd);// 释放函数模块数据
		fd = NULL;// 设置为空
		d->module_datas[dfa_module_function.index] = NULL;// 清空模块数据指针
	}

	return DFA_OK;// 返回 DFA_OK，表示清理成功
}

// _dfa_init_syntax_function：初始化函数模块的语法，定义函数的语法结构
static int _dfa_init_syntax_function(dfa_t* dfa)
{
	// 获取并定义各个语法节点
	DFA_GET_MODULE_NODE(dfa, function, comma,     comma);
	DFA_GET_MODULE_NODE(dfa, function, vargs,     vargs);

	DFA_GET_MODULE_NODE(dfa, function, lp,        lp);
	DFA_GET_MODULE_NODE(dfa, function, rp,        rp);

	DFA_GET_MODULE_NODE(dfa, type,     _const,    _const);
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, type,     identity,  identity);
	DFA_GET_MODULE_NODE(dfa, block,    entry,     block);

	// 定义函数开始的语法结构
	dfa_node_add_child(identity,  lp);// identity 后跟左括号

	// 定义函数参数列表的语法结构
	dfa_node_add_child(lp,        _const);// 左括号后可能有常量类型
	dfa_node_add_child(lp,        base_type);// 左括号后可能有基本类型
	dfa_node_add_child(lp,        type_name);// 左括号后可能有类型名称
	dfa_node_add_child(lp,        rp);// 左括号后应该有右括号

	// 定义逗号的语法结构，允许分隔参数
	dfa_node_add_child(identity,  comma);
	dfa_node_add_child(identity,  rp);

	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(type_name, comma);
	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(type_name, rp);
	dfa_node_add_child(star,      comma);
	dfa_node_add_child(star,      rp);

	// 定义可变参数语法结构
	dfa_node_add_child(comma,     _const);
	dfa_node_add_child(comma,     base_type);
	dfa_node_add_child(comma,     type_name);
	dfa_node_add_child(comma,     vargs);
	dfa_node_add_child(vargs,     rp);// 可变参数后必须是右括号

	// 定义函数体的语法结构
	dfa_node_add_child(rp,        block);// 右括号后接函数体

	// 返回 0，表示初始化成功
	return 0;
}

// dfa_module_function：函数模块的定义，包含了函数的语法初始化、模块初始化和清理等
dfa_module_t dfa_module_function =
{
	.name        = "function",// 模块名称为 function

	.init_module = _dfa_init_module_function,// 初始化函数模块
	.init_syntax = _dfa_init_syntax_function,// 初始化函数语法

	.fini_module = _dfa_fini_module_function,// 清理函数模块
};
