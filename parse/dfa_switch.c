#include "dfa.h"
#include "dfa_util.h"
#include "parse.h"
#include "utils_stack.h"

extern dfa_module_t dfa_module_switch;

/* ============================
 * switch 模块数据结构
 * 用于管理 switch 语句的解析状态
 * ============================ */
typedef struct {
    int nb_lps;            // 左括号 '(' 的计数
    int nb_rps;            // 右括号 ')' 的计数
    block_t *parent_block; // switch 所在的父代码块
    node_t *parent_node;   // switch 的父 AST 节点（用于恢复 current_node）
    node_t *_switch;       // 当前 switch AST 节点
    node_t *child;         // 子节点（case/default 节点等）
} dfa_switch_data_t;

/* 外部函数声明：用于 expr 添加变量 */
int _expr_add_var(parse_t *parse, dfa_data_t *d);

/* ----------------------------
 * 检查 switch 结束条件
 * ----------------------------
 * switch 语句的结束判断总是返回 1，表示 DFA 节点总是可结束。
 * 具体逻辑由钩子处理。
 */
static int _switch_is_end(dfa_t *dfa, void *word) {
    return 1;
}

/* ----------------------------
 * switch 关键字处理动作
 * ----------------------------
 * - 创建 switch AST 节点 OP_SWITCH
 * - 分配模块数据 dfa_switch_data_t
 * - 将 switch 节点加入 AST：
 *   - 如果有 current_node，则加入 current_node 作为子节点
 *   - 否则加入当前 block 作为子节点
 * - 更新 DFA 状态：
 *   - current_node 指向新创建的 switch 节点
 * - 将模块状态压入栈
 */
static int _switch_action_switch(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1]; // 当前 switch token
    stack_t *s = d->module_datas[dfa_module_switch.index];

    // 创建 switch AST 节点
    node_t *_switch = node_alloc(w, OP_SWITCH, NULL);

    if (!_switch)
        return -ENOMEM;

    // 分配模块状态
    dfa_switch_data_t *sd = calloc(1, sizeof(dfa_switch_data_t));
    if (!sd)
        return -ENOMEM;

    // 将 switch 节点加入 AST
    if (d->current_node)
        node_add_child(d->current_node, _switch);
    else
        node_add_child((node_t *)parse->ast->current_block, _switch);

    // 保存模块状态
    sd->_switch = _switch;
    sd->parent_block = parse->ast->current_block;
    sd->parent_node = d->current_node;
    d->current_node = _switch; // 更新 current_node 为 switch

    stack_push(s, sd); // 压入模块栈

    return DFA_NEXT_WORD;
}

/* ----------------------------
 * 处理 switch '(' 左括号
 * ----------------------------
 * - assert 确保当前没有 expr
 * - expr_local_flag++，表示表达式由模块管理
 * - 注册两个 POST 钩子：
 *   - switch_rp: 匹配右括号 ')'
 *   - switch_lp_stat: 匹配内部嵌套括号
 */
static int _switch_action_lp(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;

    assert(!d->expr);
    d->expr_local_flag++;

    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_rp"), DFA_HOOK_POST);
    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

    return DFA_NEXT_WORD;
}

/* ----------------------------
 * 处理 '(' 左括号统计钩子（嵌套括号）
 * ----------------------------
 * - 获取当前 switch 模块状态
 * - 左括号计数 nb_lps++
 * - 注册 post 钩子 switch_lp_stat 继续监听嵌套括号
 */
static int _switch_action_lp_stat(dfa_t *dfa, vector_t *words, void *data) {
    dfa_data_t *d = data;
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_top(s);

    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

    sd->nb_lps++; // 左括号计数++

    return DFA_NEXT_WORD;
}

/* ----------------------------
 * 处理 ')' 右括号
 * ----------------------------
 * - 获取当前 switch 模块状态
 * - 判断当前是否有表达式 expr，如果没有则报错
 * - 右括号计数 nb_rps++
 * - 当左、右括号数量相等：
 *   - 将 expr 作为 switch 节点子节点
 *   - expr_local_flag--，恢复表达式管理
 *   - 注册 switch_end 钩子结束模块解析
 *   - 返回 DFA_SWITCH_TO，切换状态机
 * - 否则继续注册 hook 等待匹配右括号
 */
