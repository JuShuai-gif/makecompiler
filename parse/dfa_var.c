#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_var;

int _expr_multi_rets(expr_t* e);

/**
 * 检查类型定义中是否存在递归引用（防止结构体/类型自引用）。
 *
 * @param parent 父类型（当前要检查的起点类型）
 * @param child  子类型（被嵌套的类型）
 * @param w      词法单元信息（包含变量名、行号等）
 * @return DFA_OK 表示没有递归，DFA_ERROR 表示检测到递归定义
 */
int _check_recursive(type_t* parent, type_t* child, lex_word_t* w)
{
	// 如果子类型和父类型的type id一样，说明形成了直接递归引用
	if (child->type == parent->type) {
	
		loge("recursive define '%s' type member var '%s' in type '%s', line: %d\n",
				child->name->data, 	// 子类型名字
				w->text->data, 		// 成员变量名字
				parent->name->data, // 父类型名字
				w->line);			// 出现递归的源码行号

		return DFA_ERROR;
	}

	// 如果子类型有作用域（说明它本身是一个结构体、联合体或复杂类型）
	if (child->scope) {
		assert(child->type >= STRUCT);// 必须是结构体或以上的复合类型

		variable_t* v      = NULL;// 当前成员变量
		type_t*     type_v = NULL;// 成员变量对应的类型
		int i;

		// 遍历子类型的所有成员变量
		for (i = 0; i < child->scope->vars->size; i++) {

			v = child->scope->vars->data[i];

			// 如果成员时指针类型，或者成员时基本类型(非结构体)，跳过
			if (v->nb_pointers > 0 || v->type < STRUCT)
				continue;
			
			// 找到该成员变量的类型
			type_v = block_find_type_type((block_t*)child, v->type);
			assert(type_v);

			// 递归检查：如果该成员类型继续包含 parent，就形成递归
			if (_check_recursive(parent, type_v, v->w) < 0)
				return DFA_ERROR;
		}
	}

	// 没有检测到递归
	return DFA_OK;
}


/**
 * 将当前解析到的变量加入作用域。
 * 
 * 主要工作：
 *  - 检查变量是否重复定义
 *  - 确定变量作用域（全局 / 局部 / 成员）
 *  - 检查递归定义、函数指针、extern/static 等语义合法性
 *  - 分配并注册变量到当前作用域
 *
 * @param dfa   DFA 上下文
 * @param d     当前解析数据
 * @return 0 表示成功，DFA_ERROR 表示失败
 */
