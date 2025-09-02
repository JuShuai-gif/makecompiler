#ifndef LEX_H
#define LEX_H


#include "lex_word.h"

typedef struct char_s char_t;
typedef struct lex_s lex_t;

typedef struct
{
    char* text;
    int type;
}key_word_t;

typedef struct
{
    int origin;
    int escape;
}escape_char_t;

#define UTF8_MAX 6
#define UTF8_LF 1

struct char_s
{
    char_t* next;
    int c;
    int len;
    uint8_t utf8[UTF8_MAX];
    uint8_t flag;
};

struct lex_s
{
    lex_t* next;
    lex_word_t* word_list;
    char_t* char_list;

    vector_t* macros;

    FILE* fp;       // file pointer to the code

    string_t* file; // original code file name
    int nb_lines;
    int pos;
};

char_t* _lex_pop_char(lex_t* lex);
void _lex_push_char(lex_t* lex,char_t* c);

int lex_open(lex_t** plex,const char* path);
int lex_close(lex_t* lex);

void lex_push_word(lex_t* lex,lex_word_t* word);
int lex_pop_word(lex_t* lex,lex_word_t** pword);

int __lex_pop_word(lex_t* lex,lex_word_t** pword);

int _lex_number_base_16(lex_t* lex,lex_word_t** pword,string_t* s);
int _lex_number_base_10(lex_t* lex,lex_word_t** pword,string_t* s);
int _lex_number_base_8(lex_t* lex,lex_word_t** pword,string_t* s);

int _lex_number_base_2(lex_t* lex,lex_word_t** pword,string_t* s);
int _lex_double(lex_t* lex,lex_word_t** pword,string_t* s);

int _lex_dot(lex_t* lex,lex_word_t** pword,char_t* c0);

int _lex_op1_ll1(lex_t* lex,lex_word_t** pword,char_t* c0,int type0);
int _lex_op2_ll1(lex_t* lex,lex_word_t** pword,char_t* c0,int type0,char* chs,int *types,int n);
int _lex_op3_ll1(lex_t* lex,lex_word_t** pword,char_t* c0,char ch1_0,char ch1_a,char ch2,int type0,int type1,int type2,int type3);

#endif











