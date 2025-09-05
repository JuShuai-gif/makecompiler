#ifndef SCOPE_H
#define SCOPE_H

#include "list.h"
#include "utils_vector.h"
#include "lex_word.h"
#include "core_types.h"

struct scope_t
{
    list_t list;
    string_t* name;

    lex_word_t* w;

    vector_t* vars;
    list_t type_list_head;
    list_t operator_list_head;
    list_t function_list_head;
    list_t label_list_head;
};

scope_t* scope_alloc(lex_word_t* w,const char* name);
void scope_free(scope_t* scope);

void scope_push_var(scope_t* scope,variable_t* var);

void scope_push_type(scope_t* scope,type_t* t);

void scope_push_operator(scope_t* scope,function_t* op);

void scope_push_function(scope_t* scope,function_t* f);

type_t* scope_find_type(scope_t* scope,const char* name);
type_t* scope_find_type_type(scope_t* scope,const int type);

variable_t* scope_find_variable(scope_t* scope, const char* name);
function_t* scope_find_function(scope_t* scope, const char* name);
function_t* scope_find_same_function(scope_t* scope, function_t* f0);
function_t* scope_find_proper_function(scope_t* scope, const char* name, vector_t* argv);

int scope_find_overloaded_functions(vector_t** pfunctions, scope_t* scope, const int op_type, vector_t* argv);

int scope_find_like_functions(vector_t** pfunctions, scope_t* scope, const char* name, vector_t* argv);

void scope_push_label(scope_t* scope, label_t* l);
label_t* scope_find_label(scope_t* scope, const char* name);

#endif