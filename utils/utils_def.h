#ifndef UTILS_DEF_H
#define UTILS_DEF_H

#include <bits/types/struct_timeval.h>
#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>


#if 1
#include <sys/time.h>
// 获取时间
static inline int64_t gettime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

#endif

static inline uint64_t sign_extend(uint64_t src,int src_bits){
    uint64_t sign = src >> (src_bits - 1) & 0x1;
    uint64_t mask = (~sign + 1) << (src_bits - 1);

    src |= mask;
    return src;
}

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