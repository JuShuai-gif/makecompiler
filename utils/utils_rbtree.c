#include "utils_rbtree.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <stdint.h>

static rbtree_node_t *_rbtree_min(rbtree_t *tree, rbtree_node_t *root) {
    assert(tree && root);

    while (root->left != &tree->sentinel) {
        root = root->left;
    }
    return root;
}

static rbtree_node_t *_rbtree_max(rbtree_t *tree, rbtree_node_t *root) {
    assert(tree);

    while (root->right != &tree->sentinel) {
        root = root->right;
    }

    return root;
}

rbtree_node_t *rbtree_min(rbtree_t *tree, rbtree_node_t *root) {
    if (!tree || !root) {
        return NULL;
    }
    rbtree_node_t *x = _rbtree_min(tree, root);

    if (&tree->sentinel == x) {
        return NULL;
    }
    return x;
}

rbtree_node_t *rbtree_max(rbtree_t *tree, rbtree_node_t *root) {
    if (!tree || !root) {
        return NULL;
    }
    rbtree_node_t *x = _rbtree_max(tree, root);

    if (&tree->sentinel == x) {
        return NULL;
    }
    return x;
}

static rbtree_node_t *_rbtree_next(rbtree_t *tree, rbtree_node_t *x) {
    assert(tree && x);

    if (x->right != &tree->sentinel) {
        return _rbtree_min(tree, x->right);
    }

    rbtree_node_t *y = x->parent;

    while (y != &tree->sentinel && x == y->right) {
        x = y;
        y = y->parent;
    }
    return y;
}

/* left rotate

    px          px                 px (py)      px (py)
    |           |                  |            |
    x           x                  y            y
   / \         / \                / \          / \
  xl  y  -->  xl  yl  y   --> x     yr   -->  x   yr
     / \             / \     / \             / \
    yl  yr             yr   xl yl           xl yl
*/
static void _left_rotate(rbtree_t *tree, rbtree_node_t *x) {
    rbtree_node_t *y = x->right;

    assert(y != &tree->sentinel);

    x->right = y->left;
    if (&tree->sentinel != y->left) {
        y->left->parent = x;
    }

    y->parent = x->parent;
    if (&tree->sentinel == x->parent) {
        tree->root = y;
    } else if (x->parent->left == x) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    y->left = x;
    x->parent = y;
}

/* right rotate

       py             py         py (px)           py (px)
       |              |          |                 |
       y              y          x                 x
      / \            / \        / \               / \
     x  yr -->  x   xr yr -->  xl     y   -->    xl  y
    / \        / \                   / \            / \
   xl xr      xl                    xr  yr         xr  yr
*/
static void _right_rotate(rbtree_t *tree, rbtree_node_t *y) {
    rbtree_node_t *x = y->left;

    assert(x != &tree->sentinel);

    y->left = x->right;
    if (&tree->sentinel != x->right) {
        x->right->parent = y;
    }

    x->parent = y->parent;
    if (&tree->sentinel == y->parent) {
        tree->root = x;

    } else if (y->parent->left == y) {
        y->parent->left = x;
    } else {
        y->parent->right = x;
    }

    x->right = y;
    y->parent = x;
}

static void _rb_insert_fixup(rbtree_t *tree, rbtree_node_t *z) {
    /*
    gp: z's grand-parent
    zp: z's parent
    zu: z's uncle
    */
    rbtree_node_t *y;

    while (RBTREE_RED == z->parent->color) {
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right; // y is z's uncle

            if (y->color == RBTREE_RED) {
                /*
                        gp (black)
                       /  \
                (red) zp   zu (red)
                      |
                      z (red)
                */
                y->color = RBTREE_BLACK;
                z->parent->color = RBTREE_BLACK;
                z->parent->parent->color = RBTREE_RED;

                z = z->parent->parent;

            } else {
                if (z == z->parent->right) {
                    /*
                            gp (black)                 gp (black)
                           /  \           left        / \
                    (red) zp   zu (black) ---> (red) z   zu (black)
                         /  \                       / \
                        zb   z (red)     (z', red) zp
                                                  /
                                                 zb
                    */
                    z = z->parent;
                    _left_rotate(tree, z);
                }
                /*
                                gp (red)               (black) z
                               / \           right            / \
                      (black) z   zu (black) --->  (z', red) zp  gp (red)
                             / \                            /    / \
                  (z', red) zp                             zb       zu (black)
                           /
                          zb
                */
                z->parent->color = RBTREE_BLACK;
                z->parent->parent->color = RBTREE_RED;
                _right_rotate(tree, z->parent->parent);
            }
        } else {
            y = z->parent->parent->left; // y is z's uncle

            if (y->color == RBTREE_RED) {
                y->color = RBTREE_BLACK;
                z->parent->color = RBTREE_BLACK;
                z->parent->parent->color = RBTREE_RED;

                z = z->parent->parent;

            } else {
                if (z == z->parent->left) {
                    /*
                              gp (black)                   gp (black)
                             /  \         right           / \
                    (black) zu   zp (red) --->   (black) zu  z (red)
                                /  \                        / \
                         (red) z   zb                          zp (red, z')
                                                                \
                                                                 zb
                    */

                    z = z->parent;
                    _right_rotate(tree, z);
                }

                /*
                           gp (red)                        z (black)
                          / \              left           / \
                (black) zu   z (black)     --->    (red) gp  zp (red, z')
                            / \                         / \   \
                               zp (red, z')    (black) zu      zb
                                \
                                 zb
                */
                z->parent->color = RBTREE_BLACK;
                z->parent->parent->color = RBTREE_RED;
                _left_rotate(tree, z->parent->parent);
            }
        }
    }

    tree->root->color = RBTREE_BLACK;
}

