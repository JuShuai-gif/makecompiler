#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t  dfa_module_init_data;

// 前置函数声明(外部实现)
int array_init (ast_t* ast, lex_word_t* w, variable_t* var, vector_t* init_exprs);
int struct_init(ast_t* ast, lex_word_t* w, variable_t* var, vector_t* init_exprs);

int _expr_add_var(parse_t* parse, dfa_data_t* d);

// 模块保留的数据结构：记录一次数据初始化（array/struct 初始化）过程中的临时状态
typedef struct {
	lex_word_t*  assign;// 指向 '=' 的词（assign token），记录赋值位置（用于 error/report）
	vector_t*    init_exprs;// 保存所有待处理的初始化表达式（元素类型为 dfa_init_expr_t*）

	dfa_index_t*     current_index;// 当前正在处理的索引数组（多维数组初始化时使用）
	int              current_n;// current_index 数组的长度（维度数或索引数）
	int              current_dim;// 当前正在操作的维度索引（-1 表示未进入任何维度）

	int              nb_lbs;// 左方括号 '[' 的计数（用于跟踪嵌套或语法完整性）
	int              nb_rbs;// 右方括号 ']' 的计数
} init_module_data_t;


/* ---------------------------------------------------------
 * _do_data_init
 * 当完成一个变量的初始化表达式收集后，调用这个函数实际做初始化：
 *  - 对数组变量调用 array_init
 *  - 对结构体（或类似类型）调用 struct_init
 * 然后把 md->init_exprs 列表中的表达式要么计算（全局变量初始化时）要么
 * 插入到当前 block（非全局变量），并释放临时结构。
 *
 * 返回值：成功返回 >=0（DFA 层面通常以 DFA_OK 等），失败返回 <0
 * --------------------------------------------------------- */
static int _do_data_init(dfa_t* dfa, vector_t* words, dfa_data_t* d)
{
	// 从 dfa 获取 parse 上下文（包含 ast、lex 等）
	parse_t*        parse = dfa->priv;
	// 当前正在初始化的变量
	variable_t*     var   = d->current_var;
	// 最近的词（通常不是直接使用）
	lex_word_t*     w     = words->data[words->size - 1];
	// 本模块的临时数据
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];
	// 临时用于遍历 init_exprs
	dfa_init_expr_t*    ie;

	int ret = -1;
	int i   = 0;

	/* 根据变量类型选择不同的初始化实现：
	 * - 若是数组（nb_dimentions > 0），调用 array_init
	 * - 否则若是结构体或更高类别（type >= STRUCT），调用 struct_init
	 *
	 * 这两个函数负责把 md->init_exprs 的表达式解释为实际的内存初始化表
	 *（例如为数组设置初始元素，或为 struct 的字段设置值）
	 */
	if (d->current_var->nb_dimentions > 0)
		ret = array_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	else if (d->current_var->type >=  STRUCT)
		ret = struct_init(parse->ast, md->assign, d->current_var, md->init_exprs);

	if (ret < 0)
		goto error;// 若初始化失败，跳转到清理流程

	/* 成功调用 array_init/struct_init 后，需要处理 init_exprs 中每个表达式：
	 * - 对于全局变量：在编译时就需要计算常量值（expr_calculate），并释放表达式树
	 * - 对于局部变量：把表达式节点加入当前 block 的 AST（node_add_child），以在运行时生成赋值语句
	 *
	 * 注意：ie->expr 的所有权在此处被移交或释放，循环结束后 md->init_exprs 中的元素会被逐一 free
	 */
	for (i = 0; i < md->init_exprs->size; i++) {
		ie =        md->init_exprs->data[i];

		if (d->current_var->global_flag) {

			// 全局初始化：在编译期求值表达式（例如常量折叠、生成静态数据）
			ret = expr_calculate(parse->ast, ie->expr, NULL);
			if (ret < 0)
				goto error;

			// expr_calculate 成功后，表达式树不再需要，释放它 
			expr_free(ie->expr);
			ie->expr = NULL;

		} else {
			// 局部初始化：把表达式加入当前 AST block，让后续的代码生成使用该表达式 
			ret = node_add_child((node_t*)parse->ast->current_block, ie->expr);
			if (ret < 0)
				goto error;
			// 表达式已绑定到 AST，置空局部引用以避免重复释放 
			ie->expr = NULL;
		}

		// 释放 dfa_init_expr_t 结构本身（它是为临时存储 index/expr 分配的）
		free(ie);
		ie = NULL;
	}

