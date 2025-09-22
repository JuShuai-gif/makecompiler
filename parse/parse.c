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

#define ADD_SECTION_SYMBOL(sh_index, sh_name)                                                             \
    do {                                                                                                  \
        int ret = _parse_add_sym(parse, sh_name, 0, 0, sh_index, ELF64_ST_INFO(STB_LOCAL, STT_SECTION));  \
        if (ret < 0) {                                                                                    \
            loge("\n");                                                                                   \
            return ret;                                                                                   \
        }                                                                                                 \
    } while (0)

base_type_t base_types[] =
    {
        {VAR_CHAR, "char", 1},

        {VAR_VOID, "void", 1},
        {VAR_BIT, "bit", 1},
        {VAR_U2, "bit2_t", 1},
        {VAR_U3, "bit3_t", 1},
        {VAR_U4, "bit4_t", 1},

        {VAR_I1, "int1_t", 1},
        {VAR_I2, "int2_t", 1},
        {VAR_I3, "int3_t", 1},
        {VAR_I4, "int4_t", 1},

        {VAR_INT, "int", 4},
        {VAR_FLOAT, "float", 4},
        {VAR_DOUBLE, "double", 8},

        {VAR_I8, "int8_t", 1},
        {VAR_I16, "int16_t", 2},
        {VAR_I32, "int32_t", 4},
        {VAR_I64, "int64_t", 8},

        {VAR_U8, "uint8_t", 1},
        {VAR_U16, "uint16_t", 2},
        {VAR_U32, "uint32_t", 4},
        {VAR_U64, "uint64_t", 8},

        {VAR_INTPTR, "intptr_t", sizeof(void *)},
        {VAR_UINTPTR, "uintptr_t", sizeof(void *)},
        {FUNCTION_PTR, "funcptr", sizeof(void *)},
};

// 打开解析器
int parse_open(parse_t **pparse) {
    if (!pparse)
        return -EINVAL;

    // 帮解析器分配一个空间
    parse_t *parse = calloc(1, sizeof(parse_t));
    if (!parse)
        return -EINVAL;

    // 
    if (ast_open(&parse->ast) < 0) {
        loge("\n");
        return -1;
    }

    int i;
    for (i = 0; i < sizeof(base_types) / sizeof(base_types[0]); i++) {
        ast_add_base_type(parse->ast, &base_types[i]);
    }

    if (parse_dfa_init(parse) < 0) {
        loge("\n");
        return -1;
    }

    parse->symtab = vector_alloc();
    if (!parse->symtab)
        return -1;

    parse->global_consts = vector_alloc();
    if (!parse->global_consts)
        goto const_error;

    parse->debug = dwarf_debug_alloc();
    if (!parse->debug)
        goto debug_error;

    *pparse = parse;
    return 0;

debug_error:
    vector_free(parse->global_consts);
const_error:
    vector_free(parse->symtab);
    return -1;
}

int parse_close(parse_t *parse) {
    if (parse) {
        free(parse);
        parse = NULL;
    }

    return 0;
}

static int _find_sym(const void *v0, const void *v1) {
    const char *name = v0;
    const elf_sym_t *sym = v1;

    if (!sym->name)
        return -1;

    return strcmp(name, sym->name);
}

static int _parse_add_sym(parse_t *parse, const char *name,
                           uint64_t st_size, Elf64_Addr st_value,
                           uint16_t st_shndx, uint8_t st_info) {
    elf_sym_t *sym = NULL;
    elf_sym_t *sym2 = NULL;

    if (name)
        sym = vector_find_cmp(parse->symtab, name, _find_sym);

    if (!sym) {
        sym = calloc(1, sizeof(elf_sym_t));
        if (!sym)
            return -ENOMEM;

        if (name) {
            sym->name = strdup(name);
            if (!sym->name) {
                free(sym);
                return -ENOMEM;
            }
        }

        sym->st_size = st_size;
        sym->st_value = st_value;
        sym->st_shndx = st_shndx;
        sym->st_info = st_info;

        int ret = vector_add(parse->symtab, sym);
        if (ret < 0) {
            if (sym->name)
                free(sym->name);
            free(sym);
            loge("\n");
            return ret;
        }
    }

    return 0;
}

int parse_file(parse_t *parse, const char *path) {
    if (!parse || !path)
        return -EINVAL;

    lex_t *lex = parse->lex_list;

    while (lex) {
        if (!strcmp(lex->file->data, path))
            break;

        lex = lex->next;
    }

    if (lex) {
        parse->lex = lex;
        return 0;
    }

    if (lex_open(&parse->lex, path) < 0)
        return -1;

    ast_add_file_block(parse->ast, path);

    parse->lex->next = parse->lex_list;
    parse->lex_list = parse->lex;

    dfa_data_t *d = parse->dfa_data;
    lex_word_t *w = NULL;

    int ret = 0;

    while (1) {
        ret = lex_pop_word(parse->lex, &w);
        if (ret < 0) {
            loge("lex pop word failed\n");
            break;
        }

        if (LEX_WORD_EOF == w->type) {
            logi("eof\n\n");
            lex_push_word(parse->lex, w);
            w = NULL;
            break;
        }

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

        assert(!d->expr);

        ret = dfa_parse_word(parse->dfa, w, d);
        if (ret < 0)
            break;
    }

    fclose(parse->lex->fp);
    parse->lex->fp = NULL;
    return ret;
}

