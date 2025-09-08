#include "../utils_vector.h"

int main() {
    int arr[3] = {1, 2, 3};
    for (size_t i = 0; i < 3; i++) {
        printf("%d\t", arr[i]);
    }
    printf("\n");

    int arr1[3] = {4, 5, 6};
    int arr2[3] = {7, 8, 9};

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

    for (int i = 0; i < vvv->size; i++) {
        printf("i:%d,vvv->size:%d,vvv->capacity:%d\n", i, vvv->size, vvv->capacity);
        printf("vvv data: ");
        for (size_t j = 0; j < 3; j++) {
            printf("%d\t", ((int *)vvv->data[i])[j]);
        }
        printf("\n");
    }

    vector_t *v = vector_alloc();

    int i;
    for (i = 0; i < 20; i++) {
        vector_add(v, (void *)i);
        printf("i:%d,v->size:%d,v->capacity:%d\n", i, v->size, v->capacity);
    }

    vector_del(v, (void *)0);
    for (i = 7; i < 20; i++) {
        vector_del(v, (void *)i);
        printf("i: %d, v->size: %d, v->capacity: %d\n", i, v->size, v->capacity);
    }

    for (i = 10; i < 20; i++) {
        vector_add(v, (void *)i);
        printf("i: %d, v->size: %d, v->capacity: %d\n", i, v->size, v->capacity);
    }

    for (i = 0; i < v->size; i++) {
        int j = (int)(v->data[i]);
        printf("i: %d, j: %d, v->size: %d, v->capacity: %d\n", i, j, v->size, v->capacity);
    }
}
