#include "x64.h"
#include "elf.h"
#include "basic_block.h"
#include "3ac.h"

extern native_ops_t native_ops_x64;

int x64_open(native_t *ctx,const char* arch){
    x64_context_t* x64 = calloc(1,sizeof(x64_context_t));
    if (!x64)
        return -ENOMEM;

    ctx->priv = x64;
    return 0;
    
}


int x64_close(native_t* ctx){
    x64_context_t* x64 = ctx->priv;

    if (x64)
    {
        x64_registers_clear();

        free(x64);

        x64 = NULL;
    }
    return 0;
}




