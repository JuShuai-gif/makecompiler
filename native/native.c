#include "native.h"

extern native_ops_t native_ops_x64;
extern native_ops_t native_ops_risc;

void instruction_print(instruction_t *inst) {
    if (inst->OpCode)
        printf("%s ", inst->OpCode->name);

    if (1 == inst->src.flag) {
        if (inst->src.index)
            printf("%d(%s, %s, %d), ", inst->src.disp, inst->src.base->name,
                   inst->src.index->name, inst->src.scale);

        else if (inst->src.base) {
            if (inst->src.disp < 0)
                printf("-%#x(%s), ", -inst->src.disp, inst->src.base->name);
            else
                printf("%#x(%s), ", inst->src.disp, inst->src.base->name);
        } else
            printf("%d(rip), ", inst->dst.disp);

    } else if (inst->src.base)
        printf("%s, ", inst->src.base->name);

    else if (inst->src.imm_size > 0)
        printf("%d, ", (int)inst->src.imm);

    if (1 == inst->dst.flag) {
        if (inst->dst.index)
            printf("%d(%s, %s, %d), ", inst->dst.disp, inst->dst.base->name,
                   inst->dst.index->name, inst->dst.scale);

        else if (inst->dst.base) {
            if (inst->dst.disp < 0)
                printf("-%#x(%s), ", -inst->dst.disp, inst->dst.base->name);
            else
                printf("%#x(%s), ", inst->dst.disp, inst->dst.base->name);
        } else
            printf("%d(rip), ", inst->dst.disp);

    } else if (inst->dst.base)
        printf("%s, ", inst->dst.base->name);

    else if (inst->dst.imm_size > 0)
        printf("%d, ", (int)inst->dst.imm);

    printf("\n");
}

int native_open(native_t **pctx, const char *name) {
    native_t *ctx = calloc(1, sizeof(native_t));
    assert(ctx);

    if (!strcmp(name, "x64"))
        ctx->ops = &native_ops_x64;

    else if (!strcmp(name, "eda"))
        ctx->ops = &native_ops_eda;
    else
        ctx->ops = &native_ops_risc;

    if (ctx->ops->open && ctx->ops->open(ctx, name) == 0) {
        *pctx = ctx;
        return 0;
    }

    printf("%s(),%d, error: \n", __func__, __LINE__);

    free(ctx);
    ctx = NULL;
    return -1;
}

int native_close(native_t *ctx) {
    if (ctx) {
        if (ctx->ops && ctx->ops->close) {
            ctx->ops->close(ctx);
        }

        free(ctx);
        ctx = NULL;
    }
    return 0;
}

int native_select_inst(native_t *ctx, function_t *f) {
    if (ctx && f) {
        if (ctx->ops && ctx->ops->select_inst)
            return ctx->ops->select_inst(ctx, f);
    }

    printf("%s(),%d, error: \n", __func__, __LINE__);
    return -1;
}
