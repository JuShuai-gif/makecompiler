#include <stdio.h>
#include "variable.h"
#include "lex_word.h"
#include "lex.h"

int main() {
    lex_t *lex = NULL;

    if (lex_open(&lex, "/home/ghr/code/compiler/makecompiler/lex/example.c")) {
        loge("\n");
        return -1;
    }
    lex_word_t *w = NULL;

    lex_word_t* w_var = NULL;

    while (1) {
        if (lex_pop_word(lex, &w) < 0) {
            loge("\n");
            return -1;
        }

        if (w->type == LEX_WORD_ID) {
            printf("word: type: %d, line: %d, pos: %d, text: %s\n",
                   w->type, w->line, w->pos, w->text->data);
            w_var = w;
        }

        // if (LEX_WORD_CONST_STRING == w->type) {
        //     printf(", data: %s", w->data.s->data);

        // } else if (LEX_WORD_CONST_CHAR == w->type || LEX_WORD_CONST_INT == w->type) {
        //     printf(", data: %d", w->data.i);

        // } else if (LEX_WORD_CONST_DOUBLE == w->type) {
        //     printf(", data: %lg", w->data.d);
        // }
        // printf("\n");

        if (LEX_WORD_EOF == w->type) {
            logi("eof\n");
            break;
        }

        lex_word_free(w);
        w = NULL;
    }
    logi("main ok\n");

    variable_t* var = variable_alloc(w_var,)



}
