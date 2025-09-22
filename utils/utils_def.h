#ifndef UTILS_DEF_H
#define UTILS_DEF_H

#include <stdint.h>// 定义固定宽度的整数类型
#include <stdio.h>// 标准输入输出库
#include <stdlib.h>// 标准库函数集合
#include <stddef.h>// 定义一些常用宏和类型
#include <string.h>// 字符串和内存操作函数
#include <assert.h>// 提供 assert(expr) 宏
#include <errno.h>// 用于表示系统调用或库函数的错误代码
#include <time.h>// 时间和日期处理
#include <unistd.h>// POSIX 标准头文件
#include <limits.h>// 定义整数的取值范围
#include <math.h>// 数学函数库


#if 1
#include <sys/time.h>
// 获取时间
static inline int64_t gettime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

#endif

// 把 src 的低 src_bits 位保留，其余高位清零。常用于 无符号数扩展
static inline uint64_t sign_extend(uint64_t src,int src_bits){
    uint64_t sign = src >> (src_bits - 1) & 0x1;
    uint64_t mask = (~sign + 1) << (src_bits - 1);

    src |= mask;
    return src;
}

// 把 src_bits 位的数（有符号）扩展为 64 位。
// 常用于 指令立即数扩展，例如 RISC-V、x86 的指令里，12 位、16 位、24 位等立即数需要扩展到寄存器字长
static inline uint64_t zero_extend(uint64_t src,int src_bits){
    uint64_t mask = (1ULL << src_bits) -1;

    src &= mask;
    return src;
}

// 已知结构体某个成员的指针，反推出整个结构体的指针
#define object_of(mp,type,member) ((type*)((char*)mp - offsetof(type,member)))

// 交换 x 和 y 的值。typeof(x) 保证类型安全
#define XCHG(x,y)\
    do{\
        typeof(x) tmp = x;\
        x = y;\
        y = tmp;\
    }while(0)


/*
logd：调试日志（仅 #define DEBUG 时启用）。

logi：信息日志。

loge：错误日志（红色高亮）。

logw：警告日志（黄色高亮）。

__func__：当前函数名。

__LINE__：当前代码行号。
*/    
#ifdef DEBUG
#define logd(fmt,...) printf("%s(),%d,"fmt,__func__,__LINE__,##__VA_ARGS__)
#else
#define logd(fmt,...)
#endif


#define logi(fmt,...) printf("%s(),%d,info:"fmt,__func__,__LINE__,##__VA_ARGS__)

#define loge(fmt, ...) printf("%s(), %d, \033[1;31m error:\033[0m "fmt, __func__, __LINE__, ##__VA_ARGS__)
#define logw(fmt, ...) printf("%s(), %d, \033[1;33m warning:\033[0m "fmt, __func__, __LINE__, ##__VA_ARGS__)

// 检查条件 cond，如果成立则打印错误日志，并 return ret
#define CHECK_ERROR(cond, ret, fmt, ...) \
	do { \
		if (cond) { \
			printf("%s(), %d, \033[1;31m error:\033[0m "fmt, __func__, __LINE__, ##__VA_ARGS__); \
			return ret; \
		} \
	} while (0)


#endif