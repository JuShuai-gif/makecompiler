#include"dfa.h"
#include"dfa_util.h"
#include"parse.h"

extern dfa_module_t dfa_module_class;

/*
这段代码是 DFA 模块中用于解析 class/struct 定义语法的一部分
*/
// ========================================
// class 模块数据结构
// 每当 DFA 进入 "class"/"struct" 定义时，都会在模块数据中维护一份当前状态
// ========================================
typedef struct {
	lex_word_t*  current_identity;// 当前正在解析的类名（如 class Foo → Foo）

	block_t*     parent_block;// 保存进入 class 前的父作用域（用于解析完后恢复）

	type_t*      current_class;// 当前正在解析的类类型对象（type_t 实例）

	int              nb_lbs;// 左大括号 “{” 计数（支持嵌套或检查匹配）
	int              nb_rbs;// 右大括号 “}” 计数

} class_module_data_t;


// ========================================
// 语法识别函数：判断词是否是 “class” 或 “struct”
// 这是 DFA 的条件函数，用于匹配当前词类型
// ========================================
static int _class_is_class(dfa_t* dfa, void* word)
{
	lex_word_t* w = word;

	// 若当前词为关键字 class 或 struct，则返回 true
	return LEX_WORD_KEY_CLASS  == w->type
		|| LEX_WORD_KEY_STRUCT == w->type;
}


// ========================================
// 动作函数：当识别到类名（标识符）时执行
// 对应语法： class Foo { ... }
//                      ^--- 这里就是 current_identity
// ========================================
static int _class_action_identity(dfa_t* dfa, vector_t* words, void* data)
{
	// 全局解析器对象
	parse_t*          parse = dfa->priv;
	// DFA 状态上下文
	dfa_data_t*           d     = data;
	// 当前模块数据
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];
	// 当前词（类名）
	lex_word_t*       w     = words->data[words->size - 1];

	// 检查：当前模块是否已经有一个正在处理的类名
	// 若已有，则说明语法错误（重复定义或嵌套错误）
	if (md->current_identity) {
		loge("\n");
		return DFA_ERROR;
	}

	// 从当前作用域查找是否已存在该类型（防止重复定义）
	type_t* t = block_find_type(parse->ast->current_block, w->text->data);
	if (!t) {
		// 若不存在，则新建一个类型节点
		t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
		if (!t) {
			loge("\n");
			return DFA_ERROR;
		}

		// 结构体计数 +1（全局统计）
		parse->ast->nb_structs++;
		// 标记该节点为 class 类型
		t->node.class_flag = 1;
		// 将该类型注册到当前作用域的符号表中
		scope_push_type(parse->ast->current_block->scope, t);
		// 把类型节点挂到当前语法树的块节点下（AST 建立父子关系）
		node_add_child((node_t*)parse->ast->current_block, (node_t*)t);
	}
	
	// 记录当前正在解析的类标识符和父 block
	md->current_identity = w;
	md->parent_block     = parse->ast->current_block;
	
	// 注册 “class_end” 钩子，用于在类定义结束（右大括号）时回调
	DFA_PUSH_HOOK(dfa_find_node(dfa, "class_end"), DFA_HOOK_END);
	
	// 打印调试信息：当前类型指针与类型号
	logi("\033[31m t: %p, t->type: %d\033[0m\n", t, t->type);

	return DFA_NEXT_WORD;// 继续处理下一个词
}

