#ifndef UTILS_RBTREE_H
#define UTILS_RBTREE_H

#include "utils_def.h"
#include <cstdint>

typedef struct rbtree_s rbtree_t;
typedef struct rbtree_node_s rbtree_node_t;

typedef int (*rbtree_node_do_pt )(rbtree_node_t* node0,void* data);

struct rbtree_node_s{
    rbtree_node_t* parent;
    rbtree_node_t* left;
    rbtree_node_t* right;

    uint16_t depth;
    uint16_t bdepth;
    uint8_t color;
};

struct rbtree_s{
    rbtree_node_t* root;
    rbtree_node_t sentinel;
};

#define RBTREE_BLACK 0
#define RBTREE_RED 1

#define rbtree_sentinel(tree) (&tree->sentinel)

static inline void rbtree_init(rbtree_t* tree){
#if 0
    tree->sentinel.parent = NULL;
    tree->sentinel.left = NULL;
    tree->sentinel.right = NULL;
#else
    tree->sentinel.parent = &tree->sentinel;
    tree->sentinel.left = &tree->sentinel;
    tree->sentinel.right = &tree->sentinel;
#endif
    tree->sentinel.color = RBTREE_BLACK;

    tree->root = &tree->sentinel;
}





#endif