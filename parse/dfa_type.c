#include "dfa.h"
#include "dfa_util.h"
#include "parse.h"

extern dfa_module_t dfa_module_type;

// 判断当前词是否为 struct 或 class 关键字
static int _type_is__struct(dfa_t *dfa, void *word) {
    lex_word_t *w = word;

    // 如果词类型是 LEX_WORD_KEY_CLASS 或 LEX_WORD_KEY_STRUCT，返回真
    return LEX_WORD_KEY_CLASS == w->type
           || LEX_WORD_KEY_STRUCT == w->type;
}

// 处理 const 类型修饰符，将 const_flag 置为 1
static int _type_action_const(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;

    d->const_flag = 1;

    return DFA_NEXT_WORD; // 继续处理下一个词
}

// 处理 extern 类型修饰符，将 extern_flag 置为 1
static int _type_action_extern(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;

    d->extern_flag = 1;

    return DFA_NEXT_WORD;
}

// 处理 static 类型修饰符，将 static_flag 置为 1
static int _type_action_static(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;

    d->static_flag = 1;

    return DFA_NEXT_WORD;
}

// 处理 inline 类型修饰符，将 inline_flag 置为 1
static int _type_action_inline(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;

    d->inline_flag = 1;

    return DFA_NEXT_WORD;
}

// 处理基本类型（如 int, float, 自定义类型等）
static int _type_action_base_type(dfa_t *dfa, vector_t *words, void *data) {
    // 获取 DFA 的私有解析器指针
    parse_t *parse = dfa->priv;
    // 获取当前词
    lex_word_t *w = words->data[words->size - 1];
    // DFA 数据上下文
    dfa_data_t *d = data;
    // 当前标识符栈
    stack_t *s = d->current_identities;
    // 创建一个标识符对象
    dfa_identity_t *id = calloc(1, sizeof(dfa_identity_t));

    if (!id)
        return DFA_ERROR; // 内存分配失败

    // 查找当前 block 中的类型，如果没有找到，则报错
    id->type = block_find_type(parse->ast->current_block, w->text->data);
    if (!id->type) {
        loge("can't find type '%s'\n", w->text->data);

        free(id);
        return DFA_ERROR;
    }

    // 将标识符对象压入栈中
    if (stack_push(s, id) < 0) {
        free(id);
        return DFA_ERROR;
    }
    // 保存当前词对象
    id->type_w = w;
    // 继承 const/static/extern/inline 修饰符
    id->const_flag = d->const_flag;
    id->static_flag = d->static_flag;
    id->extern_flag = d->extern_flag;
    id->inline_flag = d->inline_flag;
    // 清除 DFA 数据中的修饰符标记
    d->const_flag = 0;
    d->static_flag = 0;
    d->extern_flag = 0;
    d->inline_flag = 0;

    return DFA_NEXT_WORD;
}
// 在给定 block 及其父层级中查找函数
static function_t *_type_find_function(block_t *b, const char *name) {
    while (b) {
        // 如果 block 不是文件根或根节点，跳过
        if (!b->node.file_flag && !b->node.root_flag) {
            b = (block_t *)b->node.parent;
            continue;
        }

        assert(b->scope); // 确保作用域存在

        // 在当前作用域查找函数
        function_t *f = scope_find_function(b->scope, name);
        if (f)
            return f; // 找到函数则返回
                      // 向上查找父 block
        b = (block_t *)b->node.parent;
    }

    return NULL; // 没找到函数返回 NULL
}

// 根据标识符解析其类型
int _type_find_type(dfa_t *dfa, dfa_identity_t *id) {
    parse_t *parse = dfa->priv;

    if (!id->identity)
        return 0; // 如果没有标识符，直接返回 0

    // 首先尝试在当前 block 中查找类型
    id->type = block_find_type(parse->ast->current_block, id->identity->text->data);
    if (!id->type) {
        // 如果没找到，在全局类型表查找
        int ret = ast_find_global_type(&id->type, parse->ast, id->identity->text->data);
        if (ret < 0) {
            loge("find global function error\n");
            return DFA_ERROR;
        }
        // 如果仍然没找到，将其默认视为函数指针类型
        if (!id->type) {
            id->type = block_find_type_type(parse->ast->current_block, FUNCTION_PTR);

            if (!id->type) {
                loge("function ptr not support\n");
                return DFA_ERROR;
            }
        }
        // 如果是函数指针，则查找对应函数对象
        if (FUNCTION_PTR == id->type->type) {
            id->func_ptr = _type_find_function(parse->ast->current_block, id->identity->text->data);

            if (!id->func_ptr) {
                loge("can't find funcptr type '%s'\n", id->identity->text->data);
                return DFA_ERROR;
            }
        }
    }
    // 保存标识符词对象，并清空 identity 指针
    id->type_w = id->identity;
    id->identity = NULL;
    return 0;
}

// 处理标识符类型（如变量名、类型名等）
static int _type_action_identity(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;                   // 获取 DFA 的私有解析器指针
    lex_word_t *w = words->data[words->size - 1]; // 获取当前词（标识符）
    dfa_data_t *d = data;                         // DFA 数据上下文
    stack_t *s = d->current_identities;           // 当前标识符栈
    dfa_identity_t *id = NULL;

    if (s->size > 0) {     // 如果栈中已有标识符
        id = stack_top(s); // 获取栈顶标识符

        int ret = _type_find_type(dfa, id); // 尝试解析其类型
        if (ret < 0) {
            loge("\n");
            return ret; // 出错直接返回
        }
    }

    id = calloc(1, sizeof(dfa_identity_t)); // 创建新的标识符对象
    if (!id)
        return DFA_ERROR;

    if (stack_push(s, id) < 0) { // 创建新的标识符对象
        free(id);
        return DFA_ERROR;
    }
    id->identity = w; // 保存词对象

    return DFA_NEXT_WORD; // 继续解析下一个词
}

