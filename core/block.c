#include "block.h"
#include "scope.h"

// 分配块
block_t *block_alloc(lex_word_t *w) {
    // 分配块
    block_t *b = calloc(1, sizeof(block_t));

    if (!b)
        return NULL;

    // 申请范围
    b->scope = scope_alloc(w, "block");

    if (!b->scope) {
        free(b);
        return NULL;
    }

    // 如果词元没有
    if (w) {
        // 词元克隆
        b->node.w = lex_word_clone(w);

        // 没有分配成功
        if (!b->node.w) {
            scope_free(b->scope);
            free(b);
            return NULL;
        }
    }
    // 设置类型为 BLOCK
    b->node.type = OP_BLOCK;
    return b;
}

// 根据字符串分配块
block_t *block_alloc_cstr(const char *name) {
    block_t *b = calloc(1, sizeof(block_t));

    if (!b)
        return NULL;

    // 设置名字
    b->name = string_cstr(name);

    if (!b->name) {
        free(b);
        return NULL;
    }

    // 范围
    b->scope = scope_alloc(NULL, name);
    if (!b->scope) {
        string_free(b->name);
        free(b);
        return NULL;
    }

    b->node.type = OP_BLOCK;
    return b;
}

// 释放
void block_free(block_t *b) {
    if (b) {
        scope_free(b->scope);
        b->scope = NULL;

        if (b->name) {
            string_free(b->name);
            b->name = NULL;
        }
        node_free((node_t *)b);
    }
}

// 寻找类型
type_t *block_find_type(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                type_t *t = scope_find_type(b->scope, name);
                if (t)
                    return t;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

// 
type_t *block_find_type_type(block_t *b, const int type) {
    assert(b);

    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                type_t *t = scope_find_type_type(b->scope, type);
                if (t)
                    return t;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

// 寻找变量
variable_t *block_find_variable(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                variable_t *v = scope_find_variable(b->scope, name);
                if (v)
                    return v;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

// 寻找函数
function_t *block_find_function(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type || b->node.type >= STRUCT) {
            if (b->scope) {
                function_t *f = scope_find_function(b->scope, name);
                if (f)
                    return f;
            }
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}

// 寻找标签
label_t *block_find_label(block_t *b, const char *name) {
    assert(b);
    while (b) {
        if (OP_BLOCK == b->node.type || FUNCTION == b->node.type) {
            assert(b->scope);

            label_t *l = scope_find_label(b->scope, name);

            if (l)
                return l;
        }
        b = (block_t *)(b->node.parent);
    }
    return NULL;
}
