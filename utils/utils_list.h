#ifndef UTILS_LIST_H
#define UTILS_LIST_H

#include "utils_def.h"


typedef struct list_s list_t;

struct list_s{
    struct list_s* prev;
    struct list_s* next;
};

// 链表初始化
static inline void list_init(list_t* h)
{
    h->prev = h;
    h->next = h;
}

// 删除节点 n
static inline void list_del(list_t* n){
    n->prev->next = n->next;
    n->next->prev = n->prev;

    // only to avoid some wrong operations for these 2 invalid pointers
    n->prev = NULL;
    n->next = NULL;
}

// 在尾部增加
static inline void list_add_tail(list_t* h,list_t* n){
    h->prev->next = n;
    n->prev = h->prev;
    n->next = h;
    h->prev = n;
}

// 在头部增加
static inline void list_add_front(list_t* h,list_t* n){
    h->next->prev = n;
    n->next = h->next;
    n->prev = h;
    h->next = n;
}

// 链表删除两个节点
static inline void list_mov2(list_t* h0,list_t* h1){
    if (h1->next == h1) {
        return;
    }

    h0->prev->next = h1->next;
    h1->next->prev = h0->prev;
    h1->prev->next = h0;
    h0->prev = h1->prev;
}


#define LIST_INIT(h) {&h,&h}

#define list_data(l, type, member)	((type*)((char*)l - offsetof(type, member)))

#define list_head(h) ((h)->next)
#define list_tail(h) ((h)->prev)
#define list_sentinel(h) (h)
#define list_next(l) ((l)->next)
#define list_prev(l) ((l)->prev)
#define list_empty(h) ((h)->next == (h))

// 链表清除
#define list_clear(h, type, member, type_free) \
	do {\
		list_t* l;\
		type*       t;\
		for (l = list_head(h); l != list_sentinel(h);) {\
			t  = list_data(l, type, member);\
			l  = list_next(l);\
			list_del(&t->member);\
			type_free(t);\
			t = NULL;\
		}\
	} while(0)

#define slist_clear(h, type, next, type_free) \
	do { \
		while (h) { \
			type* p = h; \
			h = p->next; \
			type_free(p); \
			p = NULL; \
		} \
	} while (0)

// 链表移除
#define list_mov(dst, src, type, member) \
	do {\
		list_t* l;\
		type*       t;\
		for (l = list_head(src); l != list_sentinel(src);) {\
			t  = list_data(l, type, member);\
			l  = list_next(l);\
			list_del(&t->member);\
			list_add_tail(dst, &t->member);\
		}\
	} while(0)



#endif