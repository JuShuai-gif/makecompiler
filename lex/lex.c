#include "lex.h"

// 关键词表
static key_word_t key_words[] = {
    {"if", LEX_WORD_KEY_IF},
    {"else", LEX_WORD_KEY_ELSE},
    {"endif", LEX_WORD_KEY_ENDIF},

    {"for", LEX_WORD_KEY_FOR},
    {"while", LEX_WORD_KEY_WHILE},
    {"do", LEX_WORD_KEY_DO},

    {"break", LEX_WORD_KEY_BREAK},
    {"continue", LEX_WORD_KEY_CONTINUE},

    {"switch", LEX_WORD_KEY_SWITCH},
    {"case", LEX_WORD_KEY_CASE},
    {"default", LEX_WORD_KEY_DEFAULT},

    {"return", LEX_WORD_KEY_RETURN},

    {"goto", LEX_WORD_KEY_GOTO},

    {"sizeof", LEX_WORD_KEY_SIZEOF},

    {"create", LEX_WORD_KEY_CREATE},

    {"operator", LEX_WORD_KEY_OPERATOR},

    {"_", LEX_WORD_KEY_UNDERLINE},

    {"char", LEX_WORD_KEY_CHAR},

    {"int", LEX_WORD_KEY_INT},
    {"float", LEX_WORD_KEY_FLOAT},
    {"double", LEX_WORD_KEY_DOUBLE},
    {"bit", LEX_WORD_KEY_BIT},
    {"bit2_t", LEX_WORD_KEY_BIT2},
    {"bit3_t", LEX_WORD_KEY_BIT3},
    {"bit4_t", LEX_WORD_KEY_BIT4},

    {"int8_t", LEX_WORD_KEY_INT8},
    {"int1_t", LEX_WORD_KEY_INT1},
    {"int2_t", LEX_WORD_KEY_INT2},
    {"int3_t", LEX_WORD_KEY_INT3},
    {"int4_t", LEX_WORD_KEY_INT4},
    {"int16_t", LEX_WORD_KEY_INT16},
    {"int32_t", LEX_WORD_KEY_INT32},
    {"int64_t", LEX_WORD_KEY_INT64},

    {"uint8_t", LEX_WORD_KEY_UINT8},
    {"uint16_t", LEX_WORD_KEY_UINT16},
    {"uint32_t", LEX_WORD_KEY_UINT32},
    {"uint64_t", LEX_WORD_KEY_UINT64},

    {"intptr_t", LEX_WORD_KEY_INTPTR},
    {"uintptr_t", LEX_WORD_KEY_UINTPTR},

    {"void", LEX_WORD_KEY_VOID},

    {"va_start", LEX_WORD_KEY_VA_START},
    {"va_arg", LEX_WORD_KEY_VA_ARG},
    {"va_end", LEX_WORD_KEY_VA_END},

    {"container", LEX_WORD_KEY_CONTAINER},

    {"class", LEX_WORD_KEY_CLASS},

    {"const", LEX_WORD_KEY_CONST},
    {"static", LEX_WORD_KEY_STATIC},
    {"extern", LEX_WORD_KEY_EXTERN},
    {"inline", LEX_WORD_KEY_INLINE},

    {"async", LEX_WORD_KEY_ASYNC},

    {"include", LEX_WORD_KEY_INCLUDE},
    {"define", LEX_WORD_KEY_DEFINE},

    {"enum", LEX_WORD_KEY_ENUM},
    {"union", LEX_WORD_KEY_UNION},
    {"struct", LEX_WORD_KEY_STRUCT},
};

// 转移字符
static escape_char_t escape_chars[] = {
    {'n', '\n'},
    {'r', '\r'},
    {'t', '\t'},
    {'0', '\0'},
};

// 挨个关键字进行比对
static int _find_key_word(const char *text) {
    int i;
    for (i = 0; i < sizeof(key_words) / sizeof(key_words[0]); i++) {
        if (!strcmp(key_words[i].text, text))
            return key_words[i].type;
    }

    return -1;
}

// 挨个转义字符进行比对
static int _find_escape_char(const int c) {
    int i;
    for (i = 0; i < sizeof(escape_chars) / sizeof(escape_chars[0]); i++) {
        if (escape_chars[i].origin == c)
            return escape_chars[i].escape; // return the escape char
    }

    // if it isn't in the escape array, return the original char
    return c;
}

