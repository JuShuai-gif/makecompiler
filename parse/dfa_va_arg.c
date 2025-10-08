#include "dfa.h"
#include "dfa_util.h"
#include "parse.h"
#include "utils_stack.h"

// 声明外部模块
extern dfa_module_t dfa_module_va_arg;

// 声明函数：根据标识符查找类型
int _type_find_type(dfa_t *dfa, dfa_identity_t *id);

// 处理 va_start 关键字，创建 AST 节点并标记 DFA 当前状态
static int _va_arg_action_start(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;                   // 获取解析器上下文
    dfa_data_t *d = data;                         // 获取 DFA 数据
    lex_word_t *w = words->data[words->size - 1]; // 当前词

    // 防止递归使用 va_start / va_arg / va_end
    if (d->current_va_start
        || d->current_va_arg
        || d->current_va_end) {
        loge("recursive 'va_start' in file: %s, line %d\n", w->file->data, w->line);
        return DFA_ERROR;
    }

    // 分配一个 AST 节点表示 va_start
    node_t *node = node_alloc(w, OP_VA_START, NULL);
    if (!node)
        return DFA_ERROR;

    // 将节点添加到当前代码块
    node_add_child((node_t *)parse->ast->current_block, node);
    // 记录当前 DFA 状态正在处理 va_start
    d->current_va_start = node;

    return DFA_NEXT_WORD; // DFA 继续解析下一个词
}

// 处理 va_arg 关键字，创建 AST 节点并添加到表达式或当前块
static int _va_arg_action_arg(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];
    // 防止递归使用
    if (d->current_va_start
        || d->current_va_arg
        || d->current_va_end) {
        loge("recursive 'va_arg' in file: %s, line %d\n", w->file->data, w->line);
        return DFA_ERROR;
    }
    // 创建 AST 节点表示 va_arg
    node_t *node = node_alloc(w, OP_VA_ARG, NULL);
    if (!node)
        return DFA_ERROR;
    // 如果当前正在解析表达式，将节点加入表达式
    if (d->expr)
        expr_add_node(d->expr, node);
    else
        node_add_child((node_t *)parse->ast->current_block, node);
    // 记录当前 DFA 状态正在处理 va_arg
    d->current_va_arg = node;

    return DFA_NEXT_WORD;
}

// 处理 va_end 关键字，创建 AST 节点并标记状态
static int _va_arg_action_end(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];
    // 防止递归使用
    if (d->current_va_start
        || d->current_va_arg
        || d->current_va_end) {
        loge("recursive 'va_end' in file: %s, line %d\n", w->file->data, w->line);
        return DFA_ERROR;
    }
    // 创建 AST 节点表示 va_end
    node_t *node = node_alloc(w, OP_VA_END, NULL);
    if (!node)
        return DFA_ERROR;
    // 添加节点到当前代码块
    node_add_child((node_t *)parse->ast->current_block, node);
    // 记录当前 DFA 状态正在处理 va_end
    d->current_va_end = node;

    return DFA_NEXT_WORD;
}

// 处理左括号 (，为可变参数函数调用创建 DFA 钩子
static int _va_arg_action_lp(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];
    // 必须在 va_start / va_arg / va_end 之一内部
    assert(d->current_va_start
           || d->current_va_arg
           || d->current_va_end);
    // 在 DFA 中注册两个后置钩子：
    // 1. 匹配右括号 ')' 的动作
    DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_rp"), DFA_HOOK_POST);
    // 2. 匹配逗号 ',' 的动作
    DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_comma"), DFA_HOOK_POST);

    return DFA_NEXT_WORD;
}

// 处理 va_list 参数名，检查类型并加入 AST
static int _va_arg_action_ap(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];

    variable_t *ap; // va_list 变量
    type_t *t;      // va_list 类型
    node_t *node;   // AST 节点

    // 在当前块中查找变量
    ap = block_find_variable(parse->ast->current_block, w->text->data);
    if (!ap) {
        loge("va_list variable %s not found\n", w->text->data);
        return DFA_ERROR;
    }

    // 在 AST 中查找 va_list 类型
    if (ast_find_type(&t, parse->ast, "va_list") < 0) {
        loge("type 'va_list' not found, line: %d\n", w->line);
        return DFA_ERROR;
    }
    assert(t);

    // 类型和维度检查
    if (t->type != ap->type || 0 != ap->nb_dimentions) {
        loge("variable %s is not va_list type\n", w->text->data);
        return DFA_ERROR;
    }

    // 创建 AST 节点表示该变量
    node = node_alloc(w, ap->type, ap);
    if (!node)
        return DFA_ERROR;

    // 将变量节点加入到对应 va_start / va_arg / va_end 节点中
    if (d->current_va_start)
        node_add_child(d->current_va_start, node);

    else if (d->current_va_arg)
        node_add_child(d->current_va_arg, node);

    else if (d->current_va_end)
        node_add_child(d->current_va_end, node);
    else {
        loge("\n");
        return DFA_ERROR;
    }

    return DFA_NEXT_WORD;
}

