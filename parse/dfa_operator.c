#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_operator;

// 模块临时数据：在解析 operator 重载时保存一些上下文
typedef struct {

	block_t*     parent_block;// 解析 operator 时的父 block（如果需要恢复或插入节点）

	lex_word_t*  word_op;// 保存当前解析到的 operator 符号词（如 '+'、'+'+' 等）

} dfa_op_data_t;

// 判断函数：词是 operator key（模块入口用到的判定器）
static int _operator_is_key(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	return LEX_WORD_KEY_OPERATOR == w->type;// 是否为关键字 "operator"（或类似关键字）
}

// 判断函数：词是一个具体的 operator token（加减乘除、比较等连续区间）
static int _operator_is_op(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	// 判断 token type 是否在 LEX_WORD_PLUS .. LEX_WORD_GE 的范围内（假定这些是 operator token 枚举）
	return (LEX_WORD_PLUS <= w->type && LEX_WORD_GE >= w->type);
}

/* 根据此前解析到的返回类型信息和保存的 operator token，创建一个 function_t 表示 operator 重载
 *
 * 主要流程：
 *  - 检查 opd->word_op 是否存在（operator 符号）
 *  - 分配 function 对象 f（function_alloc），f->node.w 中记录了操作符的词信息（用于日志/位置）
 *  - 从 d->current_identities 栈中弹出所有身份信息（通常这些身份先前由解析返回类型语法压入）
 *    每个 id 表示一个返回值类型信息：用 VAR_ALLOC_BY_TYPE 根据 id 信息创建 variable_t（代表返回值）
 *    把这些返回值 variable_t 加入 f->rets（返回值列表）。
 *  - 因为返回值被推入时次序可能和期望顺序相反，函数末尾把 f->rets 翻转回来（使用 XCHG 交换）
 *  - 清理 opd->word_op，并把 d->current_function 指向新分配的 f，供后续参数解析使用
 *
 * 返回值：DFA_NEXT_WORD 或 DFA_ERROR
 */
int _operator_add_operator(dfa_t* dfa, dfa_data_t* d)
{
	parse_t*     parse = dfa->priv;
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];
	dfa_identity_t*  id;
	function_t*  f;
	variable_t*  v;

	// operator token 必须先被保存到 opd->word_op 
	if (!opd->word_op) {
		loge("\n");
		return DFA_ERROR;
	}

	// 根据 operator token 分配一个表示 operator 的 function 结构
	f = function_alloc(opd->word_op);
	if (!f) {
		loge("operator overloading function alloc failed\n");
		return DFA_ERROR;
	}

	// 打印调试信息（operator 名称、行列）
	logi("operator: %s,line:%d,pos:%d\n", f->node.w->text->data, f->node.w->line, f->node.w->pos);

	// 把栈中的身份 (return type 描述) 弹出并转换为 function 的返回值变量 
	while (d->current_identities->size > 0) {

		// 取得最顶的 identity 信息 
		id = stack_pop(d->current_identities);

		// 必须至少包含类型信息（type/type_w 等）
		if (!id || !id->type || !id->type_w) {
			loge("function return value type NOT found\n");
			return DFA_ERROR;
		}

		// 保留 static/inline 标志（返回值相关标志累积到函数上）
		f->static_flag |= id->static_flag;
		f->inline_flag |= id->inline_flag;

		// 根据 id 中保存的 type/type_w/const/pointers 等信息分配一个 variable_t 表示返回值
		v  = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, NULL);
		
		// 已把 id 中的必要信息转换进入 v，释放 id 结构（注意：类型描述 id->type_w 的内存责任视实现而定）
		free(id);
		id = NULL;

		if (!v) {
			// 分配失败：释放已创建的 function 并返回错误
			function_free(f);
			return DFA_ERROR;
		}

		// 把返回值变量加入到函数的返回值列表 f->rets
		if (vector_add(f->rets, v) < 0) {
			// 加入失败：释放新分配的变量与函数，并返回错误 
			variable_free(v);
			function_free(f);
			return DFA_ERROR;
		}
	}

	/* 
	 * 解析返回值时弹出的顺序通常是后缀顺序或反向的（取决于解析时 push 的策略），
	 * 这里通过把 f->rets 翻转（reverse）来保证最终返回值的顺序符合源代码中的语义顺序。
	 */
	int i;
	int j;
	for (i = 0; i < f->rets->size / 2;  i++) {
		j  =        f->rets->size - 1 - i;
		// XCHG 宏交换两个 vector 元素指针
		XCHG(f->rets->data[i], f->rets->data[j]);
	}

	// 清空临时保存的 operator 词，表示已经被消费
	opd->word_op = NULL;

	// 把解析器当前函数上下文指向刚创建的函数，后续参数解析会填充 f->argv 等
	d->current_function = f;

	return DFA_NEXT_WORD;
}

