#include "../utils_stack.h"

int main() {
    stack_t *s = stack_alloc();

    int arr[3] = {1, 2, 3};
    int arr1[3] = {4, 5, 6};
    int arr2[3] = {7, 8, 9};

    stack_push(s, arr);
    stack_push(s, arr1);
    stack_push(s, arr2);
	// 是第一个值
    int *top_arr = stack_top(s);
    for (size_t i = 0; i < 3; i++) {
        printf("%d\t", arr[i]);
    }
    printf("\n");

	stack_pop(s);

    for (int i = 0; i < s->size; i++) {
        printf("i:%d,s->size:%d,s->capacity:%d\n", i, s->size, s->capacity);
        printf("s data: ");
        for (size_t j = 0; j < 3; j++) {
            printf("%d\t", ((int *)s->data[i])[j]);
        }
        printf("\n");
    }

    vector_t *vv = malloc(sizeof(vector_t));
    vv->capacity = 16;
    vv->size = 3;
    vv->data = malloc(3 * sizeof(void *));
    vv->data[0] = arr;
    vv->data[1] = arr1;
    vv->data[2] = arr2;

    vector_t *vvv = vector_clone(vv);

    int arr4[3] = {11, 12, 13};
    vector_add(vvv, arr4);

    vector_add_front(vvv, arr4);

    int i;
    for (i = 0; i < 64; i++) {
        printf("i: %d, s->size: %d, s->capacity: %d\n", i, s->size, s->capacity);
        stack_push(s, (void *)i);
    }

    for (i = 0; i < 64; i++) {
        int j = (int)stack_pop(s);
        printf("i: %d, j: %d, s->size: %d, s->capacity: %d\n", i, j, s->size, s->capacity);
    }

    int j = (int)stack_pop(s);
    printf("i: %d, j: %d, s->size: %d, s->capacity: %d\n", i, j, s->size, s->capacity);
}