// 根据参数 path 读取到 plex
int lex_open(lex_t **plex, const char *path) { 
    if (!plex || !path)
        return -EINVAL; // 错误参数

    // 分配一个 lex_t 空间
    lex_t *lex = calloc(1, sizeof(lex_t));
    printf("%p",lex);
    if (!lex)
        return -ENOMEM;

    // 打开文件
    lex->fp = fopen(path, "r");
    // 打开失败
    if (!lex->fp) {
        char cwd[4096];
        getcwd(cwd, 4095);
        loge("open file '%s' failed, errno: %d, default path dir: %s\n", path, errno, cwd);

        free(lex);
        return -1;
    }

    // 记录文件名称
    lex->file = string_cstr(path);
    // 记录失败
    if (!lex->file) {
        fclose(lex->fp);
        free(lex);
        return -ENOMEM;
    }

    // 个人习惯而言，有的喜欢用 0 作为开始，有的喜欢用 1 作为开始
    lex->nb_lines = 1;
    *plex = lex;
    return 0;
}

// 关闭 lex
int lex_close(lex_t *lex) {
    if (lex) {
        slist_clear(lex->char_list, char_t, next, free);
        slist_clear(lex->word_list, lex_word_t, next, lex_word_free);

        if (lex->macros) {
            vector_clear(lex->macros, (void (*)(void *))macro_free);
            vector_free(lex->macros);
        }

        string_free(lex->file);

        fclose(lex->fp);
        free(lex);
    }
    return 0;
}

// 往lex中push word
void lex_push_word(lex_t *lex, lex_word_t *w) {
    // 如果不为空
    if (lex && w) {
        // 这个是倒序插入的，这个需要注意，意思是在当前word上挂载lex的word_list
        w->next = lex->word_list;
        // 将挂载后的 w 再重新赋值给 word_list
        lex->word_list = w;
    }
}

// 处理 + 号
static int _lex_plus(lex_t *lex, lex_word_t **pword, char_t *c0) {
    // 将 lex 中的char吐出一个
    char_t *c1 = _lex_pop_char(lex);
    // 如果第一个 char 是 +
    if ('+' == c1->c) {
        // 再吐出一个 char
        char_t *c2 = _lex_pop_char(lex);

        // 如果第二个符号是 +
        if ('+' == c2->c)
            logw("+++ may cause a BUG in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

        // 再把 c2 送进去
        _lex_push_char(lex, c2);
        // 将 C2 置为 NULL
        c2 = NULL;

        // 分配一个 word
        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_INC);
        // 将 ++ 设置为 w 的 text
        w->text = string_cstr("++");
        // 将位置向后 移动 2 位
        lex->pos += 2;

        *pword = w;

        free(c1);
        c1 = NULL;
    } else if ('=' == c1->c) { // 两个符号 +=
                               // 先分配一个 word
        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_ADD_ASSIGN);
        // 将文本复制给 text
        w->text = string_cstr("+=");
        // 将位置位移 2 位
        lex->pos += 2;
        //
        *pword = w;
        free(c1);
        c1 = NULL;
    } else { // 一个char +
        _lex_push_char(lex, c1);
        c1 = NULL;

        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_PLUS);
        w->text = string_cstr("+");
        lex->pos++;

        *pword = w;
    }
    free(c0);
    c0 = NULL;
    return 0;
}

// 处理 - 号，--、-=、->、-
static int _lex_minus(lex_t *lex, lex_word_t **pword, char_t *c0) {
    // 吐出第一个符号 c1
    char_t *c1 = _lex_pop_char(lex);
    // 如果符号是 - 号
    if ('-' == c1->c) {
        // 再吐出第二个 char -
        char_t *c2 = _lex_pop_char(lex);

        // 如果第二个符号是 - 号
        if ('-' == c2->c)
            logw("--- may cause a BUG in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

        //
        _lex_push_char(lex, c2);
        c2 = NULL;

        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_DEC);
        w->text = string_cstr("--");
        lex->pos += 2;

        *pword = w;
        free(c1);
        c1 = NULL;
    } else if ('>' == c1->c) {
        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_ARROW);
        w->text = string_cstr("->");
        lex->pos += 2;

        *pword = w;
        free(c1);
        c1 = NULL;
    } else if ('=' == c1->c) {
        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_SUB_ASSIGN);
        w->text = string_cstr("-=");
        lex->pos += 2;

        *pword = w;
        free(c1);
        c1 = NULL;
    } else {
        _lex_push_char(lex, c1);
        c1 = NULL;

        lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_MINUS);
        w->text = string_cstr("-");
        lex->pos++;

        *pword = w;
    }

    free(c0);
    c0 = NULL;
    return 0;
}

