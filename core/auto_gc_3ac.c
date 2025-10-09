#include "optimizer.h"
#include "pointer_alias.h"

/*
auto_gc_3ac.c 实现了自动垃圾回收(GC)相关的三地址码生成功能
*/

/* 分配自动GC操作数 - 创建函数指针操作数用于GC */
static mc_3ac_operand_t *_auto_gc_operand_alloc_pf(list_t *dag_list_head, ast_t *ast, function_t *f) {
    mc_3ac_operand_t *src;
    dag_node_t *dn;
    variable_t *v;
    type_t *t = NULL;
    // 查找函数指针类型
    int ret = ast_find_type_type(&t, ast, FUNCTION_PTR);
    assert(0 == ret); // 确保类型查找成功
    assert(t);        // 确保找到类型
    // 根据是否提供函数创建变量
    if (f)
        v = VAR_ALLOC_BY_TYPE(f->node.w, t, 1, 1, f); // 带函数的变量
    else
        v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 1, NULL); // 空函数指针变量

    if (!v)
        return NULL; // 变量分配失败

    // 标记为常量字面量
    v->const_literal_flag = 1;

    // 创建DAG节点（有向无环图节点，用于中间表示优化）
    dn = dag_node_alloc(v->type, v, (node_t *)f);

    // 释放临时变量（DAG节点会持有需要的信息）
    variable_free(v);
    v = NULL;
    if (!dn)
        return NULL; // DAG节点分配失败

    // 分配三地址码操作数
    src = _3ac_operand_alloc();
    if (!src) {
        dag_node_free(dn); // 清理DAG节点
        return NULL;
    }

    // 设置操作数属性
    src->node = (node_t *)f; // 关联的AST节点
    src->dag_node = dn;      // 关联的DAG节点

    // 将DAG节点添加到列表中管理
    list_add_tail(dag_list_head, &dn->list);
    return src;
}

/* 生成自动GC引用代码 - 创建调用_auto_ref函数的代码 */
static mc_3ac_code_t *_auto_gc_code_ref(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    mc_3ac_operand_t *src;
    mc_3ac_code_t *c;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    // 查找自动引用函数（GC的核心函数）
    int ret = ast_find_function(&f, ast, " _auto_ref");
    if (!f)
        return NULL; // 找不到GC函数

    // 分配三地址码指令
    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    // 分配源操作数向量
    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    // 设置操作符为函数调用
    c->op = _3ac_find_operator(OP_CALL);
/* 宏：添加函数指针操作数到调用指令中 */
#define AUTO_GC_CODE_ADD_FUNCPTR()                              \
    do {                                                        \
        src = _auto_gc_operand_alloc_pf(dag_list_head, ast, f); \
        if (!src) {                                             \
            _3ac_code_free(c);                                  \
            return NULL;                                        \
        }                                                       \
                                                                \
        if (vector_add(c->srcs, src) < 0) {                     \
            _3ac_operand_free(src);                             \
            _3ac_code_free(c);                                  \
            return NULL;                                        \
        }                                                       \
    } while (0)
    // 添加函数指针作为第一个参数（被调用的函数）
    AUTO_GC_CODE_ADD_FUNCPTR();
/* 宏：添加DAG节点操作数到调用指令中 */
#define AUTO_GC_CODE_ADD_DN()               \
    do {                                    \
        src = _3ac_operand_alloc();         \
        if (!src) {                         \
            _3ac_code_free(c);              \
            return NULL;                    \
        }                                   \
        src->node = dn->node;               \
        src->dag_node = dn;                 \
                                            \
        if (vector_add(c->srcs, src) < 0) { \
            _3ac_operand_free(src);         \
            _3ac_code_free(c);              \
            return NULL;                    \
        }                                   \
    } while (0)
    // 添加要引用的对象作为第二个参数
    AUTO_GC_CODE_ADD_DN();

    return c;
}

/* 生成数组内存清零代码 - 用于GC时清理数组内存 */
static mc_3ac_code_t *_auto_gc_code_memset_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    mc_3ac_operand_t *src;
    mc_3ac_code_t *c;
    dag_node_t *dn;
    function_t *f;
    variable_t *v;
    type_t *t = NULL;

    // 查找int指针类型（用于memset参数）
    int ret = ast_find_type_type(&t, ast, VAR_INTPTR);
    assert(0 == ret);
    assert(t);

    // 分配三地址码指令
    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    // 分配源操作数向量
    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为内存设置（memset）
    c->op = _3ac_find_operator(OP_3AC_MEMSET);
    // 第一个参数：要清零的数组
    dn = dn_array;
    AUTO_GC_CODE_ADD_DN(); // 使用前面定义的宏
