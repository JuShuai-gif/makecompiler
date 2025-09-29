#include "node.h"

// 获取操作数
variable_t *_mc_operand_get(const node_t *node) {
    // 判断节点类型是否是变量类型
    if (type_is_var(node->type))
        return node->var;
    else if (type_is_operator(node->type))// 是否是操作类型
        return node->result;

    return NULL;
}

// 获取函数节点
function_t *_mc_function_get(node_t *node) {
    while (node) {
        // 先判断节点类型是否是函数
        if (FUNCTION == node->type)
            return (function_t *)node;
        // 返回节点的父节点
        node = node->parent;
    }
    return NULL;
}

// 节点分配空间
node_t *mc_node_alloc(lex_word_t *w, int type, variable_t *var) {
    // 首先分配一个节点空间
    node_t *node = calloc(1, sizeof(node_t));
    if (!node) {
        loge("node alloc failed\n");
        return NULL;
    }

    // 如果类型是一个变量
    if (type_is_var(type)) {
        node->var = variable_ref(var);
        if (!node->var) {
            loge("node var clone failed\n");
            goto _failed;
        }
    } else {
        if (w) {
            node->w = lex_word_clone(w);
            if (!node->w) {
                loge("node word clone failed\n");
                goto _failed;
            }
        } else
            node->w = NULL;
    }

    if (w) {
        node->debug_w = lex_word_clone(w);
        if (!node->debug_w) {
            loge("node word clone failed\n");
            goto _failed;
        }
    }

    node->type = type;

    logd("node: %p, node->type: %d\n", node, node->type);
    return node;

_failed:
    mc_node_free(node);
    return NULL;
}

node_t *mc_node_clone(node_t *node) {
    node_t *dst = calloc(1, sizeof(node_t));
    if (!dst)
        return NULL;

    if (type_is_var(node->type)) {
        dst->var = variable_ref(node->var);
        if (!dst->var)
            goto failed;

    } else if (LABEL == node->type)
        dst->label = node->label;
    else {
        if (node->w) {
            dst->w = lex_word_clone(node->w);
            if (!dst->w)
                goto failed;
        }
    }

    if (node->debug_w) {
        dst->debug_w = lex_word_clone(node->debug_w);

        if (!dst->debug_w)
            goto failed;
    }

    dst->type = node->type;

    dst->root_flag = node->root_flag;
    dst->file_flag = node->file_flag;
    dst->class_flag = node->class_flag;
    dst->union_flag = node->union_flag;
    dst->define_flag = node->define_flag;
    dst->const_flag = node->const_flag;
    dst->split_flag = node->split_flag;
    dst->semi_flag = node->semi_flag;
    return dst;

failed:
    mc_node_free(dst);
    return NULL;
}

node_t *mc_node_alloc_label(label_t *l) {
    node_t *node = calloc(1, sizeof(node_t));
    if (!node) {
        loge("node alloc failed\n");
        return NULL;
    }

    node->type = LABEL;
    node->label = l;
    return node;
}

int mc_node_add_child(node_t *parent, node_t *child) {
    if (!parent)
        return -EINVAL;

    void *p = realloc(parent->nodes, sizeof(node_t *) * (parent->nb_nodes + 1));
    if (!p)
        return -ENOMEM;

    parent->nodes = p;
    parent->nodes[parent->nb_nodes++] = child;

    if (child)
        child->parent = parent;

    return 0;
}

void mc_node_del_child(node_t *parent, node_t *child) {
    if (!parent)
        return;

    int i;
    for (i = 0; i < parent->nb_nodes; i++) {
        if (child == parent->nodes[i])
            break;
    }

    for (++i; i < parent->nb_nodes; i++)
        parent->nodes[i - 1] = parent->nodes[i];

    parent->nodes[--parent->nb_nodes] = NULL;
}