// 处理 数字
static int _lex_number(lex_t *lex, lex_word_t **pword, char_t *c0) {
    lex_word_t *w = NULL;
    string_t *s = NULL;
    char_t *c1 = NULL;
    char_t *c2 = NULL;

    int ret = 0;
    // 如果第一个符号是0
    if ('0' == c0->c) {
        s = string_cstr_len(c0->utf8, 1);
        lex->pos++;

        free(c0);
        c0 = NULL;
        // 吐出 第二个char
        c1 = _lex_pop_char(lex);

        //
        string_cat_cstr_len(s, c1->utf8, 1);
        lex->pos++;

        // 判断第二个字符 char
        switch (c1->c) {
        // 0x、0X表示 16进制
        case 'x':
        case 'X': // base 16
            ret = _lex_number_base_16(lex, pword, s);
            break;
        // 0b 表示 二进制
        case 'b': // base 2
            ret = _lex_number_base_2(lex, pword, s);
            break;
        // 0. 表示双精度浮点数
        case '.': // double
            ret = _lex_double(lex, pword, s);
            break;
        //
        default:
            lex->pos--;

            s->data[--s->len] = "\0";

            _lex_push_char(lex, c1);
            // 0-9
            if (c1->c < '0' || c1->c > '9') {
                // 先分配一个 word
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_INT);
                //
                w->data.u64 = atoi(s->data);

                w->text = s;
                *pword = w;
                return 0;
            }

            c1 = NULL;
            return _lex_number_base_8(lex, pword, s);
            break;
        }
        free(c1);
        c1 = NULL;
        return ret;
    }

    // base 10
    s = string_alloc();
    _lex_push_char(lex, c0);
    c0 = NULL;
}

// 标识符
static int _lex_identity(lex_t *lex, lex_word_t **pword, char_t *c0) {
    string_t *s = string_cstr_len(c0->utf8, c0->len);

    lex->pos += c0->len;
    free(c0);
    c0 = NULL;

    while (1) {
        char_t *c1 = _lex_pop_char(lex);

        if ('_' == c1->c
            || ('a' <= c1->c && 'z' >= c1->c)
            || ('A' <= c1->c && 'Z' >= c1->c)
            || ('0' <= c1->c && '9' >= c1->c)
            || (0x4e00 <= c1->c && 0x9fa5 >= c1->c)) {
            string_cat_cstr_len(s, c1->utf8, c1->len);
            lex->pos += c1->len;

            free(c1);
            c1 = NULL;
        } else {
            _lex_push_char(lex, c1);
            c1 = NULL;

            lex_word_t *w = NULL;

            if (!strcmp(s->data, "NULL")) {
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
                if (w)
                    w->data.u64 = 0;

            } else if (!strcmp(s->data, "__LINE__")) {
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
                if (w)
                    w->data.u64 = lex->nb_lines;

            } else if (!strcmp(s->data, "__FILE__")) {
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_STRING);
                if (w)
                    w->data.s = string_clone(lex->file);

            } else if (!strcmp(s->data, "__func__")) {
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_STRING);
            } else {
                int type = _find_key_word(s->data);

                if (-1 == type)
                    w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_ID);
                else
                    w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type);
            }

            if (w)
                w->text = s;
            else
                string_free(s);
            s = NULL;

            *pword = w;
            return 0;
        }
    }
    return -1;
}

// char 字符
static int _lex_char(lex_t *lex, lex_word_t **pword, char_t *c0) {
    string_t *s = NULL;
    lex_word_t *w = NULL;
    char_t *c1 = _lex_pop_char(lex);
    char_t *c2 = _lex_pop_char(lex);
    char_t *c3;

    if ('\\' == c1->c) {
        c3 = _lex_pop_char(lex);

        if ('\'' == c3->c) {
            s = string_cstr_len(c0->utf8, 1);

            string_cat_cstr_len(s, c1->utf8, 1);
            string_cat_cstr_len(s, c2->utf8, c2->len);
            string_cat_cstr_len(s, c3->utf8, 1);

            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_CHAR);

            w->data.i64 = _find_escape_char(c2->c);
            lex->pos += c2->len + 3;
        } else
            loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

        free(c3);
        c3 = NULL;

    } else if ('\'' == c2->c) {
        s = string_cstr_len(c0->utf8, 1);

        string_cat_cstr_len(s, c1->utf8, c1->len);
        string_cat_cstr_len(s, c2->utf8, 1);

        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_CHAR);

        w->data.i64 = c1->c;
        lex->pos += c1->len + 2;
    } else
        loge("const char lost 2nd ' in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

    free(c0);
    free(c1);
    free(c2);

    if (!w)
        return -1;

    w->text = s;
    *pword = w;
    return 0;
}