error:
	/* 出错或正常结束后的统一清理流程（处理未处理完的 init_exprs）：
	 * 如果在循环中途发生错误，i 指示下一个待处理的索引 -> 清理剩余的表达式
	 */
	for (; i < md->init_exprs->size; i++) {
		ie =   md->init_exprs->data[i];

		expr_free(ie->expr);// 释放表达式树（若为 NULL 则安全）
		free(ie);// 释放结构
		ie = NULL;
	}

	// 清理并重置模块临时状态，避免悬挂指针/重复使用旧内存
	md->assign = NULL;

	// 释放 vector 本身
	vector_free(md->init_exprs);
	md->init_exprs = NULL;

	// 释放 current_index 数组（如果为空则安全）
	free(md->current_index);
	md->current_index = NULL;

	md->current_dim = -1;
	md->nb_lbs      = 0;
	md->nb_rbs      = 0;

	return ret;
}

/* ---------------------------------------------------------
 * _add_data_init_expr
 * 当解析到一个完整的初始化项（例如 array[1][2] = expr 中的 expr）时，
 * 用这个函数把 d->expr 封装成一个 dfa_init_expr_t 放入 md->init_exprs 列表。
 *
 * 这里只有把表达式从 d->expr 移交到新结构体，然后把 d->expr 置 NULL（表明所有权转移）。
 * 该函数为 dfa_init_expr_t 申请变长内存（末尾跟随 index 数组），并 memcpy 索引数据。
 * 返回 DFA_OK（成功）或负数（错误）。
 * --------------------------------------------------------- */
static int _add_data_init_expr(dfa_t* dfa, vector_t* words, dfa_data_t* d)
{
	init_module_data_t* md = d->module_datas[dfa_module_init_data.index];
	dfa_init_expr_t*    ie;

	// 断言：当前 d->expr 不应已被绑定到某个父节点（意味着它正是独立的表达式树）
	assert(!d->expr->parent);
	// current_dim 必须有效（说明我们处于某个维度的上下文中）
	assert(md->current_dim >= 0);

	/* 计算需要为 index 数组分配的字节数：
	 * md->current_n 表示 index 数目（也就是 dfa_init_expr_t 中 index 数组的长度）
	 */
	size_t N =  sizeof(dfa_index_t) * md->current_n;

	// 为 dfa_init_expr_t (+ 后续 index 数组) 申请内存（変长结构体模式）
	ie = malloc(sizeof(dfa_init_expr_t) + N);
	if (!ie)
		return -ENOMEM;

	// 转移表达式所有权：把 d->expr 置为 NULL，表达式由 ie 管理直到 later freed
	ie->expr = d->expr;
	d ->expr = NULL;
	ie->n    = md->current_n;// 记录 index 数量

	// 复制当前索引数组到新结构体的尾部（内联 index 区域）
	memcpy(ie->index, md->current_index, N);

	// 把新表达式对象加入 md->init_exprs 列表
	int ret = vector_add(md->init_exprs, ie);
	if (ret < 0)
		return ret;

	return DFA_OK;
}

/* ---------------------------------------------------------
 * _data_action_comma
 * 当解析到初始化列表中的逗号（','）时被调用。
 * 举例： int a[2] = { 1, 2, ... }  或者多维数组、结构体字段的多个初始化项
 *
 * 主要行为：
 *  - 如果 current_dim < 0，表示逗号不在索引上下文中，返回 DFA_NEXT_SYNTAX（交给其它语法处理）
 *  - 如果存在 d->expr（刚解析完一个表达式），把它封装并加入 init_exprs（调用 _add_data_init_expr）
 *  - 更新 current_index[ current_dim ]：把当前维度的词（w）置 NULL 并把索引计数 i++（下移到下一个元素位置）
 *  - 注册一个后处理钩子 init_data_comma（DFA_PUSH_HOOK），并返回 DFA_SWITCH_TO（切换解析子状态）
 * --------------------------------------------------------- */
