#ifndef TYPE_H
#define TYPE_H

#include "node.h"

typedef struct
{
    int type;
    const char* name;
    int size;
}base_type_t;

typedef struct
{
    const char* name;
    const char* abbrev;
}type_abbrev_t;


struct type_s
{
    node_t node;
    scope_t* scope;
    
    string_t* name;

    list_t list;

    int type;

    lex_word_t* w;

    int nb_pointers;

    function_t* func_ptr;

    int size;
    int offset;

    type_t* parent;
};

type_t* type_alloc(lex_word_t* w,const char* name,int type,int size);
void type_free(type_t* t);

const char* type_find_abbrev(const char* name);

#endif