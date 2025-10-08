#include "parse.h"
#include "x64.h"
#include "operator_handler_semantic.h"
#include "operator_handler_const.h"
#include "dfa.h"
#include "basic_block.h"
#include "optimizer.h"
#include "ghr_elf.h"
#include "leb128.h"
#include "eda.h"

/*
 * 调用内部函数 _parse_add_sym 添加一个符号
 * 参数说明：
 * - parse         : 当前解析器上下文
 * - sh_name       : 段名 (如 ".text", ".data")
 * - 0, 0          : 符号的值 (st_value) 和大小 (st_size)，这里传 0，表示段符号没有具体值和大小
 * - sh_index      : 段索引 (section index)，比如 SHNDX_TEXT、SHNDX_DATA
 * - ELF64_ST_INFO : 构造 ELF64 的符号信息 (st_info)，这里指定绑定和类型：
 *       STB_LOCAL   : 本地符号（只在本目标文件可见）
 *       STT_SECTION : 符号类型是 "段符号" (section)
 *
 * 返回值 ret：如果成功，返回 >= 0；如果失败，返回 < 0。
 */
#define ADD_SECTION_SYMBOL(sh_index, sh_name)                                                            \
    do {                                                                                                 \
        int ret = _parse_add_sym(parse, sh_name, 0, 0, sh_index, ELF64_ST_INFO(STB_LOCAL, STT_SECTION)); \
        if (ret < 0) {                                                                                   \
            loge("\n");                                                                                  \
            return ret;                                                                                  \
        }                                                                                                \
    } while (0)

// 在编译器的 语法分析 / 语义分析 阶段，用来 识别、匹配、存储 C 语言及扩展类型的元信息
/*
它的主要用途有:
1. 类型关键字识别
    当语法分析器遇到int\float\uint64_t\funcptr等关键字时，可以在base_types
    里查到对应的 内部枚举值 (VAR_INT, VAR_FLOAT …)，从而建立 AST 节点。

2. 类型大小计算
    编译器需要知道每种类型的 字节大小，用于：
        - 变量分配（符号表里的变量大小）
        - 表达式计算时的类型提升
        - 结构体、数组的内存布局
        - 生成汇编代码时的偏移量计算

3. 类型检查
    - 检查不同类型是否能合法赋值 (int32_t → double 等)
    - 函数调用时的参数匹配
    - 指针和整数类型的兼容性检查

4. 代码生成
    在生成中间代码或目标代码时，需要用到 类型大小，比如：
        - int a[10]; → 分配 4 * 10 = 40 字节
        - sizeof(uint64_t) → 结果是 8

*/
base_type_t base_types[] =
    {
        {VAR_CHAR, "char", 1}, // char 类型，占 1 字节

        {VAR_VOID, "void", 1}, // void 类型（占位，通常大小 = 1）
        {VAR_BIT, "bit", 1},   // 自定义 bit 类型，占 1 字节
        {VAR_U2, "bit2_t", 1}, // 自定义 2 位整数类型
        {VAR_U3, "bit3_t", 1}, // 自定义 3 位整数类型
        {VAR_U4, "bit4_t", 1}, // 自定义 4 位整数类型

        {VAR_I1, "int1_t", 1}, // 1 字节整数（别名）
        {VAR_I2, "int2_t", 1}, // 2 位整数（自定义扩展）
        {VAR_I3, "int3_t", 1}, // 3 位整数（自定义扩展）
        {VAR_I4, "int4_t", 1}, // 4 位整数（自定义扩展）

        {VAR_INT, "int", 4},       // int 占 4 字节
        {VAR_FLOAT, "float", 4},   // float 占 4 字节
        {VAR_DOUBLE, "double", 8}, // double 占 8 字节

        {VAR_I8, "int8_t", 1},   // 8 位整数
        {VAR_I16, "int16_t", 2}, // 16 位整数
        {VAR_I32, "int32_t", 4}, // 32 位整数
        {VAR_I64, "int64_t", 8}, // 64 位整数

        {VAR_U8, "uint8_t", 1},   // 无符号 8 位整数
        {VAR_U16, "uint16_t", 2}, // 无符号 16 位整数
        {VAR_U32, "uint32_t", 4}, // 无符号 32 位整数
        {VAR_U64, "uint64_t", 8}, // 无符号 64 位整数

        {VAR_INTPTR, "intptr_t", sizeof(void *)},   // 指针大小的有符号整数
        {VAR_UINTPTR, "uintptr_t", sizeof(void *)}, // 指针大小的无符号整数
        {FUNCTION_PTR, "funcptr", sizeof(void *)},  // 函数指针，大小与指针相同
};

// 打开解析器
int parse_open(parse_t **pparse) {
    if (!pparse)
        return -EINVAL; // 检查输入指针是否为空，返回非法参数错误

    // 为解析器结构体分配内存，并初始化为 0
    parse_t *parse = calloc(1, sizeof(parse_t));
    if (!parse)
        return -EINVAL; // 分配失败，返回错误

    // 创建一个抽象语法树(AST)
    if (ast_open(&parse->ast) < 0) {
        loge("\n");
        return -1; // AST 创建失败
    }

    int i;
    // 将所有的基本类型（int, char, float 等）注册到 AST 中
    for (i = 0; i < sizeof(base_types) / sizeof(base_types[0]); i++) {
        ast_add_base_type(parse->ast, &base_types[i]);
    }

    // 初始化 DFA（用于词法/语法分析）
    if (parse_dfa_init(parse) < 0) {
        loge("\n");
        return -1;
    }
    // 创建符号表向量，用于存储变量、函数符号
    parse->symtab = vector_alloc();
    if (!parse->symtab)
        return -1;
    // 创建全局常量向量
    parse->global_consts = vector_alloc();
    if (!parse->global_consts)
        goto const_error; // 如果创建失败，跳转释放符号表
                          // 创建 DWARF 调试信息结构
    parse->debug = dwarf_debug_alloc();
    if (!parse->debug)
        goto debug_error; // 如果失败，释放全局常量和符号表

    *pparse = parse; // 将创建好的解析器指针返回给调用者
    return 0;        // 成功返回 0

debug_error:
    vector_free(parse->global_consts); // 释放全局常量向量
const_error:
    vector_free(parse->symtab); // 释放符号表
    return -1;
}

/*
总结：

释放解析器内存

简单的资源管理，未释放 AST、符号表等（假设其他地方会释放）
*/
int parse_close(parse_t *parse) {
    if (parse) {
        free(parse);  // 释放解析器结构体
        parse = NULL; // 避免悬空指针
    }

    return 0; // 成功返回
}

/*
总结：

用于在符号表向量中查找符号

按符号名字进行比较

常用于 vector_find_cmp 回调函数
*/
static int _find_sym(const void *v0, const void *v1) {
    const char *name = v0;
    const elf_sym_t *sym = v1;

    if (!sym->name)
        return -1; // 如果符号没有名字，返回 -1

    return strcmp(name, sym->name); // 按名字比较符号
}

/*
总结：

向解析器的符号表添加 ELF 符号

支持名字查重，如果已存在则不重复添加

包含符号属性（大小、地址、段索引、类型）

分配内存失败时返回错误
*/
static int _parse_add_sym(parse_t *parse, const char *name,
                          uint64_t st_size, Elf64_Addr st_value,
                          uint16_t st_shndx, uint8_t st_info) {
    elf_sym_t *sym = NULL;
    elf_sym_t *sym2 = NULL;
    // 如果名字不为空，则查找符号表中是否已存在
    if (name)
        sym = vector_find_cmp(parse->symtab, name, _find_sym);

    if (!sym) { // 不存在，则创建新符号
        sym = calloc(1, sizeof(elf_sym_t));
        if (!sym)
            return -ENOMEM;

        if (name) { // 复制符号名字
            sym->name = strdup(name);
            if (!sym->name) {
                free(sym);
                return -ENOMEM;
            }
        }

        // 设置符号信息
        sym->st_size = st_size;   // 符号大小
        sym->st_value = st_value; // 符号地址/偏移
        sym->st_shndx = st_shndx; // 符号所在段索引
        sym->st_info = st_info;   // 符号类型/属性

        // 添加到符号表向量
        int ret = vector_add(parse->symtab, sym);
        if (ret < 0) {
            if (sym->name)
                free(sym->name);
            free(sym);
            loge("\n");
            return ret;
        }
    }

    return 0; // 成功返回
}

