#include "dfa.h"
#include "lex_word.h"
#include <unistd.h>

extern dfa_ops_t dfa_ops_parse;

static dfa_ops_t *dfa_ops_array[] =
    {
        &dfa_ops_parse,

        NULL,
};

static int _dfa_node_parse_word(dfa_t *dfa, dfa_node_t *node, vector_t *words, void *data);

// 根据名字删除钩子
void dfa_del_hook_by_name(dfa_hook_t **pp, const char *name) {
    while (*pp) {
        dfa_hook_t *hook = *pp;

        if (!strcmp(name, hook->node->name)) {
            *pp = hook->next;
            free(hook);
            hook = NULL;
            continue;
        }

        pp = &hook->next;
    }
}

// 删除 指定的某一个钩子节点（sentinel），链表中只删除这一个节点
void dfa_del_hook(dfa_hook_t **pp, dfa_hook_t *sentinel) {
    // 遍历链表找到目标节点，把它从链表中断开，释放内存
    while (*pp && *pp != sentinel)
        pp = &(*pp)->next;

    if (*pp) {
        *pp = sentinel->next;
        free(sentinel);
        sentinel = NULL;
    }
}

// 删除从头开始到 sentinel之前的所有节点
// 一直遍历释放节点，知道遇到sentinel停止
// 如果 sentinel为NULL，则会清空整个链表
// 结果是：批量删除一段节点，而不是只删一个
void dfa_clear_hooks(dfa_hook_t **pp, dfa_hook_t *sentinel) {
    while (*pp != sentinel) {
        dfa_hook_t *h = *pp;

        *pp = h->next;

        free(h);
        h = NULL;
    }
}

// 寻找钩子函数
dfa_hook_t *dfa_find_hook(dfa_t *dfa, dfa_hook_t **pp, void *word) {
    while (*pp) {
        dfa_hook_t *h = *pp;

        if (!h->node || !h->node->is) {
            logd("delete invalid hook: %p\n", h);

            *pp = h->next;
            free(h);
            h = NULL;
            continue;
        }

        if (h->node->is(dfa, word)) {
            return h;
        }
        pp = &h->next;
    }

    return NULL;
}

// 分配节点
dfa_node_t *dfa_node_alloc(const char *name, dfa_is_pt is, dfa_action_pt action) {
    if (!name || !is) {
        return NULL;
    }

    dfa_node_t *node = calloc(1, sizeof(dfa_node_t));
    if (!node) {
        return NULL;
    }

    node->name = strdup(name);
    if (!node->name) {
        free(node);
        node = NULL;
        return NULL;
    }

    node->module_index = -1;
    node->is = is;
    node->action = action;
    node->refs = 1;
    return node;
}

// 释放节点
void dfa_node_free(dfa_node_t *node) {
    if (!node)
        return;

    if (--node->refs > 0)
        return;

    assert(0 == node->refs);

    if (node->childs) {
        vector_clear(node->childs, (void (*)(void *))dfa_node_free);
        vector_free(node->childs);
        node->childs = NULL;
    }

    free(node->name);
    free(node);
    node = NULL;
}

// 节点增加孩子
int dfa_node_add_child(dfa_node_t *parent, dfa_node_t *child) {
    if (!parent || !child) {
        loge("\n");
        return -1;
    }

    dfa_node_t *node;
    int i;

    if (parent->childs) {
        for (i = 0; i < parent->childs->size; i++) {
            node = parent->childs->data[i];

            if (!strcmp(child->name, node->name)) {
                logi("repeated: child: %s, parent: %s\n", child->name, parent->name);
                return DFA_REPEATED;
            }
        }

    } else {
        parent->childs = vector_alloc();
        if (!parent->childs)
            return -ENOMEM;
    }

    int ret = vector_add(parent->childs, child);
    if (ret < 0)
        return ret;

    child->refs++;
    return 0;
}

// 打开
int dfa_open(dfa_t **pdfa, const char *name, void *priv) {
    if (!pdfa || !name) {
        loge("\n");
        return -1;
    }

    dfa_ops_t *ops = NULL;

    int i;
    for (i = 0; dfa_ops_array[i]; i++) {
        ops = dfa_ops_array[i];

        if (!strcmp(name, ops->name))
            break;
        ops = NULL;
    }

    if (!ops) {
        loge("\n");
        return -1;
    }

    dfa_t *dfa = calloc(1, sizeof(dfa_t));
    if (!dfa) {
        loge("\n");
        return -1;
    }

    dfa->nodes = vector_alloc();
    dfa->syntaxes = vector_alloc();

    dfa->priv = priv;
    dfa->ops = ops;

    *pdfa = dfa;
    return 0;
}

