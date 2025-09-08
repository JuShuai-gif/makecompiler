#ifndef VECTOR_H
#define VECTOR_H

#include "utils_def.h"

// 数组
typedef struct
{
    int capacity;// 容量
    int size;// 大小
    void** data;// 数据
}vector_t;

#undef NB_MEMBER_INC
#define NB_MEMBER_INC 16 // 数组长度

// 分配数组
static inline vector_t* vector_alloc(){
    // 分配一个 vector_t 
    vector_t* v = (vector_t*)calloc(1,sizeof(vector_t));

    // 分配失败
    if (!v)
        return NULL;

    // 分配数据
    v->data = calloc(NB_MEMBER_INC,sizeof(void*));

    // 分配不成功
    if (!v->data)
    {
        free(v);
        v = NULL;
        return NULL;
    }

    // 记录下容量大小
    v->capacity = NB_MEMBER_INC;
    return v;
}

// 数组克隆
static inline vector_t* vector_clone(vector_t* src){
    // 分配一个数组
    vector_t* dst = calloc(1,sizeof(vector_t));
    // 分配不成功
    if (!dst)
        return NULL;

    // 根据src的容量分配目标数组的容量
    dst->data = calloc(src->capacity,sizeof(void*));

    // 分配不成功
    if (!dst->data)
    {
        free(dst);
        return NULL;
    }

    // 容量
    dst->capacity = src->capacity;
    // 大小
    dst->size = src->size;
    // 拷贝数据
    memcpy(dst->data,src->data,src->size * sizeof(void*));
    return dst;
}

// 数组拼接
static inline int vector_cat(vector_t* dst,vector_t* src){
    // 参数有问题
    if (!dst || !src)
        return -EINVAL;

    // 总大小 = 数组一大小 + 数组二大小
    int size = dst->size + src->size;

    // 如果大小大于目标数组的容量
    if (size > dst->capacity)
    {   
        // 重新分配
        void* p = realloc(dst->data,sizeof(void*) * (size + NB_MEMBER_INC));
        if (!p)
            return -ENOMEM;
        // 新的数据
        dst->data = p;
        // 现有容量 + 新增容量
        dst->capacity = size + NB_MEMBER_INC;
    }
    
    memcpy(dst->data + dst->size * sizeof(void*),src->data,src->size * sizeof(void*));
    dst->size += src->size;
    return 0;
}

// 数组添加元素
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

// 在前面加入元素值
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

// 数组删除元素
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
        
        // 当数组容量远大于实际大小时，进行缩容
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

// 数组寻找元素
static inline void* vector_find(const vector_t* v,void* node){
    int i;
    for (i = 0; i < v->size; i++)
        if (node == v->data[i])
            return v->data[i];
    
    return NULL;
}

// 判断数组中是否存在这个元素，如果不存在就添加
static inline int vector_add_unique(vector_t* v,void* node){
    if (!vector_find(v,node))
        return vector_add(v,node);
    return 0;
}

// 寻找元素
static inline void* vector_find_cmp(const vector_t* v,const void* node,int (*cmp)(const void*,const void*)){
    int i;
    for (i = 0; i < v->size; i++)
        if (0 == cmp(node,v->data[i]))
            return v->data[i];
    return NULL;
}


// 对元素进行排序
static inline int vector_qsort(const vector_t* v,int (*cmp)(const void*,const void*)){
    if (!v || !v->data || 0 == v->size || !cmp)
        return -EINVAL;
    qsort(v->data,v->size,sizeof(void*),cmp);
    return 0;
}

// 清空数组
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

// 释放数组
static inline void vector_free(vector_t* v){
    if (v)
    {
        // 先释放数据
        if (v->data)
            free(v->data);
        // 在释放自己
        free(v);
        v = NULL;
    }
}

// 由于不能判断v的data存放什么数据，所以不能打印
static inline void vector_print(vector_t* v){
    if (!v)
    {
        
    }
    
}

#endif