static int _data_action_comma(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	// 若未处于有效维度上下文，说明逗号不是初始化项目分隔符，应交给其他语法处理
	if (md->current_dim < 0) {
		logi("md->current_dim: %d\n", md->current_dim);
		return DFA_NEXT_SYNTAX;
	}

	// 如果刚好有一个表达式（例如: "..., expr, ..."），把该表达式加入 init_exprs
	if (d->expr) {
		if (_add_data_init_expr(dfa, words, d) < 0)
			return DFA_ERROR;
	}

	/* 更新当前维度的索引信息：
	 * - 将索引的词指针置 NULL（表明该位置不是用词指定的索引，而是使用递增计数）
	 * - 把索引计数器 i++（表示下一项）
	 *
	 * 这里假定 dfa_index_t 包含至少两个成员：lex_word_t* w; intptr_t i;
	 */
	md->current_index[md->current_dim].w = NULL;
	md->current_index[md->current_dim].i++;

	intptr_t i = md->current_index[md->current_dim].i;

	// 调试输出当前维度索引（红色高亮仅为视觉区分）
	logi("\033[31m md->current_dim[%d]: %ld\033[0m\n", md->current_dim, i);

	// 注册一个后处理钩子（init_data_comma），以便 DFA 框架在适当时机继续处理
	DFA_PUSH_HOOK(dfa_find_node(dfa, "init_data_comma"), DFA_HOOK_POST);

	// 返回 DFA_SWITCH_TO，通知 DFA 在处理完钩子后切换解析状态
	return DFA_SWITCH_TO;
}

/* ---------------------------------------------------------
 * _data_action_entry
 * 当检测到初始化赋值的入口（通常是遇到 '=' 赋值符号）时触发。
 * 这个函数完成模块级临时数据的初始化：
 *  - 记录 assign token、重置括号计数
 *  - 分配 init_exprs 列表
 *  - 初始化 current_dim/current_n 等用于后续索引收集的变量
 *  - 设置 d->expr_local_flag 表示接下来解析到的表达式属于本地生命周期（由模块管理）
 *  - 推入 init_data_comma 的 POST 钩子（以便后续逗号分隔项能被捕捉）
 * 返回 DFA_CONTINUE 表示继续当前语法流程
 * --------------------------------------------------------- */
static int _data_action_entry(dfa_t* dfa, vector_t* words, void* data)
{
	assert(words->size >= 2);

	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	
	// '=' 通常是倒数第二个词
	lex_word_t*     w     = words->data[words->size - 2];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	// 确认该词确实是赋值符号 '='（LEX_WORD_ASSIGN）
	assert(LEX_WORD_ASSIGN == w->type);

	// 记录 assign token 便于后续生成 initializer / error 报告
	md->assign = w;
	md->nb_lbs = 0;
	md->nb_rbs = 0;

	// 初始状态：尚无初始化表达式/索引数组
	assert(!md->init_exprs);
	assert(!md->current_index);

	// 分配一个保存 init 表达式的 vector 
	md->init_exprs = vector_alloc();
	if (!md->init_exprs)
		return -ENOMEM;

	// 初始化索引元信息：尚未进入任何维度
	md->current_dim = -1;
	md->current_n   = 0;

	// 标记即将解析的 expr 为局部（模块负责管理内存/生命周期），避免外部误释放
	d->expr_local_flag = 1;

	// 注册一个钩子，让 DFA 在遇到逗号时能调用 _data_action_comma（POST）
	DFA_PUSH_HOOK(dfa_find_node(dfa, "init_data_comma"), DFA_HOOK_POST);

	return DFA_CONTINUE;
}

/* 
 * 当遇到 '['（左方括号）表示进入一个索引/维度上下文
 * 该动作负责：
 *  - 清除当前 d->expr（把解析器的 expr 置 NULL, 因为接下来会解析索引或子初始化）
 *  - 增加 current_dim（进入更深一层的维度）
 *  - 动态扩展 md->current_index 数组以容纳 new dimension
 *  - 重置从 current_dim 到 current_n-1 的 index 条目（把 w 置 NULL、i 置 0）
 * 返回 DFA_NEXT_WORD（继续读取下一个词）
 */