// 它负责在语法树作用域中添加一个新变量，并检查各种合法性(重复定义、作用域、递归定义、void、函数指针、extern/static修饰符等)
static int _var_add_var(dfa_t* dfa, dfa_data_t* d)
{
	parse_t*     parse = dfa->priv;
	// 取出当前正在处理的 identity（标识符，可能是变量名）
	dfa_identity_t*  id    = stack_top(d->current_identities);

	if (id && id->identity) {
		// 检查该变量在当前作用域是否已经定义过
		variable_t* v = scope_find_variable(parse->ast->current_block->scope, id->identity->text->data);
		if (v) {
			loge("repeated declare var '%s', line: %d\n", id->identity->text->data, id->identity->line);
			return DFA_ERROR;
		}

		// 确保至少有两个 identity，id0 表示变量的类型信息
		assert(d->current_identities->size >= 2);

		dfa_identity_t* id0 = d->current_identities->data[0];
		assert(id0 && id0->type);

		// 找到当前变量所属的 block，向上追溯直到 struct 或 function 层级
		block_t* b = parse->ast->current_block;
		while (b) {
			if (b->node.type >= STRUCT || FUNCTION == b->node.type)
				break;
			b = (block_t*)b->node.parent;
		}

		// 标记变量作用域：全局、局部、结构体成员
		uint32_t global_flag;
		uint32_t local_flag;
		uint32_t member_flag;

		if (!b) {// 没有父 block，说明是全局变量
			local_flag  = 0;
			global_flag = 1;
			member_flag = 0;

		} else if (FUNCTION == b->node.type) {// 函数内定义，说明是局部变量
			local_flag  = 1;
			global_flag = 0;
			member_flag = 0;

		} else if (b->node.type >= STRUCT) {// 结构体 / 类 / union 内成员变量
			local_flag  = 0;
			global_flag = 0;
			member_flag = 1;

			if (0 == id0->nb_pointers && id0->type->type >= STRUCT) {
				// if not pointer var, check if define recursive struct/union/class var
				// 如果定义的是一个非指针结构体成员，检查是否形成递归定义
				if (_check_recursive((type_t*)b, id0->type, id->identity) < 0) {

					loge("recursive define when define var '%s', line: %d\n",
							id->identity->text->data, id->identity->line);
					return DFA_ERROR;
				}
			}
		}

		// 检查函数指针是否合法：必须有 func_ptr 且是指针类型
		if (FUNCTION_PTR == id0->type->type
				&& (!id0->func_ptr || 0 == id0->nb_pointers)) {
			loge("invalid func ptr\n");
			return DFA_ERROR;
		}

		// 检查extern变量：必须是全局变量
		if (id0->extern_flag) {
			if (!global_flag) {
				loge("extern var must be global.\n");
				return DFA_ERROR;
			}

			// 同名 extern 已经生命过也不行
			variable_t* v = block_find_variable(parse->ast->current_block, id->identity->text->data);
			if (v) {
				loge("extern var already declared, line: %d\n", v->w->line);
				return DFA_ERROR;
			}
		}

		// 检查 void 类型：必须是指针类型（void*），不能直接定义 void 变量
		if (VAR_VOID == id0->type->type && 0 == id0->nb_pointers) {
			loge("void var must be a pointer, like void*\n");
			return DFA_ERROR;
		}

		// 分配变量对象
		v = VAR_ALLOC_BY_TYPE(id->identity, id0->type, id0->const_flag, id0->nb_pointers, id0->func_ptr);
		if (!v) {
			loge("alloc var failed\n");
			return DFA_ERROR;
		}

		// 设置作用域标记
		v->local_flag  = local_flag;
		v->global_flag = global_flag;
		v->member_flag = member_flag;

		// 设置修饰符
		v->static_flag = id0->static_flag;
		v->extern_flag = id0->extern_flag;

		// 打印调试信息
		logi("type: %d, nb_pointers: %d,nb_dimentions: %d, var: %s,line:%d,pos:%d, local: %d, global: %d, member: %d, extern: %d, static: %d\n\n",
				v->type, v->nb_pointers, v->nb_dimentions,
				v->w->text->data, v->w->line, v->w->pos,
				v->local_flag,  v->global_flag, v->member_flag,
				v->extern_flag, v->static_flag);
		
		// 将变量假如当前作用域
		scope_push_var(parse->ast->current_block->scope, v);

		// 更新 dfa_data_t 的当前变量信息
		d->current_var   = v;
		d->current_var_w = id->identity;

		// 清空 id0 的修饰符（避免影响后续变量定义）
		id0->nb_pointers = 0;
		id0->const_flag  = 0;
		id0->static_flag = 0;
		id0->extern_flag = 0;
		
		// 弹出栈顶 identity（变量名）
		stack_pop(d->current_identities);
		free(id);
		id = NULL;
	}

	return 0;
}

/**
 * 处理变量初始化的表达式
 *
 * @param dfa       DFA 上下文
 * @param d         解析过程中保存的上下文数据
 * @param words     当前词序列
 * @param semi_flag 分号标记（是否语句结束）
 * @return 0 成功，DFA_ERROR 失败
 */