// ========================================
// 动作函数：处理左大括号 '{'
// 对应语法： class Foo { 
//                      ^--- 这里进入类定义体
// ========================================
static int _class_action_lb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];
	// 当前词（左大括号）
	lex_word_t*       w     = words->data[words->size - 1];

	// 没有先解析到类名就遇到 '{'，说明语法错误
	if (!md->current_identity) {
		loge("\n");
		return DFA_ERROR;
	}

	// 根据类名找到对应的类型节点（必须已经通过 _class_action_identity 注册）
	type_t* t = block_find_type(parse->ast->current_block, md->current_identity->text->data);
	if (!t) {
		loge("\n");
		return DFA_ERROR;
	}

	// 若该类还没有自己的作用域，则创建一个新的作用域
	// 每个类定义体都是一个独立作用域（用于定义成员变量、方法等）
	if (!t->scope)
		t->scope = scope_alloc(w, "class");

	// 保存当前作用域（进入类体前的父块）
	md->parent_block  = parse->ast->current_block;
	// 当前正在定义的类对象
	md->current_class = t;
	// 记录 '{' 数量（用于后续与 '}' 匹配）
	md->nb_lbs++;
	// 将当前 AST 的上下文切换到类作用域中
	// 从此之后解析到的成员变量、函数等都属于该类
	parse->ast->current_block = (block_t*)t;

	return DFA_NEXT_WORD;
}

// ================================================
// 函数: _class_calculate_size
// 功能: 计算类(或结构体)的成员变量内存布局，包括：
//       - 字节对齐（1/2/4/8 字节）
//       - 位域偏移(bitfield)
//       - 数组容量计算
//       - 成员偏移量 offset
// ================================================
static int _class_calculate_size(dfa_t* dfa, type_t* s)
{
	// 当前成员变量指针
	variable_t* v;

	int bits = 0;// 当前累计的位偏移（bit为单位）
	int size = 0;// 当前累计的字节偏移
	int i;
	int j;

	// 遍历该类的所有成员变量
	for (i = 0; i < s->scope->vars->size; i++) {
		v  =        s->scope->vars->data[i];

		// 安全检查：变量大小必须非负
		assert(v->size >= 0);

		// -------------------------------------------------
		// Step 1: 计算字节对齐（alignment）
		// 对齐规则：
		//   1 字节类型：无需对齐
		//   2 字节类型：2字节对齐
		//   4 字节类型：4字节对齐
		//   8 字节及以上：8字节对齐
		// -------------------------------------------------
		switch (v->size) {
			case 1:
				v->offset = size;
				break;
			case 2:
				v->offset = (size + 1) & ~0x1;// 对齐到 2 字节边界
				break;
			case 3:
			case 4:
				v->offset = (size + 3) & ~0x3;// 对齐到 4 字节边界
				break;
			default:
				v->offset = (size + 7) & ~0x7;// 对齐到 8 字节边界
				break;
		};

		// -------------------------------------------------
		// Step 2: 处理数组类型成员
		// 对于多维数组，需要计算总容量(capacity)
		// capacity = dim1 * dim2 * ...
		// -------------------------------------------------
		if (v->nb_dimentions > 0) {
			v->capacity = 1;

			for (j = 0; j < v->nb_dimentions; j++) {

				// 检查维度是否合法
				if (v->dimentions[j].num < 0) {
					loge("number of %d-dimention for array '%s' is less than 0, size: %d, file: %s, line: %d\n",
							j, v->w->text->data, v->dimentions[j].num, v->w->file->data, v->w->line);
					return DFA_ERROR;
				}

				// 累乘维度，得到总容量
				v->capacity *= v->dimentions[j].num;
			}

			// 更新当前累计大小
			size = v->offset + v->size * v->capacity;
			bits = size << 3;// 转换为 bit 单位
		} else {
			// -------------------------------------------------
			// Step 3: 非数组成员
			// 若是 bitfield（位域变量），需特殊处理
			// -------------------------------------------------
			if (v->bit_size > 0) {
				int align = v->size << 3;// 对齐大小（bit）
				int used  = bits & (align - 1);// 当前结构体内已使用的位偏移
				int rest  = align - used;// 当前字节块剩余位数

				// 若剩余位不足以容纳该 bitfield，则跳到下一个字节块
				if (rest < v->bit_size) {
					bits += rest;
					used  = 0;
					rest  = align;
				}

				// 计算该 bitfield 的字节偏移与位偏移
				v->offset     = (bits >> 3) & ~(v->size - 1);
				v->bit_offset =  used;

				logd("bits: %d, align: %d, used: %d, rest: %d, v->offset: %d\n", bits, align, used, rest, v->offset);

				// 更新结构体累计 bit 偏移
				bits = (v->offset << 3) + v->bit_offset + v->bit_size;
			} else
				bits = (v->offset + v->size) << 3;// 普通变量（非位域）

			// 更新当前累计的结构体总大小
			if (size < v->offset + v->size)
				size = v->offset + v->size;
		}

		// 调试输出当前成员的布局信息
		logi("class '%s', member: '%s', member_flag: %d, offset: %d, size: %d, v->dim: %d, v->capacity: %d, bit offset: %d, bit size: %d\n",
				s->name->data, v->w->text->data, v->member_flag, v->offset, v->size, v->nb_dimentions, v->capacity, v->bit_offset, v->bit_size);
	}

	// -------------------------------------------------
	// Step 4: 对整个结构体进行总对齐
	// -------------------------------------------------
	switch (size) {
		case 1:
			s->size = size;
			break;
		case 2:
			s->size = (size + 1) & ~0x1;
			break;
		case 3:
		case 4:
			s->size = (size + 3) & ~0x3;
			break;
		default:
			s->size = (size + 7) & ~0x7;
			break;
	};

	// 标记类型定义已完成
	s->node.define_flag = 1;

	// 输出最终结构体大小
	logi("class '%s', s->size: %d, size: %d\n", s->name->data, s->size, size);
	return 0;
}