// 语法分析文件
/*
总结：

打开文件或复用已打开词法器

将文件添加到 AST

循环从词法器取词，并调用 DFA 解析

忽略空格、分号和 EOF

解析结束后关闭文件
*/
int parse_file(parse_t *parse, const char *path) {
    if (!parse || !path)
        return -EINVAL; // 检查参数是否合法

    lex_t *lex = parse->lex_list;
    // 遍历已打开的词法器列表，看是否已经打开过该文件
    while (lex) {
        if (!strcmp(lex->file->data, path))
            break;

        lex = lex->next;
    }

    if (lex) {
        parse->lex = lex; // 如果已打开，直接使用
        return 0;
    }
    // 否则新建词法器
    if (lex_open(&parse->lex, path) < 0)
        return -1;
    // 将文件添加到 AST 的文件块列表
    ast_add_file_block(parse->ast, path);
    // 将新打开的词法器插入到 lex_list 前端
    parse->lex->next = parse->lex_list;
    parse->lex_list = parse->lex;

    dfa_data_t *d = parse->dfa_data; // 获取 DFA 分析数据
    lex_word_t *w = NULL;

    int ret = 0;
    // 循环解析每个词法单元
    while (1) {
        ret = lex_pop_word(parse->lex, &w); // 从词法器中取词
        if (ret < 0) {
            loge("lex pop word failed\n");
            break;
        }
        // 如果遇到文件结束符，结束解析
        if (LEX_WORD_EOF == w->type) {
            logi("eof\n\n");
            lex_push_word(parse->lex, w); // 将 EOF 放回词法器
            w = NULL;
            break;
        }
        // 忽略空白和分号
        if (LEX_WORD_SPACE == w->type) {
            lex_word_free(w);
            w = NULL;
            continue;
        }

        if (LEX_WORD_SEMICOLON == w->type) {
            lex_word_free(w);
            w = NULL;
            continue;
        }
        // 确保当前没有未完成表达式
        assert(!d->expr);
        // 使用 DFA 解析当前词
        ret = dfa_parse_word(parse->dfa, w, d);
        if (ret < 0)
            break;
    }

    fclose(parse->lex->fp); // 关闭文件
    parse->lex->fp = NULL;
    return ret;
}

/*
用于在 DWARF abbrevs 向量中查找指定 Tag 的 abbrev

返回 0 表示匹配
*/
static int _debug_abbrev_find_by_tag(const void *v0, const void *v1) {
    const dwarf_uword_t tag = (dwarf_uword_t)(uintptr_t)v0;
    const dwarf_abbrev_declaration_t *d = v1;

    return tag != d->tag; // 按 DWARF Tag 查找
}

/*
总结：

查找 DWARF 中是否已生成对应类型的 info_entry

根据基本类型、结构体或指针类型选择不同的向量
*/
static dwarf_info_entry_t *_debug_find_type(parse_t *parse, type_t *t, int nb_pointers) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    dwarf_uword_t tag;
    vector_t *types;
    // 根据 nb_pointers 和 type 决定 DWARF 类型
    if (nb_pointers > 0) {
        tag = DW_TAG_pointer_type;
        types = parse->debug->base_types;

    } else if (t->type < STRUCT) {
        tag = DW_TAG_base_type;
        types = parse->debug->base_types;

    } else {
        tag = DW_TAG_structure_type;
        types = parse->debug->struct_types;
    }
    // 在 abbrevs 中找到对应 tag 的声明
    d = vector_find_cmp(parse->debug->abbrevs, (void *)(uintptr_t)tag, _debug_abbrev_find_by_tag);
    if (!d)
        return NULL;

    int i;
    int j;
    // 遍历已生成的类型，寻找匹配的类型条目
    for (i = 0; i < types->size; i++) {
        ie = types->data[i];

        if (ie->code != d->code)
            continue;

        assert(ie->attributes->size == d->attributes->size);

        if (ie->type == t->type && ie->nb_pointers == nb_pointers)
            return ie;
#if 0
		for (j = 0; j < d ->attributes->size; j++) {
			attr      = d ->attributes->data[j];

			if (DW_AT_name != attr->name)
				continue;

			iattr     = ie->attributes->data[j];

			if (!  string_cmp(iattr->data, t->name))
				return ie;
		}
#endif
    }

    return NULL;
}

/*
总结：

根据 type_t 和指针数生成 DWARF info_entry

自动选择对应 DWARF tag（base type / struct / pointer）

创建 info_entry 并填充所有 DWARF 属性（大小、编码、名字、声明位置、引用类型等）

用于调试信息生成
*/
static int __debug_add_type(dwarf_info_entry_t **pie, dwarf_abbrev_declaration_t **pad, parse_t *parse,
                            type_t *t, int nb_pointers, dwarf_info_entry_t *ie_type) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;
    vector_t *types;

    int ret;
    // 根据指针或类型选择 DWARF tag 和 abbrevs
    if (nb_pointers > 0) {
        d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_pointer_type, _debug_abbrev_find_by_tag);
        if (!d) {
            ret = dwarf_abbrev_add_pointer_type(parse->debug->abbrevs); // 添加 pointer type
            if (ret < 0) {
                loge("\n");
                return -1;
            }

            logd("abbrevs->size: %d\n", parse->debug->abbrevs->size);

            d = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];
        }

        types = parse->debug->base_types;

    } else if (t->type < STRUCT) {
        d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_base_type, _debug_abbrev_find_by_tag);
        if (!d) {
            ret = dwarf_abbrev_add_base_type(parse->debug->abbrevs);
            if (ret < 0) {
                loge("\n");
                return -1;
            }

            logd("abbrevs->size: %d\n", parse->debug->abbrevs->size);

            d = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];
        }

        types = parse->debug->base_types;

    } else {
        d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_structure_type, _debug_abbrev_find_by_tag);
        if (!d) {
            ret = dwarf_abbrev_add_struct_type(parse->debug->abbrevs);
            if (ret < 0) {
                loge("\n");
                return -1;
            }

            d = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];
            d->has_children = t->scope->vars->size > 0; // 结构体是否有成员
        }

        types = parse->debug->struct_types;
    }
    // 分配 info_entry
    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;

    ie->type = t->type;
    ie->nb_pointers = nb_pointers;
    // 添加到类型向量
    ret = vector_add(types, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }

    int j;
    // 为每个属性创建 DWARF 属性 entry
    for (j = 0; j < d->attributes->size; j++) {
        attr = d->attributes->data[j];

        iattr = calloc(1, sizeof(dwarf_info_attr_t));
        if (!iattr)
            return -ENOMEM;

        ret = vector_add(ie->attributes, iattr);
        if (ret < 0) {
            free(iattr);
            return ret;
        }

        iattr->name = attr->name;
        iattr->form = attr->form;
        // 根据属性名填充属性数据
        if (DW_AT_byte_size == iattr->name) {
            uint32_t byte_size;

            if (nb_pointers > 0)
                byte_size = sizeof(void *);
            else
                byte_size = t->size;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&byte_size, sizeof(byte_size));
            if (ret < 0)
                return ret;

        } else if (DW_AT_encoding == iattr->name) {
            uint8_t ate;

            if (VAR_CHAR == t->type || VAR_I8 == t->type)
                ate = DW_ATE_signed_char;

            else if (type_is_signed(t->type))
                ate = DW_ATE_signed;

            else if (VAR_U8 == t->type)
                ate = DW_ATE_unsigned_char;

            else if (type_is_unsigned(t->type))
                ate = DW_ATE_unsigned;

            else if (type_is_float(t->type))
                ate = DW_ATE_float;
            else {
                loge("\n");
                return -1;
            }

            ret = dwarf_info_fill_attr(iattr, &ate, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_name == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, t->name->data, t->name->len);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            uint8_t file = 1;

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&t->w->line, sizeof(t->w->line));
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            uint32_t type = 0;

            iattr->ref_entry = ie_type;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (DW_AT_sibling == iattr->name) {
            uint32_t type = 0;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            assert(0 == iattr->form);
        } else {
            loge("iattr->name: %d\n", iattr->name);
            return -1;
        }
    }

    *pie = ie; // 返回 info_entry
    *pad = d;  // 返回 abbrev
    return 0;
}

/*
总结：

作用：将结构体的单个成员变量映射到 DWARF 信息

支持：名字、类型、声明位置、偏移等属性

返回 info_entry 供父结构体引用
*/
static int __debug_add_member_var(dwarf_info_entry_t **pie, parse_t *parse, variable_t *v, dwarf_info_entry_t *ie_type) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;
    // 查找 DW_TAG_member 的 abbrev declaration（表示结构体成员）
    d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_member, _debug_abbrev_find_by_tag);
    if (!d) {
        // 如果不存在，则添加新的 member abbrev
        ret = dwarf_abbrev_add_member_var(parse->debug->abbrevs);
        if (ret < 0) {
            loge("\n");
            return -1;
        }

        d = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];
    }
    // 分配 info_entry 对象表示该成员
    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;
    // 将成员 info_entry 加入到 struct_types 向量
    ret = vector_add(parse->debug->struct_types, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }

    // 遍历该 abbrev 的属性，逐一填充 info_entry 属性
    int j;
    for (j = 0; j < d->attributes->size; j++) {
        attr = d->attributes->data[j];

        iattr = calloc(1, sizeof(dwarf_info_attr_t)); // 分配属性对象
        if (!iattr)
            return -ENOMEM;

        ret = vector_add(ie->attributes, iattr); // 添加到 info_entry
        if (ret < 0) {
            free(iattr);
            return ret;
        }

        iattr->name = attr->name;
        iattr->form = attr->form;
        // 根据 DWARF 属性类型填充数据
        if (DW_AT_name == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, v->w->text->data, v->w->text->len); // 成员名字
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            uint8_t file = 1; // 简化处理，文件编号

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&v->w->line, sizeof(v->w->line)); // 声明行号
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            uint32_t type = 0;

            iattr->ref_entry = ie_type; // 类型引用

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (DW_AT_data_member_location == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&v->offset, sizeof(v->offset));
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            assert(0 == iattr->form);
        } else {
            loge("iattr->name: %d\n", iattr->name);
            return -1;
        }
    }

    *pie = ie; // 返回生成的 info_entry
    return 0;
}

static int _debug_add_type(dwarf_info_entry_t **pie, parse_t *parse, type_t *t, int nb_pointers);

