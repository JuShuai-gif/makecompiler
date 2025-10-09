#include "3ac.h"
#include "function.h"
#include "basic_block.h"
#include "utils_graph.h"

static mc_3ac_operator_t mc_3ac_operators[] = {
    {OP_CALL, "call"}, // 函数调用

    {OP_ARRAY_INDEX, "array_index"}, // 访问 a[i]
    {OP_POINTER, "pointer"},         // 指针操作 p->field

    {OP_TYPE_CAST, "cast"}, //

    {OP_LOGIC_NOT, "logic_not"},
    {OP_BIT_NOT, "not"},
    {OP_NEG, "neg"},
    {OP_POSITIVE, "positive"},

    {OP_INC, "inc"}, // 前置 自增 ++x
    {OP_DEC, "dec"}, // 前置 自减 --x

    {OP_INC_POST, "inc_post"}, // 后置自增 x++
    {OP_DEC_POST, "dec_post"}, // 后置自减 x--

    {OP_DEREFERENCE, "dereference"}, // 解引用 *p
    {OP_ADDRESS_OF, "address_of"},   // 取地址(&x)

    {OP_MUL, "mul"}, // 乘法 a * b
    {OP_DIV, "div"}, // 除法 a / b
    {OP_MOD, "mod"}, // 取余 a % b

    {OP_ADD, "add"}, // 加法
    {OP_SUB, "sub"}, // 减法

    {OP_SHL, "shl"}, // 左移 a << b
    {OP_SHR, "shr"}, // 右移 a >> b

    {OP_BIT_AND, "and"}, // 按位与 (a & b)
    {OP_BIT_OR, "or"},   // 按位或 (a | b)

    {OP_EQ, "eq"},  // 相等(==)
    {OP_NE, "neq"}, // 不等(!=)
    {OP_GT, "gt"},  // 大于(>)
    {OP_LT, "lt"},  // 小于(<)
    {OP_GE, "ge"},  // 大于等于(>=)
    {OP_LE, "le"},  // 小于等于(<=)

    {OP_ASSIGN, "assign"},  // x = y
    {OP_ADD_ASSIGN, "+="},  // x+=y
    {OP_SUB_ASSIGN, "-="},  // x-=y
    {OP_MUL_ASSIGN, "*="},  // x*=y
    {OP_DIV_ASSIGN, "/="},  // x/=y
    {OP_MOD_ASSIGN, "%="},  // x%=y
    {OP_SHL_ASSIGN, "<<="}, // x <<= y
    {OP_SHR_ASSIGN, ">>="}, // x >>= y
    {OP_AND_ASSIGN, "&="},  // x &= y
    {OP_OR_ASSIGN, "|="},   // x |= y

    {OP_VA_START, "va_start"}, // 宏 IR
    {OP_VA_ARG, "va_arg"},
    {OP_VA_END, "va_end"},

    {OP_VLA_ALLOC, "vla_alloc"}, // 分配变长数组
    {OP_VLA_FREE, "vla_free"},   // 释放变长数组

    {OP_RETURN, "return"}, // 从函数返回
    {OP_GOTO, "jmp"},      // 无条件跳转

    {OP_3AC_TEQ, "teq"}, // 测试相等(test eq)
    {OP_3AC_CMP, "cmp"}, // 比较(cmp)

    {OP_3AC_LEA, "lea"}, // 取地址(Load Effective Address, 类似汇编 lea)

    {OP_3AC_SETZ, "setz"},   // 等于则置位
    {OP_3AC_SETNZ, "setnz"}, // 不等则置位
    {OP_3AC_SETGT, "setgt"}, // 大于置位
    {OP_3AC_SETGE, "setge"}, //
    {OP_3AC_SETLT, "setlt"},
    {OP_3AC_SETLE, "setle"}, // 小于置位

    {OP_3AC_SETA, "seta"},
    {OP_3AC_SETAE, "setae"},
    {OP_3AC_SETB, "setb"},
    {OP_3AC_SETBE, "setbe"},

    {OP_3AC_JZ, "jz"},   // 如果等于零跳转
    {OP_3AC_JNZ, "jnz"}, // 如果不等于零跳转
    {OP_3AC_JGT, "jgt"}, // 如果大于跳转
    {OP_3AC_JGE, "jge"}, // 如果大于等于跳转
    {OP_3AC_JLT, "jlt"}, // 如果小于跳转
    {OP_3AC_JLE, "jle"}, // 如果小于等于跳转

    {OP_3AC_JA, "ja"},   // 无符号大于跳转
    {OP_3AC_JAE, "jae"}, // 无符号大于等于跳转
    {OP_3AC_JB, "jb"},   // 无符号小于跳转
    {OP_3AC_JBE, "jbe"}, // 无符号小于等于跳转

    {OP_3AC_DUMP, "core_dump"}, // 打印 IR 栈或核心数据
    {OP_3AC_NOP, "nop"},        // 空操作 (No Operation)
    {OP_3AC_END, "end"},        // 程序结束

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
    {OP_3AC_DEC, "dec3"}, //

    {OP_3AC_ASSIGN_DEREFERENCE, "dereference="}, // *p=x
    {OP_3AC_ASSIGN_ARRAY_INDEX, "array_index="}, // a[i]=x
    {OP_3AC_ASSIGN_POINTER, "pointer="},         // p->field = x

    {OP_3AC_ADDRESS_OF_ARRAY_INDEX, "&array_index"}, // &a[i]
    {OP_3AC_ADDRESS_OF_POINTER, "&pointer"},         // &p->field
};

// 查找与给定操作类型 type 对应的三地址码操作符
mc_3ac_operator_t *mc_3ac_find_operator(const int type) {
    int i;
    // 遍历 mc_3ac_operators 数组，逐个检查 type 是否匹配
    for (i = 0; i < sizeof(mc_3ac_operators) / sizeof(mc_3ac_operators[0]); i++) {
        if (type == mc_3ac_operators[i].type) {
            // 找到匹配的操作符，返回其地址
            return &(mc_3ac_operators[i]);
        }
    }
    // 未找到匹配的操作符，返回 NULL
    return NULL;
}

// 分配一个三地址码操作数对象（mc_3ac_operand_t）
mc_3ac_operand_t *mc_3ac_operand_alloc() {
    // calloc 分配并清零内存
    mc_3ac_operand_t *operand = calloc(1, sizeof(mc_3ac_operand_t));
    assert(operand); // 确保分配成功
    return operand;
}

// 释放一个三地址码操作数对象
void mc_3ac_operand_free(mc_3ac_operand_t *operand) {
    if (operand) {
        free(operand);
        operand = NULL; // 防止悬空指针
    }
}

// 分配一个三地址码指令对象（mc_3ac_code_t）
mc_3ac_code_t *mc_3ac_code_alloc() {
    // calloc 分配并清零内存
    mc_3ac_code_t *c = calloc(1, sizeof(mc_3ac_code_t));

    return c;
}