static int _var_init_expr(dfa_t* dfa, dfa_data_t* d, vector_t* words, int semi_flag)
{
	parse_t*    parse = dfa->priv;
	variable_t* r     = NULL;

	// 取最后一个单词（通常是变量名所在行）
	lex_word_t* w     = words->data[words->size - 1];

	// 必须有初始化表达式
	if (!d->expr) {
		loge("\n");
		return DFA_ERROR;
	}

	assert(d->current_var);

	// 局部初始化表达式计数 -1（递归计数控制）
	d->expr_local_flag--;

	// 如果是全局变量，或者是 const 且不是指针/数组
    //  => 必须能在编译期求值
	if (d->current_var->global_flag
			|| (d->current_var->const_flag && 0 == d->current_var->nb_pointers + d->current_var->nb_dimentions)) {
		
		// 尝试计算表达式
		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("\n");

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		// 如果结果不是常量，且表达式不是赋值操作，则报错(比如数组大小必须是常量)
		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			loge("number of array should be constant, file: %s, line: %d\n", w->file->data, w->line);

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		// 用完释放表达式
		expr_free(d->expr);

	} else {
		// 局部变量初始化表达式，不能直接计算，挂到语法树上
		assert(d->expr->nb_nodes > 0);

		node_add_child((node_t*)parse->ast->current_block, (node_t*)d->expr);

		logd("d->expr->parent->type: %d\n", d->expr->parent->type);

		// 检查表达式返回值数量是否合法
		if (_expr_multi_rets(d->expr) < 0) {
			loge("\n");
			return DFA_ERROR;
		}

		// 标记分号（用于语句结束判断）
		d->expr->semi_flag = semi_flag;
	}

	// 表达式用完，置空
	d->expr = NULL;
	return 0;
}

/**
 * 处理 VLA（Variable Length Array，可变长数组）的运行时分配逻辑
 *
 * 原理：
 *  - 检查数组维度，生成求数组总大小的表达式
 *  - 如果维度是运行时确定的，构造相应表达式
 *  - 插入运行时检查：如果维度 <= 0，则调用 printf 输出错误信息
 *  - 构造 OP_VLA_ALLOC 节点，实现运行时分配
 */
