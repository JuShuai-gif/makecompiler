#include <stdio.h>
#include "type.h"

int main(){
    base_type_t* base_type = calloc(1,sizeof(base_type_t));
    base_type->type = 0;
    base_type->name = "int";
    base_type->size = 4;

    printf("base_type type:%d,\tname:%s\t,size:%d\n",base_type->type,base_type->name,base_type->size);


    type_abbrev_t* abbrev_t = calloc(1,sizeof(type_abbrev_t));
    abbrev_t->name  = "unsigned int";
    abbrev_t->abbrev = "uint";
    printf("name:%s\t,abbrev:%s\n",abbrev_t->name,abbrev_t->abbrev);
    
}