int rbtree_insert(rbtree_t *tree, rbtree_node_t *z, rbtree_node_do_pt cmp) {
    if (!tree || !z || !cmp) {
        return -EINVAL;
    }

    rbtree_node_t *y = &tree->sentinel;
    rbtree_node_t **px = &tree->root;
    rbtree_node_t *x = *px;

    while (&tree->sentinel != x) {
        y = x;

        if (cmp(z, x) < 0) {
            px = &x->left;
        } else {
            px = &x->right;
        }
        x = *px;
    }

    z->parent = y;

#if 0
    	if (&tree->sentinel == y)
		tree->root       = z;
	else if (cmp(z, y) < 0)
		y->left  = z;
	else
		y->right = z;
#else
    *px = z;
#endif
    z->left = &tree->sentinel;
    z->right = &tree->sentinel;
    z->color = RBTREE_RED;

    _rb_insert_fixup(tree, z);
    return 0;
}

static void _rb_delete_fixup(rbtree_t *tree, rbtree_node_t *x) {
    rbtree_node_t *w;

    while (tree->root != x && RBTREE_BLACK == x->color) {
        if (x == x->parent->left) {
            w = x->parent->right;

            if (RBTREE_RED == w->color) {
                /*
                          px (black)  left             w (black)
                         /  \         --->            / \
                (black2)x    w (red)	       (red) px  wr (black)
                            / \                     /  \
                  (black) wl   wr (black) (black2) x   wl (w', black)
                         /  \                         /  \
                       wll  wlr	                    wll  wlr
                */
                w->color == RBTREE_BLACK;
                x->parent->color = RBTREE_RED;

                _left_rotate(tree, x->parent);

                w = x->parent->right;
            }

            if (RBTREE_BLACK == w->left->color && RBTREE_BLACK == w->right->color) {
                /*
                             w (black)
                            / \
                (red, x') px  wr (black)
                         /  \
                (black2)x   wl (w', red)
                           /  \
                         wll  wlr
                */

                w->color = RBTREE_RED;
                x = x->parent;

            } else {
                if (RBTREE_BLACK == w->right->color) {
                    /*
                                 w (black)                      w (black)
                                / \                            / \
                        (red) px  wr (black)     right  (red) px  wr (black)
                             /  \                --->        /  \
                    (black2)x   wl (w', black)      (black2)x    wll (black, w'')
                               /  \                               \
                        (red) wll  wlr (black)                    wl (w', red)
                                                                   \
                                                                   wlr (black)
                    */

                    w->left->color = RBTREE_BLACK;
                    w->color = RBTREE_RED;

                    _right_rotate(tree, w);
                    w = x->parent->right; // w''
                }

                /*
                             w (black)                                 w (black)
                            / \                                       / \
                   (black) px  wr (black)         left   (red, w'') wll
                          /  \                    --->              / \
                 (black2) x   wll (red, w'')               (black) px  wl (w', black)
                               \                                  /     \
                               wl (w', black)            (black) x      wlr (black)
                                 \
                                 wlr (black)
                */
                w->color = x->parent->color;
                x->parent->color = RBTREE_BLACK;
                w->right->color = RBTREE_BLACK;
                _left_rotate(tree, x->parent);

                x = tree->root;
            }
        } else {
            w = x->parent->left;

            if (RBTREE_RED == w->color) {
                /*
                             px (black)    right            w (black)
                            /  \           --->            /        \
                     (red) w    x (black2)       (black) wl          px (red)
                          / \                                       / \
                (black) wl   wr (black)                 (black, w')wr  x (black2)
                             / \                                  / \
                           wrl  wrr	                            wrl  wrr
                */
                w->color == RBTREE_BLACK;
                x->parent->color = RBTREE_RED;

                _right_rotate(tree, x->parent);

                w = x->parent->left;
            }

            if (RBTREE_BLACK == w->left->color && RBTREE_BLACK == w->right->color) {
                /*
                           w (black)
                          /        \
                (black) wl          px (red, x')
                                    / \
                       (black, w')wr   x (black2)
                                 / \
                              wrl   wrr
                */
                w->color = RBTREE_RED;
                x = x->parent;

            } else {
                if (RBTREE_BLACK == w->left->color) {
                    /*
                               w (black)                                w (black)
                              /        \                               / \
                    (black) wl          px (red)        left  (black) wl  px (red)
                                        / \             --->             /  \
                           (black, w')wr   x (black2)     (black, w'') wrr   x (black2)
                                     / \                               /
                          (black) wrl   wrr (red)           (red, w') wr
                                                                     /
                                                           (black) wrl
                    */
                    w->right->color = RBTREE_BLACK;
                    w->color = RBTREE_RED;

                    _left_rotate(tree, w);
                    w = x->parent->left; // w''
                }

                /*
                           w (black)                               w (black)                    w (black)
                          /        \          set color           / \            right         / \
                (black) wl          px (red)    ---->    (black) wl  px (black)  ---> (black)wl  wr (w', red)
                                    / \                             / \                          / \
                       (black, w')wr   x (black2)       (red, w') wr   x (black)      (black)wrl   px (black)
                                 / \                              / \                               / \
                        (red) wrl   wrr (black)         (black) wrl  wrr (black)         (black) wrr   x (black)
                */

                w->color = x->parent->color;
                x->parent->color = RBTREE_BLACK;
                w->left->color = RBTREE_BLACK;
                _right_rotate(tree, x->parent);

                x = tree->root;
            }
        }
    }

    x->color = RBTREE_BLACK;
}