/* 宏：创建常量变量并添加到操作数中 */
#define AUTO_GC_CODE_ADD_VAR()                   \
    do {                                         \
        dn = dag_node_alloc(v->type, v, NULL);   \
                                                 \
        variable_free(v);                        \
        v = NULL;                                \
                                                 \
        if (!dn) {                               \
            _3ac_code_free(c);                   \
            return NULL;                         \
        }                                        \
        src = _3ac_operand_alloc();              \
        if (!src) {                              \
            dag_node_free(dn);                   \
            _3ac_code_free(c);                   \
            return NULL;                         \
        }                                        \
                                                 \
        if (vector_add(c->srcs, src) < 0) {      \
            _3ac_operand_free(src);              \
            dag_node_free(dn);                   \
            _3ac_code_free(c);                   \
            return NULL;                         \
        }                                        \
                                                 \
        src->node = dn->node;                    \
        src->dag_node = dn;                      \
        list_add_tail(dag_list_head, &dn->list); \
    } while (0)
    // 第二个参数：清零的值（0）
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = 0;           // 设置值为0
    v->const_literal_flag = 1; // 标记为常量
    AUTO_GC_CODE_ADD_VAR();
    // 第三个参数：要清零的字节数（数组大小）
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = variable_size(dn_array->var); // 计算变量大小
    v->const_literal_flag = 1;                  // 标记为常量
    AUTO_GC_CODE_ADD_VAR();

    return c;
}

/* 生成自动GC数组释放代码 - 用于释放整个数组内存 */
static mc_3ac_code_t *_auto_gc_code_free_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array, int capacity, int nb_pointers) {
    mc_3ac_operand_t *src;
    mc_3ac_code_t *c;
    dag_node_t *dn;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    // 查找自动数组释放函数
    int ret = ast_find_function(&f, ast, " _auto_free_array");
    if (!f)
        return NULL; // 找不到GC函数

    // 分配三地址码指令
    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    // 分配源操作数向量
    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    // 函数调用操作
    c->op = _3ac_find_operator(OP_CALL);
    // 第一个参数：函数指针（_auto_free_array）
    AUTO_GC_CODE_ADD_FUNCPTR();
    // 第二个参数：要释放的数组
    dn = dn_array;
    AUTO_GC_CODE_ADD_DN();
    // 第三个参数：数组容量
    t = block_find_type_type(ast->current_block, VAR_INTPTR);
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = capacity;    // 设置容量值
    v->const_literal_flag = 1; // 标记为常量
    AUTO_GC_CODE_ADD_VAR();

    // 第四个参数：指针数量
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = nb_pointers; // 设置指针数量
    v->const_literal_flag = 1; // 标记为常量
    AUTO_GC_CODE_ADD_VAR();

    // 第五个参数：析构函数指针（如果数组元素是结构体且有析构函数）
    if (dn_array->var->type >= STRUCT) {
        // 查找结构体类型
        t = NULL;
        ret = ast_find_type_type(&t, ast, dn_array->var->type);
        assert(0 == ret);
        assert(t);
        // 在结构体作用域中查找析构函数
        f = scope_find_function(t->scope, "__release");
    } else
        f = NULL; // 基本类型没有析构函数

    // 添加析构函数指针（可能为NULL）
    AUTO_GC_CODE_ADD_FUNCPTR();

    return c;
}

