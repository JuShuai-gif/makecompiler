#ifndef VECTOR_H
#define VECTOR_H

#include "utils_def.h"

typedef struct
{
    int capacity;
    int size;
    void** data;
}vector_t;

#undef NB_MEMBER_INC
#define NB_MEMBER_INC 16

static inline vector_t* vector_alloc(){
    vector_t* v = (vector_t*)calloc(1,sizeof(vector_t));

    if (!v)
        return NULL;

    v->data = calloc(NB_MEMBER_INC,sizeof(void*));
    if (!v->data)
    {
        free(v);
        v = NULL;
        return NULL;
    }
    v->capacity = NB_MEMBER_INC;
    return v;
}

static inline vector_t* vector_clone(vector_t* src){
    vector_t* dst = calloc(1,sizeof(vector_t));
    if (!dst)
        return NULL;

    dst->data = calloc(src->capacity,sizeof(void*));

    if (!dst->data)
    {
        free(dst);
        return NULL;
    }

    dst->capacity = src->capacity;
    dst->size = src->size;
    memccpy(dst->data,src->data,src->size * sizeof(void*));
    return dst;
}

static inline int vector_cat(vector_t* dst,vector_t* src){
    if (!dst || !src)
        return -EINVAL;

    int size = dst->size + src->size;
    if (size > dst->capacity)
    {
        void* p = realloc(dst->data,sizeof(void*) * (size + NB_MEMBER_INC));
        if (!p)
            return -ENOMEM;
        
        dst->data = p;
        dst->capacity = size + NB_MEMBER_INC;
    }
    
    memcpy(dst->data + dst->size * sizeof(void*),src->data,src->size * sizeof(void*));
    dst->size += src->size;
    return 0;
}

static inline int vector_add(vector_t* v,void* node){
    if (!v || !v->data)
        return -EINVAL;

    assert(v->size <= v->capacity);

    if (v->size == v->capacity)
    {
        void* p = realloc(v->data,sizeof(void*) * (v->capacity + NB_MEMBER_INC));
        if (!p)
            return -ENOMEM;
        
        v->data = p;
        v->capacity += NB_MEMBER_INC;
    }
    
    v->data[v->size++] = node;
    return 0;
}

static inline int vector_add_front(vector_t* v,void* node){
    int ret = vector_add(v,node);
    if (ret < 0)
        return ret;
    
    int i;
    for ( i = v->size - 2; i>= 0; i--)
        v->data[i + 1] = v->data[i];
    
    v->data[0] = node;
    return 0;    
}

static inline int vector_del(vector_t* v,void* node){
    if (!v || !v->data)
        return -EINVAL;
    
    assert(v->size <= v->capacity);

    int i;
    for (i = 0; i < v->size; i++)
    {
        if (v->data[i] != node)
            continue;
        
        int j;
        for (j = i+1; j < v->size; j++)
            v->data[j-1] = v->data[j];

        v->size--;

        if (v->size + NB_MEMBER_INC * 2 < v->capacity)
        {
            void* p = realloc(v->data,sizeof(void*)* (v->capacity - NB_MEMBER_INC));
            if (p)
            {
                v->data = p;
                v->capacity -= NB_MEMBER_INC;
            }
        }
        return 0;
    }
    return -1;
}

static inline void* vector_find(const vector_t* v,void* node){
    int i;
    for (i = 0; i < v->size; i++)
        if (node == v->data[i])
            return v->data[i];
    
    return NULL;
}

static inline int vector_add_unique(vector_t* v,void* node){
    if (!vector_find(v,node))
        return vector_add(v,node);
    return 0;
}

static inline void* vector_find_cmp(const vector_t* v,const void* node,int (*cmp)(const void*,const void*)){
    int i;
    for (i = 0; i < v->size; i++)
        if (0 == cmp(node,v->data[i]))
            return v->data[i];
    return NULL;
}

static inline int vector_qsort(const vector_t* v,int (*cmp)(const void*,const void*)){
    if (!v || !v->data || 0 == v->size || !cmp)
        return -EINVAL;
    qsort(v->data,v->size,sizeof(void*),cmp);
    return 0;
}

static inline void vector_clear(vector_t* v,void(*type_free)(void*)){
    if (!v || !v->data)
        return;

    if (type_free)
    {
        int i;
        for (i = 0; i < v->size; i++)
        {
            if (v->data[i])
            {
                type_free(v->data[i]);
                v->data[i] = NULL;
            }
        }
    }
    v->size = 0;

    if (v->capacity > NB_MEMBER_INC)
    {
        void* p=realloc(v->data,sizeof(void*)*NB_MEMBER_INC);
        if (p)
        {
            v->data = p;
            v->capacity = NB_MEMBER_INC;
        }   
    }
}

static inline void vector_free(vector_t* v){
    if (v)
    {
        if (v->data)
            free(v->data);

        free(v);
        v = NULL;
    }
}

#endif