static int _var_add_vla(ast_t* ast, variable_t* vla)
{
	function_t* f   = NULL;
	expr_t*     e   = NULL;
	expr_t*     e2  = NULL;
	node_t*     mul = NULL;

	// 查找 printf 函数，用于错误提示
	if (ast_find_function(&f, ast, "printf") < 0 || !f) {
		loge("printf() NOT found, which used to print error message when the variable length of array '%s' <= 0, file: %s, line: %d\n",
				vla->w->text->data, vla->w->file->data, vla->w->line);
		return DFA_ERROR;
	}

	int size = vla->data_size;// 基础元素大小
	int i;

	// 遍历数组的每一维
	for (i = 0; i < vla->nb_dimentions; i++) {

		if (vla->dimentions[i].num > 0) {
			// 固定大小，直接乘起来
			size *= vla->dimentions[i].num;
			continue;
		}

		if (0 == vla->dimentions[i].num) {
			// 非法：维度为 0
			loge("\n");

			expr_free(e);
			return DFA_ERROR;
		}

		if (!vla->dimentions[i].vla) {
			// 非法：维度表达式为空
			loge("\n");

			expr_free(e);
			return DFA_ERROR;
		}

		// 把维度表达式 clone 出来，挂到 e 中
		if (!e) {
			e = expr_clone(vla->dimentions[i].vla);
			if (!e)
				return -ENOMEM;
			continue;
		}

		e2 = expr_clone(vla->dimentions[i].vla);
		if (!e2) {
			expr_free(e);
			return -ENOMEM;
		}

		// 构造乘法节点(维度连乘)
		mul = node_alloc(vla->w, OP_MUL, NULL);
		if (!mul) {
			expr_free(e2);
			expr_free(e);
			return -ENOMEM;
		}

		int ret = expr_add_node(e, mul);
		if (ret < 0) {
			expr_free(mul);
			expr_free(e2);
			expr_free(e);
			return ret;
		}

		ret = expr_add_node(e, e2);
		if (ret < 0) {
			expr_free(e2);
			expr_free(e);
			return ret;
		}
	}

	assert(e);

	variable_t* v;
	type_t*     t;
	node_t*     node;

	// 如果 size > 1，再额外乘上 size（基础元素大小 * 所有维度）
	if (size > 1) {
		mul = node_alloc(vla->w, OP_MUL, NULL);
		if (!mul) {
			expr_free(e);
			return -ENOMEM;
		}

		int ret = expr_add_node(e, mul);
		if (ret < 0) {
			expr_free(mul);
			expr_free(e);
			return ret;
		}

		t = block_find_type_type(ast->current_block, VAR_INT);
		v = VAR_ALLOC_BY_TYPE(vla->w, t, 1, 0, NULL);
		if (!v) {
			expr_free(e);
			return DFA_ERROR;
		}
		v->data.i64    = size;
		v->global_flag = 1;
		v->const_literal_flag = 1;

		node = node_alloc(NULL, v->type, v);
		variable_free(v);
		v = NULL;
		if (!node) {
			expr_free(e);
			return DFA_ERROR;
		}

		ret = expr_add_node(e, node);
		if (ret < 0) {
			node_free(node);
			expr_free(e);
			return ret;
		}
	}

	node_t*  assign;
	node_t*  len;
	node_t*  alloc;

	// len = e
	assign = node_alloc(vla->w, OP_ASSIGN, NULL);
	if (!assign) {
		expr_free(e);
		return DFA_ERROR;
	}

	node_add_child(assign, e->nodes[0]);
	e->nodes[0]    = assign;
	assign->parent = e;

	node_add_child((node_t*)ast->current_block, e);
	e = NULL;

	// len 临时变量
	t = block_find_type_type(ast->current_block, VAR_INT);
	v = VAR_ALLOC_BY_TYPE(vla->w, t, 0, 0, NULL);
	if (!v)
		return DFA_ERROR;
	v->tmp_flag = 1;

	len = node_alloc(NULL, v->type, v);
	if (!len) {
		variable_free(v);
		return DFA_ERROR;
	}

	node_add_child(assign, len);
	XCHG(assign->nodes[0], assign->nodes[1]);

	// 构造 OP_VLA_ALLOC 节点
	len = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!len)
		return DFA_ERROR;

	alloc = node_alloc(vla->w, OP_VLA_ALLOC, NULL);
	if (!alloc) {
		node_free(len);
		return -ENOMEM;
	}

	// 添加 vla 节点
	node = node_alloc(NULL, vla->type, vla);
	if (!node) {
		node_free(len);
		node_free(alloc);
		return DFA_ERROR;
	}

	node_add_child(alloc, node);
	node_add_child(alloc, len);
	node = NULL;
	len  = NULL;

	// 添加 printf 节点(函数指针)
	t = block_find_type_type(ast->current_block, FUNCTION_PTR);
	v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f);
	if (!v) {
		node_free(alloc);
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;

	node = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!node) {
		node_free(alloc);
		return DFA_ERROR;
	}

	node_add_child(alloc, node);
	node = NULL;

	// 添加错误提示字符串
	char msg[1024];
	snprintf(msg, sizeof(msg) - 1, "\033[31merror:\033[0m variable length '%%d' of array '%s' not more than 0, file: %s, line: %d\n",
			vla->w->text->data, vla->w->file->data, vla->w->line);

	t = block_find_type_type(ast->current_block, VAR_CHAR);
	v = VAR_ALLOC_BY_TYPE(vla->w, t, 1, 1, NULL);
	if (!v) {
		node_free(alloc);
		return DFA_ERROR;
	}
	v->const_literal_flag = 1;
	v->global_flag = 1;

	v->data.s = string_cstr(msg);
	if (!v->data.s) {
		node_free(alloc);
		variable_free(v);
		return -ENOMEM;
	}

	node = node_alloc(NULL, v->type, v);
	variable_free(v);
	v = NULL;
	if (!node) {
		node_free(alloc);
		return DFA_ERROR;
	}
	node_add_child(alloc, node);
	node = NULL;

	// 把 alloc 节点挂到语法树
	node_add_child((node_t*)ast->current_block, alloc);
	return 0;
}