// ================================================
// 函数: _class_action_rb
// 功能: 处理类定义右大括号 “}”
//       即类定义结束时调用
// 步骤:
//   1. 调用 _class_calculate_size 计算成员布局
//   2. 更新括号计数
// ================================================
static int _class_action_rb(dfa_t* dfa, vector_t* words, void* data)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	// 类定义结束时计算整体大小与偏移
	if (_class_calculate_size(dfa, md->current_class) < 0) {
		loge("\n");
		return DFA_ERROR;
	}

	// 更新右括号 “}” 计数
	md->nb_rbs++;

	return DFA_NEXT_WORD;// 继续读取下一个 token
}

// 处理 “class X;” 这种声明语句（仅声明，不定义）
// 例如：class MyClass; 表示前置声明，不做实际定义
static int _class_action_semicolon(dfa_t* dfa, vector_t* words, void* data)
{
	return DFA_OK;// 直接返回成功，不做额外处理
}

// 当 DFA 检测到类定义的可能结束时（由 HOOK 触发）执行此函数
static int _class_action_end(dfa_t* dfa, vector_t* words, void* data)
{
	// 获取全局解析上下文
	parse_t*          parse = dfa->priv;
	// 获取 DFA 的运行时数据
	dfa_data_t*           d     = data;
	// 获取当前类模块的上下文
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	// 判断类定义是否已完整结束（左、右花括号数量匹配）
	if (md->nb_rbs == md->nb_lbs) {

		// 类定义完整结束后，恢复到父作用域
		parse->ast->current_block = md->parent_block;

		// 清空当前类的解析状态
		md->current_identity = NULL;
		md->current_class    = NULL;
		md->parent_block     = NULL;
		md->nb_lbs           = 0;
		md->nb_rbs           = 0;

		return DFA_OK;// 返回成功，结束解析
	}

	// 若括号未闭合，则继续等待匹配，重新挂钩 end 节点（递归触发自身）
	DFA_PUSH_HOOK(dfa_find_node(dfa, "class_end"), DFA_HOOK_END);

	// 告诉 DFA 继续切换到当前状态
	return DFA_SWITCH_TO;
}

