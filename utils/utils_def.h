#ifndef UTILS_DEF_H
#define UTILS_DEF_H

#include <bits/types/struct_timeval.h>
#include <cstdint>
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
static inline int64_t gettime(){
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

#endif






#endif