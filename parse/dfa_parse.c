#include"dfa.h"
#include"parse.h"

/* -------------------------
 * 外部模块声明（各模块在其它文件中定义）
 * 这些 extern 表示模块描述符（dfa_module_t）在别处被实现并导出。
 * 注意：模块在 dfa_modules[] 中的顺序通常决定了模块索引与初始化顺序。
 * ------------------------- */
extern dfa_module_t  dfa_module_macro;
extern dfa_module_t  dfa_module_include;

extern dfa_module_t  dfa_module_identity;

extern dfa_module_t  dfa_module_expr;
extern dfa_module_t  dfa_module_create;
extern dfa_module_t  dfa_module_call;
extern dfa_module_t  dfa_module_sizeof;
extern dfa_module_t  dfa_module_container;
extern dfa_module_t  dfa_module_init_data;
extern dfa_module_t  dfa_module_va_arg;

extern dfa_module_t  dfa_module_enum;
extern dfa_module_t  dfa_module_union;
extern dfa_module_t  dfa_module_class;

extern dfa_module_t  dfa_module_type;

extern dfa_module_t  dfa_module_var;

extern dfa_module_t  dfa_module_function;
extern dfa_module_t  dfa_module_operator;

extern dfa_module_t  dfa_module_if;
extern dfa_module_t  dfa_module_while;
extern dfa_module_t  dfa_module_do;
extern dfa_module_t  dfa_module_for;
extern dfa_module_t  dfa_module_switch;


extern dfa_module_t  dfa_module_break;
extern dfa_module_t  dfa_module_continue;
extern dfa_module_t  dfa_module_return;
extern dfa_module_t  dfa_module_goto;
extern dfa_module_t  dfa_module_label;
extern dfa_module_t  dfa_module_async;

extern dfa_module_t  dfa_module_block;

/* -------------------------
 * dfa_modules 数组：把所有模块指针集中在一个数组中，便于统一初始化/连接语法
 * 重要：模块在此数组中的索引会被写回到每个模块的 m->index 字段（见 parse_dfa_init）
 * 顺序影响：
 *  - 模块 index（m->index）由数组下标决定
 *  - parse->dfa_data->module_datas 的长度也基于此数组大小
 * ------------------------- */
dfa_module_t* dfa_modules[] =
{
	&dfa_module_macro,
	&dfa_module_include,

	&dfa_module_identity,

	&dfa_module_expr,
	&dfa_module_create,
	&dfa_module_call,
	&dfa_module_sizeof,
	&dfa_module_container,
	&dfa_module_init_data,
	&dfa_module_va_arg,

	&dfa_module_enum,
	&dfa_module_union,
	&dfa_module_class,

	&dfa_module_type,

	&dfa_module_var,

	&dfa_module_function,
	&dfa_module_operator,

	&dfa_module_if,
	&dfa_module_while,
	&dfa_module_do,
	&dfa_module_for,
	&dfa_module_switch,

#if 1
	&dfa_module_break,
	&dfa_module_continue,
	&dfa_module_goto,
	&dfa_module_return,
	&dfa_module_label,
	&dfa_module_async,
#endif
	&dfa_module_block,
};

/* -----------------------------------------
 * parse_dfa_init: 为解析器创建/初始化 DFA 以及所有模块
 *
 * 主要步骤：
 * 1. dfa_open 创建一个 DFA 实例并把 parse 作为 private context（dfa->priv = parse）
 * 2. 计算模块数 nb_modules（基于 dfa_modules 数组）
 * 3. 分配 parse->dfa_data 以及 module_datas（用于每个模块存放私有数据指针）
 * 4. 分配 current_identities 栈（模块间共享的解析时栈）
 * 5. 循环调用每个模块的 init_module（模块内部注册节点/钩子/分配私有数据）
 * 6. 再次循环调用每个模块的 init_syntax（将模块节点连接成语法图）
 *
 * 返回：0 表示成功，-1 表示任意初始化失败（注意：失败时部分已初始化资源可能未被回收）
 * ----------------------------------------- */