// 关闭
void dfa_close(dfa_t *dfa) {
    if (!dfa)
        return;

    if (dfa->nodes) {
        vector_clear(dfa->nodes, (void (*)(void *))dfa_node_free);
        vector_free(dfa->nodes);
        dfa->nodes = NULL;
    }

    if (dfa->syntaxes) {
        vector_free(dfa->syntaxes);
        dfa->syntaxes = NULL;
    }

    free(dfa);
    dfa = NULL;
}

// 增加节点
int dfa_add_node(dfa_t *dfa, dfa_node_t *node) {
    if (!dfa || !node)
        return -EINVAL;

    if (!dfa->nodes) {
        dfa->nodes = vector_alloc();
        if (!dfa->nodes)
            return -ENOMEM;
    }

    return vector_add(dfa->nodes, node);
}

// 寻找节点
dfa_node_t *dfa_find_node(dfa_t *dfa, const char *name) {
    if (!dfa || !name)
        return NULL;

    if (!dfa->nodes)
        return NULL;

    dfa_node_t *node;
    int i;

    for (i = 0; i < dfa->nodes->size; i++) {
        node = dfa->nodes->data[i];

        if (!strcmp(name, node->name))
            return node;
    }

    return NULL;
}

// 遍历当前节点的所有子节点，依次尝试匹配当前词 w。支持 pre-hook 优先处理逻辑，如果子节点匹配失败，则返回 DFA_NEXT_SYNTAX
static int _dfa_childs_parse_word(dfa_t *dfa, dfa_node_t **childs, int nb_childs, vector_t *words, void *data) {
    assert(words->size > 0); // 确保词向量不为空

    int i;
    for (i = 0; i < nb_childs; i++) {
        dfa_node_t *child = childs[i];                // 当前子节点
        lex_word_t *w = words->data[words->size - 1]; // 当前解析的词（最新词）

        logd("i: %d, nb_childs: %d, child: %s, w: %s\n", i, nb_childs, child->name, w->text->data);

        // 1. 处理 pre-hook（在节点解析前触发）
        dfa_hook_t *hook = dfa_find_hook(dfa, &(dfa->hooks[DFA_HOOK_PRE]), w);
        if (hook) {
            // 如果 pre-hook 的节点不是当前子节点，则跳过
            if (hook->node != child)
                continue;

            logi("\033[32mpre hook: %s\033[0m\n", hook->node->name);

            // 删除当前 hook 以及之前的 hook
            dfa_clear_hooks(&(dfa->hooks[DFA_HOOK_PRE]), hook->next);
            hook = NULL;

        } else {
            // 2. 如果没有 pre-hook，则用节点自己的匹配函数判断词是否匹配
            assert(child->is);
            if (!child->is(dfa, w))
                continue;
        }

        // 3. 递归调用节点解析
        int ret = _dfa_node_parse_word(dfa, child, words, data);

        if (DFA_OK == ret)
            return DFA_OK; // 成功解析

        else if (DFA_ERROR == ret)
            return DFA_ERROR; // 出错
    }

    logd("DFA_NEXT_SYNTAX\n\n");
    return DFA_NEXT_SYNTAX; // 当前节点及子节点均未匹配，切换到下一个语法节点
}