static int _data_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	// 进入一个新的子表达式/索引上下文：清除 d->expr，以便后续解析新的表达式
	d->expr = NULL;

	// 更新维度计数：进入下一层 current_dim
	md->current_dim++;
	md->nb_lbs++;// 记录左中括号数量（用于最终匹配判断）

	// 如果 current_dim 已经超出 current_index 数组的容量，realloc 扩展数组
	if (md->current_dim >= md->current_n) {

		// 扩容为 (current_dim + 1) 项
		void* p = realloc(md->current_index, sizeof(dfa_index_t) * (md->current_dim + 1));
		if (!p)
			return -ENOMEM;// 注意：realloc 失败后当前模块状态（如 current_dim）已被修改，调用方需处理
		md->current_index = p;
		md->current_n     = md->current_dim + 1;
	}

	// 把从 current_dim 到 current_n-1 的 index 清零（初始化新层及其后的层）
	int i;
	for (i = md->current_dim; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	return DFA_NEXT_WORD;
}

/*
 * 处理“成员访问”的语法片段，比如结构体初始化中使用 member 名：
 * 当遇到标识符并且期望 member 时，检查当前类型是否有该成员，并把该词记录到 current_index[current_dim].w
 * 返回 DFA_NEXT_WORD 继续解析。
 */
static int _data_action_member(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	// 前识别到的标识符（成员名）
	lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];
	variable_t*     v;
	type_t*         t;

	// 必须处于有效的维度上下文，否则 init 语法不对
	if (md->current_dim >= md->current_n) {
		loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 必须有正在初始化的变量
	assert(d->current_var);

	// 根据当前变量类型查找 type（例如 struct type），并获取其 scope（成员表）
	t = NULL;
	ast_find_type_type(&t, parse->ast, d->current_var->type);
	if (!t->scope) {
		// 如果 base type 没有成员表，说明这里不应该出现成员访问
		loge("base type '%s' has no member var '%s', file: %s, line: %d\n",
				t->name->data, w->text->data, w->file->data, w->line);
		return DFA_ERROR;
	}

	// 在类型的 scope 中查找成员变量 v（名字为 w->text）
	v = scope_find_variable(t->scope, w->text->data);
	if (!v) {
		loge("member var '%s' NOT found in struct '%s', file: %s, line: %d\n",
				w->text->data, t->name->data, w->file->data, w->line);
		return DFA_ERROR;
	}

	// 成员存在：把该词记录为当前维度的成员索引（后续会用到）
	md->current_index[md->current_dim].w = w;

	return DFA_NEXT_WORD;
}

// 当期望成员名但遇到错误 token 时的错误处理（打印友好错误信息）
static int _error_action_member(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = words->data[words->size - 1];

	loge("member '%s' should be an var in struct, file: %s, line: %d\n",
			w->text->data, w->file->data, w->line);
	return DFA_ERROR;
}

// 当期望整数字面量索引但遇到其它时（例如 a[foo] 中 foo 不是整型字面量）的错误处理
static int _error_action_index(dfa_t* dfa, vector_t* words, void* data)
{
	lex_word_t* w = words->data[words->size - 1];

	loge("array index '%s' should be an integer, file: %s, line: %d\n",
			w->text->data, w->file->data, w->line);
	return DFA_ERROR;
}

/*
 * 当解析到数组索引的整数字面量（const integer）时调用：
 * 把该整数写入 current_index[current_dim].i
 */
static int _data_action_index(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	lex_word_t*     w     = words->data[words->size - 1];
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	if (md->current_dim >= md->current_n) {
		loge("init data not right, file: %s, line: %d\n", w->file->data, w->line);
		return DFA_ERROR;
	}

	// 直接把词里的无符号 64 位值记录到当前维度索引的 i 字段
	md->current_index[md->current_dim].i = w->data.u64;

	return DFA_NEXT_WORD;
}