int parse_dfa_init(parse_t* parse)
{
	/* 打开/创建 dfa，名字 "parse"，私有数据为 parse（方便 DFA 回调访问 parse）*/
	if (dfa_open(&parse->dfa, "parse", parse) < 0) {
		loge("\n");
		return -1;
	}

	/* 计算模块数量（用于后续分配 module_datas） */
	int nb_modules  = sizeof(dfa_modules) / sizeof(dfa_modules[0]);

	/* 分配 dfa_data 结构体（用于存放模块私有数据数组、共享栈等） */
	parse->dfa_data = calloc(1, sizeof(dfa_data_t));
	if (!parse->dfa_data) {
		loge("\n");
		return -1;
	}

	/* 分配 module_datas 指针数组，每个模块可以在自己的 init_module 中写入 module_datas[m->index] */
	parse->dfa_data->module_datas = calloc(nb_modules, sizeof(void*));
	if (!parse->dfa_data->module_datas) {
		loge("\n");
		return -1;
	}

	/* 分配并初始化 current_identities 栈（模块在解析标识符/类型时会使用该栈）*/
	parse->dfa_data->current_identities = stack_alloc();
	if (!parse->dfa_data->current_identities) {
		loge("\n");
		return -1;
	}

	/* 逐个模块调用 init_module（模块内部可分配自己的私有数据并赋予 d->module_datas[index]）*/
	int i;
	for (i = 0; i < nb_modules; i++) {

		dfa_module_t* m = dfa_modules[i];

		if (!m)
			continue;

		/* 将模块的 index 写回模块结构，便于模块内部通过 index 定位其 module_datas 槽 */
		m->index = i;

		if (!m->init_module)
			continue;

		/* 调用模块的 init_module 回调（该函数通常会调用 DFA_MODULE_NODE 宏等）*/
		if (m->init_module(parse->dfa) < 0) {
			loge("init module: %s\n", m->name);
			return -1;/* 出错直接返回（注意：没有对已经成功初始化的模块做清理） */
		}
	}

	/* 模块语法的连接在所有模块注册节点之后进行：再次遍历模块并调用 init_syntax */
	for (i = 0; i < nb_modules; i++) {

		dfa_module_t* m = dfa_modules[i];

		if (!m || !m->init_syntax)
			continue;

		if (m->init_syntax(parse->dfa) < 0) {
			loge("init syntax: %s\n", m->name);
			return -1;/* 出错返回（同样没有做回退清理）*/
		}
	}

	return 0;
}

/* ----------------------------
 * dfa -> lex 的适配函数：pop/push/free
 *
 * 这些 wrapper 将 DFA 抽象的操作映射到具体的词法器实现（parse->lex）。
 * 语义约定：
 * - pop_word 返回对 lex_word_t* 的所有权（调用方负责 free 或 push 回）
 * - push_word 把 lex_word_t* 放回 lex 的输入流（通常放到 head，使下次被 pop）
 * - free_word 释放 lex_word_t
 * ---------------------------- */

/* 从 lexical 层弹出一个词，返回 void* 以匹配 dfa_ops_t 接口 */
static void* dfa_pop_word(dfa_t* dfa)
{
	parse_t* parse = dfa->priv;

	lex_word_t* w = NULL;
	lex_pop_word(parse->lex, &w);/* lex_pop_word 将填充 w */
	return w;/* 返回拿到的 lex_word_t*（作为 void*）*/
}

/* 把一个词推回到 lexical 层（常用于预处理或 lookahead）*/
static int dfa_push_word(dfa_t* dfa, void* word)
{
	parse_t* parse = dfa->priv;

	lex_word_t* w = word;
	lex_push_word(parse->lex, w);/* lex_push_word 将 token 放回词流 */
	return 0;
}

/* 释放一个词（供 DFA 在需要丢弃 token 时调用） */
static void dfa_free_word(void* word)
{
	lex_word_t* w = word;
	lex_word_free(w);
}

/* 将上述适配函数组织到一个 ops 表里，供 DFA 调用 */
dfa_ops_t dfa_ops_parse = 
{
	.name      = "parse",

	.pop_word  = dfa_pop_word,
	.push_word = dfa_push_word,
	.free_word = dfa_free_word,
};
