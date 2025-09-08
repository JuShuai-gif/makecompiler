#ifndef UTILS_STACK_H
#define UTILS_STACK_H

#include "utils_vector.h"

typedef vector_t stack_t;

// 栈分配
static inline stack_t *stack_alloc() {
    return vector_alloc();
}

// 栈添加一个节点(末尾添加)
static inline int stack_push(stack_t *s, void *node) {
    return vector_add(s, node);
}

// 栈吐出一个节点
static inline void *stack_pop(stack_t *s) {
    if (!s || !s->data)
        return NULL;

    assert(s->size >= 0);

    if (0 == s->size)
        return NULL;

    void *node = s->data[--s->size];

    if (s->size + NB_MEMBER_INC * 2 < s->capacity) {
        void *p = realloc(s->data, sizeof(void *) * (s->capacity - NB_MEMBER_INC));
        if (p) {
            s->data = p;
            s->capacity -= NB_MEMBER_INC;
        }
    }

    return node;
}

// 栈顶
static inline void *stack_top(stack_t *s) {
    if (!s || !s->data)
        return NULL;

    assert(s->size >= 0);

    if (0 == s->size)
        return NULL;
    return s->data[s->size - 1];
}

// 释放栈
static inline void stack_free(stack_t *s) {
    vector_free(s);
}

#endif