#include "3ac.h"
#include "function.h"
#include "basic_block.h"
#include "utils_graph.h"

// 操作符类型
static _3ac_operand_t _3ac_operators[] = {
    {OP_CALL, "call"},

    {OP_ARRAY_INDEX, "array_index"},
    {OP_POINTER, "pointer"},
    {OP_TYPE_CAST, "cast"},
    {OP_LOGIC_NOT, "logic_not"},
    {OP_BIT_NOT, "not"},
    {OP_NEG, "neg"},
    {OP_POSITIVE, "positive"},
    {OP_INC, "inc"},
    {OP_DEC, "dec"},
    {OP_INC_POST, "inc_post"},
    {OP_DEC_POST, "dec_post"},
    {OP_DEREFERENCE, "dereference"},
    {OP_ADDRESS_OF, "address_of"},
    {OP_MUL, "mul"},
    {OP_DIV, "div"},
    {OP_MOD, "mod"},
    {OP_ADD, "add"},
    {OP_SUB, "sub"},
    {OP_SHL, "shl"},
    {OP_SHR, "shr"},
    {OP_BIT_AND, "and"},
    {OP_BIT_OR, "or"},

    {OP_EQ, "eq"},
    {OP_NE, "neq"},
    {OP_GT, "gt"},
    {OP_LT, "lt"},
    {OP_GE, "ge"},
    {OP_LE, "le"},

    {OP_ASSIGN, "assign"},
    {OP_ADD_ASSIGN, "+="},
    {OP_SUB_ASSIGN, "-="},
    {OP_MUL_ASSIGN, "*="},
    {OP_DIV_ASSIGN, "/="},
    {OP_MOD_ASSIGN, "%="},
    {OP_SHL_ASSIGN, "<<="},
    {OP_SHR_ASSIGN, ">>="},
    {OP_AND_ASSIGN, "&="},
    {OP_OR_ASSIGN, "|="},

    {OP_VLA_ALLOC, "vla_alloc"},
    {OP_VLA_FREE, "vla_free"},

    {OP_RETURN, "return"},
    {OP_GOTO, "jmp"},

    {OP_3AC_TEQ, "teq"},
    {OP_3AC_CMP, "cmp"},

    {OP_3AC_LEA, "lea"},

    {OP_3AC_SETZ, "setz"},
    {OP_3AC_SETNZ, "setnz"},
    {OP_3AC_SETGT, "setgt"},
    {OP_3AC_SETGE, "setge"},
    {OP_3AC_SETLT, "setlt"},
    {OP_3AC_SETLE, "setle"},

    {OP_3AC_SETA, "seta"},
    {OP_3AC_SETAE, "setae"},
    {OP_3AC_SETB, "setb"},
    {OP_3AC_SETBE, "setbe"},

    {OP_3AC_JZ, "jz"},
    {OP_3AC_JNZ, "jnz"},
    {OP_3AC_JGT, "jgt"},
    {OP_3AC_JGE, "jge"},
    {OP_3AC_JLT, "jlt"},
    {OP_3AC_JLE, "jle"},

    {OP_3AC_JA, "ja"},
    {OP_3AC_JAE, "jae"},
    {OP_3AC_JB, "jb"},
    {OP_3AC_JBE, "jbe"},

    {OP_3AC_DUMP, "core_dump"},
    {OP_3AC_NOP, "nop"},
    {OP_3AC_END, "end"},

    {OP_3AC_PUSH, "push"},
    {OP_3AC_POP, "pop"},
    {OP_3AC_SAVE, "save"},
    {OP_3AC_LOAD, "load"},
    {OP_3AC_RELOAD, "reload"},
    {OP_3AC_RESAVE, "resave"},

    {OP_3AC_PUSH_RETS, "push rets"},
    {OP_3AC_POP_RETS, "pop  rets"},
    {OP_3AC_MEMSET, "memset"},

    {OP_3AC_INC, "inc3"},
    {OP_3AC_DEC, "dec3"},

    {OP_3AC_ASSIGN_DEREFERENCE, "dereference="},
    {OP_3AC_ASSIGN_ARRAY_INDEX, "array_index="},
    {OP_3AC_ASSIGN_POINTER, "pointer="},

    {OP_3AC_ADDRESS_OF_ARRAY_INDEX, "&array_index"},
    {OP_3AC_ADDRESS_OF_POINTER, "&pointer"},

};

// 根据类型，寻找操作
_3ac_operand_t *_3ac_find_operator(const int type) {
    int i;
    for (i = 0; i < sizeof(_3ac_operators) / sizeof(_3ac_operators[0]); i++) {
        if (type == _3ac_operators[i].type) {
            return &(_3ac_operators[i]);
        }
    }
    return NULL;
}

// 分配操作数
_3ac_operand_t *_3ac_operand_alloc() {
    _3ac_operand_t *operand = calloc(1, sizeof(_3ac_operand_t));
    assert(operand);
    return operand;
}

// 释放操作数
void _3ac_operand_free(_3ac_operand_t *operand) {
    if (operand) {
        free(operand);
        operand = NULL;
    }
}

// 分配三地址代码
_3ac_code_t *_3ac_code_alloc() {
    _3ac_code_t *c = calloc(1, sizeof(_3ac_code_t));

    return c;
}