/* 生成自动GC指针数组释放代码 - 用于释放指针数组 */
static mc_3ac_code_t *_auto_gc_code_freep_array(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array, int nb_pointers) {
    mc_3ac_operand_t *src;
    mc_3ac_code_t *c;
    dag_node_t *dn;
    function_t *f = NULL;
    variable_t *v;
    type_t *t;

    // 查找自动指针数组释放函数
    int ret = ast_find_function(&f, ast, " _auto_freep_array");
    if (!f)
        return NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    c->op = _3ac_find_operator(OP_CALL);
    // 第一个参数：函数指针（_auto_freep_array）
    AUTO_GC_CODE_ADD_FUNCPTR();
    // 第二个参数：要释放的指针数组
    dn = dn_array;
    AUTO_GC_CODE_ADD_DN();
    // 第三个参数：指针数量
    t = NULL;
    ret = ast_find_type_type(&t, ast, VAR_INTPTR);
    assert(0 == ret);
    assert(t);
    v = VAR_ALLOC_BY_TYPE(dn_array->var->w, t, 1, 0, NULL);
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    v->data.i64 = nb_pointers; // 设置指针数量
    v->const_literal_flag = 1; // 标记为常量

    // 为调试信息设置文本表示
    char buf[128];
    snprintf(buf, sizeof(buf) - 1, "%d", nb_pointers);

    if (string_cat_cstr(v->w->text, buf) < 0) {
        variable_free(v);
        _3ac_code_free(c);
        return NULL;
    }
    AUTO_GC_CODE_ADD_VAR();
    // 第四个参数：析构函数指针（如果指针指向的结构体有析构函数）
    if (dn_array->var->type >= STRUCT) {
        t = NULL;
        ret = ast_find_type_type(&t, ast, dn_array->var->type);
        assert(0 == ret);
        assert(t);

        f = scope_find_function(t->scope, "__release");
    } else
        f = NULL;
    AUTO_GC_CODE_ADD_FUNCPTR();

    return c;
}

/* 生成取地址操作代码 - 创建获取变量地址的指令 */
static mc_3ac_code_t *_code_alloc_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    mc_3ac_operand_t *src;
    mc_3ac_operand_t *dst;
    mc_3ac_code_t *c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    // 分配三地址码指令
    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    // 分配源操作数向量
    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    // 分配目标操作数向量
    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }
    // 创建源操作数（要取地址的变量）
    src = _3ac_operand_alloc();
    if (!src) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->srcs, src) < 0) {
        _3ac_operand_free(src);
        _3ac_code_free(c);
        return NULL;
    }
    src->node = dn->node; // 关联AST节点
    src->dag_node = dn;   // 关联DAG节点

    // 创建目标操作数（存储地址的变量）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为取地址操作
    c->op = _3ac_find_operator(OP_ADDRESS_OF);
    // 创建词法单词用于调试信息
    w = lex_word_alloc(dn->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }
    // 表示取地址操作
    w->text = string_cstr("&");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找类型信息
    int ret = ast_find_type_type(&t, ast, dn->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（指针类型，指针层级+1）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers + 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v); // DAG节点会复制需要的信息
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 将新DAG节点添加到管理列表中
    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成解引用操作代码 - 创建通过指针访问内存的指令 */
static mc_3ac_code_t *_code_alloc_dereference(list_t *dag_list_head, ast_t *ast, dag_node_t *dn) {
    mc_3ac_operand_t *src;
    mc_3ac_operand_t *dst;
    mc_3ac_code_t *c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }
    // 创建源操作数（要解引用的指针）
    src = _3ac_operand_alloc();
    if (!src) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->srcs, src) < 0) {
        _3ac_operand_free(src);
        _3ac_code_free(c);
        return NULL;
    }
    src->node = dn->node;
    src->dag_node = dn;
    // 创建目标操作数（解引用结果）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为解引用操作
    c->op = _3ac_find_operator(OP_DEREFERENCE);
    // 创建词法单词用于调试信息
    w = lex_word_alloc(dn->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }
    // 表示解引用操作
    w->text = string_cstr("*");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找类型信息
    int ret = ast_find_type_type(&t, ast, dn->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（指针类型，指针层级-1）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn->var->nb_pointers - 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 将新DAG节点添加到管理列表中
    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成结构体指针成员访问代码 - 创建指向结构体成员的指针操作 */
static mc_3ac_code_t *_code_alloc_member(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_member) {
    mc_3ac_operand_t *base;   // 基址操作数（结构体指针）
    mc_3ac_operand_t *member; // 成员操作数
    mc_3ac_operand_t *dst;    // 目标操作数
    mc_3ac_code_t *c;         // 三地址码指令
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;
    // 分配三地址码指令
    c = _3ac_code_alloc();
    if (!c)
        return NULL;
    // 分配源操作数向量
    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }
    // 分配目标操作数向量
    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }
    // 创建基址操作数（结构体指针）
    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node; // 关联AST节点
    base->dag_node = dn_base;   // 关联DAG节点
    // 创建成员操作数（成员信息）
    member = _3ac_operand_alloc();
    if (!member) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, member) < 0) {
        _3ac_operand_free(member);
        _3ac_code_free(c);
        return NULL;
    }
    member->node = dn_member->node; // 关联AST节点
    member->dag_node = dn_member;   // 关联DAG节点
    // 创建目标操作数（结果指针）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为指针成员访问（-> 操作）
    c->op = _3ac_find_operator(OP_POINTER);
    // 创建词法单词用于调试信息
    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }
    // 表示指针成员访问操作
    w->text = string_cstr("->");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找成员类型信息
    int ret = ast_find_type_type(&t, ast, dn_member->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（成员类型，保持相同的指针层级）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v); // DAG节点会复制需要的信息
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 将新DAG节点添加到管理列表中
    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成结构体成员地址访问代码 - 创建获取结构体成员地址的操作 */
