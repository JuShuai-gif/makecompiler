#include "lex.h"

// 从 lex_t 吐出一个char
char_t *_lex_pop_char(lex_t *lex) {
    assert(lex);
    assert(lex->fp);

    char_t *c;

    if (lex->char_list) {
        c = lex->char_list;
        lex->char_list = c->next;
        return c;
    }

    c = calloc(1, sizeof(char_t));
    if (!c)
        return NULL;

    int ret = fgetc(lex->fp);

    if (EOF == ret) {
        c->c = ret;
        return c;
    }

    if (ret < 0x80) {
        c->c = ret;
        c->len = 1;
        c->utf8[0] = ret;
        return c;
    }

    if (0x6 == (ret >> 5)) {
        c->c = ret & 0x1f;
        c->len = 2;
    } else if (0xe == (ret >> 4)) {
        c->c = ret & 0xf;
        c->len = 3;
    } else if (0x1e == ret >> 3) {
        c->c = ret & 0x7;
        c->len = 4;
    } else if (0x3e == (ret >> 2)) {
        c->c = ret & 0x3;
        c->len = 5;
    } else if (0x7e == (ret >> 1)) {
        c->c = ret & 0x1;
        c->len = 6;
    } else {
        loge("utf8 first byte wrong %#x, file: %s, line: %d\n", ret, lex->file->data, lex->nb_lines);
        free(c);
        return NULL;
    }

    c->utf8[0] = ret;

    int i;
    for (i = 0; i < c->len; i++) {
        ret = fgetc(lex->fp);

        if (0x2 == (ret >> 6)) {
            c->c <<= 6;
            c->c |= ret & 0x3f;

            c->utf8[i] = ret;
        } else {
            loge("utf8 byte[%d] wrong %#x, file: %s, line: %d\n", i + 1, ret, lex->file->data, lex->nb_lines);
            free(c);
            return NULL;
        }
    }
    return c;
}

// 追加一个char_t
void _lex_push_char(lex_t *lex, char_t *c) {
    assert(lex);
    assert(c);

    c->next = lex->char_list;
    lex->char_list = c;
}

// 处理单字符运算符，比如+ - * 、 ( )
int _lex_op1_ll1(lex_t *lex, lex_word_t **pword, char_t *c0, int type0) {
    // 用C0(已经读出的第一个字符)创建字符串s
    string_t *s = string_cstr_len(c0->utf8, c0->len);
    // 创建一个 token w 类型 type0
    lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);
    // 位置 pos 增加这个字符的长度
    lex->pos += c0->len;
    // 把字符串内容挂到 w->text 上
    w->text = s;
    // 释放 c0，返回 token
    s = NULL;
    // 释放 c0
    free(c0);
    // 将 c0置为空
    c0 = NULL;
    // 设置词法单元链表的元素为 w 词法单元
    *pword = w;
    return 0;
}

// 识别一字符或两字符的运算符，例如 = vs ==,| vs !=,< vs <=,> vs >=
int _lex_op2_ll1(lex_t *lex, lex_word_t **pword, char_t *c0, int type0, char *chs, int *types, int n) {
    // 用 c0 建立初始字符串 s，创建 token w
    lex_word_t *w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);
    // 读取下一个字符 c1
    string_t *s = string_cstr_len(c0->utf8, c0->len);
    char_t *c1 = _lex_pop_char(lex);

    lex->pos += c0->len;
    // 在 chs 表里查找 c1->c 是否是二字符运算符的候选
    int i;
    for (i = 0; i < n; i++) {
        if (chs[i] == c1->c)
            break;
    }
    // 如果匹配：把 c1 拼接到 s，更新 token 类型为对应的 types[i]
    if (i < n) {
        string_cat_cstr_len(s, c1->utf8, c1->len);

        w->type = types[i];
        lex->pos += c1->len;
        free(c1);
    } else // 如果不匹配：把 c1 推回输入流，保持原样
        _lex_push_char(lex, c1);
    // 最终 token w->text = 单字符或双字符
    c1 = NULL;

    w->text = s;
    s = NULL;

    free(c0);
    c0 = NULL;

    *pword = w;
    return 0;
}