// 处理变量声明中的逗号 ',' 情况
static int _var_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;// 获取语法分析上下文
	dfa_data_t*   d     = data;// 获取语法分析上下文

	// 将当前变量加入符号表
	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	// 重置左右大括号计数
	d->nb_lss = 0;
	d->nb_rss = 0;

	// 如果当前变量存在
	if (d->current_var) {
		// 计算变量的大小（数组维度 × 类型大小）
		variable_size(d->current_var);

		// 如果是 VLA（变长数组）
		if (d->current_var->vla_flag) {
			// 添加额外 AST 节点处理 VLA 运行时分配
			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return DFA_ERROR;
		}
	}

	// 如果有初始化表达式，需要处理初始化
    // semi_flag=0 代表当前是逗号结尾，不是分号
	if (d->expr_local_flag > 0 && _var_init_expr(dfa, d, words, 0) < 0)
		return DFA_ERROR;

	// 继续 DFA 状态机（切换到下一个状态）
	return DFA_SWITCH_TO;
}

// 处理变量声明中的分号 ';' 情况
static int _var_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_identity_t*  id    = NULL;

	d->var_semicolon_flag = 0;// 标记重置


	// 将当前变量假如符号表
	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	// 重置大括号计数
	d->nb_lss = 0;
	d->nb_rss = 0;

	// 弹出当前 ID（标识符上下文）
	id = stack_pop(d->current_identities);
	assert(id && id->type);
	free(id);
	id = NULL;

	if (d->current_var) {
		variable_size(d->current_var);

		if (d->current_var->vla_flag) {

			if (_var_add_vla(parse->ast, d->current_var) < 0)
				return DFA_ERROR;
		}
	}

	// 如果有初始化表达式，则处理
	if (d->expr_local_flag > 0) {

		if (_var_init_expr(dfa, d, words, 1) < 0)// semi_flag=1
			return DFA_ERROR;

	} else if (d->expr) {// 否则释放表达式
		expr_free(d->expr);
		d->expr = NULL;
	}

	// 给语句末尾的 AST 节点打上分号标记
	node_t* b = (node_t*)parse->ast->current_block;

	if (b->nb_nodes > 0)
		b->nodes[b->nb_nodes - 1]->semi_flag = 1;

	return DFA_OK;
}

// 处理变量声明中的赋值 '=' 情况
static int _var_action_assign(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	// 将当前变量加入符号表
	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;
	d->nb_rss = 0;

	lex_word_t*  w = words->data[words->size - 1];

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	// extern 变量不能在这里初始化
	if (d->current_var->extern_flag) {
		loge("extern var '%s' can't be inited here, line: %d\n",
				d->current_var->w->text->data, w->line);
		return DFA_ERROR;
	}

	// VLA 变量需要额外处理
	if (d->current_var->vla_flag) {

		if (_var_add_vla(parse->ast, d->current_var) < 0)
			return DFA_ERROR;
	}

	// 如果是数组声明（带维度），交给后续处理
	if (d->current_var->nb_dimentions > 0) {
		logi("var array '%s' init, nb_dimentions: %d\n",
				d->current_var->w->text->data, d->current_var->nb_dimentions);
		return DFA_NEXT_WORD;
	}

	// 构建赋值表达式 AST
	operator_t* op = find_base_operator_by_type(OP_ASSIGN);
	node_t*     n0 = node_alloc(w, op->type, NULL);// 赋值操作符节点
	n0->op = op;

	// 构建变量节点（被赋值的左值）
	node_t*     n1 = node_alloc(d->current_var_w, d->current_var->type, d->current_var);
	
	// 构建表达式
	expr_t*     e  = expr_alloc();

	node_add_child(n0, n1);// 把变量作为子节点
	expr_add_node(e, n0);// 把赋值操作加入表达式

	d->expr         = e;// 保存表达式
	d->expr_local_flag++;// 标记进入表达式初始化状态

	// 如果当前还没有分号 hook，则注册
	if (!d->var_semicolon_flag) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "var_semicolon"), DFA_HOOK_POST);
		d->var_semicolon_flag = 1;
	}

	// 注册逗号 hook，允许 `int a = 1, b = 2;`
	DFA_PUSH_HOOK(dfa_find_node(dfa, "var_comma"), DFA_HOOK_POST);

	logd("d->expr: %p\n", d->expr);

	return DFA_NEXT_WORD;// 继续读取下一个 token
}