static mc_3ac_code_t *_code_alloc_member_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_member) {
    mc_3ac_operand_t *base;   // 基址操作数（结构体）
    mc_3ac_operand_t *member; // 成员操作数
    mc_3ac_operand_t *dst;    // 目标操作数
    mc_3ac_code_t *c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }
    // 创建基址操作数（结构体）
    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;
    // 创建成员操作数（成员信息）
    member = _3ac_operand_alloc();
    if (!member) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, member) < 0) {
        _3ac_operand_free(member);
        _3ac_code_free(c);
        return NULL;
    }
    member->node = dn_member->node;
    member->dag_node = dn_member;
    // 创建目标操作数（成员地址）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为指针成员地址访问（&-> 操作）
    c->op = _3ac_find_operator(OP_3AC_ADDRESS_OF_POINTER);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }

    w->text = string_cstr("&->"); // 表示取成员地址操作
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找成员类型信息
    int ret = ast_find_type_type(&t, ast, dn_member->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（成员类型，指针层级+1）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_member->var->nb_pointers + 1, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 继承基址变量的维度信息（用于数组处理）
    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成数组成员地址访问代码 - 创建获取数组元素地址的操作 */
static mc_3ac_code_t *_code_alloc_array_member_address(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale) {
    mc_3ac_operand_t *base;  // 基址操作数（数组）
    mc_3ac_operand_t *index; // 索引操作数
    mc_3ac_operand_t *scale; // 缩放因子操作数（元素大小）
    mc_3ac_operand_t *dst;   // 目标操作数
    mc_3ac_code_t *c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }
    // 创建基址操作数（数组）
    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;
    // 创建索引操作数
    index = _3ac_operand_alloc();
    if (!index) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, index) < 0) {
        _3ac_operand_free(index);
        _3ac_code_free(c);
        return NULL;
    }
    index->node = dn_index->node;
    index->dag_node = dn_index;
    // 创建缩放因子操作数（用于计算偏移量）
    scale = _3ac_operand_alloc();
    if (!scale) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, scale) < 0) {
        _3ac_operand_free(scale);
        _3ac_code_free(c);
        return NULL;
    }
    scale->node = dn_scale->node;
    scale->dag_node = dn_scale;
    // 创建目标操作数（元素地址）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为数组索引地址访问（&[] 操作）
    c->op = _3ac_find_operator(OP_3AC_ADDRESS_OF_ARRAY_INDEX);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }
    // 表示取数组元素地址操作
    w->text = string_cstr("&[]");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找基址类型信息
    int ret = ast_find_type_type(&t, ast, dn_base->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（基址类型，保持相同的指针层级）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 继承基址数组的维度信息
    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成数组成员访问代码 - 创建数组索引访问操作 */
static mc_3ac_code_t *_code_alloc_array_member(list_t *dag_list_head, ast_t *ast, dag_node_t *dn_base, dag_node_t *dn_index, dag_node_t *dn_scale) {
    mc_3ac_operand_t *base;  // 基址操作数（数组）
    mc_3ac_operand_t *index; // 索引操作数
    mc_3ac_operand_t *scale; // 缩放因子操作数（元素大小）
    mc_3ac_operand_t *dst;   // 目标操作数
    mc_3ac_code_t *c;
    variable_t *v;
    lex_word_t *w;
    string_t *s;
    type_t *t = NULL;

    c = _3ac_code_alloc();
    if (!c)
        return NULL;

    c->srcs = vector_alloc();
    if (!c->srcs) {
        _3ac_code_free(c);
        return NULL;
    }

    c->dsts = vector_alloc();
    if (!c->dsts) {
        _3ac_code_free(c);
        return NULL;
    }

    // 创建基址操作数（数组）
    base = _3ac_operand_alloc();
    if (!base) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, base) < 0) {
        _3ac_operand_free(base);
        _3ac_code_free(c);
        return NULL;
    }
    base->node = dn_base->node;
    base->dag_node = dn_base;
    // 创建索引操作数
    index = _3ac_operand_alloc();
    if (!index) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, index) < 0) {
        _3ac_operand_free(index);
        _3ac_code_free(c);
        return NULL;
    }
    index->node = dn_index->node;
    index->dag_node = dn_index;
    // 创建缩放因子操作数（用于计算偏移量）
    scale = _3ac_operand_alloc();
    if (!scale) {
        _3ac_code_free(c);
        return NULL;
    }
    if (vector_add(c->srcs, scale) < 0) {
        _3ac_operand_free(scale);
        _3ac_code_free(c);
        return NULL;
    }
    scale->node = dn_scale->node;
    scale->dag_node = dn_scale;
    // 创建目标操作数（数组元素）
    dst = _3ac_operand_alloc();
    if (!dst) {
        _3ac_code_free(c);
        return NULL;
    }

    if (vector_add(c->dsts, dst) < 0) {
        _3ac_operand_free(dst);
        _3ac_code_free(c);
        return NULL;
    }

    // 设置操作符为数组索引访问（[] 操作）
    c->op = _3ac_find_operator(OP_ARRAY_INDEX);

    w = lex_word_alloc(dn_base->var->w->file, 0, 0, LEX_WORD_ID);
    if (!w) {
        _3ac_code_free(c);
        return NULL;
    }
    // 表示数组索引操作
    w->text = string_cstr("[]");
    if (!w->text) {
        lex_word_free(w);
        _3ac_code_free(c);
        return NULL;
    }
    // 查找基址类型信息
    int ret = ast_find_type_type(&t, ast, dn_base->var->type);
    assert(0 == ret);
    assert(t);
    // 创建目标变量（基址类型，保持相同的指针层级）
    v = VAR_ALLOC_BY_TYPE(w, t, 0, dn_base->var->nb_pointers, NULL);
    lex_word_free(w);
    w = NULL;
    if (!v) {
        _3ac_code_free(c);
        return NULL;
    }
    // 为目标操作数创建DAG节点
    dst->dag_node = dag_node_alloc(v->type, v, NULL);
    variable_free(v);
    v = NULL;
    if (!dst->dag_node) {
        _3ac_code_free(c);
        return NULL;
    }
    // 继承基址数组的维度信息
    dst->dag_node->var->nb_dimentions = dn_base->var->nb_dimentions;

    list_add_tail(dag_list_head, &dst->dag_node->list);
    return c;
}