/*
总结：

添加一个结构体类型到 DWARF

为每个成员生成 info_entry

支持成员是指向自身的指针（递归结构体）

生成结构体结束 entry
*/
static int _debug_add_struct_type(dwarf_info_entry_t **pie, dwarf_abbrev_declaration_t **pad, parse_t *parse, type_t *t) {
    //	  dwarf_abbrev_declaration_t* d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;
    int i;
    // 保存结构体成员类型对应的 info_entry
    vector_t *ie_member_types = vector_alloc();
    if (!ie_member_types)
        return -ENOMEM;

    int nb_pointers = 0; // 记录结构体成员中最大指针层数

    for (i = 0; i < t->scope->vars->size; i++) {
        // 查找是否已有该类型的 DWARF info_entry
        dwarf_info_entry_t *ie_member;
        variable_t *v_member;
        type_t *t_member;

        v_member = t->scope->vars->data[i];
        ret = ast_find_type_type(&t_member, parse->ast, v_member->type); // 查找成员类型
        if (ret < 0)
            return ret;

        ie_member = _debug_find_type(parse, t_member, v_member->nb_pointers);
        if (!ie_member) {
            if (t_member != t) {
                ret = _debug_add_type(&ie_member, parse, t_member, v_member->nb_pointers);
                if (ret < 0) {
                    loge("\n");
                    return ret;
                }
            } else {
                assert(v_member->nb_pointers > 0);

                if (nb_pointers < v_member->nb_pointers)
                    nb_pointers = v_member->nb_pointers;
            }
        }

        if (vector_add(ie_member_types, ie_member) < 0)
            return -ENOMEM;
    }

    assert(ie_member_types->size == t->scope->vars->size);
    // 创建结构体本身的 info_entry
    ret = __debug_add_type(&ie, pad, parse, t, 0, NULL);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    vector_t *refills = NULL;

    if (nb_pointers > 0) { // 如果有指针成员，后续需要填充 ref_entry
        refills = vector_alloc();
        if (!refills)
            return -ENOMEM;
    }
    // 添加每个成员变量到 DWARF
    for (i = 0; i < t->scope->vars->size; i++) {
        // 生成结构体结束标志 entry
        dwarf_info_entry_t *ie_member;
        variable_t *v_member;

        v_member = t->scope->vars->data[i];

        ret = __debug_add_member_var(&ie_member, parse, v_member, ie_member_types->data[i]);
        if (ret < 0) {
            loge("\n");
            return ret;
        }

        if (nb_pointers > 0) {
            if (vector_add(refills, ie_member) < 0) {
                return -ENOMEM;
            }
        }
    }

    dwarf_info_entry_t *ie0;

    ie0 = dwarf_info_entry_alloc();
    if (!ie0)
        return -ENOMEM;
    ie0->code = 0;

    if (vector_add(parse->debug->struct_types, ie0) < 0) {
        dwarf_info_entry_free(ie0);
        return -ENOMEM;
    }
    // 处理结构体成员是自身类型的指针的情况
    if (nb_pointers > 0) {
        dwarf_info_entry_t *ie_pointer;
        dwarf_info_attr_t *iattr;

        int j;
        for (j = 1; j <= nb_pointers; j++) {
            ret = _debug_add_type(&ie_pointer, parse, t, j); // 添加指针类型
            if (ret < 0) {
                loge("\n");
                return ret;
            }
            // 更新成员 info_entry 的类型引用
            for (i = 0; i < t->scope->vars->size; i++) {
                dwarf_info_entry_t *ie_member;
                variable_t *v_member;

                v_member = t->scope->vars->data[i];

                if (v_member->type != t->type)
                    continue;

                if (v_member->nb_pointers != j)
                    continue;

                ie_member = refills->data[i];

                int k;
                for (k = 0; k < ie_member->attributes->size; k++) {
                    iattr = ie_member->attributes->data[k];

                    if (DW_AT_type == iattr->name) {
                        assert(!iattr->ref_entry);
                        iattr->ref_entry = ie_pointer;
                        break;
                    }
                }
            }
        }
    }

    *pie = ie;
    return 0;
}

/*
总结：

负责生成任何类型（基本类型 / 结构体 / 指针）

优先复用已存在 info_entry

支持多层指针
*/
static int _debug_add_type(dwarf_info_entry_t **pie, parse_t *parse, type_t *t, int nb_pointers) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;
    int i;

    *pie = _debug_find_type(parse, t, nb_pointers);
    if (*pie)
        return 0; // 已存在直接返回

    ie = _debug_find_type(parse, t, 0); // 查找非指针类型
    if (!ie) {
        if (t->type < STRUCT) { // 基本类型
            ret = __debug_add_type(&ie, &d, parse, t, 0, NULL);
            if (ret < 0)
                return ret;

        } else { // 结构体
            ret = _debug_add_struct_type(&ie, &d, parse, t);
            if (ret < 0)
                return ret;

            *pie = _debug_find_type(parse, t, nb_pointers);
            if (*pie)
                return 0;
        }
    }

    if (0 == nb_pointers) {
        *pie = ie;
        return 0;
    }

    return __debug_add_type(pie, &d, parse, t, nb_pointers, ie); // 添加指针类型
}

/*
总结：

将 AST 中变量映射为 DWARF variable entry

填充名字、类型引用、声明位置、栈偏移等信息

避免重复创建同名变量 entry

支持局部变量偏移（DW_OP_fbreg）
*/
static int _debug_add_var(parse_t *parse, node_t *node) {
    variable_t *var = node->var;
    type_t *t = NULL;
    // 查找变量的类型对象
    int ret = ast_find_type_type(&t, parse->ast, var->type);
    if (ret < 0)
        return ret;
    // 查找或创建类型对应的 DWARF info_entry
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_declaration_t *d2;
    dwarf_abbrev_declaration_t *subp;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_entry_t *ie2;
    dwarf_info_attr_t *iattr;

    int i;
    int j;

    ie = _debug_find_type(parse, t, var->nb_pointers);
    if (!ie) {
        ret = _debug_add_type(&ie, parse, t, var->nb_pointers);
        if (ret < 0) {
            loge("\n");
            return -1;
        }
    }

    logd("ie: %p, t->name: %s\n", ie, t->name->data);
    // 查找 DW_TAG_subprogram（函数）和 DW_TAG_variable abbrev
    subp = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_subprogram, _debug_abbrev_find_by_tag);
    assert(subp);

    d2 = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_variable, _debug_abbrev_find_by_tag);
    if (!d2) {
        ret = dwarf_abbrev_add_var(parse->debug->abbrevs);
        if (ret < 0) {
            loge("\n");
            return -1;
        }

        d2 = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];

        assert(DW_TAG_variable == d2->tag);
    }
    // 遍历 debug infos，看变量是否已存在
    for (i = parse->debug->infos->size - 1; i >= 0; i--) {
        ie2 = parse->debug->infos->data[i];

        if (ie2->code == subp->code)
            break; // 遇到函数结束停止

        if (ie2->code != d2->code)
            continue;

        assert(ie2->attributes->size == d2->attributes->size);

        for (j = 0; j < d2->attributes->size; j++) {
            attr = d2->attributes->data[j];

            if (DW_AT_name != attr->name)
                continue;

            iattr = ie2->attributes->data[j];

            if (!strcmp(iattr->data->data, var->w->text->data)) {
                logd("find var: %s\n", var->w->text->data);
                return 0; // 已存在
            }
        }
    }
    // 创建新的变量 info_entry
    ie2 = dwarf_info_entry_alloc();
    if (!ie2)
        return -ENOMEM;
    ie2->code = d2->code;

    ret = vector_add(parse->debug->infos, ie2);
    if (ret < 0) {
        dwarf_info_entry_free(ie2);
        return ret;
    }
    // 填充 DWARF 属性
    for (j = 0; j < d2->attributes->size; j++) {
        attr = d2->attributes->data[j];

        iattr = calloc(1, sizeof(dwarf_info_attr_t));
        if (!iattr)
            return -ENOMEM;

        int ret = vector_add(ie2->attributes, iattr);
        if (ret < 0) {
            free(iattr);
            return ret;
        }

        iattr->name = attr->name;
        iattr->form = attr->form;

        if (DW_AT_name == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, var->w->text->data, var->w->text->len);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            uint8_t file = 1;

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&var->w->line, sizeof(var->w->line));
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            uint32_t type = 0;

            iattr->ref_entry = ie;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(uint32_t));
            if (ret < 0)
                return ret;

        } else if (DW_AT_location == iattr->name) {
            // 局部变量，使用 DW_OP_fbreg 表示相对于帧指针的偏移
            if (!var->local_flag) {
                loge("var: %s\n", var->w->text->data);
                return -1;
            }

            logd("var->bp_offset: %d, var: %s\n", var->bp_offset, var->w->text->data);

            uint8_t buf[64];

            buf[sizeof(dwarf_uword_t)] = DW_OP_fbreg;

            size_t len = leb128_encode_int32(var->bp_offset,
                                             buf + sizeof(dwarf_uword_t) + 1,
                                             sizeof(buf) - sizeof(dwarf_uword_t) - 1);
            assert(len > 0);

            *(dwarf_uword_t *)buf = 1 + len;

            ret = dwarf_info_fill_attr(iattr, buf, sizeof(dwarf_uword_t) + 1 + len);
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            assert(0 == iattr->form);
        } else {
            loge("\n");
            return -1;
        }
    }

    return 0;
}