// 处理变量声明中出现 ':' 的情况
// 典型场景：struct 的位域定义，如 `struct { int a:3; }`
static int _var_action_colon(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	// 先把前面的变量加入符号表
	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	// 如果当前没有变量，说明语法有问题
	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	return DFA_NEXT_WORD;// 继续读取下一个词
}

// 处理位域宽度，即冒号后面的整数
// 如 `int a:3;` 中的 `3`
static int _var_action_bits(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	lex_word_t* w     = words->data[words->size - 1];// 当前词（位宽值）

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	// 位域必须是结构体成员
	if (!d->current_var->member_flag) {
		loge("bits var '%s' must be a member of struct, file: %s, line: %d\n",
				d->current_var->w->text->data, d->current_var->w->file->data, d->current_var->w->line);
		return DFA_ERROR;
	}

	// 保存位域大小
	d->current_var->bit_size = w->data.u32;
	return DFA_NEXT_WORD;
}

// 处理变量声明中的 '['
// 典型场景：数组声明，如 int a[10];
static int _var_action_ls(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*  parse = dfa->priv;
	dfa_data_t*   d     = data;

	// 先把前面的变量加入符号表
	if (_var_add_var(dfa, d) < 0) {
		loge("add var error\n");
		return DFA_ERROR;
	}

	d->nb_lss = 0;// 重置 '[' 计数
	d->nb_rss = 0;// 重置 ']' 计数

	if (!d->current_var) {
		loge("\n");
		return DFA_ERROR;
	}

	assert(!d->expr);

	// 给当前变量添加一个数组维度（大小未知，先占位 -1）
	variable_add_array_dimention(d->current_var, -1, NULL);
	d->current_var->const_literal_flag = 1;// 标记数组长度必须是常量字面值

	// 注册右括号 ']' 的回调
	DFA_PUSH_HOOK(dfa_find_node(dfa, "var_rs"), DFA_HOOK_POST);

	d->nb_lss++;// 计数左括号

	return DFA_NEXT_WORD;
}

// 处理变量声明中的 ']' —— 关闭数组维度
static int _var_action_rs(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	variable_t* r     = NULL;
	lex_word_t* w     = words->data[words->size - 1];

	d->nb_rss++;// 记录右括号数量

	logd("d->expr: %p\n", d->expr);

	// 如果 `[` 和 `]` 之间有表达式（数组长度）
	if (d->expr) {
		// 找到表达式根节点
		while(d->expr->parent)
			d->expr = d->expr->parent;

		// 计算表达式的值
		if (expr_calculate(parse->ast, d->expr, &r) < 0) {
			loge("expr_calculate\n");

			expr_free(d->expr);
			d->expr = NULL;
			return DFA_ERROR;
		}

		assert(d->current_var->dim_index < d->current_var->nb_dimentions);

		// 如果不是常量维度 -> 变长数组 (VLA)
		if (!variable_const(r) && OP_ASSIGN != d->expr->nodes[0]->type) {
			// VLA 只能在局部作用域
			if (!d->current_var->local_flag) {
				loge("variable length array '%s' must in local scope, file: %s, line: %d\n",
						d->current_var->w->text->data, w->file->data, w->line);

				variable_free(r);
				r = NULL;

				expr_free(d->expr);
				d->expr = NULL;
				return DFA_ERROR;
			}

			logw("define variable length array, file: %s, line: %d\n", w->file->data, w->line);

			// 保存表达式作为 VLA 的维度定义
			d->current_var->dimentions[d->current_var->dim_index].vla = d->expr;
			d->current_var->vla_flag = 1;
			d->expr = NULL;
		} else {
			// 普通数组，保存常量维度大小
			d->current_var->dimentions[d->current_var->dim_index].num = r->data.i;

			logi("dimentions: %d, size: %d\n",
					d->current_var->dim_index, d->current_var->dimentions[d->current_var->dim_index].num);

			expr_free(d->expr);
			d->expr = NULL;
		}

		// 进入下一个维度
		d->current_var->dim_index++;

		variable_free(r);
		r = NULL;
	}

	return DFA_SWITCH_TO;
}

