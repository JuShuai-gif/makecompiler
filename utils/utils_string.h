#ifndef UTILS_STRING_H
#define UTILS_STRING_H

#include "utils_vector.h"

// 字符串结构体
typedef struct 
{
    int capacity;// 容量
    size_t len;// 长度
    char* data;// 数据
}string_t;


string_t* string_alloc();
string_t* string_clone(string_t* s);
string_t* string_cstr(const char* str);
string_t* string_cstr_len(const char* str,size_t len);

void string_free(string_t* s);
void string_print_bin(string_t* s);
int string_fill_zero(string_t* s0,size_t len);
int string_cmp(const string_t* s0,const string_t* s1);

int string_cmp_cstr(const string_t* s0,const char* str);

int string_cmp_cstr_len(const string_t* s0,const char* str,size_t len);

int string_copy(string_t* s0,const string_t* s1);
int string_cat(string_t* s0,const string_t* s1);
int string_cat_cstr(string_t* s0,const char* str);
int string_cat_cstr_len(string_t* s0,const char* str,size_t len);

int string_match_kmp(const string_t* T,const string_t* P,vector_t* offsets);
int string_match_kmp_cstr(const uint8_t* T,const uint8_t* P,vector_t* offsets);
int string_match_kmp_cstr_len(const string_t* T,const uint8_t* P,size_t Plen,vector_t* offsets);

int string_get_offset(string_t* str,const char* data,size_t len);

#endif


