// 判断两条三地址码是否等价
// 主要比较：操作符(op)、目标操作数(dsts)、源操作数(srcs)
int mc_3ac_code_same(mc_3ac_code_t *c0, mc_3ac_code_t *c1) {
    // 1. 操作符不一致，直接返回 false
    if (c0->op != c1->op)
        return 0;

    // 2. 检查目标操作数列表
    if (c0->dsts) {
        // 如果 c0 有目标但 c1 没有，返回 false
        if (!c1->dsts)
            return 0;

        // 数量不一致，也返回 false
        if (c0->dsts->size != c1->dsts->size)
            return 0;

        int i;
        for (i = 0; i < c0->dsts->size; i++) {
            mc_3ac_operand_t *dst0 = c0->dsts->data[i];
            mc_3ac_operand_t *dst1 = c1->dsts->data[i];

            if (dst0->dag_node) {
                if (!dst1->dag_node)
                    return 0;
                // 使用 DAG 节点比较是否相同
                if (!dag_dn_same(dst0->dag_node, dst1->dag_node))
                    return 0;
            } else if (dst1->dag_node)
                return 0;
        }
    } else if (c1->dsts)
        // c0 没有目标，但 c1 有，返回 false
        return 0;

    // 3. 检查源操作数列表
    if (c0->srcs) {
        if (!c1->srcs)
            return 0;

        if (c0->srcs->size != c1->srcs->size)
            return 0;

        int i;
        for (i = 0; i < c0->srcs->size; i++) {
            mc_3ac_operand_t *src0 = c0->srcs->data[i];
            mc_3ac_operand_t *src1 = c1->srcs->data[i];

            if (src0->dag_node) {
                if (!src1->dag_node)
                    return 0;

                if (!dag_dn_same(src0->dag_node, src1->dag_node))
                    return 0;
            } else if (src1->dag_node)
                return 0;
        }
    } else if (c1->srcs)
        return 0;
    // 4. 所有检查通过，认为两条指令等价
    return 1;
}

// 克隆一条三地址码（深拷贝操作数列表，浅拷贝 DAG 节点指针）
// 注意：这里并没有深度复制 DAG，只是复制了操作数结构体
mc_3ac_code_t *_3ac_code_clone(mc_3ac_code_t *c) {
    mc_3ac_code_t *c2 = calloc(1, sizeof(mc_3ac_code_t));
    if (!c2)
        return NULL;

    // 拷贝操作符
    c2->op = c->op;

    // 克隆目标操作数列表
    if (c->dsts) {
        c2->dsts = vector_alloc();
        if (!c2->dsts) {
            _3ac_code_free(c2);
            return NULL;
        }

        int i;
        for (i = 0; i < c->dsts->size; i++) {
            mc_3ac_operand_t *dst = c->dsts->data[i];
            mc_3ac_operand_t *dst2 = _3ac_operand_alloc(); // 分配新操作数

            if (!dst2) {
                _3ac_code_free(c2);
                return NULL;
            }

            if (vector_add(c2->dsts, dst2) < 0) {
                _3ac_code_free(c2);
                return NULL;
            }

            // 拷贝与 DAG 相关的引用（浅拷贝）
            dst2->node = dst->node;
            dst2->dag_node = dst->dag_node;
            dst2->code = dst->code;
            dst2->bb = dst->bb;
        }
    }

    // 克隆源操作数列表
    if (c->srcs) {
        c2->srcs = vector_alloc();
        if (!c2->srcs) {
            _3ac_code_free(c2);
            return NULL;
        }

        int i;
        for (i = 0; i < c->srcs->size; i++) {
            mc_3ac_operand_t *src = c->srcs->data[i];
            mc_3ac_operand_t *src2 = _3ac_operand_alloc();

            if (!src2) {
                _3ac_code_free(c2);
                return NULL;
            }

            if (vector_add(c2->srcs, src2) < 0) {
                _3ac_code_free(c2);
                return NULL;
            }

            src2->node = src->node;
            src2->dag_node = src->dag_node;
            src2->code = src->code;
            src2->bb = src->bb;
        }
    }

    // 拷贝 label 信息和 origin 指针
    c2->label = c->label;

    // 保留原始代码指针，便于追踪
    c2->origin = c;
    return c2;
}

// 释放一条三地址码及其相关资源
void mc_3ac_code_free(mc_3ac_code_t *c) {
    int i;

    if (c) {
        // 释放目标操作数列表
        if (c->dsts) {
            for (i = 0; i < c->dsts->size; i++)
                mc_3ac_operand_free(c->dsts->data[i]); // 释放每个操作数
            vector_free(c->dsts);                      // 释放 vector 容器
        }
        // 释放源操作数列表
        if (c->srcs) {
            for (i = 0; i < c->srcs->size; i++)
                mc_3ac_operand_free(c->srcs->data[i]);
            vector_free(c->srcs);
        }
        // 释放活跃变量列表（通常用于 liveness analysis）
        if (c->active_vars) {
            int i;
            for (i = 0; i < c->active_vars->size; i++)
                dn_status_free(c->active_vars->data[i]);
            vector_free(c->active_vars);
        }
        // 最终释放三地址码结构本身
        free(c);
        c = NULL; // 防止悬空指针
    }
}

// 根据源 DAG 节点创建一条新的三地址码
mc_3ac_code_t *mc_3ac_alloc_by_src(int op_type, dag_node_t *src) {
    mc_3ac_operator_t *mc_3ac_op = mc_3ac_find_operator(op_type);
    if (!mc_3ac_op) {
        loge("3ac operator not found\n");
        return NULL;
    }

    // 创建源操作数
    mc_3ac_operand_t *src0 = mc_3ac_operand_alloc();
    if (!src0)
        return NULL;
    src0->dag_node = src; // 绑定 DAG 节点

    // 创建源操作数列表
    vector_t *srcs = vector_alloc();
    if (!srcs)
        goto error1;
    if (vector_add(srcs, src0) < 0)
        goto error0;

    // 创建三地址码结构
    mc_3ac_code_t *c = mc_3ac_code_alloc();
    if (!c)
        goto error0;

    c->op = mc_3ac_op; // 设置操作符
    c->srcs = srcs;    // 设置源操作数列表
    return c;

error0:
    vector_free(srcs);
error1:
    mc_3ac_operand_free(src0);
    return NULL;
}

// 根据目标 DAG 节点创建一条新的三地址码
mc_3ac_code_t *mc_3ac_alloc_by_dst(int op_type, dag_node_t *dst) {
    mc_3ac_operator_t *op;
    mc_3ac_operand_t *d;
    mc_3ac_code_t *c;

    op = mc_3ac_find_operator(op_type);
    if (!op) {
        loge("3ac operator not found\n");
        return NULL;
    }

    d = mc_3ac_operand_alloc();
    if (!d)
        return NULL;
    d->dag_node = dst; // 绑定目标 DAG 节点

    c = mc_3ac_code_alloc();
    if (!c) {
        mc_3ac_operand_free(d);
        return NULL;
    }

    // 创建目标操作数列表
    c->dsts = vector_alloc();
    if (!c->dsts) {
        mc_3ac_operand_free(d);
        mc_3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, d) < 0) {
        mc_3ac_operand_free(d);
        mc_3ac_code_free(c);
        return NULL;
    }

    // 设置操作符
    c->op = op;
    return c;
}

