#include "lex_word.h"

int main(){
    enum lex_words m = LEX_WORD_PLUS;

    enum lex_words mm = LEX_WORD_ID;

    printf("%d\n", m);  // 输出 1
    printf("%d\n",mm);

    complex_t comp;
    comp.real = 5.2;
    comp.imag = 4.2;
    
    printf("comp.real:%.2f,comp.imag:%.2f\n",comp.real,comp.imag);

    lex_word_t* word0 = malloc(sizeof(lex_word_t));
    
    lex_word_t* word1 = malloc(sizeof(lex_word_t));

    word1->next = word0;
    word1->type = LEX_WORD_KEY_INT8;
    word1->data.i = 42;
    word1->text = "42";

    
    macro_t macr;
    macr.refs = 42;
    



    return 0;
}