// 字符串
static int _lex_string(lex_t *lex, lex_word_t **pword, char_t *c0) {
    lex_word_t *w = NULL;
    string_t *s = string_cstr_len(c0->utf8, 1);
    string_t *d = string_alloc();

    while (1) {
        char_t *c1 = _lex_pop_char(lex);

        if ('\"' == c1->c) {
            string_cat_cstr_len(s, c1->utf8, 1);

            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_STRING);
            w->data.s = d;
            w->text = s;
            d = NULL;
            s = NULL;

            lex->pos++;
            *pword = w;

            free(c1);
            c1 = NULL;
            return 0;

        } else if ('\\' == c1->c) {
            char_t *c2 = _lex_pop_char(lex);

            int ch2 = _find_escape_char(c2->c);

            string_cat_cstr_len(s, c1->utf8, 1);
            string_cat_cstr_len(s, c2->utf8, c2->len);
            lex->pos += 1 + c2->len;

            free(c2);
            free(c1);
            c2 = NULL;
            c1 = NULL;

            if (0 == ch2) {
                while (1) {
                    c1 = _lex_pop_char(lex);

                    if ('0' <= c1->c && c1->c <= '7') {
                        ch2 <<= 3;
                        ch2 += c1->c - '0';

                        string_cat_cstr_len(s, c1->utf8, 1);
                        lex->pos++;

                        free(c1);
                        c1 = NULL;
                    } else {
                        _lex_push_char(lex, c1);
                        break;
                    }
                }

                string_cat_cstr_len(d, (char *)&ch2, 1);
            } else
                string_cat_cstr_len(d, (char *)&ch2, 1);

        } else if (EOF == c1->c) {
            loge("const string lost 2nd \" in file: %s, line: %d\n", lex->file->data, lex->nb_lines);
            return -1;

        } else {
            string_cat_cstr_len(s, c1->utf8, c1->len);
            string_cat_cstr_len(d, c1->utf8, c1->len);
            lex->pos += c1->len;

            free(c1);
            c1 = NULL;
        }
    }
}

// 宏定义
static int _lex_macro(lex_t *lex) {
    char_t *h = NULL;
    char_t **pp = &h;
    char_t *c;
    char_t *c2;

    while (1) {
        c = _lex_pop_char(lex);
        if (!c) {
            slist_clear(h, char_t, next, free);
            return -1;
        }

        *pp = c;
        pp = &c->next;

        if (EOF == c->c)
            break;

        if ('\n' == c->c) {
            c->flag = UTF8_LF;
            break;
        }

        if ('\\' == c->c) {
            c2 = _lex_pop_char(lex);
            if (!c2) {
                slist_clear(h, char_t, next, free);
                return -1;
            }

            *pp = c2;
            pp = &c2->next;

            if (EOF == c2->c)
                break;

            if ('\n' == c2->c)
                c2->flag = 0;
        }
    }

    *pp = lex->char_list;
    lex->char_list = h;
    return 0;
}

// 从输入流中丢弃字符，直到遇到指定的结束符号为止（c0，或者 c0+c1 的组合）。同时它还会维护行号和列号。
static void _lex_drop_to(lex_t *lex, int c0, int c1) {
    char_t *c = NULL;

    while (1) {
        if (!c)
            c = _lex_pop_char(lex);

        if (EOF == c->c) {
            _lex_push_char(lex, c);
            break;
        }

        int tmp = c->c;
        free(c);
        c = NULL;

        if ('\n' == tmp) {
            lex->nb_lines++;
            lex->pos = 0;
        }

        if (c0 == tmp) {
            if (c1 < 0)
                break;

            c = _lex_pop_char(lex);

            if (c1 == c->c) {
                free(c);
                c = NULL;
                break;
            }
        }
    }
}

