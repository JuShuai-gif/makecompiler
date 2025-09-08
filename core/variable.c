#include "variable.h"
#include "type.h"
#include "function.h"

member_t* member_alloc(variable_t* base){
    member_t* m = calloc(1,sizeof(member_t));

    if (!m)
        return NULL;
    
    m->base = base;
    return m;
}

void member_free(member_t* m){
    if (m)
    {
        if (m->indexes)
        {
            /* code */
        }
        
    }
    
}



