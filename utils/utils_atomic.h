#ifndef ATOMIC_H
#define ATOMIC_H

#include "utils_def.h"

typedef struct
{
    volatile int count;
}atomic_t;

static inline void stomic_inc(atomic_t* v){
    asm volatile(
        "lock; inc1 %0"
        :"=m"(v->count)
        :
        :
    );
}

static inline void stomic_dec(atomic_t* v){
    asm volatile(
        "lock; dec1 %0"
        :"=m"(v->count)
        :
        :
    );
}

static inline int atomic_dec_and_test(atomic_t* v){
    unsigned char ret;

    asm volatile(
        "lock; dec1 %0\r\n"
        "setz %1\r\n"
        :"=m"(v->count),"=r"(ret)
        :
        :
    );
    return ret;
}



#endif