static int _debug_abbrev_find_by_tag(const void *v0, const void *v1) {
    const dwarf_uword_t tag = (dwarf_uword_t)(uintptr_t)v0;
    const dwarf_abbrev_declaration_t *d = v1;

    return tag != d->tag;
}

static dwarf_info_entry_t *_debug_find_type(parse_t *parse, type_t *t, int nb_pointers) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    dwarf_uword_t tag;
    vector_t *types;

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

    d = vector_find_cmp(parse->debug->abbrevs, (void *)(uintptr_t)tag, _debug_abbrev_find_by_tag);
    if (!d)
        return NULL;

    int i;
    int j;
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

static int __debug_add_type(dwarf_info_entry_t **pie, dwarf_abbrev_declaration_t **pad, parse_t *parse,
                            type_t *t, int nb_pointers, dwarf_info_entry_t *ie_type) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;
    vector_t *types;

    int ret;

    if (nb_pointers > 0) {
        d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_pointer_type, _debug_abbrev_find_by_tag);
        if (!d) {
            ret = dwarf_abbrev_add_pointer_type(parse->debug->abbrevs);
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
            d->has_children = t->scope->vars->size > 0;
        }

        types = parse->debug->struct_types;
    }

    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;

    ie->type = t->type;
    ie->nb_pointers = nb_pointers;

    ret = vector_add(types, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }

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

    *pie = ie;
    *pad = d;
    return 0;
}

static int __debug_add_member_var(dwarf_info_entry_t **pie, parse_t *parse, variable_t *v, dwarf_info_entry_t *ie_type) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;

    d = vector_find_cmp(parse->debug->abbrevs, (void *)DW_TAG_member, _debug_abbrev_find_by_tag);
    if (!d) {
        ret = dwarf_abbrev_add_member_var(parse->debug->abbrevs);
        if (ret < 0) {
            loge("\n");
            return -1;
        }

        d = parse->debug->abbrevs->data[parse->debug->abbrevs->size - 1];
    }

    ie = dwarf_info_entry_alloc();
    if (!ie)
        return -ENOMEM;
    ie->code = d->code;

    ret = vector_add(parse->debug->struct_types, ie);
    if (ret < 0) {
        dwarf_info_entry_free(ie);
        return ret;
    }

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

        if (DW_AT_name == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, v->w->text->data, v->w->text->len);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            uint8_t file = 1;

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&v->w->line, sizeof(v->w->line));
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            uint32_t type = 0;

            iattr->ref_entry = ie_type;

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

    *pie = ie;
    return 0;
}

static int _debug_add_type(dwarf_info_entry_t **pie, parse_t *parse, type_t *t, int nb_pointers);