/*
 * 当遇到 ']'（右方括号）时：
 *  - 如果当前有 d->expr（即当前维度内解析出了一个表达式），先把表达式封装为 init_expr 并加入 md->init_exprs
 *    注意：如果表达式引用了刚刚解析出来的身份标识（id->identity），则调用 _expr_add_var 处理（可能用于把标识替换为实际变量/索引）
 *  - 递增 nb_rbs，降低 current_dim（退出当前维度）
 *  - 清理 current_index 在被闭合维度之后的条目
 *  - 如果 nb_rbs == nb_lbs（所有 '[' 都有对应 ']'），说明整个初始化表达式完成：
 *      * 关闭 expr_local_flag（表示模块对 expr 生命周期的控制结束）
 *      * 调用 _do_data_init 进行最终数据初始化（array_init/struct_init + 把初始化表达式加入 AST / 计算常量）
 *      * 删除名为 "init_data_comma" 的 DFA POST 钩子（不再需要逗号钩子）
 */
static int _data_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	// 如果有表达式正在构造（例如 ... , expr ]），把它封装并加入 init_exprs
	if (d->expr) {
		dfa_identity_t* id = stack_top(d->current_identities);

		// 如果当前表达式是一个身份标识（或与身份有关），先做必要的变量表达式转换
		if (id && id->identity) {
			if (_expr_add_var(parse, d) < 0)
				return DFA_ERROR;
		}

		// 把 d->expr 转入一个 dfa_init_expr_t 并加入 md->init_exprs
		if (_add_data_init_expr(dfa, words, d) < 0)
			return DFA_ERROR;
	}

	// 记录右括号计数并退出当前维度
	md->nb_rbs++;
	md->current_dim--;

	// 清理已退出维度之后的索引条目（把 w/i 清零）
	int i;
	for (i = md->current_dim + 1; i < md->current_n; i++) {

		md->current_index[i].w = NULL;
		md->current_index[i].i = 0;
	}

	// 如果所有左括号都被匹配（整个初始化部分闭合完毕）
	if (md->nb_rbs == md->nb_lbs) {
		// 恢复 expr 生命周期管理：模块不再局部持有 expr 
		d->expr_local_flag = 0;

		// 调用完成初始化的处理（负责 array_init/struct_init + 把 init_exprs 的 expr 插入 AST / 计算）
		if (_do_data_init(dfa, words, d) < 0)
			return DFA_ERROR;

		// 删除 init_data_comma 的 POST 钩子（不再需要）
		dfa_del_hook_by_name(&(dfa->hooks[DFA_HOOK_POST]), "init_data_comma");
	}

	return DFA_NEXT_WORD;
}

/* ------------------------
 * 模块初始化：注册模块节点和分配模块数据结构
 * ------------------------ */
static int _dfa_init_module_init_data(dfa_t* dfa)
{
	// 注册关键节点以及其对应的识别函数和动作函数
	DFA_MODULE_NODE(dfa, init_data, entry,  dfa_is_lb,             _data_action_entry);
	DFA_MODULE_NODE(dfa, init_data, comma,  dfa_is_comma,          _data_action_comma);
	DFA_MODULE_NODE(dfa, init_data, lb,     dfa_is_lb,             _data_action_lb);
	DFA_MODULE_NODE(dfa, init_data, rb,     dfa_is_rb,             _data_action_rb);

	// 其它辅助节点（ls/rs 可能是左右方括号变体，dot 是成员访问符号 '.'）
	DFA_MODULE_NODE(dfa, init_data, ls,     dfa_is_ls,             dfa_action_next);
	DFA_MODULE_NODE(dfa, init_data, rs,     dfa_is_rs,             dfa_action_next);

	DFA_MODULE_NODE(dfa, init_data, dot,    dfa_is_dot,            dfa_action_next);
	DFA_MODULE_NODE(dfa, init_data, member, dfa_is_identity,       _data_action_member);
	DFA_MODULE_NODE(dfa, init_data, index,  dfa_is_const_integer,  _data_action_index);
	DFA_MODULE_NODE(dfa, init_data, assign, dfa_is_assign,         dfa_action_next);

	// 专门的错误分支节点（当期望 member/index 但实际不是时触发）
	DFA_MODULE_NODE(dfa, init_data, merr,   dfa_is_entry,          _error_action_member);
	DFA_MODULE_NODE(dfa, init_data, ierr,   dfa_is_entry,          _error_action_index);

	// 分配并保存模块私有数据结构 init_module_data_t
	parse_t*        parse = dfa->priv;
	dfa_data_t*         d     = parse->dfa_data;
	init_module_data_t* md    = d->module_datas[dfa_module_init_data.index];

	assert(!md);

	md = calloc(1, sizeof(init_module_data_t));
	if (!md)
		return -ENOMEM;

	d->module_datas[dfa_module_init_data.index] = md;

	return DFA_OK;
}