// 吐出 词法单元 word
int __lex_pop_word(lex_t *lex, lex_word_t **pword) {
    list_t *l = NULL;
    char_t *c = NULL;
    lex_word_t *w = NULL;

    if (lex->word_list) {
        w = lex->word_list;
        lex->word_list = w->next;
        *pword = w;
        return 0;
    }

    c = _lex_pop_char(lex);

    // 如果遇到 换行 制表符 空格 等一些符号时，表示它是一个单词的结尾
    while ('\n' == c->c
           || '\r' == c->c || '\t' == c->c
           || ' ' == c->c || '\\' == c->c) {
        if ('\n' == c->c) {
            lex->nb_lines++;
            lex->pos = 0;

            if (UTF8_LF == c->flag) {
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_LF);
                w->text = string_cstr("LF");
                *pword = w;

                free(c);
                c = NULL;
                return 0;
            }
        } else
            lex->pos++;

        free(c);
        c = _lex_pop_char(lex);
    }
    // 文件末尾
    if (EOF == c->c) {
        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_EOF);
        w->text = string_cstr("eof");

        *pword = w;

        free(c);
        c = NULL;
        return 0;
    }

    if ('+' == c->c)
        return _lex_plus(lex, pword, c);

    if ('-' == c->c)
        return _lex_minus(lex, pword, c);

    if ('*' == c->c) {
        char c1 = '=';
        int t1 = LEX_WORD_MUL_ASSIGN;
        return _lex_op2_ll1(lex, pword, c, LEX_WORD_STAR, &c1, &t1, 1);
    }

    if ('/' == c->c) {
        char c1 = '=';
        int t1 = LEX_WORD_DIV_ASSIGN;

        char_t *c2 = _lex_pop_char(lex);

        switch (c2->c) {
        case '/':
            _lex_drop_to(lex, '\n', -1);
            break;

        case '*':
            _lex_drop_to(lex, '*', '/');
            break;

        default:
            _lex_push_char(lex, c2);

            return _lex_op2_ll1(lex, pword, c, LEX_WORD_DIV, &c1, &t1, 1);
            break;
        };

        free(c);
        free(c2);
        c = NULL;
        c2 = NULL;

        return __lex_pop_word(lex, pword);
    }

    if ('%' == c->c) {
        char c1 = '=';
        int t1 = LEX_WORD_MOD_ASSIGN;

        return _lex_op2_ll1(lex, pword, c, LEX_WORD_MOD, &c1, &t1, 1);
    }

    switch (c->c) {
    case '~':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_BIT_NOT);
        break;

    case '(':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_LP);
        break;
    case ')':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_RP);
        break;
    case '[':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_LS);
        break;
    case ']':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_RS);
        break;
    case '{':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_LB);
        break;
    case '}':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_RB);
        break;

    case ',':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_COMMA);
        break;
    case ';':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_SEMICOLON);
        break;
    case ':':
        return _lex_op1_ll1(lex, pword, c, LEX_WORD_COLON);
        break;

    default:
        break;
    };

    if ('&' == c->c) {
        char chs[2] = {'&', '='};
        int types[2] = {LEX_WORD_LOGIC_AND, LEX_WORD_BIT_AND_ASSIGN};

        return _lex_op2_ll1(lex, pword, c, LEX_WORD_BIT_AND, chs, types, 2);
    }

    if ('|' == c->c) {
        char chs[2] = {'|', '='};
        int types[2] = {LEX_WORD_LOGIC_OR, LEX_WORD_BIT_OR_ASSIGN};

        return _lex_op2_ll1(lex, pword, c, LEX_WORD_BIT_OR, chs, types, 2);
    }

    if ('!' == c->c) {
        char c1 = '=';
        int t1 = LEX_WORD_NE;

        return _lex_op2_ll1(lex, pword, c, LEX_WORD_LOGIC_NOT, &c1, &t1, 1);
    }

    if ('=' == c->c) {
        char c1 = '=';
        int t1 = LEX_WORD_EQ;

        return _lex_op2_ll1(lex, pword, c, LEX_WORD_ASSIGN, &c1, &t1, 1);
    }

    if ('>' == c->c)
        return _lex_op3_ll1(lex, pword, c, '>', '=', '=',
                            LEX_WORD_SHR_ASSIGN, LEX_WORD_SHR, LEX_WORD_GE, LEX_WORD_GT);

    if ('<' == c->c)
        return _lex_op3_ll1(lex, pword, c, '<', '=', '=',
                            LEX_WORD_SHL_ASSIGN, LEX_WORD_SHL, LEX_WORD_LE, LEX_WORD_LT);

    if ('.' == c->c)
        return _lex_dot(lex, pword, c);

    if ('\'' == c->c)
        return _lex_char(lex, pword, c);

    if ('\"' == c->c) {
        lex_word_t *w0 = NULL;
        lex_word_t *w1 = NULL;

        int ret = _lex_string(lex, &w0, c);
        if (ret < 0) {
            *pword = NULL;
            return ret;
        }

        while (1) {
            ret = __lex_pop_word(lex, &w1);
            if (ret < 0) {
                lex_word_free(w0);
                *pword = NULL;
                return ret;
            }

            if (LEX_WORD_CONST_STRING != w1->type) {
                lex_push_word(lex, w1);
                break;
            }

            if (string_cat(w0->text, w1->text) < 0
                || string_cat(w0->data.s, w1->data.s) < 0) {
                lex_word_free(w1);
                lex_word_free(w0);
                *pword = NULL;
                return -1;
            }

            lex_word_free(w1);
        }
        logd("w0: %s\n", w0->data.s->data);
        *pword = w0;
        return 0;
    }

    if ('0' <= c->c && '9' >= c->c)
        return _lex_number(lex, pword, c);

    if ('_' == c->c
        || ('a' <= c->c && 'z' >= c->c)
        || ('A' <= c->c && 'Z' >= c->c)
        || (0x4e00 <= c->c && 0x9fa5 >= c->c)) { // support China chars

        return _lex_identity(lex, pword, c);
    }

    if ('#' == c->c) {
        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_HASH);

        free(c);
        c = _lex_pop_char(lex);

        if ('#' == c->c) {
            w->type = LEX_WORD_HASH2;
            w->text = string_cstr("##");

            lex->pos += 2;
            free(c);
        } else {
            w->text = string_cstr("#");

            lex->pos++;
            _lex_push_char(lex, c);
        }
        c = NULL;
        *pword = w;
        return _lex_macro(lex);
    }

    loge("unknown char: %c, utf: %#x, in file: %s, line: %d\n", c->c, c->c, lex->file->data, lex->nb_lines);
    return -1;
}

