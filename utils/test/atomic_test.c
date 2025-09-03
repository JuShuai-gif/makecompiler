#include "../utils_atomic.h"


atomic_t v = {0};

int main(){
    atomic_inc(&v);
    loge("v.count: %d\n",v.count);

    atomic_inc(&v);
    loge("v.count: %d\n",v.count);

    atomic_dec(&v);
    loge("v.count: %d\n",v.count);

    int ret = atomic_dec_and_test(&v);
    loge("v.count: %d,ret: %d\n",v.count,ret);

    loge("sizeof(v):%ld\n",sizeof(v));
    return 0;
}