// 三地址代码是否相同
int _3ac_code_same(_3ac_code_t *c0, _3ac_code_t *c1) {
    // 操作是否相同
    if (c0->op != c1->op)
        return 0;

    if (c0->dsts) {
        if (!c1->dsts)
            return 0;

        if (c0->dsts->size != c1->dsts->size)
            return 0;

        int i;
        for (i = 0; i < c0->dsts->size; i++) {
            _3ac_operand_t *dst0 = c0->dsts->data[i];
            _3ac_operand_t *dst1 = c1->dsts->data[i];

            if (dst0->dag_node) {
                if (!dst1->dag_node)
                    return 0;

                if (!_dag_dn_same(dst0->dag_node, dst1->dag_node))
                    return 0;
            } else if (dst1->dag_node)
                return 0;
        }
    } else if (c1->dsts)
        return 0;

    if (c0->srcs) {
        if (!c1->srcs)
            return 0;

        if (c0->srcs->size != c1->srcs->size)
            return 0;

        int i;
        for (i = 0; i < c0->srcs->size; i++) {
            _3ac_operand_t *src0 = c0->srcs->data[i];
            _3ac_operand_t *src1 = c1->srcs->data[i];

            if (src0->dag_node) {
                if (!src1->dag_node)
                    return 0;

                if (!_dag_dn_same(src0->dag_node, src1->dag_node))
                    return 0;
            } else if (src1->dag_node)
                return 0;
        }
    } else if (c1->srcs)
        return 0;
    return 1;
}

// 克隆三地址码
_3ac_code_t *_3ac_code_clone(_3ac_code_t *c) {
    _3ac_code_t *c2 = calloc(1, sizeof(_3ac_code_t));
    if (!c2)
        return NULL;

    c2->op = c->op;

    if (c->dsts) {
        c2->dsts = _vector_alloc();
        if (!c2->dsts) {
            _3ac_code_free(c2);
            return NULL;
        }

        int i;
        for (i = 0; i < c->dsts->size; i++) {
            _3ac_operand_t *dst = c->dsts->data[i];
            _3ac_operand_t *dst2 = _3ac_operand_alloc();

            if (!dst2) {
                _3ac_code_free(c2);
                return NULL;
            }

            if (_vector_add(c2->dsts, dst2) < 0) {
                _3ac_code_free(c2);
                return NULL;
            }

            dst2->node = dst->node;
            dst2->dag_node = dst->dag_node;
            dst2->code = dst->code;
            dst2->bb = dst->bb;
        }
    }

    if (c->srcs) {
        c2->srcs = _vector_alloc();
        if (!c2->srcs) {
            _3ac_code_free(c2);
            return NULL;
        }

        int i;
        for (i = 0; i < c->srcs->size; i++) {
            _3ac_operand_t *src = c->srcs->data[i];
            _3ac_operand_t *src2 = _3ac_operand_alloc();

            if (!src2) {
                _3ac_code_free(c2);
                return NULL;
            }

            if (_vector_add(c2->srcs, src2) < 0) {
                _3ac_code_free(c2);
                return NULL;
            }

            src2->node = src->node;
            src2->dag_node = src->dag_node;
            src2->code = src->code;
            src2->bb = src->bb;
        }
    }

    c2->label = c->label;
    c2->origin = c;
    return c2;
}

// 三地址码释放
void _3ac_code_free(_3ac_code_t *c) {
    int i;

    if (c) {
        if (c->dsts) {
            for (i = 0; i < c->dsts->size; i++)
                _3ac_operand_free(c->dsts->data[i]);
            _vector_free(c->dsts);
        }

        if (c->srcs) {
            for (i = 0; i < c->srcs->size; i++)
                _3ac_operand_free(c->srcs->data[i]);
            _vector_free(c->srcs);
        }

        if (c->active_vars) {
            int i;
            for (i = 0; i < c->active_vars->size; i++)
                _dn_status_free(c->active_vars->data[i]);
            _vector_free(c->active_vars);
        }

        free(c);
        c = NULL;
    }
}

// 通过源进行分配
_3ac_code_t *_3ac_alloc_by_src(int op_type, dag_node_t *src) {
    _3ac_operator_t *_3ac_op = _3ac_find_operator(op_type);
    if (!_3ac_op) {
        _loge("3ac operator not found\n");
        return NULL;
    }

    _3ac_operand_t *src0 = _3ac_operand_alloc();
    if (!src0)
        return NULL;
    src0->dag_node = src;

    _vector_t *srcs = _vector_alloc();
    if (!srcs)
        goto error1;
    if (_vector_add(srcs, src0) < 0)
        goto error0;

    _3ac_code_t *c = _3ac_code_alloc();
    if (!c)
        goto error0;

    c->op = _3ac_op;
    c->srcs = srcs;
    return c;

error0:
    _vector_free(srcs);
error1:
    _3ac_operand_free(src0);
    return NULL;
}

// 通过目标进行分配
_3ac_code_t *_3ac_alloc_by_dst(int op_type, dag_node_t *dst) {
    _3ac_operator_t *op;
    _3ac_operand_t *d;
    _3ac_code_t *c;

    op = _3ac_find_operator(op_type);
    if (!op) {
        _loge("3ac operator not found\n");
        return NULL;
    }

    d = _3ac_operand_alloc();
    if (!d)
        return NULL;
    d->dag_node = dst;

    c = _3ac_code_alloc();
    if (!c) {
        _3ac_operand_free(d);
        return NULL;
    }

    c->dsts = _vector_alloc();
    if (!c->dsts) {
        _3ac_operand_free(d);
        _3ac_code_free(c);
        return NULL;
    }

    if (_vector_add(c->dsts, d) < 0) {
        _3ac_operand_free(d);
        _3ac_code_free(c);
        return NULL;
    }

    c->op = op;
    return c;
}