/**
 * 将三地址码列表转换为机器指令并填充到代码字符串中
 * 同时生成DWARF行号调试信息
 *
 * @param code 目标代码输出缓冲区
 * @param h 三地址码列表头
 * @param offset 当前代码段的偏移量
 * @param parse 解析器上下文
 * @param f 当前函数信息
 * @return 成功返回0，失败返回错误码
 */
static int _fill_code_list_inst(string_t *code, list_t *h, int64_t offset, parse_t *parse, function_t *f) {
    list_t *l;
    node_t *node;

    uint32_t line = 0;  // 当前行号
    uint32_t line2 = 0; // 临时行号
                        // 遍历三地址码列表
    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        _3ac_code_t *c = list_data(l, _3ac_code_t, list);

        if (!c->instructions)
            continue;

        int i;
        int ret;
        // 将每条指令的代码追加到输出缓冲区
        for (i = 0; i < c->instructions->size; i++) {
            instruction_t *inst = c->instructions->data[i];

            ret = string_cat_cstr_len(code, inst->code, inst->len);
            if (ret < 0)
                return ret;
        }
        // 处理目标操作数的调试信息
        if (c->dsts) {
            _3ac_operand_t *dst;

            for (i = 0; i < c->dsts->size; i++) {
                dst = c->dsts->data[i];

                if (!dst->dag_node || !dst->dag_node->node)
                    continue;

                node = dst->dag_node->node;
                // 如果有调试信息，更新行号并添加局部变量到调试信息
                if (node->debug_w) {
                    if (line2 < node->debug_w->line)
                        line2 = node->debug_w->line;

                    if (type_is_var(node->type) && node->var->local_flag) {
                        ret = _debug_add_var(parse, node);
                        if (ret < 0)
                            return ret;
                    }
                }
            }
        }
        // 处理源操作数的调试信息（与目标操作数类似）
        if (c->srcs) {
            _3ac_operand_t *src;

            for (i = 0; i < c->srcs->size; i++) {
                src = c->srcs->data[i];

                if (!src->dag_node || !src->dag_node->node)
                    continue;

                node = src->dag_node->node;

                if (node->debug_w) {
                    if (line2 < node->debug_w->line)
                        line2 = node->debug_w->line;

                    if (type_is_var(node->type) && node->var->local_flag) {
                        ret = _debug_add_var(parse, node);
                        if (ret < 0)
                            return ret;
                    }
                }
            }
        }
        // 如果检测到新的行号，创建DWARF行号记录
        if (line2 > line) {
            line = line2;

            dwarf_line_result_t *r = calloc(1, sizeof(dwarf_line_result_t));
            if (!r)
                return -ENOMEM;

            r->file_name = string_clone(f->node.w->file);
            if (!r->file_name) {
                free(r);
                return -ENOMEM;
            }

            r->address = offset; // 代码地址
            r->line = line;      // 源代码行号
            r->is_stmt = 1;      // 表示这是可执行语句
                                 // 添加到行号表
            ret = vector_add(parse->debug->lines, r);
            if (ret < 0) {
                string_free(r->file_name);
                free(r);
                return ret;
            }
        }

        offset += c->inst_bytes; // 更新偏移量
    }

    return 0;
}

/**
 * 为函数添加DWARF子程序调试信息
 *
 * @param pie 输出的调试信息条目
 * @param parse 解析器上下文
 * @param f 函数信息
 * @param offset 函数代码的偏移量
 * @return 成功返回0，失败返回错误码
 */
static int _debug_add_subprogram(dwarf_info_entry_t **pie, parse_t *parse, function_t *f, int64_t offset) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_entry_t *ie2;
    dwarf_info_attr_t *iattr;
    // 查找子程序的DWARF缩写声明
    d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_subprogram, _debug_abbrev_find_by_tag);
    if (!d) {
        loge("\n");
        return -1;
    }

    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;

    int ret = vector_add(parse->debug->infos, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }
    // 为子程序添加所有必要的属性
    int j;
    for (j = 0; j < d->attributes->size; j++) {
        attr = d->attributes->data[j];

        iattr = calloc(1, sizeof(dwarf_info_attr_t));
        if (!iattr)
            return -ENOMEM;

        ret = vector_add(ie->attributes, iattr);
        if (ret < 0) {
            free(iattr);
            return ret;
        }

        iattr->name = attr->name;
        iattr->form = attr->form;
        // 根据属性类型填充不同的值
        if (DW_AT_external == iattr->name) {
            uint8_t value = 1; // 标记为外部函数

            ret = dwarf_info_fill_attr(iattr, &value, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_name == iattr->name) {
            // 函数名
            ret = dwarf_info_fill_attr(iattr, f->signature->data, f->signature->len + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            // 声明文件
            logd("f->node.w->file->data: %s\n", f->node.w->file->data);

            string_t *s;
            int k;
            // 在文件名字符串表中查找文件索引
            logd("parse->debug->file_names->size: %d\n", parse->debug->file_names->size);

            for (k = 0; k < parse->debug->file_names->size; k++) {
                s = parse->debug->file_names->data[k];

                logd("s->data: %s\n", s->data);

                if (!strcmp(s->data, f->node.w->file->data))
                    break;
            }
            assert(k < parse->debug->file_names->size);
            assert(k < 254);

            uint8_t file = k + 1; // 文件索引（从1开始）

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            // 声明行号
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&(f->node.w->line), sizeof(f->node.w->line));
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            // 返回类型
            uint32_t type = 0;
            variable_t *v = f->rets->data[0];
            type_t *t = NULL;

            ret = ast_find_type_type(&t, parse->ast, v->type);
            if (ret < 0)
                return ret;

            // 查找或创建类型调试信息
            ie2 = _debug_find_type(parse, t, v->nb_pointers);
            if (!ie2) {
                ret = _debug_add_type(&ie2, parse, t, v->nb_pointers);
                if (ret < 0) {
                    loge("\n");
                    return ret;
                }
            }

            iattr->ref_entry = ie2; // 设置类型引用

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (DW_AT_low_pc == iattr->name
                   || DW_AT_high_pc == iattr->name) {
            // 函数的起始和结束地址
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&offset, sizeof(offset));
            if (ret < 0)
                return ret;

        } else if (DW_AT_frame_base == iattr->name) {
            // 帧基址（使用CFA）
            uint8_t buf[64];

            buf[sizeof(dwarf_uword_t)] = DW_OP_call_frame_cfa;

            *(dwarf_uword_t *)buf = 1;

            ret = dwarf_info_fill_attr(iattr, buf, sizeof(dwarf_uword_t) + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_GNU_all_call_sites == iattr->name) {
            // GNU扩展属性，跳过
        } else if (DW_AT_sibling == iattr->name) {
            // 兄弟节点引用
            uint32_t type = 0;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            // 属性列表结束标记
            assert(0 == iattr->form);
        } else {
            // 未知属性
            loge("iattr->name: %d\n", iattr->name);
            return -1;
        }
    }

    *pie = ie;
    return 0;
}

/**
 * 添加DWARF编译单元调试信息
 *
 * @param pie 输出的调试信息条目
 * @param parse 解析器上下文
 * @param f 函数信息（用于获取文件信息）
 * @param offset 代码偏移量
 * @return 成功返回0，失败返回错误码
 */
static int _debug_add_cu(dwarf_info_entry_t **pie, parse_t *parse, function_t *f, int64_t offset) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;
    // 查找编译单元的DWARF缩写声明
    d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_compile_unit, _debug_abbrev_find_by_tag);
    if (!d) {
        loge("\n");
        return -1;
    }

    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;

    int ret = vector_add(parse->debug->infos, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }
    // 为编译单元添加属性
    int j;
    for (j = 0; j < d->attributes->size; j++) {
        attr = d->attributes->data[j];

        iattr = calloc(1, sizeof(dwarf_info_attr_t));
        if (!iattr)
            return -ENOMEM;

        ret = vector_add(ie->attributes, iattr);
        if (ret < 0) {
            free(iattr);
            return ret;
        }

        iattr->name = attr->name;
        iattr->form = attr->form;

        if (DW_AT_producer == iattr->name) {
            // 编译器信息
            char *producer = "GNU C11 7.4.0 -mtune=generic -march=x86-64 -g -fstack-protector-strong";

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)producer, strlen(producer));
            if (ret < 0)
                return ret;

        } else if (DW_AT_language == iattr->name) {
            // 编程语言（C语言）
            uint8_t language = 12; // DW_LANG_C99

            ret = dwarf_info_fill_attr(iattr, &language, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_name == iattr->name) {
            // 源文件名
            string_t *fname = f->node.w->file;

            ret = dwarf_info_fill_attr(iattr, fname->data, fname->len + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_comp_dir == iattr->name) {
            // 编译目录
            uint8_t buf[4096];

            uint8_t *dir = getcwd(buf, sizeof(buf) - 1);
            assert(dir);

            ret = dwarf_info_fill_attr(iattr, dir, strlen(dir) + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_low_pc == iattr->name
                   || DW_AT_high_pc == iattr->name) {
            // 代码范围
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&offset, sizeof(offset));
            if (ret < 0)
                return ret;

        } else if (DW_AT_stmt_list == iattr->name) {
            // 行号表偏移（暂设为0）
            dwarf_uword_t stmt_list = 0;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&stmt_list, sizeof(stmt_list));
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            // 属性列表结束
            assert(0 == iattr->form);
        } else {
            // 未知属性
            loge("iattr->name: %d, %s\n", iattr->name, dwarf_find_attribute(iattr->name));
            return -1;
        }
    }

    *pie = ie;
    return 0;
}