static int _va_arg_action_comma(dfa_t *dfa, vector_t *words, void *data) {
    // 当解析到逗号 ',' 时，在 DFA 中注册一个钩子节点
    // 用于解析下一个参数，可以连续处理多个参数
    DFA_PUSH_HOOK(dfa_find_node(dfa, "va_arg_comma"), DFA_HOOK_POST);

    return DFA_SWITCH_TO; // 切换到下一个状态，继续解析后续参数
}

static int _va_arg_action_fmt(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;                   // 获取解析器私有数据
    dfa_data_t *d = data;                         // DFA 模块数据
    lex_word_t *w = words->data[words->size - 1]; // 当前词（格式化字符串名）

    function_t *f;   // 当前函数
    variable_t *fmt; // 格式化字符串变量
    variable_t *arg; // 函数最后一个参数
    node_t *node;    // AST 节点

    // 在当前块查找格式化字符串变量
    fmt = block_find_variable(parse->ast->current_block, w->text->data);
    if (!fmt) {
        loge("format string %s not found\n", w->text->data);
        return DFA_ERROR;
    }

    // 检查变量类型必须是 char* / int8* / uint8* 类型
    if (VAR_CHAR != fmt->type
        && VAR_I8 != fmt->type
        && VAR_U8 != fmt->type) {
        loge("format string %s is not 'char*' or 'int8*' or 'uint8*' type\n", w->text->data);
        return DFA_ERROR;
    }

    // 指针数必须为 1
    if (variable_nb_pointers(fmt) != 1) {
        loge("format string %s is not 'char*' or 'int8*' or 'uint8*' type\n", w->text->data);
        return DFA_ERROR;
    }

    // 获取当前块对应函数
    f = (function_t *)parse->ast->current_block;

    while (f && FUNCTION != f->node.type)
        f = (function_t *)f->node.parent;

    if (!f) {
        loge("va_list format string %s not in a function\n", w->text->data);
        return DFA_ERROR;
    }

    // 函数必须支持可变参数
    if (!f->vargs_flag) {
        loge("function %s has no variable args\n", f->node.w->text->data);
        return DFA_ERROR;
    }

    // 函数必须至少有一个参数
    if (!f->argv || f->argv->size <= 0) {
        loge("function %s with variable args should have one format string\n", f->node.w->text->data);
        return DFA_ERROR;
    }

    // 检查格式化字符串必须是最后一个参数
    arg = f->argv->data[f->argv->size - 1];

    if (fmt != arg) {
        loge("format string %s is not the last arg of function %s\n", w->text->data, f->node.w->text->data);
        return DFA_ERROR;
    }

    // 创建 AST 节点表示格式化字符串
    node = node_alloc(w, fmt->type, fmt);
    if (!node)
        return DFA_ERROR;

    // 将格式化字符串节点添加到当前 va_start 节点
    if (d->current_va_start)
        node_add_child(d->current_va_start, node);
    else {
        loge("\n");
        return DFA_ERROR;
    }

    return DFA_NEXT_WORD; // 继续解析下一个词
}

static int _va_arg_action_rp(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1]; // 当前词 ')'

    if (d->current_va_arg) {
        // 检查 va_arg 节点是否已有一个子节点（参数值）
        if (d->current_va_arg->nb_nodes != 1) {
            loge("\n");
            return DFA_ERROR;
        }

        // 弹出当前标识符
        dfa_identity_t *id = stack_pop(d->current_identities);
        if (!id) {
            loge("\n");
            return DFA_ERROR;
        }

        // 如果标识符类型未知，则查找类型
        if (!id->type) {
            if (_type_find_type(dfa, id) < 0) {
                loge("\n");
                return DFA_ERROR;
            }
        }

        // 根据标识符类型创建变量
        variable_t *v = VAR_ALLOC_BY_TYPE(id->type_w, id->type, id->const_flag, id->nb_pointers, id->func_ptr);
        if (!v)
            return DFA_ERROR;

        // 创建 AST 节点表示参数
        node_t *node = node_alloc(w, v->type, v);
        if (!node)
            return DFA_ERROR;

        // 添加到 va_arg 节点
        node_add_child(d->current_va_arg, node);

        free(id);
        id = NULL;

        // 清空 va 状态
        d->current_va_start = NULL;
        d->current_va_arg = NULL;
        d->current_va_end = NULL;

        return DFA_NEXT_WORD;
    }

    // 如果不是 va_arg，清空 va 状态
    d->current_va_start = NULL;
    d->current_va_arg = NULL;
    d->current_va_end = NULL;

    return DFA_SWITCH_TO; // 切换到下一个 DFA 状态
}