static int _switch_action_rp(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_top(s);

    if (!d->expr) {
        loge("\n");
        return DFA_ERROR;
    }

    sd->nb_rps++; // 右括号计数++

    if (sd->nb_rps == sd->nb_lps) { // 括号完全匹配

        assert(0 == sd->_switch->nb_nodes); // switch 节点子节点数量应为 0

        node_add_child(sd->_switch, d->expr); // 将表达式作为 switch 子节点
        d->expr = NULL;
        assert(--d->expr_local_flag >= 0); // expr 生命周期管理恢复

        DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_end"), DFA_HOOK_END); // 注册模块结束钩子

        return DFA_SWITCH_TO; // 状态机切换
    }

    // 括号未匹配，继续注册钩子
    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_rp"), DFA_HOOK_POST);
    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_lp_stat"), DFA_HOOK_POST);

    return DFA_NEXT_WORD;
}


/* ============================
 * switch 模块：用于解析 C 语言 switch-case 语句
 * 包含 switch、case、default、冒号以及模块生命周期管理
 * ============================ */

/* ----------------------------
 * 处理 case 关键字
 * ----------------------------
 * - 为每个 case 创建 AST 节点 OP_CASE
 * - 将节点加入当前 block（即 switch 所在 block）
 * - 注册 colon 钩子，等待冒号解析
 */
static int _switch_action_case(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];// 当前 case token
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_top(s);// 获取当前 switch 状态

    assert(!d->expr);// case 关键字前不应该有表达式
    d->expr_local_flag++;// expr 生命周期由模块管理

	// 创建 case AST 节点
    sd->child = node_alloc(w, OP_CASE, NULL);
    if (!sd->child)
        return DFA_ERROR;

    node_add_child((node_t *)parse->ast->current_block, sd->child);

	// 注册 colon 钩子，等待冒号
    DFA_PUSH_HOOK(dfa_find_node(dfa, "switch_colon"), DFA_HOOK_PRE);

    return DFA_NEXT_WORD;
}


/* ----------------------------
 * 处理 default 关键字
 * ----------------------------
 * - 为 default 创建 AST 节点 OP_DEFAULT
 * - 加入当前 block
 * - 不需要冒号钩子，因为冒号属于 default 结构内部
 */
static int _switch_action_default(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    lex_word_t *w = words->data[words->size - 1];
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_top(s);

    assert(!d->expr);

	// 创建 default AST 节点
    sd->child = node_alloc(w, OP_DEFAULT, NULL);
    if (!sd->child)
        return DFA_ERROR;

    node_add_child((node_t *)parse->ast->current_block, sd->child);

    return DFA_NEXT_WORD;
}


/* ----------------------------
 * 处理冒号 ':'，用于 case 或 default
 * ----------------------------
 * - 如果是 case:
 *   - 必须有表达式 expr
 *   - 将 expr 添加到 case 节点
 *   - 检查是否需要将表达式添加为变量（identity）
 * - 如果是 default:
 *   - 不需要 expr，直接通过
 */
static int _switch_action_colon(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_top(s);

    if (OP_CASE == sd->child->type) {
        if (!d->expr) {
            loge("NOT found the expr for case\n");
            return DFA_ERROR;
        }

		// 如果 case 的表达式是标识符，尝试加入变量表
        dfa_identity_t *id = stack_top(d->current_identities);

        if (id && id->identity) {
            if (_expr_add_var(parse, d) < 0)
                return DFA_ERROR;
        }

        node_add_child(sd->child, d->expr);// expr 加入 case 节点
        d->expr = NULL;
        assert(--d->expr_local_flag >= 0);// expr 生命周期恢复

    } else {
		// default 情况
        assert(OP_DEFAULT == sd->child->type);
        assert(!d->expr);
    }

    return DFA_OK;
}


/* ----------------------------
 * switch 模块结束处理
 * ----------------------------
 * - 弹出模块栈，恢复 parent_node
 * - 释放 switch 模块数据
 */