int rbtree_delete(rbtree_t *tree, rbtree_node_t *z) {
    if (!tree || !z) {
        return -EINVAL;
    }

    rbtree_node_t *x = NULL;
    rbtree_node_t *y = NULL;

    if (&tree->sentinel == z->left || &tree->sentinel == z->right) {
        y = z;
    } else {
        y = _rbtree_next(tree, z);
    }

    assert(y);

    if (&tree->sentinel != y->left) {
        x = y->left;
    } else {
        x = y->right;
    }

    x->parent = y->parent;

    if (&tree->sentinel == y->parent) {
        tree->root = x;
    } else if (y->parent->left == y) {
        y->parent->left = x;
    } else {
        y->parent->right = x;
    }

    uint8_t color = y->color;

    logd("color: %u, root: %p, x: %p, y: %p, z: %p, sentinel: %p, y->parent: %p\n",
         color, tree->root, x, y, z, &tree->sentinel, y->parent);

    if (y != z) {
        y->parent = z->parent;
        y->left = z->left;
        y->right = z->right;
        y->color = z->color;
    }

    // avoid wrong operations to these 3 pointers, only
    z->parent = NULL;
    z->left = NULL;
    z->right = NULL;

    if (RBTREE_BLACK == color) {
        _rb_delete_fixup(tree, x);
    }

    return 0;
}

rbtree_node_t* rbtree_find(rbtree_t* tree,void* data,rbtree_node_do_pt cmp){
    if (!tree || !cmp) {
        return NULL;
    }

    rbtree_node_t* node = tree->root;

    logd("root: %p, sentinel: %p\n", tree->root, &tree->sentinel);

    while (&tree->sentinel != node) {
        int ret = cmp(node,data);

        if (ret < 0) {
            node = node->right;
        }else if (ret > 0) {
            node = node->left;
        }else {
            return node;
        }
    }
    return NULL;
}