/* 将解析到的参数信息（在 identity 栈中）转换为 function 的参数 variable，并把它加入函数参数列表
 *
 * 行为概要：
 * - 只允许最多 2 个 identity（arg count 超过 2 则视为错误）
 * - 当恰好有 2 个 identity 时，先 pop 出 id1(名字)、id0(type)，用 VAR_ALLOC_BY_TYPE 分配 arg
 * - 把 arg 加入 d->current_function->argv、函数作用域，并设置若干标志（refs, arg_flag, local_flag）
 * - 释放 id0/id1 并更新 d->argc 计数
 *
 * 返回：DFA_NEXT_WORD 或 DFA_ERROR
 */
int _operator_add_arg(dfa_t* dfa, dfa_data_t* d)
{
	variable_t* arg = NULL;

	/* 参数解析状态机限制：当前身份栈元素不应超过 2（此约束来自语法，2 表示 type+name） */
	if (d->current_identities->size > 2) {
		loge("operator parse args error\n");
		return DFA_ERROR;
	}

	/* 如果恰好有两个身份信息，则认为可以解析出一个完整的参数（type + name） */
	if (2 == d->current_identities->size) {

		/* 注意：先 pop 的通常是后出现的身份（名字），pop 顺序与 push 顺序有关 */
		dfa_identity_t* id1 = stack_pop(d->current_identities);// 期望为参数名（identity）
		dfa_identity_t* id0 = stack_pop(d->current_identities);// 期望为参数类型（type）

		/* 基本检查：id0 必须包含类型，id1 必须包含名字 */
		if (!id0 || !id0->type) {
			loge("operator parse arg type error\n");
			return DFA_ERROR;
		}

		if (!id1 || !id1->identity) {
			loge("operator parse arg name error\n");
			return DFA_ERROR;
		}

		/* 使用 id 信息分配参数变量（名字、类型、const、指针等级、函数指针信息） */
		arg = VAR_ALLOC_BY_TYPE(id1->identity, id0->type, id0->const_flag, id0->nb_pointers, id0->func_ptr);
		if (!arg) {
			loge("arg var alloc failed\n");
			return DFA_ERROR;
		}

		/* 把参数加入函数参数列表并注册到函数局部 scope 中 */
		vector_add(d->current_function->argv, arg);
		scope_push_var(d->current_function->scope, arg);

		/* 增加引用计数并设置参数标识（防止被过早释放） */
		arg->refs++;
		arg->arg_flag   = 1;/* 标记为形参 */
		arg->local_flag = 1;/* 标记为局部变量 */

		logi("d->current_function->argv->size: %d, %p\n", d->current_function->argv->size, d->current_function);

		/* 释放 id0/id1 临时结构（其内部的 type/name 已被复制/引用到 arg）*/
		free(id0);
		free(id1);

		d->argc++;/* 记录已解析的参数个数 */
	}

	return DFA_NEXT_WORD;
}

/* 当遇到参数列表中的逗号时调用（用于分隔参数）
 * 执行：把当前解析的参数通过 _operator_add_arg 固化，然后在钩子中再次注册 operator_comma 的 PRE 钩子
 */
static int _operator_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	dfa_data_t* d = data;

	if (_operator_add_arg(dfa, d) < 0) {
		loge("add arg failed\n");
		return DFA_ERROR;
	}

	/* 注册 PRE 钩子，确保后续解析中在逗号位置会触发 operator_comma 节点（框架具体行为依实现而定） */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_comma"), DFA_HOOK_PRE);

	return DFA_NEXT_WORD;
}

/* 当识别到具体的 operator token（如 '+'）时，把该词保存在模块上下文的 opd->word_op 中 */
static int _operator_action_op(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	/* 保存 operator 词，后续在 '(' 出现时会用到来建立 function 对象 */
	opd->word_op = words->data[words->size - 1];

	return DFA_NEXT_WORD;
}

/* 当识别到 '(' 时（参数列表开始），创建函数对象并准备解析参数
 *
 * 主要步骤：
 *  - 保证当前没有正在构造的 node（assert(!d->current_node)）
 *  - 调用 _operator_add_operator 创建并设置 d->current_function
 *  - 注册 operator_rp 和 operator_comma 的 PRE 钩子（以便 '(' 内部的解析能触发相应动作）
 *  - 重置 d->argc 并增加左括号计数 d->nb_lps
 */