static int _debug_add_struct_type(dwarf_info_entry_t **pie, dwarf_abbrev_declaration_t **pad, parse_t *parse, type_t *t) {
    //	  dwarf_abbrev_declaration_t* d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;
    int i;

    vector_t *ie_member_types = vector_alloc();
    if (!ie_member_types)
        return -ENOMEM;

    int nb_pointers = 0;

    for (i = 0; i < t->scope->vars->size; i++) {
        dwarf_info_entry_t *ie_member;
        variable_t *v_member;
        type_t *t_member;

        v_member = t->scope->vars->data[i];
        ret = ast_find_type_type(&t_member, parse->ast, v_member->type);
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

    ret = __debug_add_type(&ie, pad, parse, t, 0, NULL);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    vector_t *refills = NULL;

    if (nb_pointers > 0) {
        refills = vector_alloc();
        if (!refills)
            return -ENOMEM;
    }

    for (i = 0; i < t->scope->vars->size; i++) {
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

    if (nb_pointers > 0) {
        dwarf_info_entry_t *ie_pointer;
        dwarf_info_attr_t *iattr;

        int j;
        for (j = 1; j <= nb_pointers; j++) {
            ret = _debug_add_type(&ie_pointer, parse, t, j);
            if (ret < 0) {
                loge("\n");
                return ret;
            }

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

static int _debug_add_type(dwarf_info_entry_t **pie, parse_t *parse, type_t *t, int nb_pointers) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

    int ret;
    int i;

    *pie = _debug_find_type(parse, t, nb_pointers);
    if (*pie)
        return 0;

    ie = _debug_find_type(parse, t, 0);
    if (!ie) {
        if (t->type < STRUCT) {
            ret = __debug_add_type(&ie, &d, parse, t, 0, NULL);
            if (ret < 0)
                return ret;

        } else {
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

    return __debug_add_type(pie, &d, parse, t, nb_pointers, ie);
}

static int _debug_add_var(parse_t *parse, node_t *node) {
    variable_t *var = node->var;
    type_t *t = NULL;

    int ret = ast_find_type_type(&t, parse->ast, var->type);
    if (ret < 0)
        return ret;

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

    for (i = parse->debug->infos->size - 1; i >= 0; i--) {
        ie2 = parse->debug->infos->data[i];

        if (ie2->code == subp->code)
            break;

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
                return 0;
            }
        }
    }

    ie2 = dwarf_info_entry_alloc();
    if (!ie2)
        return -ENOMEM;
    ie2->code = d2->code;

    ret = vector_add(parse->debug->infos, ie2);
    if (ret < 0) {
        dwarf_info_entry_free(ie2);
        return ret;
    }

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

static int _fill_code_list_inst(string_t *code, list_t *h, int64_t offset, parse_t *parse, function_t *f) {
    list_t *l;
    node_t *node;

    uint32_t line = 0;
    uint32_t line2 = 0;

    for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {
        _3ac_code_t *c = list_data(l, _3ac_code_t, list);

        if (!c->instructions)
            continue;

        int i;
        int ret;

        for (i = 0; i < c->instructions->size; i++) {
            instruction_t *inst = c->instructions->data[i];

            ret = string_cat_cstr_len(code, inst->code, inst->len);
            if (ret < 0)
                return ret;
        }

        if (c->dsts) {
            _3ac_operand_t * dst;

            for (i = 0; i < c->dsts->size; i++) {
                dst = c->dsts->data[i];

                if (!dst->dag_node || !dst->dag_node->node)
                    continue;

                node = dst->dag_node->node;

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

        if (c->srcs) {
            _3ac_operand_t * src;

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

            r->address = offset;
            r->line = line;
            r->is_stmt = 1;

            ret = vector_add(parse->debug->lines, r);
            if (ret < 0) {
                string_free(r->file_name);
                free(r);
                return ret;
            }
        }

        offset += c->inst_bytes;
    }

    return 0;
}

static int _debug_add_subprogram(dwarf_info_entry_t **pie, parse_t *parse, function_t *f, int64_t offset) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_entry_t *ie2;
    dwarf_info_attr_t *iattr;

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

        if (DW_AT_external == iattr->name) {
            uint8_t value = 1;

            ret = dwarf_info_fill_attr(iattr, &value, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_name == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, f->signature->data, f->signature->len + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_file == iattr->name) {
            logd("f->node.w->file->data: %s\n", f->node.w->file->data);

            string_t *s;
            int k;
            logd("parse->debug->file_names->size: %d\n", parse->debug->file_names->size);

            for (k = 0; k < parse->debug->file_names->size; k++) {
                s = parse->debug->file_names->data[k];

                logd("s->data: %s\n", s->data);

                if (!strcmp(s->data, f->node.w->file->data))
                    break;
            }
            assert(k < parse->debug->file_names->size);
            assert(k < 254);

            uint8_t file = k + 1;

            ret = dwarf_info_fill_attr(iattr, &file, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_decl_line == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&(f->node.w->line), sizeof(f->node.w->line));
            if (ret < 0)
                return ret;

        } else if (DW_AT_type == iattr->name) {
            uint32_t type = 0;
            variable_t *v = f->rets->data[0];
            type_t *t = NULL;

            ret = ast_find_type_type(&t, parse->ast, v->type);
            if (ret < 0)
                return ret;

            ie2 = _debug_find_type(parse, t, v->nb_pointers);
            if (!ie2) {
                ret = _debug_add_type(&ie2, parse, t, v->nb_pointers);
                if (ret < 0) {
                    loge("\n");
                    return ret;
                }
            }

            iattr->ref_entry = ie2;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&type, sizeof(type));
            if (ret < 0)
                return ret;

        } else if (DW_AT_low_pc == iattr->name
                   || DW_AT_high_pc == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&offset, sizeof(offset));
            if (ret < 0)
                return ret;

        } else if (DW_AT_frame_base == iattr->name) {
            uint8_t buf[64];

            buf[sizeof(dwarf_uword_t)] = DW_OP_call_frame_cfa;

            *(dwarf_uword_t *)buf = 1;

            ret = dwarf_info_fill_attr(iattr, buf, sizeof(dwarf_uword_t) + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_GNU_all_call_sites == iattr->name) {
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

    *pie = ie;
    return 0;
}

static int _debug_add_cu(dwarf_info_entry_t **pie, parse_t *parse, function_t *f, int64_t offset) {
    dwarf_abbrev_declaration_t *d;
    dwarf_abbrev_attribute_t *attr;
    dwarf_info_entry_t *ie;
    dwarf_info_attr_t *iattr;

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
            char *producer = "GNU C11 7.4.0 -mtune=generic -march=x86-64 -g -fstack-protector-strong";

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)producer, strlen(producer));
            if (ret < 0)
                return ret;

        } else if (DW_AT_language == iattr->name) {
            uint8_t language = 12;

            ret = dwarf_info_fill_attr(iattr, &language, 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_name == iattr->name) {
            string_t *fname = f->node.w->file;

            ret = dwarf_info_fill_attr(iattr, fname->data, fname->len + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_comp_dir == iattr->name) {
            uint8_t buf[4096];

            uint8_t *dir = getcwd(buf, sizeof(buf) - 1);
            assert(dir);

            ret = dwarf_info_fill_attr(iattr, dir, strlen(dir) + 1);
            if (ret < 0)
                return ret;

        } else if (DW_AT_low_pc == iattr->name
                   || DW_AT_high_pc == iattr->name) {
            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&offset, sizeof(offset));
            if (ret < 0)
                return ret;

        } else if (DW_AT_stmt_list == iattr->name) {
            dwarf_uword_t stmt_list = 0;

            ret = dwarf_info_fill_attr(iattr, (uint8_t *)&stmt_list, sizeof(stmt_list));
            if (ret < 0)
                return ret;

        } else if (0 == iattr->name) {
            assert(0 == iattr->form);
        } else {
            loge("iattr->name: %d, %s\n", iattr->name, dwarf_find_attribute(iattr->name));
            return -1;
        }
    }

    *pie = ie;
    return 0;
}

static int _fill_function_inst(string_t *code, function_t *f, int64_t offset, parse_t *parse) {
    list_t *l;
    int ret;
    int i;

    dwarf_abbrev_declaration_t *abbrev0 = NULL;
    dwarf_info_entry_t *subp = NULL;
    dwarf_info_entry_t *ie0 = NULL;

    ret = _debug_add_subprogram(&subp, parse, f, offset);
    if (ret < 0)
        return ret;

    f->code_bytes = 0;

    if (f->init_code) {
        for (i = 0; i < f->init_code->instructions->size; i++) {
            instruction_t *inst = f->init_code->instructions->data[i];

            ret = string_cat_cstr_len(code, inst->code, inst->len);
            if (ret < 0)
                return ret;

            f->code_bytes += inst->len;
        }
    }

    for (l = list_head(&f->basic_block_list_head); l != list_sentinel(&f->basic_block_list_head);
         l = list_next(l)) {
        basic_block_t *bb = list_data(l, basic_block_t, list);

        ret = _fill_code_list_inst(code, &bb->code_list_head, offset + f->code_bytes, parse, f);
        if (ret < 0)
            return ret;

        f->code_bytes += bb->code_bytes;
    }
#if 1
    if (f->code_bytes & 0x7) {
        size_t n = 8 - (f->code_bytes & 0x7);

        ret = string_fill_zero(code, n);
        if (ret < 0)
            return ret;

        f->code_bytes += n;
    }
#endif
    uint64_t high_pc_ = f->code_bytes;

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

    DEBUG_UPDATE_HIGH_PC(subp, high_pc_);

#if 1
    ie0 = dwarf_info_entry_alloc();
    if (!ie0)
        return -ENOMEM;
    ie0->code = 0;

    if (vector_add(parse->debug->infos, ie0) < 0) {
        dwarf_info_entry_free(ie0);
        return -ENOMEM;
    }
#endif
    return 0;
}

static int _parse_add_rela(vector_t *relas, parse_t *parse, rela_t *r, const char *name, uint16_t st_shndx) {
    elf_rela_t *rela;

    int ret;
    int i;

    for (i = 0; i < parse->symtab->size; i++) {
        elf_sym_t *sym = parse->symtab->data[i];

        if (!sym->name)
            continue;

        if (!strcmp(name, sym->name))
            break;
    }

    if (i == parse->symtab->size) {
        ret = _parse_add_sym(parse, name, 0, 0, st_shndx, ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE));
        if (ret < 0) {
            loge("\n");
            return ret;
        }
    }

    logd("rela: %s, offset: %ld\n", name, r->text_offset);

    rela = calloc(1, sizeof(elf_rela_t));
    if (!rela)
        return -ENOMEM;

    rela->name = (char *)name;
    rela->r_offset = r->text_offset;
    rela->r_info = ELF64_R_INFO(i + 1, r->type);
    rela->r_addend = r->addend;

    ret = vector_add(relas, rela);
    if (ret < 0) {
        loge("\n");
        free(rela);
        return ret;
    }

    return 0;
}

static int _fill_data(parse_t *parse, variable_t *v, string_t *data, uint32_t shndx) {
    char *name;
    int size;
    uint8_t *v_data;

    if (v->global_flag) {
        name = v->w->text->data;
    } else
        name = v->signature->data;

    logd("v_%d_%d/%s, nb_dimentions: %d\n", v->w->line, v->w->pos, v->w->text->data, v->nb_dimentions);

    if (variable_const_string(v)) {
        size = v->data.s->len + 1;
        v_data = v->data.s->data;

    } else if (v->nb_dimentions > 0) {
        size = variable_size(v);
        v_data = v->data.p;

    } else if (v->type >= STRUCT) {
        size = v->size;
        v_data = v->data.p;
    } else {
        size = v->size;
        v_data = (uint8_t *)&v->data;
    }

    // align 8 bytes
    int fill_size = (data->len + 7) >> 3 << 3;
    fill_size -= data->len;

    int ret;

    if (fill_size > 0) {
        ret = string_fill_zero(data, fill_size);
        if (ret < 0)
            return ret;
    }
    assert(0 == (data->len & 0x7));

    v->ds_offset = data->len;

    uint64_t stb;
    if (v->static_flag)
        stb = STB_LOCAL;
    else
        stb = STB_GLOBAL;

    ret = _parse_add_sym(parse, name, size, data->len, shndx, ELF64_ST_INFO(stb, STT_OBJECT));
    if (ret < 0)
        return ret;

    if (!v_data)
        ret = string_fill_zero(data, size);
    else
        ret = string_cat_cstr_len(data, v_data, size);

    return ret;
}

static int _parse_add_data_relas(parse_t *parse, elf_context_t *elf) {
    elf_rela_t *rela;
    ast_rela_t *r;
    function_t *f;
    variable_t *v;
    variable_t *v2;
    vector_t *relas;
    string_t *rodata;

    int ret;
    int i;
    int j;

    for (i = 0; i < parse->ast->global_relas->size; i++) {
        r = parse->ast->global_relas->data[i];

        v = r->obj->base;

        if (variable_const_string(v)
            || (variable_const(v) && FUNCTION_PTR != v->type)) {
            if (vector_add_unique(parse->global_consts, v) < 0)
                return -ENOMEM;
        }
    }

    rodata = string_alloc();
    if (!rodata)
        return -ENOMEM;

    for (i = 0; i < parse->global_consts->size; i++) {
        v = parse->global_consts->data[i];

        for (j = 1; j < i; j++) {
            v2 = parse->global_consts->data[j];

            if (v2 == v)
                break;

            if (v2->type != v->type || v2->size != v->size)
                continue;

            if (variable_const_string(v)) {
                assert(variable_const_string(v2));

                if (!string_cmp(v->data.s, v2->data.s))
                    break;
                continue;
            }

            if (!memcmp(&v->data, &v2->data, v->size))
                break;
        }

        if (j < i) {
            if (v2 != v)
                v2->ds_offset = v->ds_offset;
            continue;
        }

        v->global_flag = 1;

        ret = _fill_data(parse, v, rodata, SHNDX_RODATA);
        if (ret < 0) {
            string_free(rodata);
            return ret;
        }
    }

    relas = vector_alloc();
    if (!relas) {
        string_free(rodata);
        return -ENOMEM;
    }

    for (i = 0; i < parse->ast->global_relas->size; i++) {
        r = parse->ast->global_relas->data[i];

        int offset0 = member_offset(r->ref);
        int offset1 = member_offset(r->obj);
        char *name;
        int shndx;

        if (variable_const(r->obj->base)) {
            if (FUNCTION_PTR == r->obj->base->type) {
                f = r->obj->base->func_ptr;
                name = f->node.w->text->data;
                shndx = SHNDX_TEXT;

            } else {
                name = r->obj->base->w->text->data;
                shndx = SHNDX_RODATA;
            }
        } else if (variable_const_string(r->obj->base)) {
            name = r->obj->base->w->text->data;
            shndx = SHNDX_RODATA;
        } else {
            name = r->obj->base->w->text->data;
            shndx = SHNDX_DATA;
        }

        for (j = 0; j < parse->symtab->size; j++) {
            elf_sym_t *sym = parse->symtab->data[j];

            if (!sym->name)
                continue;

            if (!strcmp(name, sym->name))
                break;
        }

        if (j == parse->symtab->size) {
            ret = _parse_add_sym(parse, name, 0, 0, 0, ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE));
            if (ret < 0) {
                loge("\n");
                return ret;
            }
        }

        rela = calloc(1, sizeof(elf_rela_t));
        if (!rela)
            return -ENOMEM;

        rela->name = name;
        rela->r_offset = r->ref->base->ds_offset + offset0;
        rela->r_info = ELF64_R_INFO(j + 1, R_X86_64_64);
        rela->r_addend = offset1;

        ret = vector_add(relas, rela);
        if (ret < 0) {
            loge("\n");
            goto error;
        }
    }

    int fill_size = ((rodata->len + 7) >> 3 << 3) - rodata->len;

    if (fill_size > 0) {
        ret = string_fill_zero(rodata, fill_size);
        if (ret < 0)
            goto error;
    }

    ret = 0;

    elf_section_t ro = {0};

    ro.name = ".rodata";
    ro.sh_type = SHT_PROGBITS;
    ro.sh_flags = SHF_ALLOC;
    ro.sh_addralign = 8;
    ro.data = rodata->data;
    ro.data_len = rodata->len;
    ro.index = SHNDX_RODATA;

    ret = elf_add_section(elf, &ro);
    if (ret < 0)
        goto error;

    if (relas->size > 0) {
        elf_section_t s = {0};

        s.name = ".rela.data";
        s.sh_type = SHT_RELA;
        s.sh_flags = SHF_INFO_LINK;
        s.sh_addralign = 8;
        s.data = NULL;
        s.data_len = 0;
        s.sh_link = 0;
        s.sh_info = SHNDX_DATA;

        if (!strcmp(elf->ops->machine, "arm32")) {
            for (i = 0; i < relas->size; i++) {
                rela = relas->data[i];

                rela->r_info = ELF32_R_INFO(ELF64_R_SYM(rela->r_info), ELF64_R_TYPE(rela->r_info));
            }
        }

        ret = elf_add_rela_section(elf, &s, relas);
    }
error:
    string_free(rodata);
    vector_clear(relas, (void (*)(void *))free);
    vector_free(relas);
    return ret;
}

static int _parse_add_ds(parse_t *parse, elf_context_t *elf, vector_t *global_vars) {
    variable_t *v;
    string_t *data;

    data = string_alloc();
    if (!data)
        return -ENOMEM;

    int ret = 0;
    int i;

    for (i = 0; i < global_vars->size; i++) {
        v = global_vars->data[i];

        if (v->extern_flag)
            continue;

        ret = _fill_data(parse, v, data, SHNDX_DATA);
        if (ret < 0)
            goto error;
    }

    int fill_size = ((data->len + 7) >> 3 << 3) - data->len;
    if (fill_size > 0) {
        ret = string_fill_zero(data, fill_size);
        if (ret < 0) {
            ret = -ENOMEM;
            goto error;
        }
    }
    assert(0 == (data->len & 0x7));

    elf_section_t ds = {0};

    ds.name = ".data";
    ds.sh_type = SHT_PROGBITS;
    ds.sh_flags = SHF_ALLOC | SHF_WRITE;
    ds.sh_addralign = 8;
    ds.data = data->data;
    ds.data_len = data->len;
    ds.index = SHNDX_DATA;

    ret = elf_add_section(elf, &ds);
    if (ret < 0) {
        loge("\n");
        goto error;
    }

error:
    string_free(data);
    return ret;
}

static int _add_debug_section(elf_context_t *elf, const char *name, const string_t *bin, uint32_t shndx) {
    elf_section_t s = {0};

    s.name = (char *)name;
    s.sh_type = SHT_PROGBITS;
    s.sh_flags = 0;
    s.sh_addralign = 8;
    s.data = bin->data;
    s.data_len = bin->len;
    s.index = shndx;

    return elf_add_section(elf, &s);
}

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

    for (i = 0; i < debug_relas->size; i++) {
        r = debug_relas->data[i];

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

        rela = calloc(1, sizeof(elf_rela_t));
        if (!rela)
            return -ENOMEM;

        if (vector_add(relas, rela) < 0) {
            free(rela);
            return -ENOMEM;
        }

        logd("r->name: %s\n", r->name->data);

        rela->name = r->name->data;
        rela->r_offset = r->text_offset;
        rela->r_info = ELF64_R_INFO(j + 1, r->type);
        rela->r_addend = r->addend;
    }

    ret = 0;
#if 1
    elf_section_t s = {0};

    s.name = (char *)sh_name;
    s.sh_type = SHT_RELA;
    s.sh_flags = SHF_INFO_LINK;
    s.sh_addralign = 8;
    s.data = NULL;
    s.data_len = 0;
    s.sh_link = 0;
    s.sh_info = sh_index;

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

static int _add_debug_sections(parse_t *parse, elf_context_t *elf) {
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

    ADD_SECTION_SYMBOL(abbrev, ".debug_abbrev");
    ADD_SECTION_SYMBOL(info, ".debug_info");
    ADD_SECTION_SYMBOL(line, ".debug_line");
    ADD_SECTION_SYMBOL(str, ".debug_str");

    return 0;
}

int parse_compile_functions(parse_t *parse, vector_t *functions) {
    function_t *f;
    int i;

    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        printf("%d, %s(), argv->size: %d, define_flag: %d, inline_flag: %d\n",
               i, f->node.w->text->data, f->argv->size, f->node.define_flag, f->inline_flag);

        if (!f->node.define_flag)
            continue;

        if (f->compile_flag)
            continue;
        f->compile_flag = 1;

        int ret = function_semantic_analysis(parse->ast, f);
        if (ret < 0)
            return ret;

        ret = function_const_opt(parse->ast, f);
        if (ret < 0)
            return ret;

        list_t h;
        list_init(&h);

        ret = function_to_3ac(parse->ast, f, &h);
        if (ret < 0) {
            list_clear(&h, _3ac_code_t, list, _3ac_code_free);
            return ret;
        }

        //		  _3ac_list_print(&h);

        ret = _3ac_split_basic_blocks(&h, f);
        if (ret < 0) {
            list_clear(&h, _3ac_code_t, list, _3ac_code_free);
            return ret;
        }

        assert(list_empty(&h));
        basic_block_print_list(&f->basic_block_list_head);
    }

    int ret = optimize(parse->ast, functions);
    if (ret < 0) {
        loge("\n");
        return ret;
    }

    return 0;
}

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
        for (j = 0; j < f->text_relas->size; j++) {
            r = f->text_relas->data[j];

            if (function_signature(parse->ast, r->func) < 0) {
                loge("\n");
                goto error;
            }

            if (r->func->node.define_flag)
                ret = _parse_add_rela(relas, parse, r, r->func->signature->data, SHNDX_TEXT);
            else
                ret = _parse_add_rela(relas, parse, r, r->func->signature->data, 0);

            if (ret < 0) {
                loge("\n");
                goto error;
            }
        }

        for (j = 0; j < f->data_relas->size; j++) {
            r = f->data_relas->data[j];

            char *name;
            if (r->var->global_flag)
                name = r->var->w->text->data;
            else
                name = r->var->signature->data;

            ret = _parse_add_rela(relas, parse, r, name, 2);
            if (ret < 0) {
                loge("\n");
                goto error;
            }
        }
    }

    ret = 0;

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
        s.sh_info = SHNDX_TEXT;

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