// 处理 '*' 符号，即指针类型
static int _type_action_star(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;
    dfa_identity_t *id = stack_top(d->current_identities);

    assert(id); // 栈顶标识符必须存在

    if (!id->type) {                        // 如果标识符类型未解析
        int ret = _type_find_type(dfa, id); // 尝试解析类型
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    id->nb_pointers++; // 指针层级 +1

    return DFA_NEXT_WORD;
}
// 处理 ',' 符号，主要用于多返回值函数或多变量声明
static int _type_action_comma(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;
    dfa_identity_t *id = stack_top(d->current_identities);

    assert(id);

    if (!id->type) { // 如果类型未解析
        int ret = _type_find_type(dfa, id);
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    return DFA_NEXT_WORD;
}
// 初始化 DFA 类型模块节点
static int _dfa_init_module_type(dfa_t *dfa) {
    DFA_MODULE_ENTRY(dfa, type); // 模块入口节点
    // 注册 struct/class 判断节点
    DFA_MODULE_NODE(dfa, type, _struct, _type_is__struct, NULL);

    // 注册类型修饰符节点
    DFA_MODULE_NODE(dfa, type, _const, dfa_is_const, _type_action_const);
    DFA_MODULE_NODE(dfa, type, _static, dfa_is_static, _type_action_static);
    DFA_MODULE_NODE(dfa, type, _extern, dfa_is_extern, _type_action_extern);
    DFA_MODULE_NODE(dfa, type, _inline, dfa_is_inline, _type_action_inline);
    // 注册基础类型、标识符、指针、逗号节点
    DFA_MODULE_NODE(dfa, type, base_type, dfa_is_base_type, _type_action_base_type);
    DFA_MODULE_NODE(dfa, type, identity, dfa_is_identity, _type_action_identity);
    DFA_MODULE_NODE(dfa, type, star, dfa_is_star, _type_action_star);
    DFA_MODULE_NODE(dfa, type, comma, dfa_is_comma, _type_action_comma);

    return DFA_OK;
}

// 初始化类型模块语法树关系
static int _dfa_init_syntax_type(dfa_t *dfa) {
    DFA_GET_MODULE_NODE(dfa, type, entry, entry); // 获取入口节点

    // 获取各修饰符节点
    DFA_GET_MODULE_NODE(dfa, type, _const, _const);
    DFA_GET_MODULE_NODE(dfa, type, _static, _static);
    DFA_GET_MODULE_NODE(dfa, type, _extern, _extern);
    DFA_GET_MODULE_NODE(dfa, type, _inline, _inline);

    // 获取类型和标识符节点
    DFA_GET_MODULE_NODE(dfa, type, _struct, _struct);
    DFA_GET_MODULE_NODE(dfa, type, base_type, base_type);
    DFA_GET_MODULE_NODE(dfa, type, identity, var_name);
    DFA_GET_MODULE_NODE(dfa, type, star, star);
    DFA_GET_MODULE_NODE(dfa, type, comma, comma);

    DFA_GET_MODULE_NODE(dfa, identity, identity, type_name); // identity 模块节点

    vector_add(dfa->syntaxes, entry); // 添加到 DFA 语法模块列表

    // 构建入口节点子节点关系（可接的修饰符和类型节点）
    dfa_node_add_child(entry, _static);
    dfa_node_add_child(entry, _extern);
    dfa_node_add_child(entry, _const);
    dfa_node_add_child(entry, _inline);

    dfa_node_add_child(entry, _struct);
    dfa_node_add_child(entry, base_type);
    dfa_node_add_child(entry, type_name);

    // 修饰符后的可接节点
    dfa_node_add_child(_static, _struct);
    dfa_node_add_child(_static, base_type);
    dfa_node_add_child(_static, type_name);

    dfa_node_add_child(_extern, _struct);
    dfa_node_add_child(_extern, base_type);
    dfa_node_add_child(_extern, type_name);

    dfa_node_add_child(_const, _struct);
    dfa_node_add_child(_const, base_type);
    dfa_node_add_child(_const, type_name);

    dfa_node_add_child(_inline, _struct);
    dfa_node_add_child(_inline, base_type);
    dfa_node_add_child(_inline, type_name);

    // 修饰符组合关系
    dfa_node_add_child(_static, _inline);
    dfa_node_add_child(_static, _const);
    dfa_node_add_child(_extern, _const);
    dfa_node_add_child(_inline, _const);

    // struct 后可接类型名
    dfa_node_add_child(_struct, type_name);

    // 多重指针解析
    dfa_node_add_child(star, star); // '**' 情况
    dfa_node_add_child(star, var_name);

    dfa_node_add_child(base_type, star);
    dfa_node_add_child(type_name, star);

    // 基础类型或类型名后可接指针或标识符
    dfa_node_add_child(base_type, var_name);
    dfa_node_add_child(type_name, var_name);

    // 多返回值函数支持
    dfa_node_add_child(base_type, comma);
    dfa_node_add_child(star, comma);
    dfa_node_add_child(comma, _struct);
    dfa_node_add_child(comma, base_type);
    dfa_node_add_child(comma, type_name);
    // 打印 base_type 子节点调试信息
    int i;
    for (i = 0; i < base_type->childs->size; i++) {
        dfa_node_t *n = base_type->childs->data[i];

        logd("n->name: %s\n", n->name);
    }

    return DFA_OK;
}
// 定义 DFA 类型模块
dfa_module_t dfa_module_type =
    {
        .name = "type",
        .init_module = _dfa_init_module_type, // 模块初始化函数
        .init_syntax = _dfa_init_syntax_type, // 模块语法树初始化函数
};
