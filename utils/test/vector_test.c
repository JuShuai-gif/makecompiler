#include "../utils_vector.h"

int main(){
    vector_t* v = vector_alloc();

    int i;
    for (i = 0; i < 20; i++)
    {
        vector_add(v,(void*)i);
        printf("i:%d,v->size:%d,v->capacity:%d\n",i,v->size,v->capacity);
    }

    vector_del(v,(void*)0);
    for (i = 7; i < 20; i++)
    {
        vector_del(v,(void*)i);
        printf("i: %d, v->size: %d, v->capacity: %d\n", i, v->size, v->capacity);
    }
    
    for (i = 10; i < 20; i++)
    {
        vector_add(v,(void*)i);
        printf("i: %d, v->size: %d, v->capacity: %d\n", i, v->size, v->capacity);
    }

    for (i = 0; i < v->size; i++)
    {
        int j = (int)(v->data[i]);
        printf("i: %d, j: %d, v->size: %d, v->capacity: %d\n", i, j, v->size, v->capacity);
    }
    
    


}













