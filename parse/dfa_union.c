#include "dfa.h"
#include "dfa_util.h"
#include "parse.h"

extern dfa_module_t dfa_module_union;

// union 模块的数据结构
typedef struct {
    lex_word_t *current_identity; // 当前 union 的标识符（名字），可能为空表示匿名 union

    block_t *parent_block; // union 所在的父 block

    type_t *current_union; // 当前解析的 union 类型对象

    dfa_hook_t *hook; // DFA hook，用于在解析特定节点后执行动作

    int nb_lbs; // 左花括号数量计数，用于匹配成对的 {}
    int nb_rbs; // 右花括号数量计数

} union_module_data_t;

// 处理 union 后跟标识符的情况（union 名称）
static int _union_action_identity(dfa_t *dfa, vector_t *words, void *data) {
    // 获取 DFA 私有解析器
    parse_t *parse = dfa->priv;
    // DFA 数据上下文
    dfa_data_t *d = data;
    // 获取 union 模块数据
    union_module_data_t *md = d->module_datas[dfa_module_union.index];
    // 当前词（标识符）
    lex_word_t *w = words->data[words->size - 1];

    // 如果已经有一个当前标识符，说明重复定义，报错
    if (md->current_identity) {
        loge("\n");
        return DFA_ERROR;
    }

    // 尝试在当前 block 查找类型（可能已经存在同名类型）
    type_t *t = block_find_type(parse->ast->current_block, w->text->data);
    if (!t) {
        // 如果类型不存在，创建新类型对象
        t = type_alloc(w, w->text->data, STRUCT + parse->ast->nb_structs, 0);
        if (!t) {
            loge("type alloc failed\n");
            return DFA_ERROR;
        }

        parse->ast->nb_structs++;                                         // AST 中结构体计数加 1
        t->node.union_flag = 1;                                           // 标记为 union
        scope_push_type(parse->ast->current_block->scope, t);             // 将类型加入当前作用域
        node_add_child((node_t *)parse->ast->current_block, (node_t *)t); // 加入 AST 树
    }

    // 保存当前 union 标识符
    md->current_identity = w;
    // 保存父 block
    md->parent_block = parse->ast->current_block;

    // 继续解析下一个词
    return DFA_NEXT_WORD;
}

// 处理 union 后的左花括号 '{'，开始解析 union 内容
static int _union_action_lb(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    union_module_data_t *md = d->module_datas[dfa_module_union.index];
    lex_word_t *w = words->data[words->size - 1];
    type_t *t = NULL;

    if (md->current_identity) { // 如果 union 有名字

        t = block_find_type(parse->ast->current_block, md->current_identity->text->data);
        if (!t) {
            loge("type '%s' not found\n", md->current_identity->text->data);
            return DFA_ERROR;
        }
    } else {                                                                // 匿名 union
        t = type_alloc(w, "anonymous", STRUCT + parse->ast->nb_structs, 0); // 创建匿名类型
        if (!t) {
            loge("type alloc failed\n");
            return DFA_ERROR;
        }

        parse->ast->nb_structs++;
        t->node.union_flag = 1;
        scope_push_type(parse->ast->current_block->scope, t);             // 入当前作用域
        node_add_child((node_t *)parse->ast->current_block, (node_t *)t); // 加入 AST 树
    }

    // 如果 union 的作用域还未创建，分配一个新的作用域
    if (!t->scope)
        t->scope = scope_alloc(w, "union");

    md->parent_block = parse->ast->current_block; // 保存父 block
    md->current_union = t;                        // 保存当前 union 类型
    md->nb_lbs++;                                 // 左花括号计数 +1

    parse->ast->current_block = (block_t *)t; // 当前 block 切换到 union 类型
                                              // 设置 hook，在解析到 union 的分号节点时触发 post 动作
    md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "union_semicolon"), DFA_HOOK_POST);

    return DFA_NEXT_WORD;
}