// 初始化类模块节点（定义各种关键字对应的 DFA 节点与行为）
static int _dfa_init_module_class(dfa_t* dfa)
{
	// 注册各种语法节点及其对应的判断函数与动作函数
	DFA_MODULE_NODE(dfa, class, _class,    _class_is_class,      NULL);

	DFA_MODULE_NODE(dfa, class, identity,  dfa_is_identity,  _class_action_identity);

	DFA_MODULE_NODE(dfa, class, lb,        dfa_is_lb,        _class_action_lb);
	DFA_MODULE_NODE(dfa, class, rb,        dfa_is_rb,        _class_action_rb);

	DFA_MODULE_NODE(dfa, class, semicolon, dfa_is_semicolon, _class_action_semicolon);
	DFA_MODULE_NODE(dfa, class, end,       dfa_is_entry,     _class_action_end);

	// 获取解析上下文和 DFA 数据
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	assert(!md);// 确保未重复初始化

	// 分配并初始化类模块的上下文数据
	md = calloc(1, sizeof(class_module_data_t));
	if (!md)
		return DFA_ERROR;

	// 存入 DFA 全局模块表中
	d->module_datas[dfa_module_class.index] = md;

	return DFA_OK;
}

// 清理类模块，释放相关内存
static int _dfa_fini_module_class(dfa_t* dfa)
{
	parse_t*          parse = dfa->priv;
	dfa_data_t*           d     = parse->dfa_data;
	class_module_data_t*  md    = d->module_datas[dfa_module_class.index];

	// 若已初始化则释放
	if (md) {
		free(md);
		md = NULL;
		d->module_datas[dfa_module_class.index] = NULL;
	}

	return DFA_OK;
}

// 定义类模块的语法规则（即：各节点之间的可达关系）
static int _dfa_init_syntax_class(dfa_t* dfa)
{
	// 获取 DFA 各节点指针（之前通过 DFA_MODULE_NODE 注册）
	DFA_GET_MODULE_NODE(dfa, class,  _class,    _class);
	DFA_GET_MODULE_NODE(dfa, class,  identity,  identity);
	DFA_GET_MODULE_NODE(dfa, class,  lb,        lb);
	DFA_GET_MODULE_NODE(dfa, class,  rb,        rb);
	DFA_GET_MODULE_NODE(dfa, class,  semicolon, semicolon);
	DFA_GET_MODULE_NODE(dfa, class,  end,       end);

	// 成员变量
	DFA_GET_MODULE_NODE(dfa, type,   entry,     member);
	// 联合体定义（union）
	DFA_GET_MODULE_NODE(dfa, union,  _union,    _union);

	// 把 class 模块添加到全局 DFA 语法列表中
	vector_add(dfa->syntaxes,     _class);

	// -------------------------
	// 以下定义了 “类语法结构图”
	// -------------------------

	// class -> identity
	dfa_node_add_child(_class,    identity);

	// class identity -> ';'（声明）
	dfa_node_add_child(identity,  semicolon);

	// class identity -> '{'（定义）
	dfa_node_add_child(identity,  lb);

	// 空类：class X {}
	dfa_node_add_child(lb,        rb);

	// 类体内部可出现 union 或 member（成员变量）
	dfa_node_add_child(lb,        _union);
	dfa_node_add_child(lb,        member);

	// 成员定义完毕后可闭合 '}'
	dfa_node_add_child(member,    rb);

	
	// 右花括号后面可接分号（结束定义）
	dfa_node_add_child(rb,        semicolon);
	
	// 结束节点后的可能延续（处理嵌套结构）
	dfa_node_add_child(end,       _union);
	dfa_node_add_child(end,       member);
	dfa_node_add_child(end,       rb);

	return 0;
}

// 类模块的描述符（注册表）
dfa_module_t dfa_module_class = 
{
	.name        = "class", // 模块名称
	.init_module = _dfa_init_module_class,// 模块初始化函数
	.init_syntax = _dfa_init_syntax_class,// 语法规则初始化函数

	.fini_module = _dfa_fini_module_class,// 模块销毁函数
};
