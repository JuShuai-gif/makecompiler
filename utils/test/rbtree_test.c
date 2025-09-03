#include "../utils_rbtree.h"


typedef struct {
	rbtree_node_t  node;
	int d;
} rbtree_test_t;

static int test_cmp(rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;
	rbtree_test_t* v1 = (rbtree_test_t*)data;

	if (v0->d < v1->d)
		return -1;
	else if (v0->d > v1->d)
		return 1;
	return 0;
}

static int test_find(rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;
	int            d1 = (intptr_t)data;

	logd("v0->d: %d, d1: %d\n", v0->d, d1);

	if (v0->d < d1)
		return -1;
	else if (v0->d > d1)
		return 1;
	return 0;
}

static int test_print(rbtree_node_t* node0, void* data)
{
	rbtree_test_t* v0 = (rbtree_test_t*)node0;

	loge("v0->d: %d\n", v0->d);
	return 0;
}

int main()
{
	rbtree_t  tree;
	rbtree_init(&tree);

	loge("tree->sentinel: %p\n", &tree.sentinel);

	rbtree_test_t* d;

#define N 17 

	int i;
	for (i = 0; i < N; i++) {
		d = calloc(1, sizeof(rbtree_test_t));
		assert(d);

		d->d = i;

		int ret = rbtree_insert(&tree, &d->node, test_cmp);
		if (ret < 0) {
			loge("\n");
			return -1;
		}
	}

	rbtree_foreach(&tree, tree.root, NULL, test_print);
	printf("\n");

	rbtree_depth(&tree, tree.root);
	printf("\n");

	for (i = 0; i < N / 2; i++) {
		d = (rbtree_test_t*) rbtree_find(&tree, (void*)(intptr_t)i, test_find);

		assert(d);
		int ret = rbtree_delete(&tree, &d->node);
		assert(0 == ret);

		free(d);
		d = NULL;
	}

	rbtree_foreach(&tree, tree.root, NULL, test_print);
	printf("\n");

	rbtree_depth(&tree, tree.root);
	printf("\n");

	for (i = 0; i < N / 2; i++) {
		d = calloc(1, sizeof(rbtree_test_t));
		assert(d);

		d->d = i;

		int ret = rbtree_insert(&tree, &d->node, test_cmp);
		if (ret < 0) {
			loge("\n");
			return -1;
		}
	}
	printf("*****************\n");
	rbtree_foreach_reverse(&tree, tree.root, NULL, test_print);

	rbtree_depth(&tree, tree.root);
	return 0;
}