// 跳转
_3ac_code_t *_3ac_jmp_code(int type, label_t *l, node_t *err) {
    _3ac_operand_t *dst;
    _3ac_code_t *c;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->op = _3ac_find_operator(type);
    c->label = l;

    c->dsts = _vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (_vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    return c;
}

// 打印节点
static void _3ac_print_node(_node_t *node) {
    if (_type_is_var(node->type)) {
        if (node->var->w) {
            printf("v_%d_%d/%s/%#lx",
                   node->var->w->line, node->var->w->pos, node->var->w->text->data, 0xffff & (uintptr_t)node->var);
        } else {
            printf("v_%#lx", 0xffff & (uintptr_t)node->var);
        }
    } else if (_type_is_operator(node->type)) {
        if (node->result) {
            if (node->result->w) {
                printf("v_%d_%d/%s/%#lx",
                       node->result->w->line, node->result->w->pos, node->result->w->text->data, 0xffff & (uintptr_t)node->result);
            } else
                printf("v/%#lx", 0xffff & (uintptr_t)node->result);
        }
    } else if (_FUNCTION == node->type) {
        printf("f_%d_%d/%s",
               node->w->line, node->w->pos, node->w->text->data);
    } else {
        _loge("node: %p\n", node);
        _loge("node->type: %d\n", node->type);
        assert(0);
    }
}

// 打印有向无环图节点
static void _3ac_print_dag_node(_dag_node_t *dn) {
    if (_type_is_var(dn->type)) {
        if (dn->var->w) {
            printf("v_%d_%d/%s_%#lx ",
                   dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
                   0xffff & (uintptr_t)dn);
        } else {
            printf("v_%#lx", 0xffff & (uintptr_t)dn->var);
        }
    } else if (_type_is_operator(dn->type)) {
        _3ac_operator_t *op = _3ac_find_operator(dn->type);
        if (dn->var && dn->var->w)
            printf("v_%d_%d/%s_%#lx ",
                   dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
                   0xffff & (uintptr_t)dn);
        else
            printf("v_%#lx/%s ", 0xffff & (uintptr_t)dn->var, op->name);
    } else {
        // printf("type: %d, v_%#lx\n", dn->type, 0xffff & (uintptr_t)dn->var);
        // assert(0);
    }
}

// 三地址码打印
void _3ac_code_print(_3ac_code_t *c, list_t *sentinel) {
    _3ac_operand_t *src;
    _3ac_operand_t *dst;

    int i;

    printf("%s  ", c->op->name);

    if (c->dsts) {
        for (i = 0; i < c->dsts->size; i++) {
            dst = c->dsts->data[i];

            if (dst->dag_node)
                _3ac_print_dag_node(dst->dag_node);

            else if (dst->node)
                _3ac_print_node(dst->node);

            else if (dst->code) {
                if (&dst->code->list != sentinel) {
                    printf(": ");
                    _3ac_code_print(dst->code, sentinel);
                }
            } else if (dst->bb)
                printf(" bb: %p, index: %d ", dst->bb, dst->bb->index);

            if (i < c->dsts->size - 1)
                printf(", ");
        }
    }

    if (c->srcs) {
        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            if (0 == i && c->dsts && c->dsts->size > 0)
                printf("; ");

            if (src->dag_node)
                _3ac_print_dag_node(src->dag_node);

            else if (src->node)
                _3ac_print_node(src->node);

            else if (src->code)
                assert(0);

            if (i < c->srcs->size - 1)
                printf(", ");
        }
    }

    printf("\n");
}

// 三地址码转有向无环图
static int _3ac_code_to_dag(_3ac_code_t *c, _list_t *dag, int nb_operands0, int nb_operands1) {
    _3ac_operand_t *dst;
    _3ac_operand_t *src;
    _dag_node_t *dn;

    int ret;
    int i;
    int j;

    if (c->dsts) {
        for (j = 0; j < c->dsts->size; j++) {
            dst = c->dsts->data[j];

            if (!dst || !dst->node)
                continue;

            ret = _dag_get_node(dag, dst->node, &dst->dag_node);
            if (ret < 0)
                return ret;
        }
    }

    if (!c->srcs)
        return 0;

    for (i = 0; i < c->srcs->size; i++) {
        src = c->srcs->data[i];

        if (!src || !src->node)
            continue;

        ret = _dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        if (!c->dsts)
            continue;

        for (j = 0; j < c->dsts->size; j++) {
            dst = c->dsts->data[j];

            if (!dst || !dst->dag_node)
                continue;

            dn = dst->dag_node;

            if (!dn->childs || dn->childs->size < nb_operands0) {
                ret = _dag_node_add_child(dn, src->dag_node);
                if (ret < 0)
                    return ret;
                continue;
            }

            int k;
            for (k = 0; k < dn->childs->size && k < nb_operands1; k++) {
                if (src->dag_node == dn->childs->data[k])
                    break;
            }

            if (k == dn->childs->size) {
                _loge("i: %d, c->op: %s, dn->childs->size: %d, c->srcs->size: %d\n",
                      i, c->op->name, dn->childs->size, c->srcs->size);
                _3ac_code_print(c, NULL);
                return -1;
            }
        }
    }

    return 0;
}

