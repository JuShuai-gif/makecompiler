#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"
#include"utils_stack.h"

extern dfa_module_t dfa_module_block;

// block的上下文数据，用来记录：
// 当前块中的 { 和 } 数量，用于配对
// 在进入块前，当前 AST 的父block和父node,用于结束时恢复
/*
进入 block 的时候：
	1、如果没有 block data → 新建一个，保存父 block / 父 node，并注册 block_end 钩子。

	2、把它压入专用栈 s，方便嵌套处理。

	3、控制 DFA 的读取：有词就继续，没有就等下一个词。
*/
typedef struct {

	int              nb_lbs;// 左大括号数量 {
	int              nb_rbs;// 右大括号数量 }

	block_t*     parent_block;// 保存进入 block 前的父block

	node_t*      parent_node;// 保存进入 block 前的父 node

} dfa_block_data_t;

// 处理进入 { 的逻辑
static int _block_action_entry(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse     = dfa->priv;// 语法解析上下文
	dfa_data_t* d         = data;// DFA 全局数据

	// block模块的专用栈
	stack_t*      s         = d->module_datas[dfa_module_block.index];
	// 栈顶 block 数据
	dfa_block_data_t* bd        = stack_top(s);

	// 如果当前没有 block data，说明是一个新的 block
	if (!bd) {
		dfa_block_data_t* bd = calloc(1, sizeof(dfa_block_data_t));
		assert(bd);

		// 保存当前父 block 和父 node（进入新的 block 前的状态）
		bd->parent_block = parse->ast->current_block;
		bd->parent_node  = d->current_node;

		// 注册一个 hook：等到 block_end 节点时触发 _block_action_end
		DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

		// 把新建的 block data 压栈
		stack_push(s, bd);

		logi("new bd: %p, s->size: %d\n", bd, s->size);
	} else
		logi("new bd: %p, s->size: %d\n", bd, s->size);// 如果已经有 bd,说明是嵌套 block

	// 如果当前 words 已经有 token，就继续解析；否则让 DFA 读下一个词
	return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}

// 处理 } 结束逻辑
/*
_block_action_end

	遇到 block 结束 } → 判断是否完全闭合。

	如果没闭合 → 等后续继续触发。

	如果闭合 → 校验状态，弹栈，释放 block 数据。
*/
static int _block_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse     = dfa->priv;// 语法解析上下文
	dfa_data_t*       d         = data; // DFA 全局数据
	// block 模块的专用栈
	stack_t*      s         = d->module_datas[dfa_module_block.index];
	// 栈顶 block data
	dfa_block_data_t* bd        = stack_top(s);

	// 如果右括号数量还小于左括号，说明 block 还没完全闭合
	if (bd->nb_rbs < bd->nb_lbs) {
		logi("end bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
				bd, bd->nb_lbs, bd->nb_rbs, s->size);

		// 继续挂一个 block_end 的 hook，等下一个右括号时再触发
		DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

		return DFA_SWITCH_TO;// 切换状态，等待更多词
	}

	// 此时 block 应该刚好闭合
	assert(bd->nb_lbs       == bd->nb_rbs);// 左右括号数相等
	assert(bd->parent_block == parse->ast->current_block);// 父 block 一致
	assert(bd->parent_node  == d->current_node);// 父 node 一致

	// block 结束，出栈
	stack_pop(s);

	logi("end bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
			bd, bd->nb_lbs, bd->nb_rbs, s->size);

	// 释放内存
	free(bd);
	bd = NULL;

	return DFA_OK;
}

