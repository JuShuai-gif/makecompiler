#include <stdio.h>
#include "lex_word.h"

int main(){
    enum lex_words word0 = LEX_WORD_PLUS;
    enum lex_words word1 = LEX_WORD_MINUS;
    
    printf("word0: %d\n",word0);
    printf("word1: %d\n",word1);

    complex_t* com_0 = calloc(1,sizeof(complex_t));
    com_0->real = 42;
    com_0->imag = -2;

    printf("复数 complex_t 实部:%f,虚部:%d\n",com_0->real,com_0->imag);





}