#include "ast.h"

// 打开/初始化一个 AST(抽象语法树)对象
int ast_open(ast_t **past) {
    // 检查输入指针是否有效
    if (!past)
        return -EINVAL; // 如果传入的二级指针为空，返回“无效参数”错误码

    // 为 AST 结构体分配内存，并清零
    ast_t *ast = calloc(1, sizeof(ast_t));
    if (!ast)
        return -ENOMEM; // 分配失败，返回“内存不足”错误码

    // 分配根 block,命名为“global”
    ast->root_block = block_alloc_cstr("global");
    if (!ast->root_block) {
        free(ast);          // 释放已经分配的 ast
        return -ENOMEM;     // 返回“内存不足”错误码
    }

    // 标记该 block 为根节点
    ast->root_block->node.root_flag = 1;

    // 分配全局常量表
    ast->global_consts = vector_alloc();
    if (!ast->global_consts)
        return -ENOMEM;

    // 分配全局重定位表
    ast->global_relas = vector_alloc();
    if (!ast->global_relas)
        return -ENOMEM;

    // 初始化成功，将结果返回给调用者
    *past = ast;
    return 0;
}

// 关闭/释放 AST对象
int ast_close(ast_t *ast) {
    if (ast) {
        // 这里只释放了 ast 本身
        // 没有释放 root_block、global_consts、global_relas
        // 所以可能会造成内存泄漏
        free(ast);
        ast = NULL;
    }
    return 0;
}

// 向抽象语法树添加基本类型
int ast_add_base_type(ast_t *ast, base_type_t *base_type) {
    if (!ast || !base_type) {
        loge("\n");
        return -EINVAL;
    }

    // 先分配类型
    type_t *t = type_alloc(NULL, base_type->name, base_type->type, base_type->size);
    if (!t) {
        loge("\n");
        return -1;
    }

    // 向根块范围中打入类型 t
    scope_push_type(ast->root_block->scope, t);
    return 0;
}

// 向抽象语法书添加文件块
int ast_add_file_block(ast_t *ast, const char *path) {
    if (!ast || !path) {
        loge("\n");
        return -EINVAL;
    }

    // 分配一个文件块
    block_t *file_block = block_alloc_cstr(path);
    if (!file_block) {
        loge("\n");
        return -1;
    }

    // 标记文件块的标志为 1
    file_block->node.file_flag = 1;

    // 节点添加儿子
    node_add_child((node_t *)ast->root_block, (node_t *)file_block);
    // 设置当前节点为文件块
    ast->current_block = file_block;

    return 0;
}

// 获取变量的类型名（包括指针和数组维度信息），返回一个字符串
string_t *variable_type_name(ast_t *ast, variable_t *v) {
    type_t *t = NULL;

    // 在 AST 中根据变量的类型 ID(v->type) 查找类型对象
    int ret = ast_find_type_type(&t, ast, v->type);
    assert(0 == ret);// 确保查找成功
    assert(t);// 确保找到的类型指针非空

    // 克隆该类型的名字（比如 "int"、"float"、"char"）
    string_t *s = string_clone(t->name);

    int i;
    // 如果变量是指针，则在类型名后面拼接对应数量的 "*" 
    // 例如 int* 或 int** 
    for (i = 0; i < v->nb_pointers; i++)
        string_cat_cstr_len(s, "*", 1);

    // 如果变量是数组，则拼接数组维度
    // 例如 int[10] 或 int*[20][30]
    for (i = 0; i < v->nb_dimentions; i++) {
        char str[256];
        // 格式化当前维度大小，写入临时字符串
        snprintf(str, sizeof(str) - 1, "[%d]", v->dimentions[i].num);

        // 拼接到类型字符串末尾
        string_cat_cstr(s, str);
    }

    // 返回构造好的类型字符串
    return s;
}

// 通过名字寻找函数 node
static int _find_function_by_name(node_t *node, void *arg, vector_t *vec) {
    if (node->class_flag)
        return 1;

    if (FUNCTION == node->type) {
        // 定义一个函数对象
        function_t *f = (function_t *)node;

        assert(!f->member_flag);

        if (f->static_flag)
            return 0;

        // 
        if (!strcmp(f->node.w->text->data, arg)) {
            int ret = vector_add(vec, f);
            if (ret < 0)
                return ret;
        }
        return 1;
    }

    return 0;
}

// 通过名字寻找类型 node
static int _find_type_by_name(node_t *node, void *arg, vector_t *vec) {
    if (FUNCTION == node->type)
        return 1;

    if (node->type >= STRUCT && node->class_flag) {
        type_t *t = (type_t *)node;

        if (!strcmp(t->name->data, arg)) {
            int ret = vector_add(vec, t);
            if (ret < 0)
                return ret;
        }

        return 1;
    }

    return 0;
}