/**
 * 填充函数的机器指令并设置调试信息
 *
 * @param code 目标代码输出缓冲区
 * @param f 函数信息
 * @param offset 函数在代码段中的偏移量
 * @param parse 解析器上下文
 * @return 成功返回0，失败返回错误码
 */
static int _fill_function_inst(string_t *code, function_t *f, int64_t offset, parse_t *parse) {
    list_t *l;
    int ret;
    int i;

    dwarf_abbrev_declaration_t *abbrev0 = NULL;
    dwarf_info_entry_t *subp = NULL;
    dwarf_info_entry_t *ie0 = NULL;
    // 为函数添加子程序调试信息
    ret = _debug_add_subprogram(&subp, parse, f, offset);
    if (ret < 0)
        return ret;

    f->code_bytes = 0; // 重置函数代码字节数
                       // 处理函数的初始化代码
    if (f->init_code) {
        for (i = 0; i < f->init_code->instructions->size; i++) {
            instruction_t *inst = f->init_code->instructions->data[i];

            ret = string_cat_cstr_len(code, inst->code, inst->len);
            if (ret < 0)
                return ret;

            f->code_bytes += inst->len;
        }
    }
    // 遍历所有基本块，生成代码
    for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
         l = list_next(l)) {
        basic_block_t *bb = list_data(l, basic_block_t, list);

        ret = _fill_code_list_inst(code, &bb->code_list_head, offset + f->code_bytes, parse, f);
        if (ret < 0)
            return ret;

        f->code_bytes += bb->code_bytes;
    }
    // 对齐到8字节边界
#if 1
    if (f->code_bytes & 0x7) {
        size_t n = 8 - (f->code_bytes & 0x7);

        ret = string_fill_zero(code, n);
        if (ret < 0)
            return ret;

        f->code_bytes += n;
    }
#endif
    uint64_t high_pc_ = f->code_bytes; // 函数结束地址