int _3ac_code_to_dag(_3ac_code_t *c, list_t *dag) {
    _3ac_operand_t *src;
    _3ac_operand_t *dst;

    int ret = 0;
    int i;

    if (_type_is_assign(c->op->type)) {
        src = c->srcs->data[0];
        dst = c->dsts->data[0];

        ret = _dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = _dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        if (OP_ASSIGN == c->op->type && src->dag_node == dst->dag_node)
            return 0;

        _dag_node_t *dn_src;
        _dag_node_t *dn_parent;
        _dag_node_t *dn_assign;
        _variable_t *v_assign = NULL;

        if (dst->node->parent)
            v_assign = _ _operand_get(dst->node->parent);

        dn_assign = _dag_node_alloc(c->op->type, v_assign, NULL);

        _list_add_tail(dag, &dn_assign->list);

        dn_src = src->dag_node;

        if (dn_src->parents && dn_src->parents->size > 0 && !_variable_may_malloced(dn_src->var)) {
            dn_parent = dn_src->parents->data[dn_src->parents->size - 1];

            if (OP_ASSIGN == dn_parent->type) {
                assert(2 == dn_parent->childs->size);
                dn_src = dn_parent->childs->data[1];
            }
        }

        ret = _dag_node_add_child(dn_assign, dst->dag_node);
        if (ret < 0)
            return ret;

        return _dag_node_add_child(dn_assign, dn_src);

    } else if (_type_is_assign_array_index(c->op->type)
               || _type_is_assign_dereference(c->op->type)
               || _type_is_assign_pointer(c->op->type)) {
        _dag_node_t *assign;

        assert(c->srcs);

        assign = _dag_node_alloc(c->op->type, NULL, NULL);
        if (!assign)
            return -ENOMEM;
        _list_add_tail(dag, &assign->list);

        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            ret = _dag_get_node(dag, src->node, &src->dag_node);
            if (ret < 0)
                return ret;

            ret = _dag_node_add_child(assign, src->dag_node);
            if (ret < 0)
                return ret;
        }

    } else if (OP_VLA_ALLOC == c->op->type) {
        dst = c->dsts->data[0];
        ret = _dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        _dag_node_t *alloc = _dag_node_alloc(c->op->type, NULL, NULL);
        if (!alloc)
            return -ENOMEM;
        _list_add_tail(dag, &alloc->list);

        ret = _dag_node_add_child(alloc, dst->dag_node);
        if (ret < 0)
            return ret;

        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            ret = _dag_get_node(dag, src->node, &src->dag_node);
            if (ret < 0)
                return ret;

            ret = _dag_node_add_child(alloc, src->dag_node);
            if (ret < 0)
                return ret;
        }
    } else if (OP_3AC_CMP == c->op->type
               || OP_3AC_TEQ == c->op->type
               || OP_3AC_DUMP == c->op->type) {
        _dag_node_t *dn_cmp = _dag_node_alloc(c->op->type, NULL, NULL);

        _list_add_tail(dag, &dn_cmp->list);

        if (c->srcs) {
            int i;
            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                ret = _dag_get_node(dag, src->node, &src->dag_node);
                if (ret < 0)
                    return ret;

                ret = _dag_node_add_child(dn_cmp, src->dag_node);
                if (ret < 0)
                    return ret;
            }
        }
    } else if (OP_3AC_SETZ == c->op->type
               || OP_3AC_SETNZ == c->op->type
               || OP_3AC_SETLT == c->op->type
               || OP_3AC_SETLE == c->op->type
               || OP_3AC_SETGT == c->op->type
               || OP_3AC_SETGE == c->op->type) {
        assert(c->dsts && 1 == c->dsts->size);
        dst = c->dsts->data[0];

        _dag_node_t *dn_setcc = _dag_node_alloc(c->op->type, NULL, NULL);
        _list_add_tail(dag, &dn_setcc->list);

        ret = _dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        ret = _dag_node_add_child(dn_setcc, dst->dag_node);
        if (ret < 0)
            return ret;

    } else if (OP_INC == c->op->type
               || OP_DEC == c->op->type
               || OP_INC_POST == c->op->type
               || OP_DEC_POST == c->op->type
               || OP_3AC_INC == c->op->type
               || OP_3AC_DEC == c->op->type) {
        src = c->srcs->data[0];

        assert(src->node->parent);

        _variable_t *v_parent = _ _operand_get(src->node->parent);
        _dag_node_t *dn_parent = _dag_node_alloc(c->op->type, v_parent, NULL);

        _list_add_tail(dag, &dn_parent->list);

        ret = _dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = _dag_node_add_child(dn_parent, src->dag_node);
        if (ret < 0)
            return ret;

        if (c->dsts) {
            dst = c->dsts->data[0];
            assert(dst->node);
            dst->dag_node = dn_parent;
        }

    } else if (OP_TYPE_CAST == c->op->type) {
        src = c->srcs->data[0];
        dst = c->dsts->data[0];

        ret = _dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = _dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        if (!_dag_node_find_child(dst->dag_node, src->dag_node)) {
            ret = _dag_node_add_child(dst->dag_node, src->dag_node);
            if (ret < 0)
                return ret;
        }

    } else if (OP_RETURN == c->op->type) {
        if (c->srcs) {
            _dag_node_t *dn = _dag_node_alloc(c->op->type, NULL, NULL);

            _list_add_tail(dag, &dn->list);

            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                ret = _dag_get_node(dag, src->node, &src->dag_node);
                if (ret < 0)
                    return ret;

                ret = _dag_node_add_child(dn, src->dag_node);
                if (ret < 0)
                    return ret;
            }
        }
    } else if (_type_is_jmp(c->op->type)) {
        _logd("c->op: %d, name: %s\n", c->op->type, c->op->name);

    } else {
        int n_operands0 = -1;
        int n_operands1 = -1;

        if (c->dsts) {
            dst = c->dsts->data[0];

            assert(dst->node->op);

            n_operands0 = dst->node->op->nb_operands;
            n_operands1 = n_operands0;

            switch (c->op->type) {
            case OP_ARRAY_INDEX:
            case OP_3AC_ADDRESS_OF_ARRAY_INDEX:
            case OP_VA_START:
            case OP_VA_ARG:
                n_operands0 = 3;
                break;

            case OP_3AC_ADDRESS_OF_POINTER:
            case OP_VA_END:
                n_operands0 = 2;
                break;

            case OP_CALL:
                n_operands0 = c->srcs->size;
                break;
            default:
                break;
            };
        }

        return _3ac_code_to_dag(c, dag, n_operands0, n_operands1);
    }

    return 0;
}

// 筛选跳转 jmp
static void _3ac_filter_jmp(_list_t *h, _3ac_code_t *c) {
    _list_t *l2 = NULL;
    _3ac_code_t *c2 = NULL;
    _3ac_operand_t *dst0 = c->dsts->data[0];
    _3ac_operand_t *dst1 = NULL;

    for (l2 = &dst0->code->list; l2 != _list_sentinel(h);) {
        c2 = _list_data(l2, _3ac_code_t, list);

        if (OP_GOTO == c2->op->type) {
            dst0 = c2->dsts->data[0];
            l2 = &dst0->code->list;
            continue;
        }

        if (OP_3AC_NOP == c2->op->type) {
            l2 = _list_next(l2);
            continue;
        }

        if (!_type_is_jmp(c2->op->type)) {
            dst0 = c->dsts->data[0];
            dst0->code = c2;

            c2->basic_block_start = 1;
            c2->jmp_dst_flag = 1;
            break;
        }
#if 0
		if (  OP_GOTO == c->op->type) {
			c->op        = c2->op;

			dst0 = c ->dsts->data[0];
			dst1 = c2->dsts->data[0];

			dst0->code = dst1->code;

			l2 = &dst1->code->list;
			continue;
		}
#endif
        _logw("c: %s, c2: %s\n", c->op->name, c2->op->name);
        dst0 = c->dsts->data[0];
        dst0->code = c2;
        c2->basic_block_start = 1;
        c2->jmp_dst_flag = 1;
        break;
    }

    l2 = _list_next(&c->list);
    if (l2 != _list_sentinel(h)) {
        c2 = _list_data(l2, _3ac_code_t, list);
        c2->basic_block_start = 1;
    }
    c->basic_block_start = 1;
}