static int _sym_cmp(const void *v0, const void *v1) {
    const elf_sym_t *sym0 = *(const elf_sym_t **)v0;
    const elf_sym_t *sym1 = *(const elf_sym_t **)v1;

    if (STB_LOCAL == ELF64_ST_BIND(sym0->st_info)) {
        if (STB_GLOBAL == ELF64_ST_BIND(sym1->st_info))
            return -1;
    } else if (STB_LOCAL == ELF64_ST_BIND(sym1->st_info))
        return 1;
    return 0;
}

static int _add_debug_file_names(parse_t *parse) {
    block_t *root = parse->ast->root_block;
    block_t *b = NULL;

    int ret;
    int i;

    for (i = 0; i < root->node.nb_nodes; i++) {
        b = (block_t *)root->node.nodes[i];

        if (OP_BLOCK != b->node.type)
            continue;

        ret = _parse_add_sym(parse, b->name->data, 0, 0, SHN_ABS, ELF64_ST_INFO(STB_LOCAL, STT_FILE));
        if (ret < 0) {
            loge("\n");
            return ret;
        }

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

        if (!f->ef)
            continue;

        int ret = eboard__add_function(b, f->ef);
        f->ef = NULL;

        if (ret < 0) {
            ScfEboard_free(b);
            return ret;
        }
    }

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

    FILE *fp = fopen(out, "wb");
    if (!fp)
        return -EINVAL;

    fwrite(buf, len, 1, fp);
    fclose(fp);
    free(buf);
    return 0;
}