// 初始化 DFA 模块：注册各个动作处理函数
static int _dfa_init_module_var(dfa_t* dfa)
{
	DFA_MODULE_NODE(dfa, var, comma,     dfa_is_comma,         _var_action_comma);
	DFA_MODULE_NODE(dfa, var, semicolon, dfa_is_semicolon,     _var_action_semicolon);

	DFA_MODULE_NODE(dfa, var, ls,        dfa_is_ls,            _var_action_ls);
	DFA_MODULE_NODE(dfa, var, rs,        dfa_is_rs,            _var_action_rs);

	DFA_MODULE_NODE(dfa, var, assign,    dfa_is_assign,        _var_action_assign);

	DFA_MODULE_NODE(dfa, var, colon,     dfa_is_colon,         _var_action_colon);
	DFA_MODULE_NODE(dfa, var, bits,      dfa_is_const_integer, _var_action_bits);

	return DFA_OK;
}

// 定义语法规则：连接 DFA 节点，描述合法的变量声明语法
static int _dfa_init_syntax_var(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, var,    comma,     comma);
	DFA_GET_MODULE_NODE(dfa, var,    semicolon, semicolon);

	DFA_GET_MODULE_NODE(dfa, var,    ls,        ls);
	DFA_GET_MODULE_NODE(dfa, var,    rs,        rs);
	DFA_GET_MODULE_NODE(dfa, var,    assign,    assign);

	DFA_GET_MODULE_NODE(dfa, var,    colon,     colon);
	DFA_GET_MODULE_NODE(dfa, var,    bits,      bits);

	DFA_GET_MODULE_NODE(dfa, type,   star,      star);
	DFA_GET_MODULE_NODE(dfa, type,   identity,  identity);

	DFA_GET_MODULE_NODE(dfa, expr,   entry,     expr);

	DFA_GET_MODULE_NODE(dfa, init_data, entry,  init_data);
	DFA_GET_MODULE_NODE(dfa, init_data, rb,     init_rb);


	dfa_node_add_child(identity,  comma);
	dfa_node_add_child(comma,     star);
	dfa_node_add_child(comma,     identity);

	// array var
	dfa_node_add_child(identity,  ls);
	dfa_node_add_child(ls,        rs);
	dfa_node_add_child(ls,        expr);
	dfa_node_add_child(expr,      rs);
	dfa_node_add_child(rs,        ls);
	dfa_node_add_child(rs,        comma);
	dfa_node_add_child(rs,        semicolon);

	// bits
	dfa_node_add_child(identity,  colon);
	dfa_node_add_child(colon,     bits);
	dfa_node_add_child(bits,      semicolon);

	// var init
	dfa_node_add_child(rs,        assign);
	dfa_node_add_child(identity,  assign);

	// normal var init
	dfa_node_add_child(assign,    expr);
	dfa_node_add_child(expr,      comma);
	dfa_node_add_child(expr,      semicolon);

	// struct or array init
	dfa_node_add_child(assign,    init_data);
	dfa_node_add_child(init_rb,   comma);
	dfa_node_add_child(init_rb,   semicolon);

	dfa_node_add_child(identity,  semicolon);
	return 0;
}

// 将本模块注册为 DFA 子模块 "var"
dfa_module_t dfa_module_var = 
{
	.name        = "var",// 模块名
	.init_module = _dfa_init_module_var,// 注册动作函数
	.init_syntax = _dfa_init_syntax_var,// 定义语法规则
};