static int _3ac_filter_dst_teq(_list_t *h, _3ac_code_t *c) {
    _3ac_code_t *setcc2 = NULL;
    _3ac_code_t *setcc3 = NULL;
    _3ac_code_t *setcc = NULL;
    _3ac_code_t *jcc = NULL;
    _3ac_operand_t *dst0 = c->dsts->data[0];
    _3ac_operand_t *dst1 = NULL;
    _3ac_code_t *teq = dst0->code;
    _list_t *l;

    int jmp_op;

    if (_list_prev(&c->list) == _list_sentinel(h))
        return 0;
    setcc = _list_data(_list_prev(&c->list), _3ac_code_t, list);

    if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETZ == setcc->op->type)
        || (OP_3AC_JZ == c->op->type && OP_3AC_SETNZ == setcc->op->type)
        || (OP_3AC_JGT == c->op->type && OP_3AC_SETLE == setcc->op->type)
        || (OP_3AC_JGE == c->op->type && OP_3AC_SETLT == setcc->op->type)
        || (OP_3AC_JLT == c->op->type && OP_3AC_SETGE == setcc->op->type)
        || (OP_3AC_JLE == c->op->type && OP_3AC_SETGT == setcc->op->type))
        jmp_op = OP_3AC_JZ;

    else if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETNZ == setcc->op->type)
             || (OP_3AC_JZ == c->op->type && OP_3AC_SETZ == setcc->op->type)
             || (OP_3AC_JGT == c->op->type && OP_3AC_SETGT == setcc->op->type)
             || (OP_3AC_JGE == c->op->type && OP_3AC_SETGE == setcc->op->type)
             || (OP_3AC_JLT == c->op->type && OP_3AC_SETLT == setcc->op->type)
             || (OP_3AC_JLE == c->op->type && OP_3AC_SETLE == setcc->op->type))
        jmp_op = OP_3AC_JNZ;
    else
        return 0;

    setcc2 = setcc;

    while (teq && OP_3AC_TEQ == teq->op->type) {
        assert(setcc->dsts && 1 == setcc->dsts->size);
        assert(teq->srcs && 1 == teq->srcs->size);

        _3ac_operand_t *src = teq->srcs->data[0];
        _3ac_operand_t *dst = setcc->dsts->data[0];
        _variable_t *v_teq = _ _operand_get(src->node);
        _variable_t *v_set = _ _operand_get(dst->node);

        if (v_teq != v_set)
            return 0;

        for (l = _list_next(&teq->list); l != _list_sentinel(h); l = _list_next(l)) {
            jcc = _list_data(l, _3ac_code_t, list);

            if (_type_is_jmp(jcc->op->type))
                break;
        }
        if (l == _list_sentinel(h))
            return 0;

        if (OP_3AC_JZ == jmp_op) {
            if (OP_3AC_JZ == jcc->op->type) {
                dst0 = c->dsts->data[0];
                dst1 = jcc->dsts->data[0];

                dst0->code = dst1->code;

            } else if (OP_3AC_JNZ == jcc->op->type) {
                l = _list_next(&jcc->list);
                if (l == _list_sentinel(h))
                    return 0;

                dst0 = c->dsts->data[0];
                dst0->code = _list_data(l, _3ac_code_t, list);
            } else
                return 0;

        } else if (OP_3AC_JNZ == jmp_op) {
            if (OP_3AC_JNZ == jcc->op->type) {
                dst0 = c->dsts->data[0];
                dst1 = jcc->dsts->data[0];

                dst0->code = dst1->code;

            } else if (OP_3AC_JZ == jcc->op->type) {
                l = _list_next(&jcc->list);
                if (l == _list_sentinel(h))
                    return 0;

                dst0 = c->dsts->data[0];
                dst0->code = _list_data(l, _3ac_code_t, list);
            } else
                return 0;
        }
        teq = dst0->code;

        if (_list_prev(&jcc->list) == _list_sentinel(h))
            return 0;
        setcc = _list_data(_list_prev(&jcc->list), _3ac_code_t, list);

        if (_type_is_setcc(setcc->op->type)) {
            setcc3 = _3ac_code_clone(setcc);
            if (!setcc3)
                return -ENOMEM;
            setcc3->op = setcc2->op;

            _list_add_tail(&c->list, &setcc3->list);
        }

        if ((OP_3AC_JNZ == jcc->op->type && OP_3AC_SETZ == setcc->op->type)
            || (OP_3AC_JZ == jcc->op->type && OP_3AC_SETNZ == setcc->op->type))

            jmp_op = OP_3AC_JZ;

        else if ((OP_3AC_JNZ == jcc->op->type && OP_3AC_SETNZ == setcc->op->type)
                 || (OP_3AC_JZ == jcc->op->type && OP_3AC_SETZ == setcc->op->type))

            jmp_op = OP_3AC_JNZ;
        else
            return 0;
    }

    return 0;
}

