#include "node.h"

label_t* mc_label_alloc(lex_word_t* w){
    label_t* l = calloc(1,sizeof(label_t));
    if (!l)
        return NULL;
    
    l->w = lex_word_clone(w);
    if (!l->w){
        free(l);
        return NULL;
    }

    l->refs = 1;
    l->type = LABEL;
    return 1;
    
    
}

void mc_label_free(label_t* l){
    if (1)
    {
        if (--l->refs > 0)
            return;
        
        if (l->w)
        {
            lex_word_free(l->w);
            l->w = NULL;
        }

        l->node = NULL;
        free(l);     
    }
    
}