static int _va_arg_action_semicolon(dfa_t *dfa, vector_t *words, void *data) {
    // 分号表示可变参数语句结束
    return DFA_OK;
}

static int _dfa_init_module_va_arg(dfa_t *dfa) {
    // 注册 DFA 模块节点及其动作函数
    DFA_MODULE_NODE(dfa, va_arg, lp, dfa_is_lp, _va_arg_action_lp);
    DFA_MODULE_NODE(dfa, va_arg, rp, dfa_is_rp, _va_arg_action_rp);

    DFA_MODULE_NODE(dfa, va_arg, start, dfa_is_va_start, _va_arg_action_start);
    DFA_MODULE_NODE(dfa, va_arg, arg, dfa_is_va_arg, _va_arg_action_arg);
    DFA_MODULE_NODE(dfa, va_arg, end, dfa_is_va_end, _va_arg_action_end);

    DFA_MODULE_NODE(dfa, va_arg, ap, dfa_is_identity, _va_arg_action_ap);
    DFA_MODULE_NODE(dfa, va_arg, fmt, dfa_is_identity, _va_arg_action_fmt);

    DFA_MODULE_NODE(dfa, va_arg, comma, dfa_is_comma, _va_arg_action_comma);
    DFA_MODULE_NODE(dfa, va_arg, semicolon, dfa_is_semicolon, _va_arg_action_semicolon);

    // 初始化 va 状态
    parse_t *parse = dfa->priv;
    dfa_data_t *d = parse->dfa_data;

    d->current_va_start = NULL;
    d->current_va_arg = NULL;
    d->current_va_end = NULL;

    return DFA_OK;
}

static int _dfa_init_syntax_va_arg(dfa_t *dfa) {
    // 获取各节点指针
    DFA_GET_MODULE_NODE(dfa, va_arg, lp, lp);
    DFA_GET_MODULE_NODE(dfa, va_arg, rp, rp);

    DFA_GET_MODULE_NODE(dfa, va_arg, start, start);
    DFA_GET_MODULE_NODE(dfa, va_arg, arg, arg);
    DFA_GET_MODULE_NODE(dfa, va_arg, end, end);

    DFA_GET_MODULE_NODE(dfa, va_arg, ap, ap);
    DFA_GET_MODULE_NODE(dfa, va_arg, fmt, fmt);

    DFA_GET_MODULE_NODE(dfa, va_arg, comma, comma);
    DFA_GET_MODULE_NODE(dfa, va_arg, semicolon, semicolon);

    DFA_GET_MODULE_NODE(dfa, type, entry, type);
    DFA_GET_MODULE_NODE(dfa, type, base_type, base_type);
    DFA_GET_MODULE_NODE(dfa, type, star, star);
    DFA_GET_MODULE_NODE(dfa, identity, identity, identity);

    // 配置 DFA 状态之间的子节点关系，实现语法树
    dfa_node_add_child(start, lp);
    dfa_node_add_child(lp, ap);
    dfa_node_add_child(ap, comma);
    dfa_node_add_child(comma, fmt);
    dfa_node_add_child(fmt, rp);
    dfa_node_add_child(rp, semicolon);

    dfa_node_add_child(end, lp);
    dfa_node_add_child(ap, rp);

    dfa_node_add_child(arg, lp);
    dfa_node_add_child(ap, type);

    dfa_node_add_child(base_type, rp);
    dfa_node_add_child(identity, rp);
    dfa_node_add_child(star, rp);

    return 0;
}

dfa_module_t dfa_module_va_arg =
    {
        .name = "va_arg",                       // 模块名称
        .init_module = _dfa_init_module_va_arg, // 初始化模块节点
        .init_syntax = _dfa_init_syntax_va_arg, // 初始化语法节点关系
};
