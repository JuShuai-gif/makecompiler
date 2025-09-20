

#include "optimizer.h"
#include "pointer_alias.h"

static int _alias_call(vector_t *aliases, _3ac_code_t *c, basic_block_t *bb, list_t *bb_list_head) {
    _3ac_operand_t *src;
    dn_status_t *ds;
    dag_node_t *dn;
    variable_t *v;
    dag_node_t *dn_pointer;
    vector_t *dn_pointers;

    int i;
    int j;
    int k = aliases->size;

    assert(c->srcs && c->srcs->size >= 1);

    for (i = 1; i < c->srcs->size; i++) {
        src = c->srcs->data[i];
        dn = src->dag_node;
        v = dn->var;

        if (0 == v->nb_pointers)
            continue;

        if (variable_const_string(v))
            continue;

        if (OP_VA_ARG == dn->type)
            continue;

        logd("pointer: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

        dn_pointers = vector_alloc();
        if (!dn_pointers)
            return -ENOMEM;

        int ret = vector_add_unique(dn_pointers, dn);
        if (ret < 0) {
            vector_free(dn_pointers);
            return ret;
        }

        for (j = 0; j < dn_pointers->size; j++) {
            dn_pointer = dn_pointers->data[j];
            v = dn_pointer->var;

            logd("i: %d, dn_pointers->size: %d, pointer: v_%d_%d/%s\n", i, dn_pointers->size, v->w->line, v->w->pos, v->w->text->data);

            ret = __alias_dereference(aliases, dn_pointer, c, bb, bb_list_head);
            if (ret < 0) {
                if (dn == dn_pointer || 1 == dn->var->nb_pointers) {
                    loge("\n");
                    vector_free(dn_pointers);
                    return ret;
                }
            }

            if (dn != dn_pointer && dn->var->nb_pointers > 1) {
                logd("pointer: v_%d_%d/%s,   DN_ALIAS_ALLOC\n", v->w->line, v->w->pos, v->w->text->data);

                ds = calloc(1, sizeof(dn_status_t));
                if (!ds) {
                    vector_free(dn_pointers);
                    return -ENOMEM;
                }

                ret = vector_add(aliases, ds);
                if (ret < 0) {
                    dn_status_free(ds);
                    vector_free(dn_pointers);
                    return ret;
                }

                ds->dag_node = dn_pointer;
                ds->alias_type = DN_ALIAS_ALLOC;
            }

            for (; k < aliases->size; k++) {
                ds = aliases->data[k];

                if (DN_ALIAS_VAR != ds->alias_type)
                    continue;

                if (0 == ds->alias->var->nb_pointers)
                    continue;

                ret = vector_add_unique(dn_pointers, ds->alias);
                if (ret < 0) {
                    vector_free(dn_pointers);
                    return ret;
                }
            }
        }

        vector_free(dn_pointers);
        dn_pointers = NULL;
    }

    return 0;
}

static int __optimize_call_bb(_3ac_code_t *c, basic_block_t *bb, list_t *bb_list_head) {
    vector_t *aliases = vector_alloc();
    if (!aliases)
        return -ENOMEM;

    int ret = _alias_call(aliases, c, bb, bb_list_head);
    if (ret < 0) {
        loge("\n");
        vector_free(aliases);
        return ret;
    }

    if (aliases->size > 0) {
        ret = _bb_copy_aliases2(bb, aliases);
        if (ret < 0) {
            vector_free(aliases);
            return ret;
        }
    }

    vector_free(aliases);
    aliases = NULL;
    return 0;
}

static int _optimize_call_bb(basic_block_t *bb, list_t *bb_list_head) {
    _3ac_code_t *c;
    list_t *l;

    l = list_head(&bb->code_list_head);
    c = list_data(l, _3ac_code_t, list);

    assert(OP_CALL == c->op->type);
    assert(list_next(l) == list_sentinel(&bb->code_list_head));

    return __optimize_call_bb(c, bb, bb_list_head);
}

static int _optimize_call(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    list_t *bb_list_head = &f->basic_block_list_head;
    list_t *l;
    basic_block_t *bb;

    if (list_empty(bb_list_head))
        return 0;

    for (l = list_head(bb_list_head); l != list_sentinel(bb_list_head);) {
        bb = list_data(l, basic_block_t, list);
        l = list_next(l);

        if (bb->jmp_flag || bb->end_flag || bb->cmp_flag)
            continue;

        if (!bb->call_flag)
            continue;

        int ret = _optimize_call_bb(bb, bb_list_head);
        if (ret < 0)
            return ret;
    }

    //	  basic_block_print_list(bb_list_head);
    return 0;
}

optimizer_t optimizer_call =
    {
        .name = "call",

        .optimize = _optimize_call,

        .flags = OPTIMIZER_LOCAL,
};
