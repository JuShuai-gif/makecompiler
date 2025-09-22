#ifndef TYPE_H
#define TYPE_H

#include "node.h"

// -------------------------
// 基础类型 (如 int, char, float)
// -------------------------
typedef struct
{
    int type;           // 类型枚举/ID，用来标识是哪一种基础类型
    const char* name;   // 类型名字（如 "int", "float", "char"）
    int size;           // 类型大小（单位：字节），例如 int=4, char=1
} base_type_t;

// -------------------------
// 类型缩写 (Type Abbreviation)
// -------------------------
// 用来存放类型的名字和缩写，比如 "unsigned int" -> "uint"
typedef struct
{
    const char* name;// 类型全称（如 "unsigned int"）
    const char* abbrev;// 类型缩写（如 "uint"）
}type_abbrev_t;


// -------------------------
// 类型结构体 (完整类型定义)
// -------------------------
// 表示复杂类型（包括指针、函数类型、结构体、用户自定义类型等）
struct type_s
{
    node_t node;           // AST 节点，用于和语法树挂钩
    scope_t* scope;        // 类型所在的作用域（方便查找类型/符号）

    string_t* name;        // 类型名（如 "MyStruct"、"int"）

    list_t list;           // 链表节点（方便把多个类型挂在 type_list 里）

    int type;              // 类型枚举/ID（例如 TYPE_INT, TYPE_FLOAT, TYPE_STRUCT）

    lex_word_t* w;         // 定义该类型的词法单元（用于报错提示）

    int nb_pointers;       // 指针层级（如 int* =1, int** =2）

    function_t* func_ptr;  // 若是函数类型，则存储函数签名信息

    int size;              // 类型大小（字节数）
    int offset;            // 偏移量（用于结构体/联合体成员的布局）

    type_t* parent;        // 父类型（如 typedef 派生出的类型，或结构体继承）
};

// 类型分配
type_t* type_alloc(lex_word_t* w,const char* name,int type,int size);
// 类型释放
void type_free(type_t* t);
// 类型寻找缩写
const char* type_find_abbrev(const char* name);

#endif