// 创建一条跳转类三地址码（例如 jmp, je, jne）
mc_3ac_code_t *mc_3ac_jmp_code(int type, label_t *l, node_t *err) {
    mc_3ac_operand_t *dst;
    mc_3ac_code_t *c;

    c = mc_3ac_code_alloc();
    if (!c)
        return NULL;

    // 设置跳转操作符
    c->op = mc_3ac_find_operator(type);
    // 设置跳转标签
    c->label = l;
    // 创建目标操作数列表
    c->dsts = vector_alloc();
    if (!c->dsts) {
        mc_3ac_code_free(c);
        return NULL;
    }

    dst = mc_3ac_operand_alloc();
    if (!dst) {
        mc_3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        mc_3ac_operand_free(dst);
        mc_3ac_code_free(c);
        return NULL;
    }

    return c;
}

// 打印一个语法树节点 node_t
static void mc_3ac_print_node(node_t *node) {
    if (type_is_var(node->type)) {
        if (node->var->w) {
            printf("v_%d_%d/%s/%#lx",
                   node->var->w->line, node->var->w->pos, node->var->w->text->data, 0xffff & (uintptr_t)node->var);
        } else {
            printf("v_%#lx", 0xffff & (uintptr_t)node->var);
        }
    } else if (type_is_operator(node->type)) {
        if (node->result) {
            if (node->result->w) {
                printf("v_%d_%d/%s/%#lx",
                       node->result->w->line, node->result->w->pos, node->result->w->text->data, 0xffff & (uintptr_t)node->result);
            } else
                printf("v/%#lx", 0xffff & (uintptr_t)node->result);
        }
    } else if (FUNCTION == node->type) {
        printf("f_%d_%d/%s",
               node->w->line, node->w->pos, node->w->text->data);
    } else {
        loge("node: %p\n", node);
        loge("node->type: %d\n", node->type);
        assert(0); // 不支持的节点类型
    }
}

// 打印一个 DAG 节点 dag_node_t
static void mc_3ac_print_dag_node(dag_node_t *dn) {
    if (type_is_var(dn->type)) {
        if (dn->var->w) {
            printf("v_%d_%d/%s_%#lx ",
                   dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
                   0xffff & (uintptr_t)dn);
        } else {
            printf("v_%#lx", 0xffff & (uintptr_t)dn->var);
        }
    } else if (type_is_operator(dn->type)) {
        mc_3ac_operator_t *op = mc_3ac_find_operator(dn->type);
        if (dn->var && dn->var->w)
            printf("v_%d_%d/%s_%#lx ",
                   dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
                   0xffff & (uintptr_t)dn);
        else
            printf("v_%#lx/%s ", 0xffff & (uintptr_t)dn->var, op->name);
    } else {
        // 暂不处理其他类型
        // printf("type: %d, v_%#lx\n", dn->type, 0xffff & (uintptr_t)dn->var);
        // assert(0);
    }
}

// 打印一条三地址码
// sentinel 用于判断循环/链表的终止条件
void mc_3ac_code_print(mc_3ac_code_t *c, list_t *sentinel) {
    mc_3ac_operand_t *src;
    mc_3ac_operand_t *dst;

    int i;

    // 打印操作符名称
    printf("%s  ", c->op->name);

    // 打印目标操作数列表
    if (c->dsts) {
        for (i = 0; i < c->dsts->size; i++) {
            dst = c->dsts->data[i];

            if (dst->dag_node)
                mc_3ac_print_dag_node(dst->dag_node);

            else if (dst->node)
                mc_3ac_print_node(dst->node);

            else if (dst->code) {
                // 如果目标是嵌套三地址码，递归打印
                if (&dst->code->list != sentinel) {
                    printf(": ");
                    mc_3ac_code_print(dst->code, sentinel);
                }
            } else if (dst->bb)
                printf(" bb: %p, index: %d ", dst->bb, dst->bb->index);

            if (i < c->dsts->size - 1)
                printf(", ");
        }
    }
    // 打印源操作数列表
    if (c->srcs) {
        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            if (0 == i && c->dsts && c->dsts->size > 0)
                printf("; "); // dst 与 src 分隔符

            if (src->dag_node)
                mc_3ac_print_dag_node(src->dag_node);

            else if (src->node)
                mc_3ac_print_node(src->node);

            else if (src->code)
                assert(0); // src 为嵌套 code 不被允许

            if (i < c->srcs->size - 1)
                printf(", ");
        }
    }

    printf("\n");
}

// 内部辅助函数，将普通三地址码转换为 DAG 节点
// nb_operands0 / nb_operands1 用于限制 DAG 子节点数量
/*
总结：
    用于把普通 3AC 转换成 DAG 节点关系。

    通过 dag_get_node 获取 DAG 节点，源节点挂到目标节点的子节点列表中。

    nb_operands0/1 限制 DAG 子节点数量，防止非法连接。
*/
static int _3ac_code_to_dag(mc_3ac_code_t *c, list_t *dag, int nb_operands0, int nb_operands1) {
    mc_3ac_operand_t *dst;
    mc_3ac_operand_t *src;
    dag_node_t *dn;

    int ret;
    int i;
    int j;
    // 先处理目标操作数，将其对应的 DAG 节点建立连接
    if (c->dsts) {
        for (j = 0; j < c->dsts->size; j++) {
            dst = c->dsts->data[j];

            if (!dst || !dst->node)
                continue;

            ret = dag_get_node(dag, dst->node, &dst->dag_node); // 获取或创建 DAG 节点
            if (ret < 0)
                return ret;
        }
    }

    if (!c->srcs)
        return 0;
    // 遍历源操作数
    for (i = 0; i < c->srcs->size; i++) {
        src = c->srcs->data[i];

        if (!src || !src->node)
            continue;

        ret = dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        if (!c->dsts)
            continue;
        // 将源操作数挂到每个目标操作数的 DAG 节点上
        for (j = 0; j < c->dsts->size; j++) {
            dst = c->dsts->data[j];

            if (!dst || !dst->dag_node)
                continue;

            dn = dst->dag_node;
            // 如果目标 DAG 节点的子节点不足 nb_operands0，则直接添加
            if (!dn->childs || dn->childs->size < nb_operands0) {
                ret = dag_node_add_child(dn, src->dag_node);
                if (ret < 0)
                    return ret;
                continue;
            }

            // 检查是否已经存在子节点
            int k;
            for (k = 0; k < dn->childs->size && k < nb_operands1; k++) {
                if (src->dag_node == dn->childs->data[k])
                    break;
            }

            if (k == dn->childs->size) {
                // 超出限制，报错并打印调试信息
                loge("i: %d, c->op: %s, dn->childs->size: %d, c->srcs->size: %d\n",
                     i, c->op->name, dn->childs->size, c->srcs->size);
                mc_3ac_code_print(c, NULL);
                return -1;
            }
        }
    }

    return 0;
}