// 宏：更新调试信息中的high_pc属性
#define DEBUG_UPDATE_HIGH_PC(ie, high_pc)                                                \
    do {                                                                                 \
        dwarf_info_attr_t *iattr;                                                        \
        int i;                                                                           \
                                                                                         \
        for (i = 0; i < ie->attributes->size; i++) {                                     \
            iattr = ie->attributes->data[i];                                             \
                                                                                         \
            if (DW_AT_high_pc == iattr->name) {                                          \
                ret = dwarf_info_fill_attr(iattr, (uint8_t *)&high_pc, sizeof(high_pc)); \
                if (ret < 0)                                                             \
                    return ret;                                                          \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (0)
    // 更新子程序的high_pc
    DEBUG_UPDATE_HIGH_PC(subp, high_pc_);
    // 添加调试信息条目结束标记

#if 1
    ie0 = dwarf_info_entry_alloc();
    if (!ie0)
        return -ENOMEM;
    ie0->code = 0; // 0表示条目结束

    if (vector_add(parse->debug->infos, ie0) < 0) {
        dwarf_info_entry_free(ie0);
        return -ENOMEM;
    }
#endif
    return 0;
}

/**
 * 向重定位表添加重定位项
 *
 * @param relas 重定位表向量
 * @param parse 解析器上下文
 * @param r 重定位信息
 * @param name 符号名
 * @param st_shndx 符号所在的段索引
 * @return 成功返回0，失败返回错误码
 */
static int _parse_add_rela(vector_t *relas, parse_t *parse, rela_t *r, const char *name, uint16_t st_shndx) {
    elf_rela_t *rela;

    int ret;
    int i;
    // 在符号表中查找符号
    for (i = 0; i < parse->symtab->size; i++) {
        elf_sym_t *sym = parse->symtab->data[i];

        if (!sym->name)
            continue;

        if (!strcmp(name, sym->name))
            break;
    }
    // 如果符号不存在，添加到符号表
    if (i == parse->symtab->size) {
        ret = _parse_add_sym(parse, name, 0, 0, st_shndx, ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE));
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    logd("rela: %s, offset: %ld\n", name, r->text_offset);
    // 创建重定位项
    rela = calloc(1, sizeof(elf_rela_t));
    if (!rela)
        return -ENOMEM;

    rela->name = (char *)name;
    rela->r_offset = r->text_offset;             // 重定位位置
    rela->r_info = ELF64_R_INFO(i + 1, r->type); // 符号索引和重定位类型
    rela->r_addend = r->addend;                  // 加数

    ret = vector_add(relas, rela);
    if (ret < 0) {
        loge("\n");
        free(rela);
        return ret;
    }

    return 0;
}

// 将变量数据填充到 .data 或 .rodata 区段
// 总结：该函数将一个变量在 ELF 段中布局，并生成对应符号
/*
功能：

将变量 v 的数据填充到指定的 data 字符串（通常是 ELF 数据段 buffer）

处理局部变量、全局变量、常量字符串、数组、结构体等

生成符号表条目 _parse_add_sym
*/
static int _fill_data(parse_t *parse, variable_t *v, string_t *data, uint32_t shndx) {
    char *name;
    int size;
    uint8_t *v_data;

    // 1. 确定变量名字
    if (v->global_flag) {
        name = v->w->text->data;
    } else
        name = v->signature->data;

    logd("v_%d_%d/%s, nb_dimentions: %d\n", v->w->line, v->w->pos, v->w->text->data, v->nb_dimentions);

    // 2. 确定变量数据与大小
    // 常量字符串
    if (variable_const_string(v)) {
        size = v->data.s->len + 1;
        v_data = v->data.s->data;

    } else if (v->nb_dimentions > 0) { // 数组
        size = variable_size(v);
        v_data = v->data.p;

    } else if (v->type >= STRUCT) { // 结构体
        size = v->size;
        v_data = v->data.p;
    } else { // 普通标量
        size = v->size;
        v_data = (uint8_t *)&v->data;
    }

    // align 8 bytes
    // 8 字节对齐
    int fill_size = (data->len + 7) >> 3 << 3;
    fill_size -= data->len;

    int ret;

    if (fill_size > 0) {
        ret = string_fill_zero(data, fill_size);
        if (ret < 0)
            return ret;
    }
    assert(0 == (data->len & 0x7));

    // 设置变量在段中的偏移
    v->ds_offset = data->len;

    uint64_t stb;
    if (v->static_flag)
        stb = STB_LOCAL;
    else
        stb = STB_GLOBAL;

    // 生成符号表信息
    ret = _parse_add_sym(parse, name, size, data->len, shndx, ELF64_ST_INFO(stb, STT_OBJECT));
    if (ret < 0)
        return ret;

    // 将变量数据写入 data buffer,如果 v_data 为NULL，则填零，否则直接拷贝
    if (!v_data)
        ret = string_fill_zero(data, size);
    else
        ret = string_cat_cstr_len(data, v_data, size);

    return ret;
}

// 生成全局常量、.rodata 数据段及重定位
/*
功能：
遍历 AST中的全局重定位信息(global_relas)

将常量变量或字符串加入 .rodata 段

去重已有常量(相同内容复用偏移)

生成对应的 ELF 重定位表(.rela.data)
*/
static int _parse_add_data_relas(parse_t *parse, elf_context_t *elf) {
    elf_rela_t *rela;
    ast_rela_t *r;
    function_t *f;
    variable_t *v;
    variable_t *v2;
    vector_t *relas;  // 重定位表
    string_t *rodata; // 只读数据段内容

    int ret;
    int i;
    int j;

    // 1. 收集全局常量
    // 遍历 AST 中的全局重定位信息，收集所有全局常量
    for (i = 0; i < parse->ast->global_relas->size; i++) {
        r = parse->ast->global_relas->data[i];
        v = r->obj->base;

        // 收集字符串常量和其他非函数指针的常量
        if (variable_const_string(v)
            || (variable_const(v) && FUNCTION_PTR != v->type)) {
            if (vector_add_unique(parse->global_consts, v) < 0)
                return -ENOMEM;
        }
    }

    // 创建 .rodata 段缓冲区
    rodata = string_alloc();
    if (!rodata)
        return -ENOMEM;

    // 2. 处理全局常量：去重和数据填充
    for (i = 0; i < parse->global_consts->size; i++) {
        v = parse->global_consts->data[i];
        // 去重：检查是否已经存在相同的常量
        for (j = 1; j < i; j++) {
            v2 = parse->global_consts->data[j];

            if (v2 == v) // 同一个变量，跳过
                break;

            // 类型和大小必须相同
            if (v2->type != v->type || v2->size != v->size)
                continue;

            // 字符串常量比较内容
            if (variable_const_string(v)) {
                assert(variable_const_string(v2));

                if (!string_cmp(v->data.s, v2->data.s))
                    break;
                continue;
            }

            // 其它常量比较二进制数据
            if (!memcmp(&v->data, &v2->data, v->size))
                break;
        }

        // 如果找到重复的常量，使用已存在的那个
        if (j < i) {
            if (v2 != v)
                v2->ds_offset = v->ds_offset; // 更新偏移引用
            continue;
        }

        v->global_flag = 1; // 标记为全局

        // 3. 将常量数据填充到.rodata段
        ret = _fill_data(parse, v, rodata, SHNDX_RODATA);
        if (ret < 0) {
            string_free(rodata);
            return ret;
        }
    }

    /* 4. 生成 .rela.data 重定位段
     * 遍历所有global_relas，生成重定位信息：
     *   r_offset: 引用位置的偏移量
     *   r_info: 符号索引 + 重定位类型
     *   r_addend: 被引用对象的偏移量
     */
    relas = vector_alloc();
    if (!relas) {
        string_free(rodata);
        return -ENOMEM;
    }

    // 遍历所有全局重定位关系
    for (i = 0; i < parse->ast->global_relas->size; i++) {
        r = parse->ast->global_relas->data[i];

        // 计算成员偏移量
        int offset0 = member_offset(r->ref); // 引用位置的偏移
        int offset1 = member_offset(r->obj); // 被引用对象的偏移
        char *name;
        int shndx;

        // 根据被引用对象的类型确定符号名和段索引
        if (variable_const(r->obj->base)) {
            if (FUNCTION_PTR == r->obj->base->type) {
                // 函数指针：引用.text段的函数
                f = r->obj->base->func_ptr;
                name = f->node.w->text->data;
                shndx = SHNDX_TEXT;

            } else {
                // 其他常量：引用.rodata段
                name = r->obj->base->w->text->data;
                shndx = SHNDX_RODATA;
            }
        } else if (variable_const_string(r->obj->base)) {
            // 字符串常量：引用.rodata段
            name = r->obj->base->w->text->data;
            shndx = SHNDX_RODATA;
        } else {
            // 其他变量：引用.data段
            name = r->obj->base->w->text->data;
            shndx = SHNDX_DATA;
        }

        // 在符号表中查找符号
        for (j = 0; j < parse->symtab->size; j++) {
            elf_sym_t *sym = parse->symtab->data[j];

            if (!sym->name)
                continue;

            if (!strcmp(name, sym->name))
                break;
        }

        // 如果符号不存在，添加到符号表
        if (j == parse->symtab->size) {
            ret = _parse_add_sym(parse, name, 0, 0, 0, ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE));
            if (ret < 0) {
                loge("\n");
                return ret;
            }
        }

        // 创建重定位项
        rela = calloc(1, sizeof(elf_rela_t));
        if (!rela)
            return -ENOMEM;

        rela->name = name;
        rela->r_offset = r->ref->base->ds_offset + offset0; // 引用位置在数据段中的偏移
        rela->r_info = ELF64_R_INFO(j + 1, R_X86_64_64);    // 符号索引 + 重定位类型
        rela->r_addend = offset1;                           // 被引用对象内部的偏移

        ret = vector_add(relas, rela);
        if (ret < 0) {
            loge("\n");
            goto error;
        }
    }

    // 5. 对齐.rodata段到8字节边界
    int fill_size = ((rodata->len + 7) >> 3 << 3) - rodata->len;

    if (fill_size > 0) {
        ret = string_fill_zero(rodata, fill_size);
        if (ret < 0)
            goto error;
    }

    ret = 0;

    // 6. 创建.rodata段
    elf_section_t ro = {0};

    ro.name = ".rodata";
    ro.sh_type = SHT_PROGBITS; // 程序数据段
    ro.sh_flags = SHF_ALLOC;   // 运行时需要分配内存
    ro.sh_addralign = 8;       // 8字节对齐
    ro.data = rodata->data;    // 段数据
    ro.data_len = rodata->len; // 段长度
    ro.index = SHNDX_RODATA;   // 段索引

    ret = elf_add_section(elf, &ro);
    if (ret < 0)
        goto error;

    // 7. 如果有重定位项，创建重定位段
    if (relas->size > 0) {
        elf_section_t s = {0};

        s.name = ".rela.data";
        s.sh_type = SHT_RELA;       // 重定位段类型
        s.sh_flags = SHF_INFO_LINK; // 包含链接信息
        s.sh_addralign = 8;         // 8字节对齐
        s.data = NULL;
        s.data_len = 0;
        s.sh_link = 0;          // 链接到符号表
        s.sh_info = SHNDX_DATA; // 应用于.data段

        // 如果是ARM32架构，调整重定位信息格式
        if (!strcmp(elf->ops->machine, "arm32")) {
            for (i = 0; i < relas->size; i++) {
                rela = relas->data[i];
                // 将64位重定位信息转换为32位格式
                rela->r_info = ELF32_R_INFO(ELF64_R_SYM(rela->r_info), ELF64_R_TYPE(rela->r_info));
            }
        }
        // 添加重定位段到ELF文件
        ret = elf_add_rela_section(elf, &s, relas);
    }
error:
    // 清理资源
    string_free(rodata);
    vector_clear(relas, (void (*)(void *))free);
    vector_free(relas);
    return ret;
}

/**
 * 创建并填充.data数据段
 * 处理全局变量（非外部声明）的数据初始化
 *
 * @param parse 解析器上下文
 * @param elf ELF上下文
 * @param global_vars 全局变量向量
 * @return 成功返回0，失败返回错误码
 */
static int _parse_add_ds(parse_t *parse, elf_context_t *elf, vector_t *global_vars) {
    variable_t *v;
    string_t *data; // .data 段数据缓冲区

    data = string_alloc();
    if (!data)
        return -ENOMEM;

    int ret = 0;
    int i;

    // 遍历所有全局变量
    for (i = 0; i < global_vars->size; i++) {
        v = global_vars->data[i];

        if (v->extern_flag) // 跳过外部声明的变量
            continue;

        // 将变量数据填充到 .data 段
        ret = _fill_data(parse, v, data, SHNDX_DATA);
        if (ret < 0)
            goto error;
    }

    // 对齐到 8 字节边界
    int fill_size = ((data->len + 7) >> 3 << 3) - data->len;
    if (fill_size > 0) {
        ret = string_fill_zero(data, fill_size);
        if (ret < 0) {
            ret = -ENOMEM;
            goto error;
        }
    }
    assert(0 == (data->len & 0x7)); // 确保 8 字节对齐

    // 创建 .data 段
    elf_section_t ds = {0};
    ds.name = ".data";
    ds.sh_type = SHT_PROGBITS;           // 程序数据
    ds.sh_flags = SHF_ALLOC | SHF_WRITE; // 可分配、可写
    ds.sh_addralign = 8;                 // 8 字节对齐
    ds.data = data->data;                // 段数据
    ds.data_len = data->len;             // 段长度
    ds.index = SHNDX_DATA;               // 段索引

    ret = elf_add_section(elf, &ds);
    if (ret < 0) {
        loge("\n");
        goto error;
    }

error:
    string_free(data);
    return ret;
}

/**
 * 添加调试信息段到ELF文件
 */
static int _add_debug_section(elf_context_t *elf, const char *name, const string_t *bin, uint32_t shndx) {
    elf_section_t s = {0};

    s.name = (char *)name;
    s.sh_type = SHT_PROGBITS; // 程序数据
    s.sh_flags = 0;           // 无特殊标志
    s.sh_addralign = 8;       // 8字节对齐
    s.data = bin->data;       // 调试数据
    s.data_len = bin->len;    // 数据长度
    s.index = shndx;          // 段索引

    return elf_add_section(elf, &s);
}

/**
 * 添加调试信息重定位段
 * 处理调试信息中对符号的引用
 */
static int _add_debug_relas(vector_t *debug_relas, parse_t *parse, elf_context_t *elf, int sh_index, const char *sh_name) {
    elf_rela_t *rela;
    elf_sym_t *sym;
    vector_t *relas;
    rela_t *r;

    int ret;
    int i;
    int j;

    relas = vector_alloc();
    if (!relas)
        return -ENOMEM;
    // 处理所有调试重定位项
    for (i = 0; i < debug_relas->size; i++) {
        r = debug_relas->data[i];
        // 在符号表中查找符号
        for (j = 0; j < parse->symtab->size; j++) {
            sym = parse->symtab->data[j];

            if (!sym->name)
                continue;

            if (!strcmp(sym->name, r->name->data))
                break;
        }

        if (j == parse->symtab->size) {
            loge("r->name: %s\n", r->name->data);
            return -EINVAL;
        }
        // 创建重定位项
        rela = calloc(1, sizeof(elf_rela_t));
        if (!rela)
            return -ENOMEM;

        if (vector_add(relas, rela) < 0) {
            free(rela);
            return -ENOMEM;
        }

        logd("r->name: %s\n", r->name->data);

        rela->name = r->name->data;
        rela->r_offset = r->text_offset;             // 重定位位置
        rela->r_info = ELF64_R_INFO(j + 1, r->type); // 符号索引+类型
        rela->r_addend = r->addend;                  // 加数
    }

    ret = 0;

    // 创建重定位段
#if 1
    elf_section_t s = {0};

    s.name = (char *)sh_name;
    s.sh_type = SHT_RELA;       // 重定位段
    s.sh_flags = SHF_INFO_LINK; // 包含链接信息
    s.sh_addralign = 8;         // 8字节对齐
    s.data = NULL;
    s.data_len = 0;
    s.sh_link = 0;        // 链接到符号表
    s.sh_info = sh_index; // 应用于哪个段

    // ARM32架构需要调整重定位格式
    if (!strcmp(elf->ops->machine, "arm32")) {
        for (i = 0; i < relas->size; i++) {
            rela = relas->data[i];

            rela->r_info = ELF32_R_INFO(ELF64_R_SYM(rela->r_info), ELF64_R_TYPE(rela->r_info));
        }
    }

    ret = elf_add_rela_section(elf, &s, relas);
#endif

    vector_clear(relas, (void (*)(void *))free);
    vector_free(relas);
    return ret;
}

/**
 * 添加所有调试信息段到ELF文件
 */
static int _add_debug_sections(parse_t *parse, elf_context_t *elf) {
    // 添加各种DWARF调试段
    int abbrev = _add_debug_section(elf, ".debug_abbrev", parse->debug->debug_abbrev, SHNDX_DEBUG_ABBREV);
    if (abbrev < 0)
        return abbrev;

    int info = _add_debug_section(elf, ".debug_info", parse->debug->debug_info, SHNDX_DEBUG_INFO);
    if (info < 0)
        return info;

    int line = _add_debug_section(elf, ".debug_line", parse->debug->debug_line, SHNDX_DEBUG_LINE);
    if (line < 0)
        return line;

    int str = _add_debug_section(elf, ".debug_str", parse->debug->str, SHNDX_DEBUG_STR);
    if (str < 0)
        return str;

    // 为调试段添加符号表条目
    ADD_SECTION_SYMBOL(abbrev, ".debug_abbrev");
    ADD_SECTION_SYMBOL(info, ".debug_info");
    ADD_SECTION_SYMBOL(line, ".debug_line");
    ADD_SECTION_SYMBOL(str, ".debug_str");

    return 0;
}

/**
 * 编译函数：进行语义分析、优化和代码生成
 */
int parse_compile_functions(parse_t *parse, vector_t *functions) {
    function_t *f;
    int i;

    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        printf("%d, %s(), argv->size: %d, define_flag: %d, inline_flag: %d\n",
               i, f->node.w->text->data, f->argv->size, f->node.define_flag, f->inline_flag);

        if (!f->node.define_flag) // 跳过未定义的函数声明
            continue;

        if (f->compile_flag) // 跳过已编译的函数
            continue;
        f->compile_flag = 1;

        // 1. 语义分析
        int ret = function_semantic_analysis(parse->ast, f);
        if (ret < 0)
            return ret;

        // 2. 常量优化
        ret = function_const_opt(parse->ast, f);
        if (ret < 0)
            return ret;

        // 3. 转换为三地址码
        list_t h;
        list_init(&h);

        ret = function_to_3ac(parse->ast, f, &h);
        if (ret < 0) {
            list_clear(&h, _3ac_code_t, list, _3ac_code_free);
            return ret;
        }

        //		  _3ac_list_print(&h);
        // 4. 分割基本块
        ret = _3ac_split_basic_blocks(&h, f);
        if (ret < 0) {
            list_clear(&h, _3ac_code_t, list, _3ac_code_free);
            return ret;
        }

        assert(list_empty(&h));
        basic_block_print_list(&f->basic_block_list_head);
    }
    // 5. 整体优化
    int ret = optimize(parse->ast, functions);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    return 0;
}