/*
作用：

    执行节点动作 node->action

    执行 post-hook 和 end-hook

    弹出下一词继续解析

    递归解析子节点

    根据 DFA 返回状态决定切换词、切换节点或返回错误
*/
static int _dfa_node_parse_word(dfa_t *dfa, dfa_node_t *node, vector_t *words, void *data) {
    int ret = DFA_NEXT_WORD; // 默认返回状态为解析下一个词
    lex_word_t *w = words->data[words->size - 1];

    logi("\033[35m%s->action(), w: %s, position: %d,%d\033[0m\n", node->name, w->text->data, w->line, w->pos);

    if (node->action) { // 1. 调用节点动作函数

        ret = node->action(dfa, words, data);
        if (ret < 0)
            return ret; // DFA_ERROR 等错误直接返回

        if (DFA_NEXT_SYNTAX == ret)
            return DFA_NEXT_SYNTAX; // 切换到下一个语法

        if (DFA_SWITCH_TO == ret)
            ret = DFA_NEXT_WORD; // 切换节点，但继续解析当前词
    }

    if (DFA_CONTINUE == ret)
        goto _continue; // 跳过后续逻辑，直接解析子节点

#if 1
    // 打印 post hook 和 end hook 的调试信息
    dfa_hook_t *h = dfa->hooks[DFA_HOOK_POST];
    while (h) {
        logd("\033[32m post hook: %s\033[0m\n", h->node->name);
        h = h->next;
    }

    h = dfa->hooks[DFA_HOOK_END];
    while (h) {
        logd("\033[32m end hook: %s\033[0m\n", h->node->name);
        h = h->next;
    }
    printf("\n");
#endif
    // 2. 处理 post-hook
    dfa_hook_t *hook = dfa_find_hook(dfa, &(dfa->hooks[DFA_HOOK_POST]), w);
    if (hook) {
        dfa_node_t *hook_node = hook->node;

        dfa_clear_hooks(&(dfa->hooks[DFA_HOOK_POST]), hook->next);
        hook = NULL;

        logi("\033[32m post hook: %s->action()\033[0m\n", hook_node->name);

        if (hook_node != node && hook_node->action) {
            ret = hook_node->action(dfa, words, data);

            if (DFA_SWITCH_TO == ret) {
                logi("\033[32m post hook: switch to %s->%s\033[0m\n", node->name, hook_node->name);

                node = hook_node;
                ret = DFA_NEXT_WORD;
            }
        }
    }
    // 3. 处理 end-hook（模块结束钩子）
    if (DFA_OK == ret) {
        dfa_hook_t **pp = &(dfa->hooks[DFA_HOOK_END]);

        while (*pp) {
            dfa_hook_t *hook = *pp;
            dfa_node_t *hook_node = hook->node;
            // 删除 hook
            *pp = hook->next;
            free(hook);
            hook = NULL;

            logi("\033[34m end hook: %s->action()\033[0m\n", hook_node->name);

            if (!hook_node->action)
                continue;

            ret = hook_node->action(dfa, words, data);

            if (DFA_OK == ret)
                continue;

            if (DFA_SWITCH_TO == ret) {
                logi("\033[34m end hook: switch to %s->%s\033[0m\n\n", node->name, hook_node->name);

                node = hook_node;
                ret = DFA_NEXT_WORD;
            }
            break;
        }
    }
    // 4. 根据解析结果处理下一步
    if (DFA_NEXT_WORD == ret) {
        lex_word_t *w = dfa->ops->pop_word(dfa); // 弹出下一个词
        if (!w) {
            loge("DFA_ERROR\n");
            return DFA_ERROR;
        }

        vector_add(words, w); // 加入词向量

        logd("pop w->type: %d, '%s', line: %d, pos: %d\n", w->type, w->text->data, w->line, w->pos);

    } else if (DFA_OK == ret) {
        logi("DFA_OK\n\n");
        return DFA_OK;

    } else if (DFA_CONTINUE == ret) {
        // 继续处理子节点
    } else {
        logd("DFA: %d\n", ret);
        return DFA_ERROR;
    }

_continue:
    if (!node->childs || node->childs->size <= 0) {
        logi("DFA_NEXT_SYNTAX\n");
        return DFA_NEXT_SYNTAX;
    }
    // 5. 递归解析子节点
    ret = _dfa_childs_parse_word(dfa, (dfa_node_t **)node->childs->data, node->childs->size, words, data);
    return ret;
}

/*
作用：

    DFA 解析入口函数

    检查输入合法性

    初始化词向量并将词加入

    调用 _dfa_childs_parse_word 遍历语法节点解析

    错误处理和资源释放
*/
int dfa_parse_word(dfa_t *dfa, void *word, void *data) {
    if (!dfa || !word)
        return -EINVAL;

    if (!dfa->syntaxes || dfa->syntaxes->size <= 0)
        return -EINVAL;

    if (!dfa->ops || !dfa->ops->pop_word)
        return -EINVAL;
    // 1. 分配词向量
    vector_t *words = vector_alloc();
    if (!words)
        return -ENOMEM;

    int ret = vector_add(words, word); // 将首词加入向量
    if (ret < 0)
        return ret;
    // 将首词加入向量
    ret = _dfa_childs_parse_word(dfa, (dfa_node_t **)dfa->syntaxes->data, dfa->syntaxes->size, words, data);
    // 3. 如果解析失败，打印错误信息
    if (DFA_OK != ret) {
        assert(words->size >= 1);

        lex_word_t *w = words->data[words->size - 1];

        loge("ret: %d, w->type: %d, '%s', line: %d\n\n", ret, w->type, w->text->data, w->line);

        ret = DFA_ERROR;
    }
    // 4. 释放词向量
    vector_clear(words, (void (*)(void *))dfa->ops->free_word);
    vector_free(words);
    words = NULL;
    return ret;
}