int parse_native_functions(parse_t *parse, vector_t *functions, const char *arch) {
    function_t *f;
    native_t *native;

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

        if (f->native_flag)
            continue;
        f->native_flag = 1;

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

int parse_write_elf(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code, const char *arch, const char *out) {
    elf_context_t *elf = NULL;
    elf_section_t cs = {0};

    int ret = elf_open(&elf, arch, out, "wb");
    if (ret < 0) {
        loge("open '%s' elf file '%s' failed\n", arch, out);
        return ret;
    }

    cs.name = ".text";
    cs.sh_type = SHT_PROGBITS;
    cs.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    cs.sh_addralign = 1;
    cs.data = code->data;
    cs.data_len = code->len;
    cs.index = SHNDX_TEXT;

    ret = elf_add_section(elf, &cs);
    if (ret < 0)
        goto error;

    ret = _parse_add_ds(parse, elf, global_vars);
    if (ret < 0)
        goto error;

    ret = dwarf_debug_encode(parse->debug);
    if (ret < 0)
        goto error;

    ret = _add_debug_sections(parse, elf);
    if (ret < 0)
        goto error;

    qsort(parse->symtab->data, parse->symtab->size, sizeof(void *), _sym_cmp);

    ret = _parse_add_data_relas(parse, elf);
    if (ret < 0)
        goto error;

    ret = _parse_add_text_relas(parse, elf, functions);
    if (ret < 0)
        goto error;

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

    elf_sym_t *sym;
    int i;

    for (i = 0; i < parse->symtab->size; i++) {
        sym = parse->symtab->data[i];

        ret = elf_add_sym(elf, sym, ".symtab");
        if (ret < 0)
            goto error;
    }

    ret = elf_write_rel(elf);
error:
    elf_close(elf);
    return ret;
}

int64_t parse_fill_code2(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code, dwarf_info_entry_t **cu) {
    function_t *f;
    rela_t *r;

    int64_t offset = 0;

    int i;
    int j;

    for (i = 0; i < functions->size; i++) {
        f = functions->data[i];

        if (!f->node.define_flag)
            continue;

        if (!*cu) {
            int ret = _debug_add_cu(cu, parse, f, offset);
            if (ret < 0)
                return ret;
        }

        if (function_signature(parse->ast, f) < 0)
            return -ENOMEM;

        int ret = _fill_function_inst(code, f, offset, parse);
        if (ret < 0)
            return ret;

        ret = _parse_add_sym(parse, f->signature->data, f->code_bytes, offset, SHNDX_TEXT, ELF64_ST_INFO(STB_GLOBAL, STT_FUNC));
        if (ret < 0)
            return ret;

        for (j = 0; j < f->text_relas->size; j++) {
            r = f->text_relas->data[j];

            r->text_offset = offset + r->inst_offset;

            logd("rela text %s, text_offset: %#lx, offset: %ld, inst_offset: %d\n",
                 r->func->node.w->text->data, r->text_offset, offset, r->inst_offset);
        }

        for (j = 0; j < f->data_relas->size; j++) {
            r = f->data_relas->data[j];

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

        offset += f->code_bytes;
    }

    return offset;
}

int parse_fill_code(parse_t *parse, vector_t *functions, vector_t *global_vars, string_t *code) {
    int ret = _add_debug_file_names(parse);
    if (ret < 0)
        return ret;

    assert(parse->debug->file_names->size > 0);

    string_t *file_name = parse->debug->file_names->data[0];
    const char *path = file_name->data;

    ADD_SECTION_SYMBOL(SHNDX_TEXT, ".text");
    ADD_SECTION_SYMBOL(SHNDX_RODATA, ".rodata");
    ADD_SECTION_SYMBOL(SHNDX_DATA, ".data");

    dwarf_info_entry_t *cu = NULL;
    dwarf_line_result_t *r = NULL;
    dwarf_line_result_t *r2 = NULL;

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

    int64_t offset = parse_fill_code2(parse, functions, global_vars, code, &cu);
    if (offset < 0)
        return offset;

    if (cu)
        DEBUG_UPDATE_HIGH_PC(cu, offset);

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
    r->end_sequence = 1;

    if (vector_add(parse->debug->lines, r) < 0) {
        string_free(r->file_name);
        free(r);
        return -ENOMEM;
    }
    r = NULL;

    dwarf_abbrev_declaration_t *abbrev0 = NULL;

    abbrev0 = dwarf_abbrev_declaration_alloc();
    if (!abbrev0)
        return -ENOMEM;
    abbrev0->code = 0;

    if (vector_add(parse->debug->abbrevs, abbrev0) < 0) {
        dwarf_abbrev_declaration_free(abbrev0);
        return -ENOMEM;
    }

    return 0;
}

int parse_compile(parse_t *parse, const char *arch, int _3ac) {
    block_t *b = parse->ast->root_block;
    if (!b)
        return -EINVAL;

    vector_t *functions = vector_alloc();
    if (!functions)
        return -ENOMEM;

    int ret = node_search_bfs((node_t *)b, NULL, functions, -1, _find_function);
    if (ret < 0)
        goto error;

    logi("all functions: %d\n", functions->size);

    ret = parse_compile_functions(parse, functions);
    if (ret < 0)
        goto error;

    if (_3ac)
        goto error;

    ret = parse_native_functions(parse, functions, arch);
error:
    vector_free(functions);
    return ret;
}

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

    int ret = node_search_bfs((node_t *)b, NULL, functions, -1, _find_function);
    if (ret < 0) {
        vector_free(functions);
        return ret;
    }

    if (!strcmp(arch, "eda")) {
        ret = eda_write_cpk(parse, out, functions, NULL);

        vector_free(functions);
        return ret;
    }

    global_vars = vector_alloc();
    if (!global_vars) {
        ret = -ENOMEM;
        goto global_vars_error;
    }

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

    ret = parse_fill_code(parse, functions, global_vars, code);
    if (ret < 0) {
        loge("\n");
        goto error;
    }

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