static int _switch_action_end(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    stack_t *s = d->module_datas[dfa_module_switch.index];
    dfa_switch_data_t *sd = stack_pop(s);

    assert(parse->ast->current_block == sd->parent_block);


	// 恢复父节点
    d->current_node = sd->parent_node;

    logi("switch: %d, sd: %p, s->size: %d\n", sd->_switch->w->line, sd, s->size);

    free(sd);
    sd = NULL;

    assert(s->size >= 0);

    return DFA_OK;
}


/* ----------------------------
 * 初始化 switch 模块
 * ----------------------------
 * - 定义 DFA 节点与对应动作函数
 * - 分配模块数据栈
 */
static int _dfa_init_module_switch(dfa_t *dfa) {
    DFA_MODULE_NODE(dfa, switch, lp, dfa_is_lp, _switch_action_lp);
    DFA_MODULE_NODE(dfa, switch, rp, dfa_is_rp, _switch_action_rp);
    DFA_MODULE_NODE(dfa, switch, lp_stat, dfa_is_lp, _switch_action_lp_stat);
    DFA_MODULE_NODE(dfa, switch, colon, dfa_is_colon, _switch_action_colon);

    DFA_MODULE_NODE(dfa, switch, _switch, dfa_is_switch, _switch_action_switch);
    DFA_MODULE_NODE(dfa, switch, _case, dfa_is_case, _switch_action_case);
    DFA_MODULE_NODE(dfa, switch, _default, dfa_is_default, _switch_action_default);
    DFA_MODULE_NODE(dfa, switch, end, _switch_is_end, _switch_action_end);

    parse_t *parse = dfa->priv;
    dfa_data_t *d = parse->dfa_data;
    stack_t *s = d->module_datas[dfa_module_switch.index];

    assert(!s);

	// 分配模块栈
    s = stack_alloc();
    if (!s) {
        logi("\n");
        return DFA_ERROR;
    }

    d->module_datas[dfa_module_switch.index] = s;

    return DFA_OK;
}


/* ----------------------------
 * 释放 switch 模块
 * ----------------------------
 * - 释放模块栈
 */
static int _dfa_fini_module_switch(dfa_t *dfa) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = parse->dfa_data;
    stack_t *s = d->module_datas[dfa_module_switch.index];

    if (s) {
        stack_free(s);
        s = NULL;
        d->module_datas[dfa_module_switch.index] = NULL;
    }

    return DFA_OK;
}


/* ----------------------------
 * 初始化 switch 模块语法节点
 * ----------------------------
 * - 定义节点之间的父子关系
 * - 构建 AST 解析路径
 */
static int _dfa_init_syntax_switch(dfa_t *dfa) {
	// 获取模块节点
    DFA_GET_MODULE_NODE(dfa, switch, lp, lp);
    DFA_GET_MODULE_NODE(dfa, switch, rp, rp);
    DFA_GET_MODULE_NODE(dfa, switch, lp_stat, lp_stat);
    DFA_GET_MODULE_NODE(dfa, switch, colon, colon);

    DFA_GET_MODULE_NODE(dfa, switch, _switch, _switch);
    DFA_GET_MODULE_NODE(dfa, switch, _case, _case);
    DFA_GET_MODULE_NODE(dfa, switch, _default, _default);
    DFA_GET_MODULE_NODE(dfa, switch, end, end);

    DFA_GET_MODULE_NODE(dfa, expr, entry, expr);
    DFA_GET_MODULE_NODE(dfa, block, entry, block);


	// 构建 switch -> ( expr ) block 的 AST 关系
    dfa_node_add_child(_switch, lp);
    dfa_node_add_child(lp, expr);
    dfa_node_add_child(expr, rp);
    dfa_node_add_child(rp, block);


	// 构建 case/default -> expr -> colon 的关系
    dfa_node_add_child(_case, expr);
    dfa_node_add_child(expr, colon);
    dfa_node_add_child(_default, colon);

    return 0;
}


/* ----------------------------
 * switch 模块结构体
 * ----------------------------
 * - 提供初始化、语法初始化和结束函数
 */
dfa_module_t dfa_module_switch =
    {
        .name = "switch",

        .init_module = _dfa_init_module_switch,
        .init_syntax = _dfa_init_syntax_switch,

        .fini_module = _dfa_fini_module_switch,
};