// 根据类型寻找类型
static int _find_type_by_type(node_t *node, void *arg, vector_t *vec) {
    if (FUNCTION == node->type)
        return 1;

    if (node->type >= STRUCT && (node->class_flag || node->union_flag)) {
        type_t *t = (type_t *)node;

        if (t->type == (intptr_t)arg) {
            int ret = vector_add(vec, t);
            if (ret < 0)
                return ret;
        }

        if (node->union_flag)
            return 1;
    }

    return 0;
}

// 根据名字寻找变量
static int _find_var_by_name(node_t *node, void *arg, vector_t *vec) {
    if (FUNCTION == node->type)
        return 1;

    if (node->class_flag)
        return 1;

    // 如果是块
    if (OP_BLOCK == node->type) {
        // 先定义一个块
        block_t *b = (block_t *)node;

        if (!b->scope)
            return 0;

        // 范围内寻找变量
        variable_t *v = scope_find_variable(b->scope, arg);
        if (!v)
            return 0;

        assert(!v->local_flag && !v->member_flag);

        if (v->static_flag)
            return 0;

        return vector_add(vec, v);
    }
    return 0;
}

// 抽象语法树寻找全局函数
int ast_find_global_function(function_t **pf, ast_t *ast, char *fname) {
    // 先分配一个 vector_t
    vector_t *vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    // 对node节点进行bfs(宽度优先搜索)
    int ret = node_search_bfs((node_t *)ast->root_block, fname, vec, -1, _find_function_by_name);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

    if (0 == vec->size) {
        *pf = NULL;
        vector_free(vec);
        return 0;
    }

    // 
    function_t *f2 = vec->data[0];
    function_t *f;
    int n = 0;
    int i;
    for (i = 0; i < vec->size; i++) {
        f = vec->data[i];

        if (f->node.define_flag) {
            f2 = f;
            n++;
        }
    }

    if (n > 1) {
        for (i = 0; i < vec->size; i++) {
            f = vec->data[i];

            if (f->node.define_flag)
                loge("multi-define: '%s' in file: %s, line: %d\n", fname, f->node.w->file->data, f->node.w->line);
        }

        vector_free(vec);
        return -1;
    }

    *pf = f2;

    vector_free(vec);
    return 0;
}

int ast_find_global_variable(variable_t **pv, ast_t *ast, char *name) {
    vector_t *vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    int ret = node_search_bfs((node_t *)ast->root_block, name, vec, -1, _find_var_by_name);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

    if (0 == vec->size) {
        *pv = NULL;
        vector_free(vec);
        return 0;
    }

    variable_t *v2 = vec->data[0];
    variable_t *v;
    int n = 0;
    int i;
    for (i = 0; i < vec->size; i++) {
        v = vec->data[i];

        if (!v->extern_flag) {
            v2 = v;
            n++;
        }
    }

    if (n > 1) {
        for (i = 0; i < vec->size; i++) {
            v = vec->data[i];

            if (!v->extern_flag)
                loge("multi-define: '%s' in file: %s, line: %d\n", name, v->w->file->data, v->w->line);
        }

        vector_free(vec);
        return -1;
    }

    *pv = v2;

    vector_free(vec);
    return 0;
}

static int _type_check(type_t **pt, vector_t *vec) {
    type_t *t2 = vec->data[0];
    type_t *t;

    int n = 0;
    int i;

    for (i = 0; i < vec->size; i++) {
        t = vec->data[i];

        if (t->node.define_flag) {
            t2 = t;
            n++;
        }
    }

    if (n > 1) {
        for (i = 0; i < vec->size; i++) {
            t = vec->data[i];

            if (t->node.define_flag)
                loge("multi-define: '%s' in file: %s, line: %d\n", t->name->data, t->node.w->file->data, t->node.w->line);
        }

        return -1;
    }

    *pt = t2;
    return 0;
}

int ast_find_global_type(type_t **pt, ast_t *ast, char *name) {
    vector_t *vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    int ret = node_search_bfs((node_t *)ast->root_block, name, vec, -1, _find_type_by_name);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

    if (0 == vec->size) {
        *pt = NULL;
        ret = 0;
    } else
        ret = _type_check(pt, vec);

    vector_free(vec);
    return ret;
}

int ast_find_global_type_type(type_t **pt, ast_t *ast, int type) {
    vector_t *vec = vector_alloc();
    if (!vec)
        return -ENOMEM;

    int ret = node_search_bfs((node_t *)ast->root_block, (void *)(intptr_t)type, vec, -1, _find_type_by_type);
    if (ret < 0) {
        vector_free(vec);
        return ret;
    }

    if (0 == vec->size) {
        *pt = NULL;
        ret = 0;
    } else
        ret = _type_check(pt, vec);

    vector_free(vec);
    return ret;
}

int ast_find_function(function_t **pf, ast_t *ast, char *name) {
    *pf = block_find_function(ast->current_block, name);
    if (*pf)
        return 0;

    return ast_find_global_function(pf, ast, name);
}

