#ifndef VARIABLE_H
#define VARIABLE_H

#include "core_types.h"
#include "lex_word.h"

// 表示数组维度信息
typedef struct
{
    expr_t *vla; // 如果是变长数组，保存对应的表达式
    int num;     // 该维度的大小(静态数组长度，或vla的长度表达式值)
} dimention_t;

// 表示一个变量
struct variable_s {
    int refs; // 引用计数，用于内存管理

    int type;      // 变量类型
    lex_word_t *w; // 变量名对应的词法单元

    int nb_pointers;      // 指针层数
    function_t *func_ptr; // 如果是函数指针，保存函数信息

    dimention_t *dimentions; // 数组维度信息数组
    int nb_dimentions;       // 数组维度数量
    int dim_index;           // 当前维度索引
    int capacity;            // 容量

    int size;      // 类型大小
    int data_size; // 实际数据大小

    int bit_offsets; // 位域偏移
    int bit_size;    // 位域大小

    int n_pins; // EDA（电子设计自动化）相关：绑定的引脚数

    // 每一位在电路中的读写引脚和触发器
    Epin *r_pins[EDA_MAX_BITS];
    Epin *w_pins[EDA_MAX_BITS];
    Ecomponent *DFFs[EDA_MAX_BITS];

    // 内存布局相关的偏移
    int offset;       // 总体偏移
    int bp_offsets;   // 基址指针偏移
    int sp_offset;    // 栈指针偏移
    int ds_offset;    // 数据段偏移
    register_t *rabi; // 寄存器分配信息

    // ==========================
    // 变量的具体数据（联合体形式）
    // 根据类型不同，只会使用其中一种
    // ==========================
    union {
        int32_t i;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
        complex_t z;
        string_t *s;
        void *p;
    } data;

    string_t *signature; // 变量的签名信息（比如函数签名）

    // ==========================
    // 一组标志位（按位存储，节省内存）
    // ==========================
    uint32_t const_literal_flag : 1; // 是否是字面值常量（如 123，"abc"）
    uint32_t const_flag : 1;         // 是否是 const
    uint32_t static_flag : 1;        // 是否是 static
    uint32_t extern_flag : 1;        // 是否是 extern
    uint32_t extra_flag : 1;         // 额外标记（自定义用途）

    uint32_t tmp_flag : 1;    // 是否是临时变量
    uint32_t local_flag : 1;  // 是否是局部变量
    uint32_t global_flag : 1; // 是否是全局变量
    uint32_t member_flag : 1; // 是否是结构体成员

    uint32_t vla_flag : 1;     // 是否是变长数组
    uint32_t arg_flag : 1;     // 是否是函数参数
    uint32_t auto_gc_flag : 1; // 是否启用自动垃圾回收
};

// ==========================
// 数组/结构体访问相关
// ==========================
struct index_s {
    variable_t *member; // 被索引的成员（比如数组元素、结构体成员）
    int index;          // 索引值（数组下标）
};

struct member_s {
    variable_t *base;  // 基础变量（数组/结构体）
    vector_t *indexes; // 索引列表（可以是多维数组或多层嵌套）
};

// ==========================
// 成员相关函数接口
// ==========================
member_t *member_alloc(variable_t *base); // 创建一个成员引用
void member_free(member_t *m);            // 释放成员引用
int member_offset(member_t *m);           // 计算成员偏移

int member_add_index(member_t *m, variable_t *member, int index); // 向成员添加索引（数组访问）

// ==========================
// 变量相关函数接口
// ==========================
variable_t *variable_alloc(lex_word_t *w, type_t *t); // 分配一个新变量
variable_t *variable_clone(variable_t *var);          // 克隆变量
variable_t *variable_ref(variable_t *var);            // 增加引用计数
void variable_free(variable_t *var);                  // 释放变量

void variable_print(variable_t *var); // 打印变量信息

void variable_add_array_dimention(variable_t *array, int index, variable_t *member); // 添加数组维度
void variable_set_array_member(variable_t *array, int index, variable_t *member);    // 设置数组成员
void variable_get_array_member(variable_t *array, int index, variable_t *member);    // 获取数组成员

int variable_same_type(variable_t *v0, variable_t *v1); // 类型完全相同
int variable_type_like(variable_t *v0, variable_t *v1); // 类型相似（如 int 和 long）

void variable_sign_extend(variable_t *v, int bytes);      // 有符号扩展到指定字节数
void variable_zero_extend(variable_t *v, int bytes);      // 无符号扩展到指定字节数
void variable_extend_std(variable_t *v, variable_t *std); // 扩展到标准类型
void variable_extend_bytes(variable_t *v, int bytes);     // 扩展到指定字节数

int variable_size(variable_t *v); // 获取变量大小

// ==========================
// 内联函数：各种变量判定
// ==========================

// 是否是常量
static inline int variable_const(variable_t *v) {
    if (FUNCTION_PTR == v->type)
        return v->const_literal_flag;

    if (v->nb_pointers + v->nb_dimentions > 0)
        return v->const_literal_flag && !v->vla_flag;

    return v->const_flag && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是常量整数
static inline int variable_const_integer(variable_t *v) {
    return type_is_integer(v->type) && v->const_flag && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是常量字符串
static inline int variable_const_string(variable_t *v) {
    return VAR_CHAR == v->type
           && v->const_flag
           && v->const_literal_flag
           && 1 == v->nb_pointers
           && 0 == v->nb_dimentions;
}

// 是否是字符串（char* 或 char[]）
static inline int variable_string(variable_t *v) {
    return VAR_CHAR == v->type && 1 == v->nb_pointers + v->nb_dimentions;
}

// 是否是浮点数（非指针/数组）
static inline int variable_float(variable_t *v) {
    return type_is_float(v->type) && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是整数类型（包括指针/数组）
static inline int variable_integer(variable_t *v) {
    return type_is_integer(v->type) || v->nb_pointers > 0 || v->nb_dimentions > 0;
}

// 是否是有符号整数（非指针/数组）
static inline int variable_signed(variable_t *v) {
    return type_is_signed(v->type) && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是无符号整数（或指针/数组）
static inline int variable_unsigned(variable_t *v) {
    return type_is_unsigned(v->type) || v->nb_pointers > 0 || v->nb_dimentions > 0;
}

// 获取指针层数（包括数组维度）
static inline int variable_nb_pointers(variable_t *v) {
    return v->nb_pointers + v->nb_dimentions;
}

// 是否是结构体（非指针/数组）
static inline int variable_is_struct(variable_t *v) {
    return v->type >= STRUCT && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是结构体指针
static inline int variable_is_struct_pointer(variable_t *v) {
    return v->type >= STRUCT && 1 == v->nb_pointers && 0 == v->nb_dimentions;
}

// 是否是数组
static inline int variable_is_array(variable_t *v) {
    return v->nb_dimentions > 0;
}

// 是否可能需要 malloc 分配
static inline int variable_may_malloced(variable_t *v) {
    if (v->nb_dimentions > 0)
        return 0;

    if (FUNCTION_PTR == v->type) {
        if (v->nb_pointers > 1)
            return 1;
        return 0;
    }

    return v->nb_pointers > 0;
}

#endif