static int _operator_action_lp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	/* 期望在顶层或特定上下文定义 operator，不能嵌入到另一个 current_node 下 */
	assert(!d->current_node);

	if (_operator_add_operator(dfa, d) < 0) {
		loge("add operator failed\n");
		return DFA_ERROR;
	}

	/* 注册两个钩子：在遇到右括号或逗号时预处理（框架会调用相应的节点动作） */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_rp"), DFA_HOOK_PRE);
	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_comma"), DFA_HOOK_PRE);

	d->argc = 0;/* 参数计数清零，准备开始解析参数 */
	d->nb_lps++;/* 记录左括号数量（用于匹配） */

	return DFA_NEXT_WORD;
}

/* operator 模块的动作：遇到 ')'（右括号）时处理
 *
 * 主要职责：
 *  - 跟踪右括号计数（d->nb_rps），处理嵌套括号
 *  - 在最后一个右括号闭合时，把最后一个参数加入函数参数列表（_operator_add_arg）
 *  - 验证当前上下文是 class/struct（只有 class 支持 operator overloading）
 *  - 检查是否已存在同名同参数的 operator（通过 scope_find_same_function）
 *      - 若存在且未定义（f->node.define_flag==0），把新定义（d->current_function）合并部分信息并用已有函数 f 替代（释放临时 current_function）
 *      - 若存在且已定义，则报重复定义错误（检查后面的分号）
 *      - 若不存在，则为内置 base operator 查找签名，设置 op_type，并把函数注册到 scope 并挂到 AST
 *  - 注册 operator_end 的 END 钩子，并把解析器 current_block 切换到当前函数（以便解析函数体）
 */
static int _operator_action_rp(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];
	function_t*  f     = NULL;

	/* 增加右括号计数（用于支持嵌套参数括号） */
	d->nb_rps++;

	/* 如果还没匹配完所有左括号（多层嵌套），继续注册 rp 钩子并返回 */
	if (d->nb_rps < d->nb_lps) {
		DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_rp"), DFA_HOOK_PRE);
		return DFA_NEXT_WORD;
	}

	/* 把最后一个参数（若存在）加入函数参数列表 */
	if (_operator_add_arg(dfa, d) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	/* 下面检查当前 AST 的挂载点是否是 class/struct 类型（只有 class 支持 operator 重载） */
	if (parse->ast->current_block->node.type >= STRUCT) {

		/* 这里把 current_block 强制转换为 type_t*。
		 * 说明 parse->ast->current_block 在 class context 下实际上是一个 type_t（class）结构。
		 * 假定 type_t 的起始布局与 block_t 兼容（含 node 字段），因此允许这样 cast。
		 */
		type_t* t = (type_t*)parse->ast->current_block;

		if (!t->node.class_flag) {
			/* 即使 node.type >= STRUCT，还要进一步检查是不是 class（有 class_flag） */
			loge("only class has operator overloading\n");
			return DFA_ERROR;
		}

		assert(t->scope);/* class 必须有 scope */

		/* 查找在 class scope 中是否存在与 d->current_function 签名相同的函数 */
		f = scope_find_same_function(t->scope, d->current_function);

	} else {
		/* 如果不是 class/struct 节点，则不允许 operator 重载 */
		loge("only class has operator overloading\n");
		return DFA_ERROR;
	}

	/* 如果找到了同名同签名的函数 f（可能是前向声明或已有定义） */
	if (f) {
		/* 若已有函数存在但尚未定义（只有声明）, 则把当前解析得到的信息合并到已存在函数 f */
		if (!f->node.define_flag) {
			int i;

			/* 把当前解析的参数词（d->current_function->argv 中的 v1->w）复制给已存在函数 f 中对应参数 v0->w */
			for (i = 0; i < f->argv->size; i++) {
				variable_t* v0 = f->argv->data[i]; /* 已存在函数的参数 */
				variable_t* v1 = d->current_function->argv->data[i];/* 新解析到的参数 */

				if (v1->w) {
					/* 如果已存在参数 v0->w 有词，先释放它（替换旧的词位置信息） */
					if (v0->w)
						lex_word_free(v0->w);
					/* 克隆 v1->w 保存到 v0->w（保留位置信息/词内容） */
					v0->w = lex_word_clone(v1->w);
				}
			}

			/* 释放新创建的临时函数对象（因为信息已合并到 f），并把当前函数指针设置为已存在函数 f */
			function_free(d->current_function);
			d->current_function = f;
		} else {
			/* 如果已存在函数且已被定义（重复定义），需要报错或至少检测语法后续 */
			lex_word_t* w = dfa->ops->pop_word(dfa);

			if (LEX_WORD_SEMICOLON != w->type) {
				/* 如果不是以分号结束，则视为语义/重复定义错误（报告首次和再次定义行号） */
				loge("repeated define operator '%s', first in line: %d, second in line: %d\n",
						f->node.w->text->data, f->node.w->line, d->current_function->node.w->line); 
				
				/* 推回读取的词并返回错误（让上层处理）*/
				dfa->ops->push_word(dfa, w);
				return DFA_ERROR;
			}
			/* 若是分号，函数选择不报错（其实这里仅检查 semicolon 然后把词推回）。
			 * 代码随后没有采用重复定义的新函数，不做合并，保持原来 f。
			 */
			dfa->ops->push_word(dfa, w);
		}
	} else {
		/* 如果 class 中没有同名函数，则视为对一个 base operator 的重载实现。
		 * 需要在基础 operator 列表中找到对应签名（operator 名称 + 参数数量）以确认合法性。
		 */
		operator_t* op = find_base_operator(d->current_function->node.w->text->data, d->current_function->argv->size);

		if (!op || !op->signature) {
			/* 未知或不支持的 operator 签名 */
			loge("operator: '%s', nb_operands: %d\n",
					d->current_function->node.w->text->data, d->current_function->argv->size);
			return DFA_ERROR;
		}

		/* 把查到的 base operator 类型写入当前函数 */
		d->current_function->op_type = op->type;

		/* 将这个 operator 函数注册到 class 的 scope 中，并把它作为 AST 子节点加入当前 block */
		scope_push_operator(parse->ast->current_block->scope, d->current_function);
		node_add_child((node_t*)parse->ast->current_block, (node_t*)d->current_function);
	}

	/* 注册 operator_end 的 END 钩子（表示 operator 定义体结束时会调用的回调） */
	DFA_PUSH_HOOK(dfa_find_node(dfa, "operator_end"), DFA_HOOK_END);

	/* 保存并切换 AST 的 current_block 到新函数（以便后续解析函数体）
	 * - 记住 parent_block，以便结束时恢复
	 */
	opd->parent_block = parse->ast->current_block;
	parse->ast->current_block = (block_t*)d->current_function;

	return DFA_NEXT_WORD;
}

