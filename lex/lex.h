#ifndef LEX_H
#define LEX_H

#include "lex_word.h"

// 给结构体取一个别名，后面使用可以使用这个别名进行定义结构体
typedef struct char_s char_t;
typedef struct lex_s lex_t;

// 关键字
typedef struct
{
    char *text;// 关键字
    int type;// 类型
} key_word_t;

// 转义字符
typedef struct
{
    int origin;// 源数据
    int escape;// 转换之后
} escape_char_t;

#define UTF8_MAX 6
#define UTF8_LF 1

// 表示一个字符单元
/*
一个字母的存储，而utf8存储的是他的码点
*/
struct char_s {
    char_t *next;           // 下一个字符单元
    int c;                  // 保存当前字符的 Unicode 码点  对于ASCLL c就是0x00 - 0x7F
                            // 对于中文或其它非ASCII字符，则保存的是对应的Unicode编码值
    int len;                // 当前字符在 UTF-8 编码下占用的字节数(1-4)
                            // ‘a’ UTF-8 长度是1   ‘中’ UTF-8 长度是3
    uint8_t utf8[UTF8_MAX]; // 保存该字符的UTF-8 编码(字节序列)
    uint8_t flag;           // 保存字符的一些标记信息(是否是换行符、是否是空白、是否是标识符的一部分、是否是转义后的字符)
};

// 编译器前端 词法分析器lexer 的核心上下文结构体，用于保存 源代码扫描的状态
// 一个 lex_s 对应一个源文件
struct lex_s {
    lex_t *next;           // 指向下一个 lex_t 节点
    lex_word_t *word_list; // 指向词法分析得到的 单词(token)链表/列表，每个lex_word_t 可能表示一个标识符、关键字、常量、运算符等
    char_t *char_list;     // 指向字符链表，保存源码的所有字符(包含UTF-8信息、码点、flag)

    vector_t *macros; // 保存宏定义

    FILE *fp; // 标准 C 的文件指针，指向正在读取的源代码文件
              // 从文件流中读字符，逐个填入 char_list

    string_t *file; // 保存源代码的文件名
    int nb_lines;   // 记录源代码的总行数，用于错误信息、调试信息，lexer在扫描过程中会不断更新这个值
    int pos;        // 当前位置，通常表示在当前行或整个文件中的 字符索引
};

// 吐出一个字符 char
char_t *_lex_pop_char(lex_t *lex);
// 往 lex 推一个char
void _lex_push_char(lex_t *lex, char_t *c);

// 打开 lex
int lex_open(lex_t **plex, const char *path);
// 关闭 lex
int lex_close(lex_t *lex);

// 根据 char_t 分析得到的word
void lex_push_word(lex_t *lex, lex_word_t *word);
int lex_pop_word(lex_t *lex, lex_word_t **pword);

int __lex_pop_word(lex_t *lex, lex_word_t **pword);

int _lex_number_base_16(lex_t *lex, lex_word_t **pword, string_t *s);
int _lex_number_base_10(lex_t *lex, lex_word_t **pword, string_t *s);
int _lex_number_base_8(lex_t *lex, lex_word_t **pword, string_t *s);

int _lex_number_base_2(lex_t *lex, lex_word_t **pword, string_t *s);
int _lex_double(lex_t *lex, lex_word_t **pword, string_t *s);

int _lex_dot(lex_t *lex, lex_word_t **pword, char_t *c0);

int _lex_op1_ll1(lex_t *lex, lex_word_t **pword, char_t *c0, int type0);
int _lex_op2_ll1(lex_t *lex, lex_word_t **pword, char_t *c0, int type0, char *chs, int *types, int n);
int _lex_op3_ll1(lex_t *lex, lex_word_t **pword, char_t *c0, char ch1_0, char ch1_a, char ch2, int type0, int type1, int type2, int type3);

#endif