/* 生成自动GC引用代码列表 - 为复杂数据结构生成引用计数增加代码 */
static int _auto_gc_code_list_ref(list_t *h, list_t *dag_list_head, ast_t *ast, dn_status_t *ds) {
    dag_node_t *dn = ds->dag_node; // 起始DAG节点
    mc_3ac_code_t *c;
    mc_3ac_operand_t *dst;
    // 如果有索引路径（结构体成员或数组索引）
    if (ds->dn_indexes) {
        int i;
        // 从最内层开始处理（逆序处理索引路径）
        for (i = ds->dn_indexes->size - 1; i >= 0; i--) {
            dn_index_t *di = ds->dn_indexes->data[i];
            // 如果是结构体成员访问
            if (di->member) {
                assert(di->dn);
                // 生成成员访问代码
                c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

            } else {
                // 数组索引访问
                assert(di->index >= 0 || -1 == di->index);
                assert(di->dn_scale);
                // 生成数组成员访问代码
                c = _code_alloc_array_member(dag_list_head, ast, dn, di->dn, di->dn_scale);
            }
            // 将生成的代码添加到列表中
            list_add_tail(h, &c->list);
            // 更新当前DAG节点为访问结果
            dst = c->dsts->data[0];
            dn = dst->dag_node;
        }
    }
    // 生成最终的引用计数增加代码
    c = _auto_gc_code_ref(dag_list_head, ast, dn);

    list_add_tail(h, &c->list);
    return 0;
}