// 识别一字符、二字符、三字符运算符，例如：
// &、&&、&=
// |、||、|=
// -、->
// <<、<<=
int _lex_op3_ll1(lex_t *lex, lex_word_t **pword, char_t *c0,
                 char ch1_0, char ch1_1, char ch2, int type0, int type1, int type2, int type3) {
    // 读取下一个字符 c1
    char_t *c1 = _lex_pop_char(lex);
    char_t *c2 = NULL;
    lex_word_t *w = NULL;
    // 用 c0 建立初始字符串 s
    string_t *s = string_cstr_len(c0->utf8, c0->len);
    // case1: 如果 c1 == ch1_0
    if (ch1_0 == c1->c) {
        // 拼接 c1，再看 c2
        string_cat_cstr_len(s, c1->utf8, c1->len);

        c2 = _lex_pop_char(lex);
        // 如果 c2 == ch2 → 拼接 c2，token 类型 = type0（三字符运算符）
        if (ch2 == c2->c) {
            string_cat_cstr_len(s, c2->utf8, c2->len);

            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type0);

            w->text = s;
            s = NULL;
            lex->pos += c0->len + c1->len + c2->len;

            free(c2);
            c2 = NULL;

        } else { // 否则 → 回退 c2，token 类型 = type1（二字符运算符）
            _lex_push_char(lex, c2);
            c2 = NULL;

            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type1);
            w->text = s;
            s = NULL;
            lex->pos += c0->len + c1->len;
        }
        free(c1);
        c1 = NULL;

    } else if (ch1_1 == c1->c) { // case2: 如果 c1 == ch1_1
        // 拼接 c1，token 类型 = type2（二字符运算符的另一种情况）
        string_cat_cstr_len(s, c1->utf8, c1->len);

        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type2);
        w->text = s;
        s = NULL;
        lex->pos += c0->len + c1->len;

        free(c1);
        c1 = NULL;
    } else { // case3: 否则
        // 回退 c1，token 类型 = type3（单字符运算符）
        _lex_push_char(lex, c1);
        c1 = NULL;

        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, type3);

        w->text = s;
        s = NULL;
        lex->pos += c0->len;
    }

    free(c0);
    c0 = NULL;
    *pword = w;
    return 0;
}

int _lex_number_base_10(lex_t *lex, lex_word_t **pword, string_t *s) {
    char_t *c2;
    char_t *c3;
    lex_word_t *w;

    int dot = 0;
    int exp = 0;
    int neg = 0;
    uint64_t value = 0;
    uint64_t num;

    while (1) {
        c2 = _lex_pop_char(lex);

        if (c2->c >= '0' && c2->c <= '9') {
            num = c2->c - '0';
            value *= 10;
            value += num;
        } else if ('.' == c2->c) {
            c3 = _lex_pop_char(lex);

            _lex_push_char(lex, c3);

            if ('.' == c3->c) {
                c3 = NULL;

                _lex_push_char(lex, c2);

                c2 = NULL;
                break;
            }

            c3 = NULL;

            if (++dot > 1) {
                loge("\n");
                return -EINVAL;
            }
        } else if ('e' == c2->c || 'E' == c2->c) {
            exp++;

            if (exp > 1) {
                loge("\n");
                return -EINVAL;
            }

        } else if ('-' == c2->c) {
            neg++;
            if (0 == exp || neg > 1) {
                loge("\n");
                return -EINVAL;
            }

        } else if ('_' == c2->c) {
        } else {
            _lex_push_char(lex, c2);
            c2 = NULL;
            break;
        }

        assert(1 == c2->len);
        string_cat_cstr_len(s, c2->utf8, 1);
        lex->pos++;
        free(c2);
        c2 = NULL;
    }

    if (exp > 0 || dot > 0) {
        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_DOUBLE);
        w->data.d = atof(s->data);
    } else {
        if (value & ~0xffffffffULL)
            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
        else
            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U32);

        w->data.u64 = value;
    }

    w->text = s;
    s = NULL;

    *pword = w;
    return 0;
}

int _lex_number_base_16(lex_t *lex, lex_word_t **pword, string_t *s) {
    char_t *c2;
    lex_word_t *w;

    uint64_t value = 0;
    uint64_t value2;

    while (1) {
        c2 = _lex_pop_char(lex);

        if (c2->c >= '0' && c2->c <= '9')
            value2 = c2->c - '0';

        else if ('a' <= c2->c && 'f' >= c2->c)
            value2 = c2->c - 'a' + 10;

        else if ('A' <= c2->c && 'F' >= c2->c)
            value2 = c2->c - 'A' + 10;

        else if ('_' == c2->c) {
            assert(1 == c2->len);
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            free(c2);
            c2 = NULL;

        } else {
            _lex_push_char(lex, c2);
            c2 = NULL;

            if (value & ~0xffffffffULL)
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
            else
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U32);

            w->data.u64 = value;

            w->text = s;
            s = NULL;

            *pword = w;
            return 0;
        }

        value <<= 4;
        value += value2;

        assert(1 == c2->len);
        string_cat_cstr_len(s, c2->utf8, 1);

        lex->pos++;
        free(c2);
        c2 = NULL;
    }
}

