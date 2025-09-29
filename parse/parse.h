#ifndef PARSE_H
#define PARSE_H

#include "lex.h"
#include "ast.h"
#include "dfa.h"
#include "utils_stack.h"
#include "dwarf.h"

/*
parse 语法分析(parsing)

它的主要作用是：
- 接收词法分析器产生的token序列
- 按照语法规则(文法/grammer)把这些 token 组织成语法结构，例如表达式、语句、函数、程序等
- 生成抽象语法书(AST),为后续的语义分析、中间代码生成等阶段提供结构化的输入
*/

typedef struct parse_s parse_t;
typedef struct dfa_data_s dfa_data_t;

// 段索引(section index)的宏定义，用于标识 ELF 文件或编译器内部的不同段

// .text 段：存放程序代码(机器指令)
#define SHNDX_TEXT 1
// .rodata 段：存放只读数据(如字符串常量、const 全局变量)
#define SHNDX_RODATA 2
// .data 段: 存放已初始化的全局变量和静态变量
#define SHNDX_DATA 3

// DWARF 调试信息相关的段
// .debug_abbrev 段：存放调试信息的缩写表（Abbreviation Table）
#define SHNDX_DEBUG_ABBREV 4
// .debug_info 段：存放具体的调试信息（变量、类型、行号等）
#define SHNDX_DEBUG_INFO 5
// .debug_line 段：存放源码行号与机器指令的映射关系
#define SHNDX_DEBUG_LINE 6
// .debug_str 段：存放调试信息中使用的字符串（如变量名、函数名）
#define SHNDX_DEBUG_STR 7

/*          解析器核心结构体            */

// parse_s 表示整个解析器(parser)的上下文
struct parse_s {
    lex_t *lex_list; // 分词器列表，对应着多个分词器
    lex_t *lex;      // 当前使用的分词器

    ast_t *ast; // 抽象语法树的根节点

    dfa_t *dfa;           // DFA(确定性有限自动机)，用于语法分析/词法状态机
    dfa_data_t *dfa_data; // DFA 相关的上下文数据

    vector_t *symtab;        // 符号表(symbol table),存储标识符及其属性
    vector_t *global_consts; // 全局常量表

    dwarf_t *debug; // 调试信息
};

// 表示数组下标或索引
typedef struct {
    lex_word_t *w; // 对应的词法单元(标识符/数字等)
    intptr_t i;    // 索引值(整数下标)
} dfa_index_t;

// 表示初始化表达式(如数组/结构体初始化)
typedef struct {
    expr_t *expr; // 表达式指针

    int n;                // 索引数组的数量
    dfa_index_t index[0]; // 索引数组(用于存储 n 个索引)

} dfa_init_expr_t;

// 表示变量或函数的"身份信息"(声明/定义时的属性)
typedef struct {
    lex_word_t *identity; // 标识符(变量/函数名)
    lex_word_t *type_w;   // 类型的词法单元(如 int、float、struct 等)
    type_t *type;         // 类型信息(语义解析后)

    int number;           // 可能是数组长度或序号
    int nb_pointers;      // 指针层级(如 int* 是1，int**是2)
    function_t *func_ptr; // 若为函数，则保存函数信息指针

    // C语言修饰符
    uint32_t const_flag : 1;  // const 修饰符
    uint32_t extern_flag : 1; // extern 修饰符
    uint32_t static_flag : 1; // static 修饰符
    uint32_t inline_flag : 1; // inline 修饰符

} dfa_identity_t;

// 表示 DFA 的运行时上下文(语义分析过程中需要的状态)
struct dfa_data_s {
    void **module_datas; // 各个模块的数据指针

    expr_t *expr;        // 当前正在处理的表达式
    int expr_local_flag; // 表达式是否为局部变量

    stack_t *current_identities; // 当前作用域中的标识符栈
    variable_t *current_var;     // 当前正在声明/使用的变量
    lex_word_t *current_var_w;   // 当前变量的词法单元

    int nb_sizeofs;    // 出现的 sizeof 次数
    int nb_containers; // 出现的容器数量(结构体/数组等)

    function_t *current_function; // 当前正在处理的函数
    int argc;                     // 当前函数的参数数量

    lex_word_t *current_async_w; // 异常标识符(可能与 async/await 相关)

    type_t *root_struct;    // 根结构体类型
    type_t *current_struct; // 当前正在处理的结构体

    node_t *current_node; // 当前 AST 节点

    // 控制流相关的节点
    node_t *current_while;

    node_t *current_for;
    vector_t *for_exprs;

    node_t *current_return;
    node_t *current_goto;

    // 可变参数相关节点(va_start、va_arg、va_end)
    node_t *current_va_start;
    node_t *current_va_arg;
    node_t *current_va_end;

    // C 语言修饰符标志
    uint32_t const_flag : 1;
    uint32_t extern_flag : 1;
    uint32_t static_flag : 1;
    uint32_t inline_flag : 1;

    // 变量声明是否带分号标志
    uint32_t var_semicolon_flag : 1;

    // 括号匹配计数(用于语法检查)
    int nb_lbs; // 左大括号 {
    int nb_rbs; // 右大括号 }

    int nb_lss; // 左中括号 [
    int nb_rss; // 右中括号 ]

    int nb_lps; // 左小括号 (
    int nb_rps; // 右小括号 )
};

// ---------------------
// 解析器相关函数声明
// ---------------------

// 初始化 DFA 的解析
int parse_dfa_init(parse_t *parse);

// 打开解析器
int parse_open(parse_t **pparse);

// 关闭解析器
int parse_close(parse_t *parse);

// 解析一个文件
int parse_file(parse_t *parse, const char *path);

// 编译(生成中间代码或 IR)
int parse_compile(parse_t *parse, const char *arch, int _3ac);
// 生成目标文件(obj 文件)
int parse_to_obj(parse_t *parse, const char *out, const char *arch);
// 内部函数：查找全局变量
int _find_global_var(node_t *node, void *arg, vector_t *vec);

// 内部函数：查找函数
int _find_function(node_t *node, void *arg, vector_t *vec);

#endif