// 解析宏定义参数
static int __parse_macro_argv(lex_t *lex, macro_t *m) {
    lex_word_t *w = NULL;
    lex_word_t *w2;

    int comma = 0;
    int id = 0;

    while (1) {
        int ret = __lex_pop_word(lex, &w);
        if (ret < 0)
            return ret;

        if (!comma) {
            if (LEX_WORD_RP == w->type) {
                lex_word_free(w);
                break;
            }

            if (LEX_WORD_COMMA == w->type) {
                lex_word_free(w);
                w = NULL;

                if (!id) {
                    loge("an identity is needed before ',' in file: %s, line: %d\n", w->file->data, w->line);
                    return -1;
                }

                id = 0;
                comma = 1;
                continue;
            }
        }

        if (!lex_is_identity(w)) {
            loge("macro arg '%s' should be an identity, file: %s, line: %d\n", w->text->data, w->file->data, w->line);
            lex_word_free(w);
            return -1;
        }

        if (id) {
            loge("',' is needed before macro arg '%s', file: %s, line: %d\n", w->text->data, w->file->data, w->line);
            lex_word_free(w);
            return -1;
        }

        int i;
        for (i = 0; i < m->argv->size; i++) {
            w2 = m->argv->data[i];

            if (!string_cmp(w2->text, w->text)) {
                loge("macro has same args '%s', file: %s, line: %d\n", w->text->data, w->file->data, w->line);
                lex_word_free(w);
                return -1;
            }
        }

        logw("macro '%s' arg: %s\n", m->w->text->data, w->text->data);

        ret = vector_add(m->argv, w);
        if (ret < 0) {
            lex_word_free(w);
            return ret;
        }
        w = NULL;

        id = 1;
        comma = 0;
    }

    return 0;
}

// 解析宏定义
static int __parse_macro_define(lex_t *lex) {
    lex_word_t **pp;
    lex_word_t *w = NULL;
    macro_t *m;
    macro_t *m0;

    int ret = __lex_pop_word(lex, &w);
    if (ret < 0)
        return ret;

    if (!lex_is_identity(w)) {
        loge("macro '%s' should be an identity, file: %s, line: %d\n", w->text->data, w->file->data, w->line);
        lex_word_free(w);
        return -1;
    }

    m = macro_alloc(w);
    if (!m) {
        lex_word_free(w);
        return -ENOMEM;
    }

    w = NULL;
    ret = __lex_pop_word(lex, &w);
    if (ret < 0) {
        macro_free(m);
        return ret;
    }

    pp = &m->text_list;

    if (LEX_WORD_LP == w->type) {
        lex_word_free(w);
        w = NULL;

        m->argv = vector_alloc();
        if (!m->argv) {
            macro_free(m);
            return -ENOMEM;
        }

        ret = __parse_macro_argv(lex, m);
        if (ret < 0) {
            macro_free(m);
            return ret;
        }
    } else {
        *pp = w;
        pp = &w->next;
        w = NULL;
    }

    while (1) {
        ret = __lex_pop_word(lex, &w);
        if (ret < 0) {
            macro_free(m);
            return ret;
        }

        if (LEX_WORD_LF == w->type) {
            lex_word_free(w);
            w = NULL;
            break;
        }

        *pp = w;
        pp = &w->next;
        w = NULL;
    }

    if (!lex->macros) {
        lex->macros = vector_alloc();
        if (!lex->macros)
            return -ENOMEM;

    } else {
        int i;
        for (i = lex->macros->size - 1; i >= 0; i--) {
            m0 = lex->macros->data[i];

            if (!string_cmp(m->w->text, m0->w->text)) {
                logw("macro '%s' defined before in file: %s, line: %d\n",
                     m0->w->text->data, m0->w->file->data, m0->w->line);
                break;
            }
        }
    }

    ret = vector_add(lex->macros, m);
    if (ret < 0) {
        macro_free(m);
        return ret;
    }

    return 0;
}