int _lex_number_base_8(lex_t *lex, lex_word_t **pword, string_t *s) {
    char_t *c2;
    lex_word_t *w;

    uint64_t value = 0;

    while (1) {
        c2 = _lex_pop_char(lex);

        if (c2->c >= '0' && c2->c <= '7') {
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            value = (value << 3) + c2->c - '0';

            free(c2);
            c2 = NULL;

        } else if ('8' == c2->c || '9' == c2->c) {
            loge("number must be 0-7 when base 8");

            free(c2);
            c2 = NULL;
            return -1;

        } else if ('_' == c2->c) {
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            free(c2);
            c2 = NULL;

        } else {
            _lex_push_char(lex, c2);
            c2 = NULL;

            if (value & ~0xffffffffULL)
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
            else
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U32);
            w->data.u64 = value;

            w->text = s;
            s = NULL;

            *pword = w;
            return 0;
        }
    }
}

int _lex_number_base_2(lex_t *lex, lex_word_t **pword, string_t *s) {
    char_t *c2;
    lex_word_t *w;

    uint64_t value = 0;

    while (1) {
        c2 = _lex_pop_char(lex);

        if (c2->c >= '0' && c2->c <= '1') {
            assert(1 == c2->len);
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            value = (value << 1) + c2->c - '0';

            free(c2);
            c2 = NULL;

        } else if (c2->c >= '2' && c2->c <= '9') {
            loge("number must be 0-1 when base 2");

            free(c2);
            c2 = NULL;
            return -1;

        } else if ('_' == c2->c) {
            assert(1 == c2->len);
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            free(c2);
            c2 = NULL;

        } else {
            _lex_push_char(lex, c2);
            c2 = NULL;

            if (value & ~0xffffffffULL)
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U64);
            else
                w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_U32);
            w->data.u64 = value;

            w->text = s;
            s = NULL;

            *pword = w;
            return 0;
        }
    }
}

int _lex_double(lex_t *lex, lex_word_t **pword, string_t *s) {
    lex_word_t *w;
    char_t *c2;

    while (1) {
        c2 = _lex_pop_char(lex);

        if (c2->c >= '0' && c2->c <= '9') {
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            free(c2);
            c2 = NULL;
        } else if ('.' == c2->c) {
            loge("too many '.' for number in file: %s, line: %d\n", lex->file->data, lex->nb_lines);

            free(c2);
            c2 = NULL;
            return -1;
        } else {
            _lex_push_char(lex, c2);
            c2 = NULL;
            break;
        }
    }

    w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_CONST_DOUBLE);
    w->data.d = atof(s->data);

    w->text = s;
    *pword = w;
    return 0;
}

int _lex_dot(lex_t *lex, lex_word_t **pword, char_t *c0) {
    char_t *c1 = _lex_pop_char(lex);
    char_t *c2 = NULL;
    lex_word_t *w = NULL;
    lex_word_t *w1 = NULL;
    lex_word_t *w2 = NULL;
    string_t *s = string_cstr_len(c0->utf8, c0->len);

    lex->pos += c0->len;

    free(c0);
    c0 = NULL;

    if ('.' == c1->c) {
        string_cat_cstr_len(s, c1->utf8, 1);
        lex->pos++;

        free(c1);
        c1 = NULL;

        c2 = _lex_pop_char(lex);

        if ('.' == c2->c) {
            string_cat_cstr_len(s, c2->utf8, 1);
            lex->pos++;

            free(c2);
            c2 = NULL;

            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_VAR_ARGS);
            w->text = s;
            s = NULL;
        } else {
            w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_RANGE);
            w->text = s;
            s = NULL;

            _lex_push_char(lex, c2);
            c2 = NULL;
        }

    } else {
        w = lex_word_alloc(lex->file, lex->nb_lines, lex->pos, LEX_WORD_DOT);
        w->text = s;
        s = NULL;

        _lex_push_char(lex, c1);
        c1 = NULL;

        int ret = __lex_pop_word(lex, &w1);
        if (ret < 0) {
            lex_word_free(w);
            return ret;
        }

        if (LEX_WORD_CONST_CHAR <= w1->type && w1->type <= LEX_WORD_CONST_U64) {
            ret = __lex_pop_word(lex, &w2);
            if (ret < 0) {
                lex_word_free(w);
                lex_word_free(w1);
                return ret;
            }

            lex_push_word(lex, w2);

            if (w2->type != LEX_WORD_ASSIGN && w2->type != LEX_WORD_DOT) {
                w->type = LEX_WORD_CONST_DOUBLE;

                ret = string_cat(w->text, w1->text);
                lex_word_free(w1);
                w1 = NULL;

                if (ret < 0) {
                    lex_word_free(w);
                    return ret;
                }

                w->data.d = atof(w->text->data);
            }
        }

        if (w1)
            lex_push_word(lex, w1);
    }

    *pword = w;
    return 0;
}