/* ------------------------
 * 语法初始化：把模块节点连成期望的语法树（状态转移）
 * 注：下面的 dfa_node_add_child 调用描述了合法的 token/节点序列
 * ------------------------ */
static int _dfa_init_syntax_init_data(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, init_data, entry,  entry);
	DFA_GET_MODULE_NODE(dfa, init_data, comma,  comma);
	DFA_GET_MODULE_NODE(dfa, init_data, lb,     lb);
	DFA_GET_MODULE_NODE(dfa, init_data, rb,     rb);

	DFA_GET_MODULE_NODE(dfa, init_data, ls,     ls);
	DFA_GET_MODULE_NODE(dfa, init_data, rs,     rs);

	DFA_GET_MODULE_NODE(dfa, init_data, dot,    dot);
	DFA_GET_MODULE_NODE(dfa, init_data, member, member);
	DFA_GET_MODULE_NODE(dfa, init_data, index,  index);
	DFA_GET_MODULE_NODE(dfa, init_data, assign, assign);

	DFA_GET_MODULE_NODE(dfa, init_data, merr,   merr);
	DFA_GET_MODULE_NODE(dfa, init_data, ierr,   ierr);

	DFA_GET_MODULE_NODE(dfa, expr,      entry,  expr);

	
	/* - 空初始化： {} -> 用 0 填充数据
	 *   entry -> lb -> rb
	 */
	dfa_node_add_child(entry,     lb);
	dfa_node_add_child(lb,        rb);

	/* - 多维数组初始化：允许连续的 lb/rb / rb->comma / comma->lb
	 *   lb -> lb              // nested [
	 *   rb -> rb              // nested ]
	 *   rb -> comma           // after ] 可以是逗号，继续下一个初始化项
	 *   comma -> lb           // 逗号后可再进入 [
	 */
	dfa_node_add_child(lb,        lb);
	dfa_node_add_child(rb,        rb);
	dfa_node_add_child(rb,        comma);
	dfa_node_add_child(comma,     lb);

	/* - 成员初始化（struct member）
	 *   lb -> dot             // 在节点内可以使用 .member = expr
	 *   lb -> ls              // 或者使用 [index] 形式（ls/rs）
	 *   comma -> dot
	 *   comma -> ls
	 */
	dfa_node_add_child(lb,        dot);
	dfa_node_add_child(lb,        ls);
	dfa_node_add_child(comma,     dot);
	dfa_node_add_child(comma,     ls);

	/* - 表达式项的排列： lb -> expr -> comma 或 expr -> rb
	 *   支持 lb -> expr, expr -> comma, comma -> expr, expr -> rb
	 */
	dfa_node_add_child(lb,        expr);
	dfa_node_add_child(expr,      comma);
	dfa_node_add_child(comma,     expr);
	dfa_node_add_child(expr,      rb);

	// 成员赋值语法： .member = expr
	dfa_node_add_child(dot,       member);
	dfa_node_add_child(member,    assign);
	dfa_node_add_child(assign,    expr);

	/* - 下标形式的语法： [ index ] -> rs -> ls （看起来 ls/rs 在此处用于某种方括号或 token 别名）
	 *   ls -> index
	 *   index -> rs
	 *   rs -> ls  (允许嵌套或互相转换)
	 *   rs -> assign (允许 .member = expr 或 [index] = expr)
	 */
	dfa_node_add_child(ls,        index);
	dfa_node_add_child(index,     rs);
	dfa_node_add_child(rs,        ls);
	dfa_node_add_child(rs,        assign);

	// 错误处理分支：当 dot 之后不是 member 时走 merr；当 ls 之后 index 无效时走 ierr
	dfa_node_add_child(dot,       merr);
	dfa_node_add_child(ls,        ierr);
	return 0;
}

// 导出模块描述符（供 DFA 总表注册）
dfa_module_t dfa_module_init_data =
{
	.name        = "init_data",
	.init_module = _dfa_init_module_init_data,
	.init_syntax = _dfa_init_syntax_init_data,
};