// 填充宏定义
static int __fill_macro_argv(lex_t *lex, macro_t *m, lex_word_t *use, vector_t *argv) {
    lex_word_t **pp;
    lex_word_t *w = NULL;

    int ret = __lex_pop_word(lex, &w);
    if (ret < 0)
        return ret;

    if (LEX_WORD_LP != w->type) {
        loge("macro '%s' needs args, file: %s, line: %d\n", m->w->text->data, w->file->data, w->line);
        lex_word_free(w);
        return -1;
    }

    lex_word_free(w);
    w = NULL;

    int n_lps = 0;
    int n_rps = 0;
    int i;

    pp = NULL;

    while (1) {
        ret = __lex_pop_word(lex, &w);
        if (ret < 0)
            return ret;

        if (LEX_WORD_COMMA == w->type) {
            if (!pp) {
                loge("unexpected ',' in macro '%s', file: %s, line: %d\n", m->w->text->data, w->file->data, w->line);

                lex_word_free(w);
                ret = -1;
                goto error;
            }

            lex_word_free(w);
            w = NULL;

            if (n_rps == n_lps)
                pp = NULL;
            continue;

        } else if (LEX_WORD_LP == w->type)
            n_lps++;
        else if (LEX_WORD_RP == w->type) {
            n_rps++;

            if (n_rps > n_lps) {
                lex_word_free(w);
                w = NULL;
                break;
            }
        }

        w->next = NULL;

        if (pp)
            *pp = w;
        else {
            ret = vector_add(argv, w);
            if (ret < 0) {
                lex_word_free(w);
                goto error;
            }
        }

        pp = &w->next;
        w = NULL;
    }

    if (m->argv->size != argv->size) {
        loge("macro '%s' needs %d args, but in fact give %d args,  file: %s, line: %d\n",
             m->w->text->data, m->argv->size, argv->size, use->file->data, use->line);
        ret = -1;
        goto error;
    }

    return 0;

error:
    for (i = 0; i < argv->size; i++) {
        w = argv->data[i];
        slist_clear(w, lex_word_t, next, lex_word_free);
    }

    argv->size = 0;
    return ret;
}

// 转换 str
static int __convert_str(lex_word_t *h) {
    lex_word_t *w;
    string_t *s;

    if (h->type != LEX_WORD_CONST_STRING) {
        h->type = LEX_WORD_CONST_STRING;

        h->data.s = string_clone(h->text);
        if (!h->data.s)
            return -ENOMEM;
    }

    while (h->next) {
        w = h->next;

        if (LEX_WORD_CONST_STRING != w->type)
            s = w->text;
        else
            s = w->data.s;

        int ret = string_cat(h->data.s, s);
        if (ret < 0)
            return ret;

        ret = string_cat(h->text, w->text);
        if (ret < 0)
            return ret;

        h->next = w->next;

        lex_word_free(w);
        w = NULL;
    }

    logw("h: '%s', file: %s, line: %d\n", h->data.s->data, h->file->data, h->line);
    return 0;
}

// 寻找宏定义
static macro_t *__find_macro(lex_t *lex, lex_word_t *w) {
    if (!lex->macros)
        return NULL;

    macro_t *m;
    int i;

    for (i = lex->macros->size - 1; i >= 0; i--) {
        m = lex->macros->data[i];

        if (!string_cmp(m->w->text, w->text))
            return m;
    }

    return NULL;
}

// 使用宏
static int __use_macro(lex_t *lex, macro_t *m, lex_word_t *use) {
    lex_word_t **pp;
    lex_word_t *p;
    lex_word_t *h;
    lex_word_t *w;
    lex_word_t *prev;
    vector_t *argv = NULL;

    if (m->argv) {
        argv = vector_alloc();
        if (!argv)
            return -ENOMEM;

        int ret = __fill_macro_argv(lex, m, use, argv);
        if (ret < 0) {
            vector_free(argv);
            return ret;
        }
    }

    h = NULL;
    pp = &h;

    int ret = 0;
    int hash = 0;
    int i;

    for (p = m->text_list; p; p = p->next) {
        if (LEX_WORD_HASH == p->type) {
            hash = 1;
            continue;
        }

        logd("p: '%s', line: %d, hash: %d\n", p->text->data, p->line, hash);

        if (m->argv) {
            assert(argv);
            assert(argv->size >= m->argv->size);

            for (i = 0; i < m->argv->size; i++) {
                w = m->argv->data[i];

                if (!string_cmp(w->text, p->text))
                    break;
            }

            if (i < m->argv->size) {
                lex_word_t **tmp = pp;

                for (w = argv->data[i]; w; w = w->next) {
                    *pp = lex_word_clone(w);
                    if (!*pp) {
                        ret = -ENOMEM;
                        goto error;
                    }

                    if (!strcmp((*pp)->text->data, "__LINE__"))
                        (*pp)->data.u64 = use->line;

                    pp = &(*pp)->next;
                }

                if (1 == hash) {
                    ret = __convert_str(*tmp);
                    if (ret < 0)
                        goto error;

                    pp = &(*tmp)->next;
                }

                hash = 0;
                continue;
            }
        }

        *pp = lex_word_clone(p);
        if (!*pp) {
            ret = -ENOMEM;
            goto error;
        }

        if (!strcmp((*pp)->text->data, "__LINE__"))
            (*pp)->data.u64 = use->line;

        pp = &(*pp)->next;

        hash = 0;
    }

error:
    if (argv) {
        for (i = 0; i < argv->size; i++) {
            w = argv->data[i];

            if (w)
                slist_clear(w, lex_word_t, next, lex_word_free);
        }

        vector_free(argv);
        argv = NULL;
    }

    if (ret < 0) {
        slist_clear(h, lex_word_t, next, lex_word_free);
        return ret;
    }

#if 0
	w = h;
	while (w) {
		logi("---------- %s, line: %d\n", w->text->data, w->line);
		w = w->next;
	}
#endif
    *pp = lex->word_list;
    lex->word_list = h;
    return 0;
}