int ast_find_variable(variable_t **pv, ast_t *ast, char *name) {
    *pv = block_find_variable(ast->current_block, name);
    if (*pv)
        return 0;

    return ast_find_global_variable(pv, ast, name);
}

int ast_find_type(type_t **pt, ast_t *ast, char *name) {
    *pt = block_find_type(ast->current_block, name);
    if (*pt)
        return 0;

    return ast_find_global_type(pt, ast, name);
}

int ast_find_type_type(type_t **pt, ast_t *ast, int type) {
    *pt = block_find_type_type(ast->current_block, type);
    if (*pt)
        return 0;

    return ast_find_global_type_type(pt, ast, type);
}

int ast_add_const_str(ast_t *ast, node_t *parent, lex_word_t *w) {
    variable_t *v;
    lex_word_t *w2;
    type_t *t = block_find_type_type(ast->current_block, VAR_CHAR);
    node_t *node;

    w2 = lex_word_clone(w);
    if (!w2)
        return -ENOMEM;

    int ret = string_cat_cstr(w2->text, "__cstr");
    if (ret < 0) {
        lex_word_free(w2);
        return ret;
    }

    v = VAR_ALLOC_BY_TYPE(w2, t, 1, 1, NULL);
    lex_word_free(w2);
    w2 = NULL;
    if (!v)
        return -ENOMEM;
    v->const_literal_flag = 1;

    logi("w->text: %s\n", w->text->data);
    v->data.s = string_clone(w->text);
    if (!v->data.s) {
        variable_free(v);
        return -ENOMEM;
    }

    node = node_alloc(NULL, v->type, v);
    variable_free(v);
    v = NULL;
    if (!node)
        return -ENOMEM;

    ret = node_add_child(parent, node);
    if (ret < 0) {
        node_free(node);
        return ret;
    }

    return 0;
}

int ast_add_const_var(ast_t *ast, node_t *parent, int type, const uint64_t u64) {
    variable_t *v;
    type_t *t = block_find_type_type(ast->current_block, type);
    node_t *node;

    v = VAR_ALLOC_BY_TYPE(NULL, t, 1, 0, NULL);
    if (!v)
        return -ENOMEM;
    v->data.u64 = u64;
    v->const_literal_flag = 1;

    node = node_alloc(NULL, v->type, v);
    variable_free(v);
    v = NULL;
    if (!node)
        return -ENOMEM;

    int ret = node_add_child(parent, node);
    if (ret < 0) {
        node_free(node);
        return ret;
    }

    return 0;
}

int function_signature(ast_t *ast, function_t *f) {
    string_t *s;
    type_t *t = (type_t *)f->node.parent;

    int ret;
    int i;

    s = string_alloc();
    if (!s)
        return -ENOMEM;

    if (t->node.type >= STRUCT) {
        assert(t->node.class_flag);

        ret = string_cat(s, t->name);
        if (ret < 0)
            goto error;

        ret = string_cat_cstr(s, "_");
        if (ret < 0)
            goto error;
    }

    if (f->op_type >= 0) {
        operator_t *op = find_base_operator_by_type(f->op_type);

        if (!op->signature)
            goto error;

        ret = string_cat_cstr(s, op->signature);
    } else
        ret = string_cat(s, f->node.w->text);

    if (ret < 0)
        goto error;
    logd("f signature: %s\n", s->data);

    if (t->node.type < STRUCT) {
        if (f->signature)
            string_free(f->signature);

        f->signature = s;
        return 0;
    }

    if (f->argv) {
        for (i = 0; i < f->argv->size; i++) {
            variable_t *v = f->argv->data[i];
            type_t *t_v = NULL;

            t_v = block_find_type_type((block_t *)t, v->type);
            if (!t_v) {
                ret = ast_find_global_type_type(&t_v, ast, v->type);
                if (ret < 0)
                    goto error;
            }

            ret = string_cat_cstr(s, "_");
            if (ret < 0)
                goto error;

            logd("t_v: %p, v->type: %d, v->w->text->data: %s\n", t_v, v->type, v->w->text->data);

            const char *abbrev = type_find_abbrev(t_v->name->data);
            if (abbrev)
                ret = string_cat_cstr(s, abbrev);
            else
                ret = string_cat(s, t_v->name);
            if (ret < 0)
                goto error;

            if (v->nb_pointers > 0) {
                char buf[64];
                snprintf(buf, sizeof(buf) - 1, "%d", v->nb_pointers);

                ret = string_cat_cstr(s, buf);
                if (ret < 0)
                    goto error;
            }
        }
    }

    logd("f signature: %s\n", s->data);

    if (f->signature)
        string_free(f->signature);
    f->signature = s;
    return 0;

error:
    string_free(s);
    return -1;
}