int scf_rbtree_foreach(scf_rbtree_t* tree, scf_rbtree_node_t* root, void* data, scf_rbtree_node_do_pt done)
{
	if (!tree || !root || !done)
		return -EINVAL;

	if (&tree->sentinel == root)
		return 0;

	int ret;

	if (&tree->sentinel != root->left) {

		ret = scf_rbtree_foreach(tree, root->left, data, done);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

	ret = done(root, data);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	if (&tree->sentinel != root->right) {

		ret = scf_rbtree_foreach(tree, root->right, data, done);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}
	return 0;
}

int scf_rbtree_foreach_reverse(scf_rbtree_t* tree, scf_rbtree_node_t* root, void* data, scf_rbtree_node_do_pt done)
{
	if (!tree || !root || !done)
		return -EINVAL;

	if (&tree->sentinel == root)
		return 0;

	int ret;

	if (&tree->sentinel != root->right) {

		ret = scf_rbtree_foreach_reverse(tree, root->right, data, done);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

	ret = done(root, data);
	if (ret < 0) {
		scf_loge("\n");
		return ret;
	}

	if (&tree->sentinel != root->left) {

		ret = scf_rbtree_foreach_reverse(tree, root->left, data, done);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}
	return 0;
}

int scf_rbtree_depth(scf_rbtree_t* tree, scf_rbtree_node_t* root)
{
	if (!tree || !root)
		return -EINVAL;

	if (&tree->sentinel == root)
		return 0;

	int ret;

	if (root == tree->root) {
		root->depth  = 1;
		root->bdepth = 1;
	} else if (SCF_RBTREE_BLACK == root->color) {
		root->bdepth = root->parent->bdepth + 1;
		root->depth  = root->parent->depth  + 1;
	} else {
		root->bdepth = root->parent->bdepth;
		root->depth  = root->parent->depth  + 1;
	}

	scf_loge("root->depth: %d, root->bdepth: %d\n", root->depth, root->bdepth);

	if (&tree->sentinel != root->left) {

		ret = scf_rbtree_depth(tree, root->left);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}

	if (&tree->sentinel != root->right) {

		ret = scf_rbtree_depth(tree, root->right);
		if (ret < 0) {
			scf_loge("\n");
			return ret;
		}
	}
	return 0;
}

#if 0
typedef struct {
	scf_rbtree_node_t  node;
	int d;
} rbtree_test_t;

static int test_cmp(scf_rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;
	rbtree_test_t* v1 = (rbtree_test_t*)data;

	if (v0->d < v1->d)
		return -1;
	else if (v0->d > v1->d)
		return 1;
	return 0;
}

static int test_find(scf_rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;
	int            d1 = (intptr_t)data;

	scf_logd("v0->d: %d, d1: %d\n", v0->d, d1);

	if (v0->d < d1)
		return -1;
	else if (v0->d > d1)
		return 1;
	return 0;
}

static int test_print(scf_rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;

	scf_loge("v0->d: %d\n", v0->d);
	return 0;
}

int main()
{
	scf_rbtree_t  tree;
	scf_rbtree_init(&tree);

	scf_loge("tree->sentinel: %p\n", &tree.sentinel);

	rbtree_test_t* d;

#define N 17 

	int i;
	for (i = 0; i < N; i++) {
		d = calloc(1, sizeof(rbtree_test_t));
		assert(d);

		d->d = i;

		int ret = scf_rbtree_insert(&tree, &d->node, test_cmp);
		if (ret < 0) {
			scf_loge("\n");
			return -1;
		}
	}

	scf_rbtree_foreach(&tree, tree.root, NULL, test_print);
	printf("\n");

	scf_rbtree_depth(&tree, tree.root);
	printf("\n");

	for (i = 0; i < N / 2; i++) {
		d = (rbtree_test_t*) scf_rbtree_find(&tree, (void*)(intptr_t)i, test_find);

		assert(d);
		int ret = scf_rbtree_delete(&tree, &d->node);
		assert(0 == ret);

		free(d);
		d = NULL;
	}

	scf_rbtree_foreach(&tree, tree.root, NULL, test_print);
	printf("\n");

	scf_rbtree_depth(&tree, tree.root);
	printf("\n");

	for (i = 0; i < N / 2; i++) {
		d = calloc(1, sizeof(rbtree_test_t));
		assert(d);

		d->d = i;

		int ret = scf_rbtree_insert(&tree, &d->node, test_cmp);
		if (ret < 0) {
			scf_loge("\n");
			return -1;
		}
	}
	printf("*****************\n");
	scf_rbtree_foreach_reverse(&tree, tree.root, NULL, test_print);

	scf_rbtree_depth(&tree, tree.root);
	return 0;
}
#endif