/*
DFA 语法解析器里 block { ... } 模块的完整实现，包括左括号 {、右括号 } 的处理，以及 block 模块的初始化、语法注册和释放

遇到左括号 { 时，新建一个 block 节点，压栈保存上下文，更新 AST 当前作用域
*/
static int _block_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;// 语法解析上下文
	dfa_data_t* d     = data;// DFA 全局数据
	lex_word_t*   w     = words->data[words->size - 1];// 当前词(最近一个词)
	stack_t*      s     = d->module_datas[dfa_module_block.index];// block 模块的专用栈

	// 分配一个新的 block data（保存括号计数、父 block/node）
	dfa_block_data_t* bd = calloc(1, sizeof(dfa_block_data_t));
	assert(bd);

	// 创建一个新的 AST block 节点
	block_t* b = block_alloc(w);
	assert(b);

	// 将 block 节点挂接到当前 node 或者父 block 下
	if (d->current_node)
		node_add_child(d->current_node, (node_t*)b);
	else
		node_add_child((node_t*)parse->ast->current_block, (node_t*)b);

	// 保存进入前的父 block 和父 node
	bd->parent_block = parse->ast->current_block;
	bd->parent_node  = d->current_node;

	// 切换 AST 上下文：当前 block 就是新建的 b
	parse->ast->current_block = b;
	d->current_node = NULL;
	
    // 注册一个 hook：等到遇到 block_end 时，调用 _block_action_end
	DFA_PUSH_HOOK(dfa_find_node(dfa, "block_end"), DFA_HOOK_END);

	// 左括号计数 +1
	bd->nb_lbs++;
	stack_push(s, bd);// 压栈保存上下文

	logi("new bd: %p, s->size: %d\n", bd, s->size);

	return DFA_NEXT_WORD;// DFA 继续读下一个词
}

// 处理右括号 } 的动作
// 遇到右括号 } 时，右括号计数 +1，检查是否匹配。
// 如果匹配成功 → 恢复到进入 { 前的父 block / node 上下文。
static int _block_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = data;
	// 当前 token（右括号）
	lex_word_t*   w     = words->data[words->size - 1];
	stack_t*      s     = d->module_datas[dfa_module_block.index];
	// 获取栈顶 block data
	dfa_block_data_t* bd    = stack_top(s);

	// 右括号数量 +1
	bd->nb_rbs++;

	logi("bd: %p, bd->nb_lbs: %d, bd->nb_rbs: %d, s->size: %d\n",
			bd, bd->nb_lbs, bd->nb_rbs, s->size);

	// 必须保证左右括号匹配
	assert(bd->nb_lbs == bd->nb_rbs);

	// 恢复进入block前的父block和父node
	parse->ast->current_block = bd->parent_block;
	d->current_node = bd->parent_node;

	return DFA_OK;
}