static int __use_hash2(lex_t *lex, lex_word_t *prev) {
    lex_word_t *after = NULL;

    int ret = __lex_pop_word(lex, &after);
    if (ret < 0)
        return ret;

    switch (after->type) {
    case LEX_WORD_ID:
        ret = string_cat(prev->text, after->text);
        break;

    default:
        ret = -1;
        loge("needs identity after '##', file: %s, line: %d\n", after->file->data, after->line);
        break;
    };

    lex_word_free(after);
    return ret;
}

int __lex_use_macro(lex_t *lex, lex_word_t **pp) {
    lex_word_t *w1 = NULL;
    lex_word_t *w = *pp;
    macro_t *m;

    *pp = NULL;

    while (LEX_WORD_ID == w->type) {
        m = __find_macro(lex, w);
        if (m) {
            int ret = __use_macro(lex, m, w);

            lex_word_free(w);
            w = NULL;
            if (ret < 0)
                return ret;

            ret = __lex_pop_word(lex, &w);
            if (ret < 0)
                return ret;
            continue;
        }

        int ret = __lex_pop_word(lex, &w1);
        if (ret < 0) {
            lex_word_free(w);
            return ret;
        }

        if (LEX_WORD_HASH2 != w1->type) {
            lex_push_word(lex, w1);
            break;
        }

        lex_word_free(w1);
        w1 = NULL;

        ret = __use_hash2(lex, w);
        if (ret < 0) {
            lex_word_free(w);
            return ret;
        }
    }

    *pp = w;
    return 0;
}

// 吐出一个词法单元 word
int lex_pop_word(lex_t *lex, lex_word_t **pword) {
    if (!lex || !lex->fp || !pword)
        return -EINVAL;

    lex_word_t *w = NULL;
    lex_word_t *w1 = NULL;

    // 从 lex 中吐出一个 word
    int ret = __lex_pop_word(lex, &w);
    if (ret < 0)
        return ret;

    // 处理宏（如果遇到的是 # 开头的 hash word）
    while (LEX_WORD_HASH == w->type) {
        // 再弹出下一个词，查看 hash 后面的关键字
        ret = __lex_pop_word(lex, &w1);
        if (ret < 0) {
            lex_word_free(w); // 释放上一个 word
            return ret;
        }

        // 根据宏关键字类型进行分支处理
        switch (w1->type) {
        case LEX_WORD_KEY_INCLUDE: // #include
        case LEX_WORD_KEY_IF:      // #if
        case LEX_WORD_KEY_ELSE:    // #else
        case LEX_WORD_KEY_ENDIF:   // #endif
            // 这些关键字直接推回词法流，返回 w 给调用方
            lex_push_word(lex, w1);
            *pword = w;
            return 0;
            break;

        case LEX_WORD_KEY_DEFINE: // #define
            // 处理宏定义
            ret = __parse_macro_define(lex);
            break;

        default:
            // 遇到未知宏，报错并返回 -1
            loge("unknown macro '%s', file: %s, line: %d\n",
                 w1->text->data, w1->file->data, w1->line);
            ret = -1;
            break;
        };

        // 释放已经用过的词
        lex_word_free(w);
        lex_word_free(w1);
        w = NULL;
        w1 = NULL;

        // 如果处理宏时出错，直接返回
        if (ret < 0)
            return ret;

        // 宏处理完成后，再弹出一个新词，继续检查是否还有宏
        ret = __lex_pop_word(lex, &w);
        if (ret < 0)
            return ret;
    }

    // 对取出的单词尝试做宏展开（比如替换掉宏名为宏定义内容）
    ret = __lex_use_macro(lex, &w);
    if (ret < 0)
        return ret;

    // 将最终得到的词返回给调用方
    *pword = w;
    return 0;
}