// 将一条三地址码转换为 DAG 结构，并添加到 DAG 列表
/*
总结：

    根据三地址码类型分别处理 DAG 转换。

    赋值、数组访问、VLA、比较、SETcc、类型转换、return、jmp 等都各自处理逻辑。

    对普通算术、函数调用等使用 _3ac_code_to_dag 通用方法。
*/
int mc_3ac_code_to_dag(mc_3ac_code_t *c, list_t *dag) {
    mc_3ac_operand_t *src;
    mc_3ac_operand_t *dst;

    int ret = 0;
    int i;
    // 处理普通赋值操作
    if (type_is_assign(c->op->type)) {
        src = c->srcs->data[0];
        dst = c->dsts->data[0];

        ret = dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;
        // 避免 x = x 的冗余赋值
        if (OP_ASSIGN == c->op->type && src->dag_node == dst->dag_node)
            return 0;

        dag_node_t *dn_src;
        dag_node_t *dn_parent;
        dag_node_t *dn_assign;
        variable_t *v_assign = NULL;

        if (dst->node->parent)
            v_assign = _operand_get(dst->node->parent);
        // 创建赋值 DAG 节点
        dn_assign = dag_node_alloc(c->op->type, v_assign, NULL);
        // 添加到 DAG 列表
        list_add_tail(dag, &dn_assign->list);

        dn_src = src->dag_node;
        // 如果源节点有父节点且未动态分配，则使用父节点的右值
        if (dn_src->parents && dn_src->parents->size > 0 && !variable_may_malloced(dn_src->var)) {
            dn_parent = dn_src->parents->data[dn_src->parents->size - 1];

            if (OP_ASSIGN == dn_parent->type) {
                assert(2 == dn_parent->childs->size);
                dn_src = dn_parent->childs->data[1];
            }
        }

        ret = dag_node_add_child(dn_assign, dst->dag_node);
        if (ret < 0)
            return ret;

        return dag_node_add_child(dn_assign, dn_src);

    } else if (type_is_assign_array_index(c->op->type) // 处理数组、指针等赋值
               || type_is_assign_dereference(c->op->type)
               || type_is_assign_pointer(c->op->type)) {
        dag_node_t *assign;

        assert(c->srcs);

        assign = dag_node_alloc(c->op->type, NULL, NULL);
        if (!assign)
            return -ENOMEM;
        list_add_tail(dag, &assign->list);

        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            ret = dag_get_node(dag, src->node, &src->dag_node);
            if (ret < 0)
                return ret;

            ret = dag_node_add_child(assign, src->dag_node);
            if (ret < 0)
                return ret;
        }

    } else if (OP_VLA_ALLOC == c->op->type) { // VLA 动态数组分配
        dst = c->dsts->data[0];
        ret = dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        dag_node_t *alloc = dag_node_alloc(c->op->type, NULL, NULL);
        if (!alloc)
            return -ENOMEM;
        list_add_tail(dag, &alloc->list);

        ret = dag_node_add_child(alloc, dst->dag_node);
        if (ret < 0)
            return ret;

        for (i = 0; i < c->srcs->size; i++) {
            src = c->srcs->data[i];

            ret = dag_get_node(dag, src->node, &src->dag_node);
            if (ret < 0)
                return ret;

            ret = dag_node_add_child(alloc, src->dag_node);
            if (ret < 0)
                return ret;
        }
    } else if (OP_3AC_CMP == c->op->type
               || OP_3AC_TEQ == c->op->type
               || OP_3AC_DUMP == c->op->type) { // 处理比较、打印等操作
        dag_node_t *dn_cmp = dag_node_alloc(c->op->type, NULL, NULL);

        list_add_tail(dag, &dn_cmp->list);

        if (c->srcs) {
            int i;
            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                ret = dag_get_node(dag, src->node, &src->dag_node);
                if (ret < 0)
                    return ret;

                ret = dag_node_add_child(dn_cmp, src->dag_node);
                if (ret < 0)
                    return ret;
            }
        }
    } else if (OP_3AC_SETZ == c->op->type
               || OP_3AC_SETNZ == c->op->type
               || OP_3AC_SETLT == c->op->type
               || OP_3AC_SETLE == c->op->type
               || OP_3AC_SETGT == c->op->type
               || OP_3AC_SETGE == c->op->type) { // 处理条件设置（SETcc）
        assert(c->dsts && 1 == c->dsts->size);
        dst = c->dsts->data[0];

        dag_node_t *dn_setcc = dag_node_alloc(c->op->type, NULL, NULL);
        list_add_tail(dag, &dn_setcc->list);

        ret = dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        ret = dag_node_add_child(dn_setcc, dst->dag_node);
        if (ret < 0)
            return ret;

    } else if (OP_INC == c->op->type
               || OP_DEC == c->op->type
               || OP_INC_POST == c->op->type
               || OP_DEC_POST == c->op->type
               || OP_3AC_INC == c->op->type
               || OP_3AC_DEC == c->op->type) { // 处理自增/自减
        src = c->srcs->data[0];

        assert(src->node->parent);

        variable_t *v_parent = _operand_get(src->node->parent);
        dag_node_t *dn_parent = dag_node_alloc(c->op->type, v_parent, NULL);

        list_add_tail(dag, &dn_parent->list);

        ret = dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = dag_node_add_child(dn_parent, src->dag_node);
        if (ret < 0)
            return ret;

        if (c->dsts) {
            dst = c->dsts->data[0];
            assert(dst->node);
            dst->dag_node = dn_parent;
        }

    } else if (OP_TYPE_CAST == c->op->type) { // 类型转换
        src = c->srcs->data[0];
        dst = c->dsts->data[0];

        ret = dag_get_node(dag, src->node, &src->dag_node);
        if (ret < 0)
            return ret;

        ret = dag_get_node(dag, dst->node, &dst->dag_node);
        if (ret < 0)
            return ret;

        if (!dag_node_find_child(dst->dag_node, src->dag_node)) {
            ret = dag_node_add_child(dst->dag_node, src->dag_node);
            if (ret < 0)
                return ret;
        }

    } else if (OP_RETURN == c->op->type) { // return 语句
        if (c->srcs) {
            dag_node_t *dn = dag_node_alloc(c->op->type, NULL, NULL);

            list_add_tail(dag, &dn->list);

            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                ret = dag_get_node(dag, src->node, &src->dag_node);
                if (ret < 0)
                    return ret;

                ret = dag_node_add_child(dn, src->dag_node);
                if (ret < 0)
                    return ret;
            }
        }
    } else if (type_is_jmp(c->op->type)) { // 跳转指令（暂时只打印日志）
        logd("c->op: %d, name: %s\n", c->op->type, c->op->name);

    } else { // 其他情况使用 _3ac_code_to_dag 通用处理
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

// 对跳转指令进行优化，找到真正的基本块起点
/*
总结：

    功能：优化跳转链，把连续的 GOTO 和 NOP 跳过，找到实际的基本块起点。

    标记 basic_block_start 和 jmp_dst_flag，为后续 CFG（控制流图）分析准备。

    类似 “跳转折叠” 的处理。
*/
static void mc_3ac_filter_jmp(list_t *h, mc_3ac_code_t *c) {
    list_t *l2 = NULL;
    mc_3ac_code_t *c2 = NULL;
    mc_3ac_operand_t *dst0 = c->dsts->data[0];
    mc_3ac_operand_t *dst1 = NULL;
    // 遍历跳转目标的链表
    for (l2 = &dst0->code->list; l2 != list_sentinel(h);) {
        c2 = list_data(l2, mc_3ac_code_t, list);
        // 如果目标是 GOTO，继续跟踪下一跳
        if (OP_GOTO == c2->op->type) {
            dst0 = c2->dsts->data[0];
            l2 = &dst0->code->list;
            continue;
        }
        // 如果是 NOP，跳过
        if (OP_3AC_NOP == c2->op->type) {
            l2 = list_next(l2);
            continue;
        }
        // 遇到非跳转指令，则把它标记为基本块起点
        if (!type_is_jmp(c2->op->type)) {
            dst0 = c->dsts->data[0];
            dst0->code = c2;

            c2->basic_block_start = 1;
            c2->jmp_dst_flag = 1;
            break;
        }
#if 0
		if (OP_GOTO == c->op->type) {
			c->op        = c2->op;

			dst0 = c ->dsts->data[0];
			dst1 = c2->dsts->data[0];

			dst0->code = dst1->code;

			l2 = &dst1->code->list;
			continue;
		}
#endif
        logw("c: %s, c2: %s\n", c->op->name, c2->op->name);
        dst0 = c->dsts->data[0];
        dst0->code = c2;
        c2->basic_block_start = 1;
        c2->jmp_dst_flag = 1;
        break;
    }
    // 设置下一条指令为基本块起点
    l2 = list_next(&c->list);
    if (l2 != list_sentinel(h)) {
        c2 = list_data(l2, mc_3ac_code_t, list);
        c2->basic_block_start = 1;
    }
    c->basic_block_start = 1;
}

/*
函数作用
    优化三地址码中 TEQ → SETCC → 条件跳转 链。

    将连续的条件跳转和 setcc 组合，尝试更新跳转目标，减少冗余跳转。

核心逻辑
    根据跳转类型 JZ/JNZ 和前一条 SETCC 指令确定当前跳转方向。

    遍历 TEQ 指令链，确保操作变量一致。

    查找 TEQ 后的第一个跳转指令 jcc。

    根据跳转逻辑更新 dst0->code，即跳转目标。

    如果前一条指令是 setcc，则克隆并添加到当前链表末尾，以保持跳转逻辑正确。

    循环处理直到 TEQ 链结束。

细节注意点
    assert 确保操作数数量为 1。

    每次跳转更新都严格检查列表边界。

    内存不足时返回 -ENOMEM。

    最终返回 0（成功或未处理）。
*/
static int mc_3ac_filter_dst_teq(list_t *h, mc_3ac_code_t *c) {
    // 声明一些辅助指针
    mc_3ac_code_t *setcc2 = NULL;              // 用于保存初始 setcc 指令
    mc_3ac_code_t *setcc3 = NULL;              // 用于克隆的 setcc 指令
    mc_3ac_code_t *setcc = NULL;               // 用于遍历前一条 setcc 指令
    mc_3ac_code_t *jcc = NULL;                 // 用于找到下一个跳转指令
    mc_3ac_operand_t *dst0 = c->dsts->data[0]; // 当前条件跳转指令的目标操作数
    mc_3ac_operand_t *dst1 = NULL;             // 临时操作数
    mc_3ac_code_t *teq = dst0->code;           // dst0 对应的 TEQ（测试等于）指令
    list_t *l;                                 // 遍历列表使用

    // 当前跳转类型
    int jmp_op;

    // 如果 c 是列表头的第一个元素，则直接返回 0，不处理
    if (list_prev(&c->list) == list_sentinel(h))
        return 0;

    // 获取 c 前一条指令
    setcc = list_data(list_prev(&c->list), mc_3ac_code_t, list);

    // 根据 c 的跳转类型和前一条 setcc 指令类型，确定 jmp_op
    if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETZ == setcc->op->type)
        || (OP_3AC_JZ == c->op->type && OP_3AC_SETNZ == setcc->op->type)
        || (OP_3AC_JGT == c->op->type && OP_3AC_SETLE == setcc->op->type)
        || (OP_3AC_JGE == c->op->type && OP_3AC_SETLT == setcc->op->type)
        || (OP_3AC_JLT == c->op->type && OP_3AC_SETGE == setcc->op->type)
        || (OP_3AC_JLE == c->op->type && OP_3AC_SETGT == setcc->op->type))
        jmp_op = OP_3AC_JZ; // 对应条件跳转类型

    else if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETNZ == setcc->op->type)
             || (OP_3AC_JZ == c->op->type && OP_3AC_SETZ == setcc->op->type)
             || (OP_3AC_JGT == c->op->type && OP_3AC_SETGT == setcc->op->type)
             || (OP_3AC_JGE == c->op->type && OP_3AC_SETGE == setcc->op->type)
             || (OP_3AC_JLT == c->op->type && OP_3AC_SETLT == setcc->op->type)
             || (OP_3AC_JLE == c->op->type && OP_3AC_SETLE == setcc->op->type))
        jmp_op = OP_3AC_JNZ; // 另一种条件跳转类型
    else
        return 0; // 不符合模式，直接返回

    // 保存初始 setcc 指令
    setcc2 = setcc;

    // 循环处理 TEQ 链
    while (teq && OP_3AC_TEQ == teq->op->type) {
        // TEQ 和 SETCC 必须满足操作数数量一致
        assert(setcc->dsts && 1 == setcc->dsts->size);
        assert(teq->srcs && 1 == teq->srcs->size);

        mc_3ac_operand_t *src = teq->srcs->data[0];
        mc_3ac_operand_t *dst = setcc->dsts->data[0];
        variable_t *v_teq = _operand_get(src->node); // 获取 TEQ 的操作变量
        variable_t *v_set = _operand_get(dst->node); // 获取 SETCC 的操作变量

        // 如果操作变量不一致，则不处理
        if (v_teq != v_set)
            return 0;
        // 寻找 TEQ 后的第一个跳转指令 jcc
        for (l = list_next(&teq->list); l != list_sentinel(h); l = list_next(l)) {
            jcc = list_data(l, mc_3ac_code_t, list);

            if (type_is_jmp(jcc->op->type))
                break; // 找到跳转指令
        }
        if (l == list_sentinel(h))
            return 0; // 没找到跳转指令，退出
        // 根据 jmp_op 类型处理跳转链
        if (OP_3AC_JZ == jmp_op) {
            if (OP_3AC_JZ == jcc->op->type) {
                // 更新 dst0 的 code 指向 jcc 的目标
                dst0 = c->dsts->data[0];
                dst1 = jcc->dsts->data[0];

                dst0->code = dst1->code;

            } else if (OP_3AC_JNZ == jcc->op->type) {
                // 如果是反向跳转，指向下一个指令
                l = list_next(&jcc->list);
                if (l == list_sentinel(h))
                    return 0;

                dst0 = c->dsts->data[0];
                dst0->code = list_data(l, mc_3ac_code_t, list);
            } else
                return 0;

        } else if (OP_3AC_JNZ == jmp_op) {
            if (OP_3AC_JNZ == jcc->op->type) {
                dst0 = c->dsts->data[0];
                dst1 = jcc->dsts->data[0];

                dst0->code = dst1->code;

            } else if (OP_3AC_JZ == jcc->op->type) {
                l = list_next(&jcc->list);
                if (l == list_sentinel(h))
                    return 0;

                dst0 = c->dsts->data[0];
                dst0->code = list_data(l, mc_3ac_code_t, list);
            } else
                return 0;
        }
        // 更新 teq 指向 dst0 的 code
        teq = dst0->code;

        // 获取 jcc 前一条 setcc 指令
        if (list_prev(&jcc->list) == list_sentinel(h))
            return 0;
        setcc = list_data(list_prev(&jcc->list), mc_3ac_code_t, list);

        // 如果是 setcc 类型，克隆一条并加入当前 c 的链表末尾
        if (type_is_setcc(setcc->op->type)) {
            setcc3 = mc_3ac_code_clone(setcc);
            if (!setcc3)
                return -ENOMEM;      // 内存不足
            setcc3->op = setcc2->op; // 替换操作类型

            list_add_tail(&c->list, &setcc3->list); // 加入链表
        }
        // 更新 jmp_op，用于下一轮循环
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

// 用于 处理前一个 TEQ 指令链，并生成对应的条件跳转（JCC）
static int mc_3ac_filter_prev_teq(list_t *h, mc_3ac_code_t *c, mc_3ac_code_t *teq) {
    mc_3ac_code_t *setcc3 = NULL; // 用于克隆 setcc 指令
    mc_3ac_code_t *setcc2 = NULL; // 保存前一条 setcc
    mc_3ac_code_t *setcc = NULL;  // 前一个 setcc
    mc_3ac_code_t *jcc = NULL;    // 当前生成的跳转指令
    mc_3ac_code_t *jmp = NULL;    // 临时跳转指令
    list_t *l;

    int jcc_type; // 用于记录需要生成的跳转类型

    // 检查 TEQ 前一条指令是否存在
    if (list_prev(&teq->list) == list_sentinel(h))
        return 0;

    // 获取 TEQ 前一条指令
    setcc = list_data(list_prev(&teq->list), mc_3ac_code_t, list);
    jcc_type = -1;

    // 根据 c 的跳转类型和 setcc 类型确定生成的 jcc_type
    // 根据 TEQ + SETCC + 原始跳转，生成对应的反向跳转类型
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
        return 0; // c 不是 JZ/JNZ，不处理

    // 确保操作数一致
    assert(setcc->dsts && 1 == setcc->dsts->size);
    assert(teq->srcs && 1 == teq->srcs->size);

    mc_3ac_operand_t *src = teq->srcs->data[0];
    mc_3ac_operand_t *dst0 = setcc->dsts->data[0];
    mc_3ac_operand_t *dst1 = NULL;
    variable_t *v_teq = _operand_get(src->node);
    variable_t *v_set = _operand_get(dst0->node);

    if (v_teq != v_set)
        return 0; // TEQ 与 SETCC 操作变量不一致，不处理

// 宏定义：生成 JCC 指令
// 作用：为指定类型 cc 分配一条新的跳转指令 j。
#define MC_3AC_JCC_ALLOC(j, cc)                          \
    do {                                                 \
        j = mc_3ac_code_alloc();                         \
        if (!j)                                          \
            return -ENOMEM;                              \
                                                         \
        j->dsts = vector_alloc();                        \
        if (!j->dsts) {                                  \
            mc_3ac_code_free(j);                         \
            return -ENOMEM;                              \
        }                                                \
                                                         \
        mc_3ac_operand_t *dst0 = mc_3ac_operand_alloc(); \
        if (!dst0) {                                     \
            mc_3ac_code_free(j);                         \
            return -ENOMEM;                              \
        }                                                \
                                                         \
        if (vector_add(j->dsts, dst0) < 0) {             \
            mc_3ac_code_free(j);                         \
            mc_3ac_operand_free(dst0);                   \
            return -ENOMEM;                              \
        }                                                \
        j->op = mc_3ac_find_operator(cc);                \
        assert(j->op);                                   \
    } while (0)

    // 生成 JCC 指令并插入链表
    mc_3ac_JCC_ALLOC(jcc, jcc_type);
    dst0 = jcc->dsts->data[0];
    dst1 = c->dsts->data[0];
    dst0->code = dst1->code;                  // JCC 跳转目标指向 c 的目标
    list_add_front(&setcc->list, &jcc->list); // 插入到 setcc 前

    // 克隆前一条 SETCC（如果存在）
    // 作用：保持条件计算逻辑一致，避免破坏跳转链
    l = list_prev(&c->list);
    if (l != list_sentinel(h)) {
        setcc2 = list_data(l, mc_3ac_code_t, list);

        if (type_is_setcc(setcc2->op->type)) {
            setcc3 = mc_3ac_code_clone(setcc2);
            if (!setcc3)
                return -ENOMEM;
            setcc3->op = setcc->op;

            list_add_tail(&jcc->list, &setcc3->list);
        }
    }

    // 添加跳转到下一个指令（GOTO）
    /*
    作用：
        如果 TEQ 后有下一条指令，为 JCC 添加 GOTO 跳转。

        调用 mc_3ac_filter_jmp 对生成的跳转做进一步优化。
    */
    l = list_next(&c->list);
    if (l == list_sentinel(h)) {
        mc_3ac_filter_jmp(h, jcc);
        return 0;
    }

    mc_3ac_JCC_ALLOC(jmp, OP_GOTO);
    dst0 = jmp->dsts->data[0];
    dst0->code = list_data(l, mc_3ac_code_t, list);
    list_add_front(&jcc->list, &jmp->list);

    mc_3ac_filter_jmp(h, jcc);
    mc_3ac_filter_jmp(h, jmp);

    return 0;
}

// 打印整个三地址码链表
void mc_3ac_list_print(list_t *h) {
    mc_3ac_code_t *c;
    list_t *l;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        c = list_data(l, mc_3ac_code_t, list);

        mc_3ac_code_print(c, NULL); // 打印每条 3AC
    }
}