/* operator 模块的 END 动作：operator 定义体结束时调用（通过之前注册的 hook 触发）
 *
 * 执行：
 *  - 恢复 parse->ast->current_block 到 opd->parent_block（原来的 class block）
 *  - 如果该函数节点含有子节点（node.nb_nodes>0），设置 define_flag（标记该函数已定义）
 *  - 清理模块上下文（parent_block、current_function、计数器等）
 */
static int _operator_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*     parse = dfa->priv;
	dfa_data_t*      d     = data;
	lex_word_t*  w     = words->data[words->size - 1];
	dfa_op_data_t*   opd   = d->module_datas[dfa_module_operator.index];

	/* 恢复 AST 的 current_block 到 operator 定义之前的 block（通常是 class node） */
	parse->ast->current_block  = (block_t*)(opd->parent_block);

	/* 如果当前函数（d->current_function）已经包含子节点，说明有函数体，设置 define_flag */
	if (d->current_function->node.nb_nodes > 0)
		d->current_function->node.define_flag = 1;

	opd->parent_block = NULL;

	/* 清理解析上下文：函数解析完成，重置相关计数和指针 */
	d->current_function = NULL;
	d->argc   = 0;
	d->nb_lps = 0;
	d->nb_rps = 0;

	logi("\n");
	return DFA_OK;
}

