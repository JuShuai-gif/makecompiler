#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_include;

// ================================================
// include 模块的动作函数和初始化逻辑
// 负责解析形如：#include "path/to/file"
// ================================================

// 当识别到“include”关键字时触发的动作
static int _include_action_include(dfa_t* dfa, vector_t* words, void* data)
{
	// 当前的语法解析器上下文
	parse_t*     parse  = dfa->priv;
	// 当前 DFA 的上下文数据（存储状态、栈等）
	dfa_data_t*      d      = data;
	// 当前识别到的词（include）
	lex_word_t*  w      = words->data[words->size - 1];
	// 此动作仅用于确认识别到“include”关键字，不做额外处理
	return DFA_NEXT_WORD;// 继续读取下一个词
}

// 当识别到 include 后面的路径（即 "xxx.h" 或 'xxx'）时触发
static int _include_action_path(dfa_t* dfa, vector_t* words, void* data)
{
	// 当前解析器
	parse_t*     parse  = dfa->priv;
	// 当前 DFA 数据
	dfa_data_t*      d      = data;
	// 当前路径词
	lex_word_t*  w      = words->data[words->size - 1];
	// 当前词法分析器
	lex_t*       lex    = parse->lex;
	// 当前正在构建的 AST 代码块
	block_t*     cur    = parse->ast->current_block;
	// 路径字符串不应为空
	assert(w->data.s);
	logd("include '%s', line %d\n", w->data.s->data, w->line);

	// 1、暂时切换解析上下文 ---
    // 因为我们要解析被 include 的文件，需要暂时替换当前的词法分析器与 AST 位置。
	parse->lex = NULL;
	// 回到根代码块（顶层作用域）
	parse->ast->current_block = parse->ast->root_block;

	// 2、调用 parse_file 解析 include 的文件 ---
	int ret = parse_file(parse, w->data.s->data);// 解析目标文件
	if (ret < 0) {
		loge("parse file '%s' failed, 'include' line: %d\n", w->data.s->data, w->line);
		goto error;// 如果解析失败则跳到错误处理
	}

	// 3、如果 include 文件定义了宏
	if (parse->lex != lex && parse->lex->macros) { // copy macros
		// 若当前词法分析器尚未有宏表，则克隆一份
		if (!lex->macros) {
			lex->macros = vector_clone(parse->lex->macros);

			if (!lex->macros) {
				ret = -ENOMEM;
				goto error;
			}
		} else {
			// 否则，将 include 文件中的宏追加到当前宏表
			ret = vector_cat(lex->macros, parse->lex->macros);
			if (ret < 0)
				goto error;
		}
		// 4、增加宏引用计数（防止宏结构体被释放）---
		macro_t* m;
		int i;
		for (i = 0; i < parse->lex->macros->size; i++) {
			m  =        parse->lex->macros->data[i];
			m->refs++;// 引用计数 +1
		}
	}

	ret = DFA_NEXT_WORD;// 正常继续下一步解析
error:
	// 5、恢复解析上下文 
	parse->lex = lex;
	parse->ast->current_block = cur;
	return ret;
}

// 当遇到换行（LF）时的动作函数
// 对于 include 指令，换行意味着一条指令结束
static int _include_action_LF(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;// 完成一条 include 语句的解析
}

// include 模块的初始化函数（注册状态节点）
static int _dfa_init_module_include(dfa_t* dfa)
{
	// 定义 include 模块的三个节点：
    //   include 关键字 → 文件路径 → 换行
	DFA_MODULE_NODE(dfa, include, include,   dfa_is_include,      _include_action_include);
	DFA_MODULE_NODE(dfa, include, path,      dfa_is_const_string, _include_action_path);
	DFA_MODULE_NODE(dfa, include, LF,        dfa_is_LF,           _include_action_LF);

	return DFA_OK;
}

// include 模块语法连接定义（建立状态转移关系）
static int _dfa_init_syntax_include(dfa_t* dfa)
{
	// 获取 include 模块内的各个节点
	DFA_GET_MODULE_NODE(dfa, include, include, include);
	DFA_GET_MODULE_NODE(dfa, include, path,    path);
	DFA_GET_MODULE_NODE(dfa, include, LF,      LF);

	// 获取宏模块中的 '#' 节点，用于匹配 "#include"
	DFA_GET_MODULE_NODE(dfa, macro,   hash,    hash);

	// 定义状态转移路径：
    //    hash (#) → include → path → LF
    // 即 "#include "xxx"" 后换行结束
	dfa_node_add_child(hash,     include);
	dfa_node_add_child(include,  path);
	dfa_node_add_child(path,     LF);

	return 0;
}

// include 模块描述符（注册到 DFA 系统）
dfa_module_t dfa_module_include =
{
	.name        = "include",// 模块名称
	.init_module = _dfa_init_module_include,// 模块节点注册函数
	.init_syntax = _dfa_init_syntax_include,// 语法关系定义函数
};