void mc_node_free_data(node_t *node) {
    if (!node)
        return;

    if (type_is_var(node->type)) {
        if (node->var) {
            variable_free(node->var);
            node->var = NULL;
        }
    } else if (LABEL == node->type) {
        if (node->label) {
            mc_label_free(node->label);
            node->label = NULL;
        }
    } else {
        if (node->w) {
            lex_word_free(node->w);
            node->w = NULL;
        }
    }

    if (node->debug_w) {
        lex_word_free(node->debug_w);
        node->debug_w = NULL;
    }

    if (node->result) {
        variable_free(node->result);
        node->result = NULL;
    }

    if (node->result_nodes) {
        vector_clear(node->result_nodes, (void (*)(void *))mc_node_free);
        vector_free(node->result_nodes);
    }

    int i;
    for (i = 0; i < node->nb_nodes; i++) {
        if (node->nodes[i]) {
            mc_node_free(node->nodes[i]);
            node->nodes[i] = NULL;
        }
    }
    node->nb_nodes = 0;

    if (node->nodes) {
        free(node->nodes);
        node->nodes = NULL;
    }
}

void mc_node_move_data(node_t *dst, node_t *src) {
    dst->type = src->type;
    dst->nb_nodes = src->nb_nodes;
    dst->nodes = src->nodes;
    dst->var = src->var; // w, label share same pointer

    dst->debug_w = src->debug_w;

    dst->priority = src->priority;
    dst->op = src->op;
    dst->result = src->result;

    dst->result_nodes = src->result_nodes;
    dst->split_parent = src->split_parent;

    dst->root_flag = src->root_flag;
    dst->file_flag = src->file_flag;
    dst->class_flag = src->class_flag;
    dst->union_flag = src->union_flag;
    dst->define_flag = src->define_flag;
    dst->const_flag = src->const_flag;
    dst->split_flag = src->split_flag;

    int i;
    for (i = 0; i < dst->nb_nodes; i++) {
        if (dst->nodes[i])
            dst->nodes[i]->parent = dst;
    }

    if (dst->result_nodes) {
        node_t *res;

        for (i = 0; i < dst->result_nodes->size; i++) {
            res = dst->result_nodes->data[i];

            res->split_parent = dst;
        }
    }

    src->nb_nodes = 0;
    src->nodes = NULL;
    src->var = NULL;
    src->op = NULL;
    src->result = NULL;

    src->result_nodes = NULL;
}

void mc_node_free(node_t *node) {
    if (!node)
        return;

    mc_node_free_data(node);

    node->parent = NULL;

    free(node);
    node = NULL;
}

void mc_node_print(node_t *node) {
    if (node) {
        logw("node: %p, type: %d", node, node->type);

        if (LABEL == node->type) {
            if (node->label && node->label->w)
                printf(", w: %s, line: %d", node->label->w->text->data, node->label->w->line);

        } else if (type_is_var(node->type)) {
            if (node->var && node->var->w)
                printf(", w: %s, line: %d", node->var->w->text->data, node->var->w->line);

        } else if (node->w) {
            printf(", w: %s, line:%d", node->w->text->data, node->w->line);
        }
        printf("\n");
    }
}

// BFS search
int mc_node_search_bfs(node_t *root, void *arg, vector_t *results, int max, mc_node_find_pt find) {
    if (!root || !results || !find)
        return -EINVAL;

    // BFS search
    vector_t *queue = vector_alloc();
    if (!queue)
        return -ENOMEM;

    vector_t *checked = vector_alloc();
    if (!queue) {
        vector_free(queue);
        return -ENOMEM;
    }

    int ret = vector_add(queue, root);
    if (ret < 0)
        goto failed;

    ret = 0;
    int i = 0;
    while (i < queue->size) {
        node_t *node = queue->data[i];

        int j;
        for (j = 0; j < checked->size; j++) {
            if (node == checked->data[j])
                goto next;
        }

        ret = find(node, arg, results);
        if (ret < 0)
            break;

        if (max > 0 && results->size == max)
            break;

        if (vector_add(checked, node) < 0) {
            ret = -ENOMEM;
            break;
        }

        if (ret > 0) {
            logd("jmp node's child, type: %d,  FUNCTION: %d\n", node->type, FUNCTION);
            goto next;
        }

        for (j = 0; j < node->nb_nodes; j++) {
            assert(node->nodes);

            if (!node->nodes[j])
                continue;

            ret = vector_add(queue, node->nodes[j]);
            if (ret < 0)
                goto failed;
        }
    next:
        i++;
    }

failed:
    vector_free(queue);
    vector_free(checked);
    queue = NULL;
    checked = NULL;
    return ret;
}