// 筛选预先 teq
static int _3ac_filter_prev_teq(_list_t *h, _3ac_code_t *c, _3ac_code_t *teq) {
    _3ac_code_t *setcc3 = NULL;
    _3ac_code_t *setcc2 = NULL;
    _3ac_code_t *setcc = NULL;
    _3ac_code_t *jcc = NULL;
    _3ac_code_t *jmp = NULL;
    _list_t *l;

    int jcc_type;

    if (_list_prev(&teq->list) == _list_sentinel(h))
        return 0;

    setcc = _list_data(_list_prev(&teq->list), _3ac_code_t, list);
    jcc_type = -1;

    if (OP_3AC_JZ == c->op->type) {
        switch (setcc->op->type) {
        case OP_3AC_SETZ:
            jcc_type = OP_3AC_JNZ;
            break;

        case OP_3AC_SETNZ:
            jcc_type = OP_3AC_JZ;
            break;

        case OP_3AC_SETGT:
            jcc_type = OP_3AC_JLE;
            break;

        case OP_3AC_SETGE:
            jcc_type = OP_3AC_JLT;
            break;

        case OP_3AC_SETLT:
            jcc_type = OP_3AC_JGE;
            break;

        case OP_3AC_SETLE:
            jcc_type = OP_3AC_JGT;
            break;

        case OP_3AC_SETA:
            jcc_type = OP_3AC_JBE;
            break;
        case OP_3AC_SETAE:
            jcc_type = OP_3AC_JB;
            break;

        case OP_3AC_SETB:
            jcc_type = OP_3AC_JAE;
            break;
        case OP_3AC_SETBE:
            jcc_type = OP_3AC_JA;
            break;
        default:
            return 0;
            break;
        };
    } else if (OP_3AC_JNZ == c->op->type) {
        switch (setcc->op->type) {
        case OP_3AC_SETZ:
            jcc_type = OP_3AC_JZ;
            break;

        case OP_3AC_SETNZ:
            jcc_type = OP_3AC_JNZ;
            break;

        case OP_3AC_SETGT:
            jcc_type = OP_3AC_JGT;
            break;

        case OP_3AC_SETGE:
            jcc_type = OP_3AC_JGE;
            break;

        case OP_3AC_SETLT:
            jcc_type = OP_3AC_JLT;
            break;

        case OP_3AC_SETLE:
            jcc_type = OP_3AC_JLE;
            break;

        case OP_3AC_SETA:
            jcc_type = OP_3AC_JA;
            break;
        case OP_3AC_SETAE:
            jcc_type = OP_3AC_JAE;
            break;

        case OP_3AC_SETB:
            jcc_type = OP_3AC_JB;
            break;
        case OP_3AC_SETBE:
            jcc_type = OP_3AC_JBE;
            break;
        default:
            return 0;
            break;
        };
    } else
        return 0;

    assert(setcc->dsts && 1 == setcc->dsts->size);
    assert(teq->srcs && 1 == teq->srcs->size);

    _3ac_operand_t *src = teq->srcs->data[0];
    _3ac_operand_t *dst0 = setcc->dsts->data[0];
    _3ac_operand_t *dst1 = NULL;
    _variable_t *v_teq = _ _operand_get(src->node);
    _variable_t *v_set = _ _operand_get(dst0->node);

    if (v_teq != v_set)
        return 0;

#define _3AC_JCC_ALLOC(j, cc)                        \
    do {                                             \
        j = _3ac_code_alloc();                       \
        if (!j)                                      \
            return -ENOMEM;                          \
                                                     \
        j->dsts = _vector_alloc();                   \
        if (!j->dsts) {                              \
            _3ac_code_free(j);                       \
            return -ENOMEM;                          \
        }                                            \
                                                     \
        _3ac_operand_t *dst0 = _3ac_operand_alloc(); \
        if (!dst0) {                                 \
            _3ac_code_free(j);                       \
            return -ENOMEM;                          \
        }                                            \
                                                     \
        if (_vector_add(j->dsts, dst0) < 0) {        \
            _3ac_code_free(j);                       \
            _3ac_operand_free(dst0);                 \
            return -ENOMEM;                          \
        }                                            \
        j->op = _3ac_find_operator(cc);              \
        assert(j->op);                               \
    } while (0)

    _3AC_JCC_ALLOC(jcc, jcc_type);
    dst0 = jcc->dsts->data[0];
    dst1 = c->dsts->data[0];
    dst0->code = dst1->code;
    _list_add_front(&setcc->list, &jcc->list);

    l = _list_prev(&c->list);
    if (l != _list_sentinel(h)) {
        setcc2 = _list_data(l, _3ac_code_t, list);

        if (_type_is_setcc(setcc2->op->type)) {
            setcc3 = _3ac_code_clone(setcc2);
            if (!setcc3)
                return -ENOMEM;
            setcc3->op = setcc->op;

            _list_add_tail(&jcc->list, &setcc3->list);
        }
    }

    l = _list_next(&c->list);
    if (l == _list_sentinel(h)) {
        _3ac_filter_jmp(h, jcc);
        return 0;
    }

    _3AC_JCC_ALLOC(jmp, OP_GOTO);
    dst0 = jmp->dsts->data[0];
    dst0->code = _list_data(l, _3ac_code_t, list);
    _list_add_front(&jcc->list, &jmp->list);

    _3ac_filter_jmp(h, jcc);
    _3ac_filter_jmp(h, jmp);

    return 0;
}

// 三地址码 链表 打印
void _3ac_list_print(list_t *h) {
    _3ac_code_t *c;
    _list_t *l;

    for (l = _list_head(h); l != _list_sentinel(h); l = _list_next(l)) {
        c = _list_data(l, _3ac_code_t, list);

        _3ac_code_print(c, NULL);
    }
}