// 计算 union 的大小
// union 的大小 = 最大成员的大小，因为 union 的所有成员共用同一块内存
static int _union_calculate_size(dfa_t *dfa, type_t *s) {
    variable_t *v;

    int max_size = 0; // union 的最终大小
    int i;
    int j;

    for (i = 0; i < s->scope->vars->size; i++) {
        v = s->scope->vars->data[i]; // 遍历 union 的每个成员

        assert(v->size >= 0); // 成员大小必须 >=0

        int size = 0;

        if (v->nb_dimentions > 0) { // 如果成员是数组
            v->capacity = 1;

            for (j = 0; j < v->nb_dimentions; j++) {
                if (v->dimentions[j].num < 0) { // 数组长度不能小于 0
                    loge("number of %d-dimention for array '%s' is less than 0, number: %d, file: %s, line: %d\n",
                         j, v->w->text->data, v->dimentions[j].num, v->w->file->data, v->w->line);
                    return DFA_ERROR;
                }

                if (0 == v->dimentions[j].num && j < v->nb_dimentions - 1) {
                    // 只有数组最后一维可以为 0
                    loge("only the number of array's last dimention can be 0, array '%s', dimention: %d, file: %s, line: %d\n",
                         v->w->text->data, j, v->w->file->data, v->w->line);
                    return DFA_ERROR;
                }

                v->capacity *= v->dimentions[j].num; // 总容量 = 各维度元素乘积
            }

            size = v->size * v->capacity; // 数组占用总字节数
        } else
            size = v->size; // 普通成员大小

        if (max_size < size)
            max_size = size; // union 的大小取最大成员大小

        logi("union '%s', member: '%s', size: %d, v->dim: %d, v->capacity: %d\n",
             s->name->data, v->w->text->data, v->size, v->nb_dimentions, v->capacity);
    }
    s->size = max_size; // 保存 union 大小

    logi("union '%s', size: %d\n", s->name->data, s->size);
    return 0;
}

// 处理 union 右花括号 '}'，即 union 解析结束
static int _union_action_rb(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;                                        // 获取 DFA 私有解析器
    dfa_data_t *d = data;                                              // DFA 数据上下文
    union_module_data_t *md = d->module_datas[dfa_module_union.index]; // 获取 union 模块数据

    // 计算 union 大小（根据成员大小最大值）
    if (_union_calculate_size(dfa, md->current_union) < 0) {
        loge("\n");
        return DFA_ERROR;
    }
    // 右花括号计数 +1
    md->nb_rbs++;
    // 检查左右花括号匹配
    assert(md->nb_rbs == md->nb_lbs);
    // 解析完成，将 AST 当前 block 恢复为 union 所在父 block
    parse->ast->current_block = md->parent_block;
    md->parent_block = NULL;

    return DFA_NEXT_WORD; // 继续解析下一个词
}

// 处理 union 成员变量定义
static int _union_action_var(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    union_module_data_t *md = d->module_datas[dfa_module_union.index];
    lex_word_t *w = words->data[words->size - 1]; // 当前成员变量名

    // 如果 union 未初始化，报错
    if (!md->current_union) {
        loge("\n");
        return DFA_ERROR;
    }

    // 创建变量对象并绑定到当前 union 类型
    variable_t *var = variable_alloc(w, md->current_union);
    if (!var) {
        loge("var alloc failed\n");
        return DFA_ERROR;
    }

    // 将变量加入当前 block 的作用域
    scope_push_var(parse->ast->current_block->scope, var);

    logi("union var: '%s', type: %d, size: %d\n", w->text->data, var->type, var->size);

    return DFA_NEXT_WORD;
}

// 处理 union 末尾分号 ';'
static int _union_action_semicolon(dfa_t *dfa, vector_t *words, void *data) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = data;
    union_module_data_t *md = d->module_datas[dfa_module_union.index];

    // 如果 union 已经闭合
    if (md->nb_rbs == md->nb_lbs) {
        logi("DFA_OK\n");

        //		parse->ast->current_block = md->parent_block;
        //		md->parent_block     = NULL;
        // 重置 union 模块数据
        md->current_identity = NULL;
        md->current_union = NULL;
        md->nb_lbs = 0;
        md->nb_rbs = 0;
        // 删除 hook
        dfa_del_hook(&(dfa->hooks[DFA_HOOK_POST]), md->hook);
        md->hook = NULL;

        return DFA_OK;
    }
    // 如果 union 尚未闭合，再次设置 hook 等待分号触发
    md->hook = DFA_PUSH_HOOK(dfa_find_node(dfa, "union_semicolon"), DFA_HOOK_POST);

    logi("\n");
    return DFA_SWITCH_TO; // 切换状态，等待后续解析
}