// 标记 基本块起始点 并调用跳转优化函数
/*
mc_3ac_find_basic_block_start
    标记基本块起始点。

    对逻辑表达式、跳转链进行优化。

    删除多余 NOP/GOTO 指令，整理控制流。
*/
static int mc_3ac_find_basic_block_start(list_t *h) {
    int start = 0;
    list_t *l;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        mc_3ac_code_t *c = list_data(l, mc_3ac_code_t, list);

        list_t *l2 = NULL;
        mc_3ac_code_t *c2 = NULL;

        if (!start) {
            c->basic_block_start = 1;
            start = 1;
        }
#if 0
		if (type_is_assign_dereference(c->op->type)) {

			l2	= list_next(&c->list);
			if (l2 != list_sentinel(h)) {
				c2 = list_data(l2, 3ac_code_t, list);
				c2->basic_block_start = 1;
			}

			c->basic_block_start = 1;
			continue;
		}
		if (OP_DEREFERENCE == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (OP_CALL == c->op->type) {

			l2	= list_next(&c->list);
			if (l2 != list_sentinel(h)) {
				c2 = list_data(l2, 3ac_code_t, list);
				c2->basic_block_start = 1;
			}

//			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (OP_RETURN == c->op->type) {
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
        // CMP / TEQ 后遇到跳转，标记基本块开始
        if (OP_3AC_CMP == c->op->type
            || OP_3AC_TEQ == c->op->type) {
            for (l2 = list_next(&c->list); l2 != list_sentinel(h); l2 = list_next(l2)) {
                c2 = list_data(l2, mc_3ac_code_t, list);

                if (type_is_setcc(c2->op->type))
                    continue;

                if (OP_3AC_TEQ == c2->op->type)
                    continue;

                if (type_is_jmp(c2->op->type))
                    c->basic_block_start = 1;
                break;
            }
            continue;
        }

        if (type_is_jmp(c->op->type)) {
            mc_3ac_operand_t *dst0 = c->dsts->data[0];

            assert(dst0->code);

            // 优化第一个逻辑表达式，如 '&&', '||'
            if (OP_3AC_TEQ == dst0->code->op->type) {
                int ret = mc_3ac_filter_dst_teq(h, c);
                if (ret < 0)
                    return ret;
            }

            for (l2 = list_prev(&c->list); l2 != list_sentinel(h); l2 = list_prev(l2)) {
                c2 = list_data(l2, mc_3ac_code_t, list);

                if (type_is_setcc(c2->op->type))
                    continue;

                // 优化第二个逻辑表达式，如 '&&', '||'
                if (OP_3AC_TEQ == c2->op->type) {
                    int ret = mc_3ac_filter_prev_teq(h, c, c2);
                    if (ret < 0)
                        return ret;
                }
                break;
            }

            mc_3ac_filter_jmp(h, c);
        }
    }
#if 1
    // 删除 NOP 和多余的 GOTO
    for (l = list_head(h); l != list_sentinel(h);) {
        mc_3ac_code_t *c = list_data(l, mc_3ac_code_t, list);

        list_t *l2 = NULL;
        mc_3ac_code_t *c2 = NULL;
        mc_3ac_operand_t *dst0 = NULL;

        if (OP_3AC_NOP == c->op->type) {
            assert(!c->jmp_dst_flag);

            l = list_next(l);

            list_del(&c->list);
            mc_3ac_code_free(c);
            c = NULL;
            continue;
        }

        if (OP_GOTO != c->op->type) {
            l = list_next(l);
            continue;
        }
        assert(!c->jmp_dst_flag);

        for (l2 = list_next(&c->list); l2 != list_sentinel(h);) {
            c2 = list_data(l2, mc_3ac_code_t, list);

            if (c2->jmp_dst_flag)
                break;

            l2 = list_next(l2);

            list_del(&c2->list);
            mc_3ac_code_free(c2);
            c2 = NULL;
        }

        l = list_next(l);
        dst0 = c->dsts->data[0];

        if (l == &dst0->code->list) {
            list_del(&c->list);
            mc_3ac_code_free(c);
            c = NULL;
        }
    }
#endif
    return 0;
}

/* 基本块分割函数 - 将三地址码列表分割成基本块 */
static int mc_3ac_split_basic_blocks(list_t *h, function_t *f) {
    list_t *l;                // 列表遍历指针
    basic_block_t *bb = NULL; // 当前基本块指针
    // 遍历三地址码列表
    for (l = list_head(h); l != list_sentinel(h);) {
        mc_3ac_code_t *c = list_data(l, mc_3ac_code_t, list); // 获取当前三地址码

        l = list_next(l); // 提前移动到下一个，因为当前节点可能被删除
        // 如果当前指令是基本块的开始
        if (c->basic_block_start) {
            // 分配新的基本块
            bb = basic_block_alloc();
            if (!bb)
                return -ENOMEM; // 内存分配失败
            // 设置基本块索引并添加到函数的基本块列表中
            bb->index = f->nb_basic_blocks++;
            list_add_tail(&f->basic_block_list_head, &bb->list);
            // 将指令关联到当前基本块
            c->basic_block = bb;
            /* 处理比较和测试指令的特殊情况 */
            if (OP_3AC_CMP == c->op->type
                || OP_3AC_TEQ == c->op->type) {
                mc_3ac_operand_t *src;
                mc_3ac_code_t *c2;
                list_t *l2;
                node_t *e;
                int i;
                // 向后查找相关的跳转指令
                for (l2 = list_next(&c->list); l2 != list_sentinel(h); l2 = list_next(l2)) {
                    c2 = list_data(l2, mc_3ac_code_t, list);
                    // 跳过条件码设置指令
                    if (type_is_setcc(c2->op->type))
                        continue;
                    // 跳过测试指令
                    if (OP_3AC_TEQ == c2->op->type)
                        continue;
                    // 如果找到跳转指令
                    if (type_is_jmp(c2->op->type)) {
                        bb->cmp_flag = 1; // 设置比较标志
                        // 分析比较操作的源操作数
                        if (c->srcs) {
                            for (i = 0; i < c->srcs->size; i++) {
                                src = c->srcs->data[i];
                                e = src->node;
                                // 遍历表达式节点，找到最底层的操作
                                while (e && OP_EXPR == e->type)
                                    e = e->nodes[0];
                                assert(e);
                                // 如果涉及解引用操作，设置解引用标志
                                if (OP_DEREFERENCE == e->type)
                                    bb->dereference_flag = 1;
                            }
                        }
                    }
                    break; // 找到跳转指令后退出循环
                }
                // 将比较指令移动到当前基本块的代码列表中
                list_del(&c->list);
                list_add_tail(&bb->code_list_head, &c->list);
                continue; // 继续处理下一个指令
            }
            // 对于非比较指令，移动到当前基本块
            list_del(&c->list);
            list_add_tail(&bb->code_list_head, &c->list);
            /* 根据指令类型设置基本块的各种标志 */

            // 函数调用指令
            if (OP_CALL == c->op->type) {
                bb->call_flag = 1;
                continue;
            }
            // 返回指令
            if (OP_RETURN == c->op->type) {
                bb->ret_flag = 1;
                continue;
            }
            // 调试输出指令
            if (OP_3AC_DUMP == c->op->type) {
                bb->dump_flag = 1;
                continue;
            }
            // 结束指令
            if (OP_3AC_END == c->op->type) {
                bb->end_flag = 1;
                continue;
            }
            // 解引用相关指令
            if (type_is_assign_dereference(c->op->type)
                || OP_DEREFERENCE == c->op->type) {
                bb->dereference_flag = 1;
                continue;
            }
            // 数组索引赋值指令
            if (type_is_assign_array_index(c->op->type)) {
                bb->array_index_flag = 1;
                continue;
            }
            // 可变参数相关指令
            if (OP_VA_START == c->op->type
                || OP_VA_ARG == c->op->type
                || OP_VA_END == c->op->type) {
                bb->varg_flag = 1;
                continue;
            }
            // 跳转指令
            if (type_is_jmp(c->op->type)) {
                bb->jmp_flag = 1; // 设置跳转标志
                // 如果是条件跳转，设置条件跳转标志
                if (type_is_jcc(c->op->type))
                    bb->jcc_flag = 1;
                // 将跳转指令添加到函数的跳转指令向量中
                int ret = vector_add_unique(f->jmps, c);
                if (ret < 0)
                    return ret;
            }
        } else {
            // 如果不是基本块开始指令，添加到当前基本块
            assert(bb); // 确保当前基本块存在
            c->basic_block = bb;
            // 根据指令类型设置相应的标志
            if (type_is_assign_dereference(c->op->type) || OP_DEREFERENCE == c->op->type)
                bb->dereference_flag = 1;

            else if (type_is_assign_array_index(c->op->type))
                bb->array_index_flag = 1;

            else if (OP_CALL == c->op->type)
                bb->call_flag = 1;

            else if (OP_RETURN == c->op->type)
                bb->ret_flag = 1;

            else if (OP_VLA_ALLOC == c->op->type) {
                bb->vla_flag = 1; // 可变长度数组标志
                f->vla_flag = 1;  // 函数级别的VLA标志

            } else if (OP_VA_START == c->op->type
                       || OP_VA_ARG == c->op->type
                       || OP_VA_END == c->op->type)
                bb->varg_flag = 1;
            // 将指令移动到当前基本块的代码列表中
            list_del(&c->list);
            list_add_tail(&bb->code_list_head, &c->list);
        }
    }
    // 成功返回
    return 0;
}

/* 连接基本块，构建控制流图 */
static int mc_3ac_connect_basic_blocks(function_t *f) {
    int i;
    int ret;

    list_t *l;
    // 列表哨兵节点
    list_t *sentinel = list_sentinel(&f->basic_block_list_head);

    /* 第一遍：连接连续的非跳转基本块 */
    for (l = list_head(&f->basic_block_list_head); l != sentinel; l = list_next(l)) {
        basic_block_t *current_bb = list_data(l, basic_block_t, list);
        basic_block_t *prev_bb = NULL;
        basic_block_t *next_bb = NULL;
        list_t *l2 = list_prev(l);
        // 跳过跳转基本块，它们需要特殊处理
        if (current_bb->jmp_flag)
            continue;
        // 连接前一个基本块到当前基本块
        if (l2 != sentinel) {
            prev_bb = list_data(l2, basic_block_t, list);

            if (!prev_bb->jmp_flag) {
                ret = basic_block_connect(prev_bb, current_bb);
                if (ret < 0)
                    return ret;
            }
        }
        // 连接当前基本块到下一个基本块
        l2 = list_next(l);
        if (l2 == sentinel)
            continue;

        next_bb = list_data(l2, basic_block_t, list);

        if (!next_bb->jmp_flag) {
            ret = basic_block_connect(current_bb, next_bb);
            if (ret < 0)
                return ret;
        }
    }
    /* 第二遍：处理跳转指令的连接 */
    for (i = 0; i < f->jmps->size; i++) {
        mc_3ac_code_t *c = f->jmps->data[i];       // 跳转指令
        mc_3ac_operand_t *dst0 = c->dsts->data[0]; // 跳转目标操作数
        mc_3ac_code_t *dst = dst0->code;           // 跳转目标指令

        basic_block_t *current_bb = c->basic_block; // 当前基本块（包含跳转）
        basic_block_t *dst_bb = dst->basic_block;   // 目标基本块
        basic_block_t *prev_bb = NULL;
        basic_block_t *next_bb = NULL;

        // 更新操作数信息
        dst0->bb = dst_bb;
        dst0->code = NULL;

        // 向前查找前驱基本块（非跳转基本块）
        for (l = list_prev(&current_bb->list); l != sentinel; l = list_prev(l)) {
            prev_bb = list_data(l, basic_block_t, list);

            if (!prev_bb->jmp_flag)
                break;

            if (!prev_bb->jcc_flag) {
                prev_bb = NULL;
                break;
            }
            prev_bb = NULL;
        }
        // 连接前驱基本块到目标基本块
        if (prev_bb) {
            ret = basic_block_connect(prev_bb, dst_bb);
            if (ret < 0)
                return ret;
        } else
            continue;
        // 如果是条件跳转，还需要处理fall-through路径
        if (!current_bb->jcc_flag)
            continue;
        // 向后查找后继基本块（非跳转基本块）
        for (l = list_next(&current_bb->list); l != sentinel; l = list_next(l)) {
            next_bb = list_data(l, basic_block_t, list);

            if (!next_bb->jmp_flag)
                break;

            if (!next_bb->jcc_flag) {
                next_bb = NULL;
                break;
            }
            next_bb = NULL;
        }
        // 连接前驱基本块到后继基本块（fall-through路径）
        if (next_bb) {
            ret = basic_block_connect(prev_bb, next_bb);
            if (ret < 0)
                return ret;
        }
    }

    return 0;
}

/* 主要的基本块分割函数 - 对外接口 */
int mc_3ac_split_basic_blocks(list_t *list_head_3ac, function_t *f) {
    // 第一步：找到所有基本块的起始位置
    int ret = mc_3ac_find_basic_block_start(list_head_3ac);
    if (ret < 0)
        return ret;
    // 第二步：执行基本块分割
    ret = mc_3ac_split_basic_blocks(list_head_3ac, f);
    if (ret < 0)
        return ret;
    // 第三步：连接基本块，构建控制流图
    return mc_3ac_connect_basic_blocks(f);
}

/* 创建三地址码指令 */
mc_3ac_code_t *mc_3ac_code_NN(int op_type, node_t **dsts, int nb_dsts, node_t **srcs, int nb_srcs) {
    // 查找操作符定义
    mc_3ac_operator_t *op = mc_3ac_find_operator(op_type);
    if (!op) {
        loge("\n"); // 记录错误
        return NULL;
    }

    mc_3ac_operand_t *operand;
    mc_3ac_code_t *c;
    vector_t *vsrc = NULL; // 源操作数向量
    vector_t *vdst = NULL; // 目标操作数向量
    node_t *node;

    int i;
    // 处理源操作数
    if (srcs) {
        vsrc = vector_alloc();
        for (i = 0; i < nb_srcs; i++) {
            operand = mc_3ac_operand_alloc();

            node = srcs[i];
            // 遍历表达式节点，找到最底层的操作
            while (node && OP_EXPR == node->type)
                node = node->nodes[0];

            operand->node = node;

            vector_add(vsrc, operand);
        }
    }
    // 处理目标操作数
    if (dsts) {
        vdst = vector_alloc();
        for (i = 0; i < nb_dsts; i++) {
            operand = mc_3ac_operand_alloc();

            node = dsts[i];
            // 遍历表达式节点，找到最底层的操作
            while (node && OP_EXPR == node->type)
                node = node->nodes[0];

            operand->node = node;

            vector_add(vdst, operand);
        }
    }
    // 创建三地址码指令
    c = mc_3ac_code_alloc();
    c->op = op;     // 设置操作符
    c->dsts = vdst; // 设置目标操作数
    c->srcs = vsrc; // 设置源操作数
    return c;
}
