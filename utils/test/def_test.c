#include "utils_def.h"

struct Node {
    int id;
    double value;
    struct Node *next;
};

int main() {
    int64_t cur_time = gettime();

    printf("cur_time:%ld\n", cur_time);

    uint64_t x = 0b1111111111111010; // 实际只关心低8位
    uint64_t r = zero_extend(x, 8);
    printf("r:%ld\n", r);

    struct Node node;
    node.id = 42;
    node.value = 3.14;
    node.next = NULL;

    // 我们只拿到 node.value 的指针
    double *pval = &node.value;

    // 用宏反推出整个结构体的地址
    struct Node *pnode = object_of(pval, struct Node, value);

    // 验证
    printf("原始 node 地址: %p\n", (void *)&node);
    printf("反推 node 地址: %p\n", (void *)pnode);

    printf("pnode->id = %d, pnode->value = %.2f\n", pnode->id, pnode->value);

    int a = 42, b = 24;
    printf("原来 a = %d,b= %d\n", a, b);

    XCHG(a, b);
    printf("交换后 a = %d,b= %d\n", a, b);

    logd("logd的日志\n");
    logi("logi的日志信息\n");
    loge("loge的日志错误\n");
    logw("logw的日志警告\n");

    CHECK_ERROR(1, 42, "1 是 ture\n");
    CHECK_ERROR(0, 42, "0 是 false\n");
}