// 初始化 union 模块节点
static int _dfa_init_module_union(dfa_t *dfa) {
    DFA_MODULE_NODE(dfa, union, _union, dfa_is_union, NULL); // union 关键字节点

    DFA_MODULE_NODE(dfa, union, identity, dfa_is_identity, _union_action_identity); // union 名称

    DFA_MODULE_NODE(dfa, union, lb, dfa_is_lb, _union_action_lb);                      // '{'
    DFA_MODULE_NODE(dfa, union, rb, dfa_is_rb, _union_action_rb);                      // '}'
    DFA_MODULE_NODE(dfa, union, semicolon, dfa_is_semicolon, _union_action_semicolon); // ';'

    DFA_MODULE_NODE(dfa, union, var, dfa_is_identity, _union_action_var); // 成员变量

    parse_t *parse = dfa->priv;
    dfa_data_t *d = parse->dfa_data;
    union_module_data_t *md = d->module_datas[dfa_module_union.index];

    assert(!md);
    // 分配 union 模块数据
    md = calloc(1, sizeof(union_module_data_t));
    if (!md) {
        loge("\n");
        return DFA_ERROR;
    }

    d->module_datas[dfa_module_union.index] = md;

    return DFA_OK;
}

// 释放 union 模块数据
static int _dfa_fini_module_union(dfa_t *dfa) {
    parse_t *parse = dfa->priv;
    dfa_data_t *d = parse->dfa_data;
    union_module_data_t *md = d->module_datas[dfa_module_union.index];

    if (md) {
        free(md);
        md = NULL;
        d->module_datas[dfa_module_union.index] = NULL;
    }

    return DFA_OK;
}

// 初始化 union 模块语法树
static int _dfa_init_syntax_union(dfa_t *dfa) {
    DFA_GET_MODULE_NODE(dfa, union, _union, _union);
    DFA_GET_MODULE_NODE(dfa, union, identity, identity);
    DFA_GET_MODULE_NODE(dfa, union, lb, lb);
    DFA_GET_MODULE_NODE(dfa, union, rb, rb);
    DFA_GET_MODULE_NODE(dfa, union, semicolon, semicolon);
    DFA_GET_MODULE_NODE(dfa, union, var, var);

    // type 模块的入口节点，用于 union 成员
    DFA_GET_MODULE_NODE(dfa, type, entry, member);

    // 将 union 模块入口加入 DFA 语法列表
    vector_add(dfa->syntaxes, _union);

    // union 开始：可接标识符
    dfa_node_add_child(_union, identity);
    // union 名称后可接 ';'（空 union）或 '{'
    dfa_node_add_child(identity, semicolon);
    dfa_node_add_child(identity, lb);

    // 匿名 union 可直接 '{'
    dfa_node_add_child(_union, lb);

    // 空 union：{}，左花括号直接接右花括号
    dfa_node_add_child(lb, rb);

    // 成员变量定义
    dfa_node_add_child(lb, member);
    dfa_node_add_child(member, semicolon);
    dfa_node_add_child(semicolon, rb);
    dfa_node_add_child(semicolon, member);

    // union 结束后可定义变量或分号
    dfa_node_add_child(rb, var);
    dfa_node_add_child(var, semicolon);
    dfa_node_add_child(rb, semicolon);

    return 0;
}
// 定义 DFA union 模块结构
dfa_module_t dfa_module_union =
    {
        .name = "union",
        .init_module = _dfa_init_module_union, // 初始化模块节点
        .init_syntax = _dfa_init_syntax_union, // 初始化模块语法树

        .fini_module = _dfa_fini_module_union, // 释放模块数据
};