/**
 * 为.text段添加重定位信息
 * 处理函数调用和全局变量访问的重定位
 */
static int _parse_add_text_relas(parse_t *parse, elf_context_t *elf, vector_t *functions) {
    function_t *f;
    vector_t *relas;
    rela_t *r;

    relas = vector_alloc();
    if (!relas)
        return -ENOMEM;

    int ret = 0;
    int i;
    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;

        int j;
        // 处理函数调用重定位
        for (j = 0; j < f->text_relas->size; j++) {
            r = f->text_relas->data[j];

            if (function_signature(parse->ast, r->func) < 0) {
                loge("\n");
                goto error;
            }

            // 根据函数是否定义设置不同的段索引
            if (r->func->node.define_flag)
                ret = _parse_add_rela(relas, parse, r, r->func->signature->data, SHNDX_TEXT);
            else
                ret = _parse_add_rela(relas, parse, r, r->func->signature->data, 0);

            if (ret < 0) {
                loge("\n");
                goto error;
            }
        }
        // 处理数据访问重定位
        for (j = 0; j < f->data_relas->size; j++) {
            r = f->data_relas->data[j];

            char *name;
            if (r->var->global_flag)
                name = r->var->w->text->data; // 全局变量使用原名
            else
                name = r->var->signature->data; // 局部变量使用签名

            ret = _parse_add_rela(relas, parse, r, name, 2); // SHNDX_DATA = 2
            if (ret < 0) {
                loge("\n");
                goto error;
            }
        }
    }

    ret = 0;
    // 如果有重定位项，创建.rela.text段
    if (relas->size > 0) {
        elf_section_t s = {0};
        elf_rela_t *r;

        s.name = ".rela.text";
        s.sh_type = SHT_RELA;
        s.sh_flags = SHF_INFO_LINK;
        s.sh_addralign = 8;
        s.data = NULL;
        s.data_len = 0;
        s.sh_link = 0;
        s.sh_info = SHNDX_TEXT; // 应用于.text段
                                // ARM32架构调整
        if (!strcmp(elf->ops->machine, "arm32")) {
            for (i = 0; i < relas->size; i++) {
                r = relas->data[i];

                r->r_info = ELF32_R_INFO(ELF64_R_SYM(r->r_info), ELF64_R_TYPE(r->r_info));
            }
        }

        ret = elf_add_rela_section(elf, &s, relas);
    }
error:
    vector_clear(relas, (void (*)(void *))free);
    vector_free(relas);
    return ret;
}

/**
 * 符号比较函数：用于符号表排序
 * 局部符号排在全局符号前面
 */
static int _sym_cmp(const void *v0, const void *v1) {
    const elf_sym_t *sym0 = *(const elf_sym_t **)v0;
    const elf_sym_t *sym1 = *(const elf_sym_t **)v1;

    if (STB_LOCAL == ELF64_ST_BIND(sym0->st_info)) {
        if (STB_GLOBAL == ELF64_ST_BIND(sym1->st_info))
            return -1; // sym0(局部) < sym1(全局)
    } else if (STB_LOCAL == ELF64_ST_BIND(sym1->st_info))
        return 1; // sym0(全局) > sym1(局部)
    return 0;
}

/**
 * 添加调试文件名称信息
 */
static int _add_debug_file_names(parse_t *parse) {
    block_t *root = parse->ast->root_block;
    block_t *b = NULL;

    int ret;
    int i;

    // 遍历所有块，收集文件名
    for (i = 0; i < root->node.nb_nodes; i++) {
        b = (block_t *)root->node.nodes[i];

        if (OP_BLOCK != b->node.type)
            continue;
        // 添加文件符号到符号表
        ret = _parse_add_sym(parse, b->name->data, 0, 0, SHN_ABS, ELF64_ST_INFO(STB_LOCAL, STT_FILE));
        if (ret < 0) {
            loge("\n");
            return ret;
        }
        // 保存文件名用于调试信息
        string_t *file_str = string_clone(b->name);
        if (!file_str)
            return -ENOMEM;

        ret = vector_add(parse->debug->file_names, file_str);
        if (ret < 0) {
            string_free(file_str);
            return ret;
        }
    }

    return 0;
}

/**
 * 写入EDA格式的CPK文件（特定硬件目标）
 */
int eda_write_cpk(parse_t *parse, const char *out, vector_t *functions, vector_t *global_vars) {
    function_t *f;
    ScfEboard *b;

    b = eboard__alloc();
    if (!b)
        return -ENOMEM;

    int i;
    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;

        if (!f->ef) // 没有EDA格式函数，跳过
            continue;
        // 添加函数到EDA板
        int ret = eboard__add_function(b, f->ef);
        f->ef = NULL; // 转移所有权

        if (ret < 0) {
            ScfEboard_free(b);
            return ret;
        }
    }
    // 打包EDA板数据
    uint8_t *buf = NULL;
    long len = 0;

    long ret = ScfEboard_pack(b, &buf, &len);
    if (ret < 0) {
        ScfEboard_free(b);
        free(buf);
        return ret;
    }

    logi("len: %ld\n", len);

    ScfEboard_free(b);
    b = NULL;
    // 写入文件
    FILE *fp = fopen(out, "wb");
    if (!fp)
        return -EINVAL;

    fwrite(buf, len, 1, fp);
    fclose(fp);
    free(buf);
    return 0;
}

/**
 * 为目标架构选择本地指令
 */
int parse_native_functions(parse_t *parse, vector_t *functions, const char *arch) {
    function_t *f;
    native_t *native;
    // 打开目标架构后端
    int ret = native_open(&native, arch);
    if (ret < 0) {
        loge("open native '%s' failed\n", arch);
        return ret;
    }

    int i;
    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;

        if (f->native_flag) // 跳过已处理函数
            continue;
        f->native_flag = 1;
        // 为函数选择目标指令
        ret = native_select_inst(native, f);
        if (ret < 0) {
            loge("\n");
            goto error;
        }
    }

    ret = 0;
error:
    native_close(native);
    return ret;
}

/**
 * 写入ELF目标文件
 */
