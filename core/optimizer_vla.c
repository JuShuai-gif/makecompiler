#include "optimizer.h"

static int __bb_add_vla_free(basic_block_t *back, function_t *f) {
    basic_block_t *jcc = list_data(list_next(&back->list), basic_block_t, list);
    basic_block_t *next = list_data(list_next(&jcc->list), basic_block_t, list);
    basic_block_t *free;
    basic_block_t *jmp;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    _3ac_code_t * c2;
    list_t *l;

    assert(jcc->jmp_flag);

    free = basic_block_alloc();
    if (!free)
        return -ENOMEM;
    list_add_front(&jcc->list, &free->list);

    jmp = basic_block_alloc();
    if (!jmp)
        return -ENOMEM;
    list_add_front(&free->list, &jmp->list);

    l = list_head(&jcc->code_list_head);
    c = list_data(l, _3ac_code_t, list);

    c2 = _3ac_code_clone(c);
    if (!c2)
        return -ENOMEM;
    list_add_tail(&jmp->code_list_head, &c2->list);

    c2->op = _3ac_find_operator(OP_GOTO);
    c2->basic_block = jmp;

    int ret = vector_add(f->jmps, c2);
    if (ret < 0)
        return ret;

    switch (c->op->type) {
    case OP_3AC_JZ:
        c->op = _3ac_find_operator(OP_3AC_JNZ);
        break;
    case OP_3AC_JNZ:
        c->op = _3ac_find_operator(OP_3AC_JZ);
        break;

    case OP_3AC_JGE:
        c->op = _3ac_find_operator(OP_3AC_JLT);
        break;
    case OP_3AC_JLT:
        c->op = _3ac_find_operator(OP_3AC_JGE);
        break;

    case OP_3AC_JGT:
        c->op = _3ac_find_operator(OP_3AC_JLE);
        break;
    case OP_3AC_JLE:
        c->op = _3ac_find_operator(OP_3AC_JGT);
        break;

    case OP_3AC_JA:
        c->op = _3ac_find_operator(OP_3AC_JBE);
        break;
    case OP_3AC_JBE:
        c->op = _3ac_find_operator(OP_3AC_JA);
        break;

    case OP_3AC_JB:
        c->op = _3ac_find_operator(OP_3AC_JAE);
        break;
    case OP_3AC_JAE:
        c->op = _3ac_find_operator(OP_3AC_JB);
        break;
    default:
        break;
    };
    dst = c->dsts->data[0];
    dst->bb = next;

    back->vla_free = free;
    return 0;
}

static int __loop_add_vla_free(bb_group_t *loop, function_t *f) {
    basic_block_t *bb;
    basic_block_t *back;
    basic_block_t *jcc;
    _3ac_operand_t * dst;
    _3ac_code_t * c;
    _3ac_code_t * c2;
    list_t *l;

    int i;
    int j;
    int k;

    for (i = 0; i < loop->body->size; i++) {
        bb = loop->body->data[i];

        if (!bb->vla_flag)
            continue;

        for (j = i; j < loop->body->size; j++) {
            back = loop->body->data[j];

            if (!back->back_flag)
                continue;

            jcc = list_data(list_next(&back->list), basic_block_t, list);

            l = list_head(&jcc->code_list_head);
            c = list_data(l, _3ac_code_t, list);
            dst = c->dsts->data[0];

            for (k = j; k > i; k--) {
                if (dst->bb == loop->body->data[k])
                    break;
            }
            if (k > i)
                continue;

            if (!back->vla_free) {
                int ret = __bb_add_vla_free(back, f);
                if (ret < 0)
                    return ret;

                ret = vector_add(loop->body, back->vla_free);
                if (ret < 0)
                    return ret;

                for (k = loop->body->size - 2; k > j; k--)
                    loop->body->data[k + 1] = loop->body->data[k];
                loop->body->data[k + 1] = back->vla_free;

                back->vla_free->loop_flag = 1;
            }

            for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); l = list_next(l)) {
                c = list_data(l, _3ac_code_t, list);

                if (OP_VLA_ALLOC != c->op->type)
                    continue;

                c2 = _3ac_code_clone(c);
                if (!c2)
                    return -ENOMEM;
                list_add_front(&back->vla_free->code_list_head, &c2->list);

                c2->op = _3ac_find_operator(OP_VLA_FREE);
                c2->basic_block = back->vla_free;
            }
        }
    }

    return 0;
}

static int _optimize_vla(ast_t *ast, function_t *f, vector_t *functions) {
    if (!f)
        return -EINVAL;

    if (!f->vla_flag || f->bb_loops->size <= 0)
        return 0;

    bb_group_t *loop;
    int i;

    for (i = 0; i < f->bb_loops->size; i++) {
        loop = f->bb_loops->data[i];

        int ret = __loop_add_vla_free(loop, f);
        if (ret < 0)
            return ret;
    }

    return 0;
}

optimizer_t optimizer_vla =
    {
        .name = "vla",

        .optimize = _optimize_vla,

        .flags = OPTIMIZER_LOCAL,
};
