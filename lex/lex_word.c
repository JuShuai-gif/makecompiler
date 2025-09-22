#include "lex_word.h"

// 词法单元分配
lex_word_t* lex_word_alloc(string_t* file,int line,int pos,int type){
    // 文件名称不对，直接返回
    if (!file)
        return NULL;
    
    // 创建一个 lex_word_t 变量
    lex_word_t* w = calloc(1,sizeof(lex_word_t));

    // 如果 w 为空
    if (!w)
        return NULL;
    // 
    w->file = string_clone(file);
    if (!w->file)
    {
        free(w);
        return NULL;
    }

    w->type = type;
    w->line = line;
    w->pos = pos;
    return w;
}

// 克隆 word
lex_word_t* lex_word_clone(lex_word_t* w){
    if (!w)
        return NULL;

    lex_word_t* w1 = calloc(1,sizeof(lex_word_t));

    if (!w1)
        return NULL;
    
    w1->type = w->type;

    switch (w->type)
    {
    // float
    case LEX_WORD_CONST_FLOAT:
        w1->data.f = w->data.f;
        break;
    // double
    case LEX_WORD_CONST_DOUBLE:
        w1->data.d = w->data.d;
        break;
    // complex
    case LEX_WORD_CONST_COMPLEX:
        w1->data.z = w->data.z;
        break;
    // string
    case LEX_WORD_CONST_STRING:
        if (w->data.s)
        {
            w1->data.s = string_clone(w->data.s);

            if (!w1->data.s)
            {
                free(w1);
                return NULL;
            }
        }
        break;
    
    default:
        w1->data.u64 = w->data.u64;
        break;
    };

    if (w->text)
    {
        w1->text = string_clone(w->text);

        if (!w1->text)
        {
            lex_word_free(w1);
            return NULL;
        }
    }

    if (w->file)
    {
        w1->file = string_clone(w->file);

        if (!w1->file)
        {
            lex_word_free(w1);
            return NULL;
        }
    }

    w1->line = w->line;
    w1->pos = w->pos;
    return w1;
}



// 释放 lex_word_t
void lex_word_free(lex_word_t* w)
{
	if (w) {
		if (LEX_WORD_CONST_STRING == w->type) {
			if (w->data.s)
				string_free(w->data.s);
		}

		if (w->text)
			string_free(w->text);

		if (w->file)
			string_free(w->file);

		free(w);
	}
}

// 宏分配
macro_t* macro_alloc(lex_word_t* w)
{
    // 
	if (!w)
		return NULL;

    // 分配空间
	macro_t* m = calloc(1, sizeof(macro_t));
	if (!m)
		return NULL;

    // 
	m->w    = w;
	m->refs = 1;
	return m;
}

void macro_free(macro_t* m)
{
	if (m) {
		if (--m->refs > 0)
			return;

		assert(0 == m->refs);

		lex_word_free(m->w);

		if (m->argv) {
			vector_clear(m->argv, ( void (*)(void*) ) lex_word_free);
			vector_free (m->argv);
		}

		slist_clear(m->text_list, lex_word_t, next, lex_word_free);

		free(m);
	}
}