int parse_write_elf(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code, const char *arch, const char *out) {
    elf_context_t *elf = NULL;
    elf_section_t cs = {0};
    // 打开ELF文件
    int ret = elf_open(&elf, arch, out, "wb");
    if (ret < 0) {
        loge("open '%s' elf file '%s' failed\n", arch, out);
        return ret;
    }
    // 添加.text段
    cs.name = ".text";
    cs.sh_type = SHT_PROGBITS;
    cs.sh_flags = SHF_ALLOC | SHF_EXECINSTR; // 可分配、可执行
    cs.sh_addralign = 1;
    cs.data = code->data;
    cs.data_len = code->len;
    cs.index = SHNDX_TEXT;

    ret = elf_add_section(elf, &cs);
    if (ret < 0)
        goto error;
    // 添加.data段
    ret = _parse_add_ds(parse, elf, global_vars);
    if (ret < 0)
        goto error;
    // 编码调试信息
    ret = dwarf_debug_encode(parse->debug);
    if (ret < 0)
        goto error;
    // 添加调试段
    ret = _add_debug_sections(parse, elf);
    if (ret < 0)
        goto error;
    // 符号表排序（局部符号在前）
    qsort(parse->symtab->data, parse->symtab->size, sizeof(void *), _sym_cmp);
    // 添加数据重定位
    ret = _parse_add_data_relas(parse, elf);
    if (ret < 0)
        goto error;
    // 添加代码重定位
    ret = _parse_add_text_relas(parse, elf, functions);
    if (ret < 0)
        goto error;

    // 添加调试信息重定位
    if (parse->debug->line_relas->size > 0) {
        ret = _add_debug_relas(parse->debug->line_relas, parse, elf, SHNDX_DEBUG_LINE, ".rela.debug_line");
        if (ret < 0)
            goto error;
    }

    if (parse->debug->info_relas->size > 0) {
        ret = _add_debug_relas(parse->debug->info_relas, parse, elf, SHNDX_DEBUG_INFO, ".rela.debug_info");
        if (ret < 0)
            goto error;
    }
    // 添加符号表
    elf_sym_t *sym;
    int i;

    for (i = 0; i < parse->symtab->size; i++) {
        sym = parse->symtab->data[i];

        ret = elf_add_sym(elf, sym, ".symtab");
        if (ret < 0)
            goto error;
    }
    // 写入重定位信息
    ret = elf_write_rel(elf);
error:
    elf_close(elf);
    return ret;
}

/**
 * 填充代码到缓冲区（第二阶段）
 * 处理函数代码生成和重定位信息收集
 */
int64_t parse_fill_code2(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code, dwarf_info_entry_t **cu) {
    function_t *f;
    rela_t *r;

    int64_t offset = 0; // 代码段偏移

    int i;
    int j;

    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;
        // 为第一个函数创建编译单元调试信息
        if (!*cu) {
            int ret = _debug_add_cu(cu, parse, f, offset);
            if (ret < 0)
                return ret;
        }
        // 生成函数签名
        if (function_signature(parse->ast, f) < 0)
            return -ENOMEM;
        // 填充函数指令
        int ret = _fill_function_inst(code, f, offset, parse);
        if (ret < 0)
            return ret;
        // 添加函数符号
        ret = _parse_add_sym(parse, f->signature->data, f->code_bytes, offset, SHNDX_TEXT, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC));
        if (ret < 0)
            return ret;
        // 处理函数调用重定位
        for (j = 0; j < f->text_relas->size; j++) {
            r = f->text_relas->data[j];

            r->text_offset = offset + r->inst_offset; // 计算重定位位置

            logd("rela text %s, text_offset: %#lx, offset: %ld, inst_offset: %d\n",
                 r->func->node.w->text->data, r->text_offset, offset, r->inst_offset);
        }
        // 处理数据访问重定位
        for (j = 0; j < f->data_relas->size; j++) {
            r = f->data_relas->data[j];
            // 收集常量和变量
            if (variable_const_string(r->var)
                || (variable_const(r->var) && FUNCTION_PTR != r->var->type))

                ret = vector_add_unique(parse->global_consts, r->var);
            else
                ret = vector_add_unique(global_vars, r->var);
            if (ret < 0)
                return ret;

            r->text_offset = offset + r->inst_offset;

            logd("rela data %s, text_offset: %ld, offset: %ld, inst_offset: %d\n",
                 r->var->w->text->data, r->text_offset, offset, r->inst_offset);
        }

        offset += f->code_bytes; // 更新代码段偏移
    }

    return offset;
}

/**
 * 填充代码到缓冲区（第一阶段）
 * 初始化调试信息并调用第二阶段
 */
int parse_fill_code(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code) {
    // 添加调试文件名
    int ret = _add_debug_file_names(parse);
    if (ret < 0)
        return ret;

    assert(parse->debug->file_names->size > 0);

    string_t *file_name = parse->debug->file_names->data[0];
    const char *path = file_name->data;
    // 添加段符号
    ADD_SECTION_SYMBOL(SHNDX_TEXT, ".text");
    ADD_SECTION_SYMBOL(SHNDX_RODATA, ".rodata");
    ADD_SECTION_SYMBOL(SHNDX_DATA, ".data");

    dwarf_info_entry_t *cu = NULL;
    dwarf_line_result_t *r = NULL;
    dwarf_line_result_t *r2 = NULL;
    // 添加初始行号记录
    r = calloc(1, sizeof(dwarf_line_result_t));
    if (!r)
        return -ENOMEM;

    r->file_name = string_cstr(path);
    if (!r->file_name) {
        free(r);
        return -ENOMEM;
    }
    r->address = 0;
    r->line = 1;
    r->is_stmt = 1;

    if (vector_add(parse->debug->lines, r) < 0) {
        string_free(r->file_name);
        free(r);
        return -ENOMEM;
    }
    r = NULL;
    // 第二阶段：填充代码
    int64_t offset = parse_fill_code2(parse, functions, global_vars, code, &cu);
    if (offset < 0)
        return offset;
    // 更新编译单元的high_pc
    if (cu)
        DEBUG_UPDATE_HIGH_PC(cu, offset);

    // 添加结束序列行号记录
    assert(parse->debug->lines->size > 0);
    r2 = parse->debug->lines->data[parse->debug->lines->size - 1];

    r = calloc(1, sizeof(dwarf_line_result_t));
    if (!r)
        return -ENOMEM;

    r->file_name = string_cstr(path);
    if (!r->file_name) {
        free(r);
        return -ENOMEM;
    }

    r->address = offset;
    r->line = r2->line;
    r->is_stmt = 1;
    r->end_sequence = 1; // 标记序列结束

    if (vector_add(parse->debug->lines, r) < 0) {
        string_free(r->file_name);
        free(r);
        return -ENOMEM;
    }
    r = NULL;
    // 添加调试缩写表结束标记
    dwarf_abbrev_declaration_t *abbrev0 = NULL;

    abbrev0 = dwarf_abbrev_declaration_alloc();
    if (!abbrev0)
        return -ENOMEM;
    abbrev0->code = 0; // 0表示结束

    if (vector_add(parse->debug->abbrevs, abbrev0) < 0) {
        dwarf_abbrev_declaration_free(abbrev0);
        return -ENOMEM;
    }

    return 0;
}

/**
 * 编译入口点：进行语义分析、优化和目标代码选择
 */
int parse_compile(parse_t *parse, const char *arch, int _3ac) {
    block_t *b = parse->ast->root_block;
    if (!b)
        return -EINVAL;

    vector_t *functions = vector_alloc();
    if (!functions)
        return -ENOMEM;
    // 搜索所有函数
    int ret = node_search_bfs((node_t *)b, NULL, functions, -1, _find_function);
    if (ret < 0)
        goto error;

    logi("all functions: %d\n", functions->size);
    // 编译函数
    ret = parse_compile_functions(parse, functions);
    if (ret < 0)
        goto error;

    if (_3ac) // 如果只需要三地址码，直接返回
        goto error;
    // 选择目标指令
    ret = parse_native_functions(parse, functions, arch);
error:
    vector_free(functions);
    return ret;
}

/**
 * 生成目标文件主入口
 */
int parse_to_obj(parse_t *parse, const char *out, const char *arch) {
    block_t *b = parse->ast->root_block;
    if (!b)
        return -EINVAL;

    vector_t *functions = NULL;
    vector_t *global_vars = NULL;
    string_t *code = NULL;

    functions = vector_alloc();
    if (!functions)
        return -ENOMEM;
    // 搜索所有函数
    int ret = node_search_bfs((node_t *)b, NULL, functions, -1, _find_function);
    if (ret < 0) {
        vector_free(functions);
        return ret;
    }
    // EDA特殊目标格式
    if (!strcmp(arch, "eda")) {
        ret = eda_write_cpk(parse, out, functions, NULL);

        vector_free(functions);
        return ret;
    }
    // 普通ELF目标文件
    global_vars = vector_alloc();
    if (!global_vars) {
        ret = -ENOMEM;
        goto global_vars_error;
    }
    // 搜索全局变量
    ret = node_search_bfs((node_t *)b, NULL, global_vars, -1, _find_global_var);
    if (ret < 0) {
        loge("\n");
        goto code_error;
    }

    logd("all global_vars: %d\n", global_vars->size);

    parse->debug->arch = (char *)arch;

    code = string_alloc();
    if (!code) {
        ret = -ENOMEM;
        goto code_error;
    }

    // 填充代码
    ret = parse_fill_code(parse, functions, global_vars, code);
    if (ret < 0) {
        loge("\n");
        goto error;
    }
    // 写入ELF文件
    ret = parse_write_elf(parse, functions, global_vars, code, arch, out);
    if (ret < 0) {
        loge("\n");
        goto error;
    }

    ret = 0;

    logi("ok\n\n");

error:
    string_free(code);
code_error:
    vector_free(global_vars);
global_vars_error:
    vector_free(functions);
    return ret;
}