/* 生成自动GC指针释放代码列表 - 为复杂指针数据结构生成释放代码 */
static int _auto_gc_code_list_freep(list_t *h, list_t *dag_list_head, ast_t *ast, dn_status_t *ds) {
    dag_node_t *dn = ds->dag_node;
    mc_3ac_code_t *c;
    mc_3ac_operand_t *dst;
    // 如果有索引路径
    if (ds->dn_indexes) {
        dn_index_t *di;
        int i;
        // 处理除第一个索引外的所有索引（从内层到外层）
        for (i = ds->dn_indexes->size - 1; i >= 1; i--) {
            di = ds->dn_indexes->data[i];

            if (di->member) {
                // 结构体成员访问
                assert(di->dn);

                c = _code_alloc_member(dag_list_head, ast, dn, di->dn);

            } else {
                // 数组索引访问（特殊处理索引0的情况）
                assert(di->index >= 0);

                assert(0 == di->index); // 应该是第一个元素？
                // 生成解引用代码
                c = _code_alloc_dereference(dag_list_head, ast, dn);
            }

            list_add_tail(h, &c->list);

            dst = c->dsts->data[0];
            dn = dst->dag_node;
        }
        // 处理第一个索引（最外层）
        di = ds->dn_indexes->data[0];

        if (di->member) {
            // 结构体成员地址访问
            assert(di->dn);

            c = _code_alloc_member_address(dag_list_head, ast, dn, di->dn);

        } else {
            // 数组索引地址访问
            assert(di->index >= 0 || -1 == di->index);
            assert(di->dn_scale);

            c = _code_alloc_array_member_address(dag_list_head, ast, dn, di->dn, di->dn_scale);
        }

        list_add_tail(h, &c->list);

        dst = c->dsts->data[0];
        dn = dst->dag_node;

    } else {
        // 没有索引路径，直接取地址
        c = _code_alloc_address(dag_list_head, ast, dn);

        list_add_tail(h, &c->list);

        dst = c->dsts->data[0];
        dn = dst->dag_node;
    }
    // 计算指针数量（用于释放）
    int nb_pointers = variable_nb_pointers(dn->var);

    assert(nb_pointers >= 2); // 确保是多级指针
    // 生成指针数组释放代码
    c = _auto_gc_code_freep_array(dag_list_head, ast, dn, nb_pointers - 1);

    list_add_tail(h, &c->list);
    return 0;
}

/* 生成自动GC数组释放代码列表 */
static int _auto_gc_code_list_free_array(list_t *h, list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    mc_3ac_code_t *c;
    // 验证数组属性
    assert(dn_array->var->nb_dimentions > 0); // 必须有维度
    assert(dn_array->var->capacity > 0);      // 必须有容量
    // 生成数组释放代码
    c = _auto_gc_code_free_array(dag_list_head, ast, dn_array, dn_array->var->capacity, dn_array->var->nb_pointers);

    list_add_tail(h, &c->list);
    return 0;
}

/* 生成自动GC数组内存清零代码列表 */
static int _auto_gc_code_list_memset_array(list_t *h, list_t *dag_list_head, ast_t *ast, dag_node_t *dn_array) {
    mc_3ac_code_t *c;
    // 验证数组容量
    assert(dn_array->var->capacity > 0);
    // 生成数组内存清零代码
    c = _auto_gc_code_memset_array(dag_list_head, ast, dn_array);

    list_add_tail(h, &c->list);
    return 0;
}

/* 向基本块添加GC引用代码 */
static int _bb_add_gc_code_ref(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dn_status_t *ds) {
    list_t h;
    list_init(&h); // 初始化临时代码列表
    // 将DAG节点添加到基本块的活跃节点列表中
    if (vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
        return -ENOMEM;
    // 生成引用计数增加代码列表
    int ret = _auto_gc_code_list_ref(&h, dag_list_head, ast, ds);
    if (ret < 0)
        return ret;
    // 将生成的代码添加到基本块
    basic_block_add_code(bb, &h);
    return 0;
}

/* 向基本块添加GC指针释放代码 */
static int _bb_add_gc_code_freep(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dn_status_t *ds) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, ds->dag_node) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_freep(&h, dag_list_head, ast, ds);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}

/* 向基本块添加GC数组内存清零代码 */
static int _bb_add_gc_code_memset_array(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dag_node_t *dn_array) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_memset_array(&h, dag_list_head, ast, dn_array);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}

/* 向基本块添加GC数组释放代码 */
static int _bb_add_gc_code_free_array(list_t *dag_list_head, ast_t *ast, basic_block_t *bb, dag_node_t *dn_array) {
    list_t h;
    list_init(&h);

    if (vector_add_unique(bb->entry_dn_actives, dn_array) < 0)
        return -ENOMEM;

    int ret = _auto_gc_code_list_free_array(&h, dag_list_head, ast, dn_array);
    if (ret < 0)
        return ret;

    basic_block_add_code(bb, &h);
    return 0;
}