// 模块初始化函数
/*
作用：
	定义 block 模块的 DFA 节点（entry、end、lb、rb）。

	构造语法规则：block 内可以包含表达式、类型定义、控制语句、宏、async 等。

	分配专用栈 s，用于管理嵌套 block 的上下文。
*/
static int _dfa_init_module_block(dfa_t* dfa)
{
	// 定义 block 模块的 DFA 节点：entry、end、lb、rb
	DFA_MODULE_NODE(dfa, block, entry, dfa_is_entry, _block_action_entry);
	DFA_MODULE_NODE(dfa, block, end,   dfa_is_entry, _block_action_end);
	DFA_MODULE_NODE(dfa, block, lb,    dfa_is_lb,    _block_action_lb);
	DFA_MODULE_NODE(dfa, block, rb,    dfa_is_rb,    _block_action_rb);

	// 获取这些 DFA 节点，准备构建语法
	DFA_GET_MODULE_NODE(dfa, block,     entry,     entry);
	DFA_GET_MODULE_NODE(dfa, block,     end,       end);
	DFA_GET_MODULE_NODE(dfa, block,     lb,        lb);
	DFA_GET_MODULE_NODE(dfa, block,     rb,        rb);

	// 语法块中可能包含的语句节点
	DFA_GET_MODULE_NODE(dfa, expr,      entry,     expr);
	DFA_GET_MODULE_NODE(dfa, type,      entry,     type);

	DFA_GET_MODULE_NODE(dfa, macro,     hash,      macro);

	DFA_GET_MODULE_NODE(dfa, if,       _if,       _if);
	DFA_GET_MODULE_NODE(dfa, while,    _while,    _while);
	DFA_GET_MODULE_NODE(dfa, do,       _do,       _do);
	DFA_GET_MODULE_NODE(dfa, for,      _for,      _for);

	DFA_GET_MODULE_NODE(dfa, switch,   _switch,   _switch);
	DFA_GET_MODULE_NODE(dfa, switch,   _case,     _case);
	DFA_GET_MODULE_NODE(dfa, switch,   _default,  _default);

	DFA_GET_MODULE_NODE(dfa, break,    _break,    _break);
	DFA_GET_MODULE_NODE(dfa, continue, _continue, _continue);
	DFA_GET_MODULE_NODE(dfa, return,   _return,   _return);
	DFA_GET_MODULE_NODE(dfa, goto,     _goto,     _goto);
	DFA_GET_MODULE_NODE(dfa, label,    label,     label);
	DFA_GET_MODULE_NODE(dfa, async,    async,     async);

	DFA_GET_MODULE_NODE(dfa, va_arg,   start,     va_start);
	DFA_GET_MODULE_NODE(dfa, va_arg,   end,       va_end);

	// 构造语法树关系
	dfa_node_add_child(entry, lb);// { 开始
	dfa_node_add_child(entry, rb);// } 结束

	dfa_node_add_child(entry, va_start);
	dfa_node_add_child(entry, va_end);
	dfa_node_add_child(entry, expr);
	dfa_node_add_child(entry, type);

	dfa_node_add_child(entry, macro);

	dfa_node_add_child(entry, _if);
	dfa_node_add_child(entry, _while);
	dfa_node_add_child(entry, _do);
	dfa_node_add_child(entry, _for);
	dfa_node_add_child(entry, _switch);
	dfa_node_add_child(entry, _case);
	dfa_node_add_child(entry, _default);

	dfa_node_add_child(entry, _break);
	dfa_node_add_child(entry, _continue);
	dfa_node_add_child(entry, _return);
	dfa_node_add_child(entry, _goto);
	dfa_node_add_child(entry, label);
	dfa_node_add_child(entry, async);

	// 为 block 模块分配一个专用栈
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = parse->dfa_data;
	stack_t*      s     = d->module_datas[dfa_module_block.index];

	assert(!s);// 必须还没分配过

	s = stack_alloc();
	if (!s) {
		loge("error: \n");
		return DFA_ERROR;
	}

	d->module_datas[dfa_module_block.index] = s;
	return DFA_OK;
}

// 模块释放
// 作用：清理 block 模块时，释放它用到的栈
static int _dfa_fini_module_block(dfa_t* dfa)
{
	parse_t*      parse = dfa->priv;
	dfa_data_t* d     = parse->dfa_data;
	stack_t*      s     = d->module_datas[dfa_module_block.index];

	if (s) {
		// 释放栈
		stack_free(s);
		s = NULL;
		d->module_datas[dfa_module_block.index] = NULL;
	}

	return DFA_OK;
}

// 语法规则初始化
/*
作用：
	定义语法循环关系：entry → end → entry，允许多条语句出现在 block 内。

	调试打印所有 entry 的子节点。
*/
static int _dfa_init_syntax_block(dfa_t* dfa)
{
	DFA_GET_MODULE_NODE(dfa, block,     entry,     entry);
	DFA_GET_MODULE_NODE(dfa, block,     end,       end);

	// 语法关系：entry → end，end → entry
	dfa_node_add_child(entry, end);
	dfa_node_add_child(end,   entry);

	// 打印 entry 的所有子节点（调试用）
	int i;
	for (i = 0; i < entry->childs->size; i++) {
		dfa_node_t* n = entry->childs->data[i];

		logd("n->name: %s\n", n->name);
	}

	return 0;
}

// 模块定义
// 定义了一个 block 模块，提供了初始化、语法注册和释放的接口
dfa_module_t dfa_module_block =
{
	.name        = "block",// 模块名
	.init_module = _dfa_init_module_block,// 初始化模块节点
	.init_syntax = _dfa_init_syntax_block,// 初始化语法规则

	.fini_module = _dfa_fini_module_block,// 模块释放
};
