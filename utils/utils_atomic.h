#ifndef ATOMIC_H
#define ATOMIC_H

#include "utils_def.h"

// 原子计数器
typedef struct
{
    // volatile 的作用：告诉编译器，这个变量的值可能会在意想不到的地方被改变（比如硬件寄存器、多线程或中断程序中）
    volatile int count;
}atomic_t;


static inline void atomic_inc(atomic_t* v) {
    asm volatile(
        "lock; incl %0"
        : "+m"(v->count)   // v->count 既是输入也是输出
        :                  // 无额外输入
        : "memory"         // 告诉编译器内存可能被修改，防止重排序
    );
}


static inline void atomic_dec(atomic_t* v) {
    asm volatile(
        "lock; decl %0"
        : "+m"(v->count)  // 读-改-写
        :
        : "memory"        // 防止重排序
    );
}

static inline int atomic_dec_and_test(atomic_t* v) {
    unsigned char ret;

    asm volatile(
        "lock; decl %0\n\t"   // v->count 原子减 1
        "setz %1"             // 如果结果为 0，则 ret = 1，否则 0
        : "+m"(v->count), "=q"(ret) // "+m" 表示读写内存, "=q" 表示输出到寄存器
        : 
        : "memory"
    );
    return ret;
}




#endif