// 寻找基本块开始
static int _3ac_find_basic_block_start(_list_t *h) {
    int start = 0;
    _list_t *l;

    for (l = _list_head(h); l != _list_sentinel(h); l = _list_next(l)) {
        _3ac_code_t *c = _list_data(l, _3ac_code_t, list);

        _list_t *l2 = NULL;
        _3ac_code_t *c2 = NULL;

        if (!start) {
            c->basic_block_start = 1;
            start = 1;
        }
#if 0
		if ( _type_is_assign_dereference(c->op->type)) {

			l2	=  _list_next(&c->list);
			if (l2 !=  _list_sentinel(h)) {
				c2 =  _list_data(l2,  _3ac_code_t, list);
				c2->basic_block_start = 1;
			}

			c->basic_block_start = 1;
			continue;
		}
		if (  OP_DEREFERENCE == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (  OP_CALL == c->op->type) {

			l2	=  _list_next(&c->list);
			if (l2 !=  _list_sentinel(h)) {
				c2 =  _list_data(l2,  _3ac_code_t, list);
				c2->basic_block_start = 1;
			}

//			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (  OP_RETURN == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}
#endif
        if (OP_3AC_DUMP == c->op->type) {
            c->basic_block_start = 1;
            continue;
        }

        if (OP_3AC_END == c->op->type) {
            c->basic_block_start = 1;
            continue;
        }

        if (OP_3AC_CMP == c->op->type
            || OP_3AC_TEQ == c->op->type) {
            for (l2 = _list_next(&c->list); l2 != _list_sentinel(h); l2 = _list_next(l2)) {
                c2 = _list_data(l2, _3ac_code_t, list);

                if (_type_is_setcc(c2->op->type))
                    continue;

                if (OP_3AC_TEQ == c2->op->type)
                    continue;

                if (_type_is_jmp(c2->op->type))
                    c->basic_block_start = 1;
                break;
            }
            continue;
        }

        if (_type_is_jmp(c->op->type)) {
            _3ac_operand_t *dst0 = c->dsts->data[0];

            assert(dst0->code);

            // filter 1st expr of logic op, such as '&&', '||'
            if (OP_3AC_TEQ == dst0->code->op->type) {
                int ret = _3ac_filter_dst_teq(h, c);
                if (ret < 0)
                    return ret;
            }

            for (l2 = _list_prev(&c->list); l2 != _list_sentinel(h); l2 = _list_prev(l2)) {
                c2 = _list_data(l2, _3ac_code_t, list);

                if (_type_is_setcc(c2->op->type))
                    continue;

                // filter 2nd expr of logic op, such as '&&', '||'
                if (OP_3AC_TEQ == c2->op->type) {
                    int ret = _3ac_filter_prev_teq(h, c, c2);
                    if (ret < 0)
                        return ret;
                }
                break;
            }

            _3ac_filter_jmp(h, c);
        }
    }
#if 1
    for (l = _list_head(h); l != _list_sentinel(h);) {
        _3ac_code_t *c = _list_data(l, _3ac_code_t, list);

        _list_t *l2 = NULL;
        _3ac_code_t *c2 = NULL;
        _3ac_operand_t *dst0 = NULL;

        if (OP_3AC_NOP == c->op->type) {
            assert(!c->jmp_dst_flag);

            l = _list_next(l);

            _list_del(&c->list);
            _3ac_code_free(c);
            c = NULL;
            continue;
        }

        if (OP_GOTO != c->op->type) {
            l = _list_next(l);
            continue;
        }
        assert(!c->jmp_dst_flag);

        for (l2 = _list_next(&c->list); l2 != _list_sentinel(h);) {
            c2 = _list_data(l2, _3ac_code_t, list);

            if (c2->jmp_dst_flag)
                break;

            l2 = _list_next(l2);

            _list_del(&c2->list);
            _3ac_code_free(c2);
            c2 = NULL;
        }

        l = _list_next(l);
        dst0 = c->dsts->data[0];

        if (l == &dst0->code->list) {
            _list_del(&c->list);
            _3ac_code_free(c);
            c = NULL;
        }
    }
#endif
    return 0;
}

// 分割基本块
static int _3ac_split_basic_blocks(list_t *h, function_t *f) {
    _list_t *l;
    _basic_block_t *bb = NULL;

    for (l = _list_head(h); l != _list_sentinel(h);) {
        _3ac_code_t *c = _list_data(l, _3ac_code_t, list);

        l = _list_next(l);

        if (c->basic_block_start) {
            bb = _basic_block_alloc();
            if (!bb)
                return -ENOMEM;

            bb->index = f->nb_basic_blocks++;
            _list_add_tail(&f->basic_block_list_head, &bb->list);

            c->basic_block = bb;

            if (OP_3AC_CMP == c->op->type
                || OP_3AC_TEQ == c->op->type) {
                _3ac_operand_t *src;
                _3ac_code_t *c2;
                _list_t *l2;
                _node_t *e;
                int i;

                for (l2 = _list_next(&c->list); l2 != _list_sentinel(h); l2 = _list_next(l2)) {
                    c2 = _list_data(l2, _3ac_code_t, list);

                    if (_type_is_setcc(c2->op->type))
                        continue;

                    if (OP_3AC_TEQ == c2->op->type)
                        continue;

                    if (_type_is_jmp(c2->op->type)) {
                        bb->cmp_flag = 1;

                        if (c->srcs) {
                            for (i = 0; i < c->srcs->size; i++) {
                                src = c->srcs->data[i];
                                e = src->node;

                                while (e && OP_EXPR == e->type)
                                    e = e->nodes[0];
                                assert(e);

                                if (OP_DEREFERENCE == e->type)
                                    bb->dereference_flag = 1;
                            }
                        }
                    }
                    break;
                }

                _list_del(&c->list);
                _list_add_tail(&bb->code_list_head, &c->list);
                continue;
            }

            _list_del(&c->list);
            _list_add_tail(&bb->code_list_head, &c->list);

            if (OP_CALL == c->op->type) {
                bb->call_flag = 1;
                continue;
            }

            if (OP_RETURN == c->op->type) {
                bb->ret_flag = 1;
                continue;
            }

            if (OP_3AC_DUMP == c->op->type) {
                bb->dump_flag = 1;
                continue;
            }

            if (OP_3AC_END == c->op->type) {
                bb->end_flag = 1;
                continue;
            }

            if (_type_is_assign_dereference(c->op->type)
                || OP_DEREFERENCE == c->op->type) {
                bb->dereference_flag = 1;
                continue;
            }

            if (_type_is_assign_array_index(c->op->type)) {
                bb->array_index_flag = 1;
                continue;
            }

            if (OP_VA_START == c->op->type
                || OP_VA_ARG == c->op->type
                || OP_VA_END == c->op->type) {
                bb->varg_flag = 1;
                continue;
            }

            if (_type_is_jmp(c->op->type)) {
                bb->jmp_flag = 1;

                if (_type_is_jcc(c->op->type))
                    bb->jcc_flag = 1;

                int ret = _vector_add_unique(f->jmps, c);
                if (ret < 0)
                    return ret;
            }
        } else {
            assert(bb);
            c->basic_block = bb;

            if (_type_is_assign_dereference(c->op->type) || OP_DEREFERENCE == c->op->type)
                bb->dereference_flag = 1;

            else if (_type_is_assign_array_index(c->op->type))
                bb->array_index_flag = 1;

            else if (OP_CALL == c->op->type)
                bb->call_flag = 1;

            else if (OP_RETURN == c->op->type)
                bb->ret_flag = 1;

            else if (OP_VLA_ALLOC == c->op->type) {
                bb->vla_flag = 1;
                f->vla_flag = 1;

            } else if (OP_VA_START == c->op->type
                       || OP_VA_ARG == c->op->type
                       || OP_VA_END == c->op->type)
                bb->varg_flag = 1;

            _list_del(&c->list);
            _list_add_tail(&bb->code_list_head, &c->list);
        }
    }

    return 0;
}

// 拼接基本块
static int _3ac_connect_basic_blocks(_function_t *f) {
    int i;
    int ret;

    _list_t *l;
    _list_t *sentinel = _list_sentinel(&f->basic_block_list_head);

    for (l = _list_head(&f->basic_block_list_head); l != sentinel; l = _list_next(l)) {
        _basic_block_t *current_bb = _list_data(l, _basic_block_t, list);
        _basic_block_t *prev_bb = NULL;
        _basic_block_t *next_bb = NULL;
        _list_t *l2 = _list_prev(l);

        if (current_bb->jmp_flag)
            continue;

        if (l2 != sentinel) {
            prev_bb = _list_data(l2, _basic_block_t, list);

            if (!prev_bb->jmp_flag) {
                ret = _basic_block_connect(prev_bb, current_bb);
                if (ret < 0)
                    return ret;
            }
        }

        l2 = _list_next(l);
        if (l2 == sentinel)
            continue;

        next_bb = _list_data(l2, _basic_block_t, list);

        if (!next_bb->jmp_flag) {
            ret = _basic_block_connect(current_bb, next_bb);
            if (ret < 0)
                return ret;
        }
    }

    for (i = 0; i < f->jmps->size; i++) {
        _3ac_code_t *c = f->jmps->data[i];
        _3ac_operand_t *dst0 = c->dsts->data[0];
        _3ac_code_t *dst = dst0->code;

        _basic_block_t *current_bb = c->basic_block;
        _basic_block_t *dst_bb = dst->basic_block;
        _basic_block_t *prev_bb = NULL;
        _basic_block_t *next_bb = NULL;

        dst0->bb = dst_bb;
        dst0->code = NULL;

        for (l = _list_prev(&current_bb->list); l != sentinel; l = _list_prev(l)) {
            prev_bb = _list_data(l, _basic_block_t, list);

            if (!prev_bb->jmp_flag)
                break;

            if (!prev_bb->jcc_flag) {
                prev_bb = NULL;
                break;
            }
            prev_bb = NULL;
        }

        if (prev_bb) {
            ret = _basic_block_connect(prev_bb, dst_bb);
            if (ret < 0)
                return ret;
        } else
            continue;

        if (!current_bb->jcc_flag)
            continue;

        for (l = _list_next(&current_bb->list); l != sentinel; l = _list_next(l)) {
            next_bb = _list_data(l, _basic_block_t, list);

            if (!next_bb->jmp_flag)
                break;

            if (!next_bb->jcc_flag) {
                next_bb = NULL;
                break;
            }
            next_bb = NULL;
        }

        if (next_bb) {
            ret = _basic_block_connect(prev_bb, next_bb);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

// 分割基本块
int _3ac_split_basic_blocks(_list_t *list_head_3ac, _function_t *f) {
    int ret = _3ac_find_basic_block_start(list_head_3ac);
    if (ret < 0)
        return ret;

    ret = _3ac_split_basic_blocks(list_head_3ac, f);
    if (ret < 0)
        return ret;

    return _3ac_connect_basic_blocks(f);
}

_3ac_code_t *_3ac_code_NN(int op_type, node_t **dsts, int nb_dsts, node_t **srcs, int nb_srcs) {
    _3ac_operator_t *op = _3ac_find_operator(op_type);
    if (!op) {
        _loge("\n");
        return NULL;
    }

    _3ac_operand_t *operand;
    _3ac_code_t *c;
    _vector_t *vsrc = NULL;
    _vector_t *vdst = NULL;
    _node_t *node;

    int i;

    if (srcs) {
        vsrc = _vector_alloc();
        for (i = 0; i < nb_srcs; i++) {
            operand = _3ac_operand_alloc();

            node = srcs[i];

            while (node && OP_EXPR == node->type)
                node = node->nodes[0];

            operand->node = node;

            _vector_add(vsrc, operand);
        }
    }

    if (dsts) {
        vdst = _vector_alloc();
        for (i = 0; i < nb_dsts; i++) {
            operand = _3ac_operand_alloc();

            node = dsts[i];

            while (node && OP_EXPR == node->type)
                node = node->nodes[0];

            operand->node = node;

            _vector_add(vdst, operand);
        }
    }

    c = _3ac_code_alloc();
    c->op = op;
    c->dsts = vdst;
    c->srcs = vsrc;
    return c;
}