/* 模块初始化：注册 operator 模块内部的节点与动作函数，并为模块分配私有数据结构（dfa_op_data_t） */
static int _dfa_init_module_operator(dfa_t* dfa)
{
	/* 注册各个模块内节点：逗号、end、括号、操作符、关键字判断等 */
	DFA_MODULE_NODE(dfa, operator, comma,  dfa_is_comma, _operator_action_comma);
	DFA_MODULE_NODE(dfa, operator, end,    dfa_is_entry, _operator_action_end);

	DFA_MODULE_NODE(dfa, operator, lp,     dfa_is_lp,    _operator_action_lp);
	DFA_MODULE_NODE(dfa, operator, rp,     dfa_is_rp,    _operator_action_rp);

	DFA_MODULE_NODE(dfa, operator, ls,     dfa_is_ls,    _operator_action_op);
	DFA_MODULE_NODE(dfa, operator, rs,     dfa_is_rs,    NULL);

	DFA_MODULE_NODE(dfa, operator, key,    _operator_is_key, NULL);
	DFA_MODULE_NODE(dfa, operator, op,     _operator_is_op,  _operator_action_op);

	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = parse->dfa_data;
	dfa_op_data_t*  opd   = d->module_datas[dfa_module_operator.index];

	assert(!opd);

	/* 分配模块私有数据结构（用于暂存 operator 词与 parent_block 等） */
	opd = calloc(1, sizeof(dfa_op_data_t));
	if (!opd) {
		loge("\n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_operator.index] = opd;

	return DFA_OK;
}

/* 模块反初始化：释放在 init 时分配的私有数据结构 */
static int _dfa_fini_module_operator(dfa_t* dfa)
{
	parse_t*    parse = dfa->priv;
	dfa_data_t*     d     = parse->dfa_data;
	dfa_op_data_t*  opd    = d->module_datas[dfa_module_operator.index];

	if (opd) {
		free(opd);
		opd = NULL;
		d->module_datas[dfa_module_operator.index] = NULL;
	}

	return DFA_OK;
}

/* 语法初始化：把模块内节点与其他模块的节点组合成语法图（状态转移）
 *
 * 下面的 dfa_node_add_child 调用定义了 operator 定义的合法 token 序列与嵌套关系。
 * 例如：
 *   - base_type/type_name/star -> key ("operator" 关键字) 开始 operator 定义
 *   - key -> op（operator 符号）或 ls/rs（某些 operator 形式）
 *   - op -> lp 开始参数列表
 *   - lp -> base_type / type_name / rp （参数可以为空或由类型/名字构成）
 *   - 参数列表通过 comma 分隔，最后 rp 后接 block（函数体）
 */
static int _dfa_init_syntax_operator(dfa_t* dfa)
{
	/* 获取模块节点句柄 */
	DFA_GET_MODULE_NODE(dfa, operator, comma,     comma);
	DFA_GET_MODULE_NODE(dfa, operator, end,       end);

	DFA_GET_MODULE_NODE(dfa, operator, lp,        lp);
	DFA_GET_MODULE_NODE(dfa, operator, rp,        rp);

	DFA_GET_MODULE_NODE(dfa, operator, ls,        ls);
	DFA_GET_MODULE_NODE(dfa, operator, rs,        rs);

	DFA_GET_MODULE_NODE(dfa, operator, key,       key);
	DFA_GET_MODULE_NODE(dfa, operator, op,        op);

	/* 引用其它模块的节点（类型、标识符、block 等） */
	DFA_GET_MODULE_NODE(dfa, type,     base_type, base_type);
	DFA_GET_MODULE_NODE(dfa, identity, identity,  type_name);

	DFA_GET_MODULE_NODE(dfa, type,     star,      star);
	DFA_GET_MODULE_NODE(dfa, type,     identity,  identity);
	DFA_GET_MODULE_NODE(dfa, block,    entry,     block);

	// operator start: 指定哪些 token 可以引入 operator 定义
	dfa_node_add_child(base_type, key);
	dfa_node_add_child(type_name, key);
	dfa_node_add_child(star,      key);

	dfa_node_add_child(key,       op);

	dfa_node_add_child(key,       ls);
	dfa_node_add_child(ls,        rs);

	dfa_node_add_child(op,        lp);
	dfa_node_add_child(rs,        lp);
	
	// operator args: 参数解析允许的元素和分隔
	dfa_node_add_child(lp,        base_type);
	dfa_node_add_child(lp,        type_name);
	dfa_node_add_child(lp,        rp);

	dfa_node_add_child(identity,  comma);
	dfa_node_add_child(identity,  rp);

	dfa_node_add_child(base_type, comma);
	dfa_node_add_child(type_name, comma);
	dfa_node_add_child(base_type, rp);
	dfa_node_add_child(type_name, rp);
	dfa_node_add_child(star,      comma);
	dfa_node_add_child(star,      rp);

	dfa_node_add_child(comma,     base_type);
	dfa_node_add_child(comma,     type_name);


	// operator body: 参数列表闭合后可以跟一个 block（函数体）
	dfa_node_add_child(rp,        block);

	return 0;
}

/* 导出模块描述结构体 */
dfa_module_t dfa_module_operator =
{
	.name        = "operator",

	.init_module = _dfa_init_module_operator,
	.init_syntax = _dfa_init_syntax_operator,

	.fini_module = _dfa_fini_module_operator,
};
