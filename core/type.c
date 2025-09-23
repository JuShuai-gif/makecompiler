#include "scope.h"
#include "type.h"

// 
type_t *type_alloc(lex_word_t *w, const char *name, int type, int size) {
    type_t *t = calloc(1, sizeof(type_t));
    if (!t)
        return NULL;

    t->type = type;
    t->node.type = type;

    t->name = string_cstr(name);
    if (!t->name) {
        free(t);
        return NULL;
    }

    if (w) {
        t->w = lex_word_clone(w);
        if (!t->w) {
            string_free(t->name);
            free(t);
            return NULL;
        }
    }

    t->size = size;
    return t;
}

void type_free(type_t *t) {
    if (t) {
        string_free(t->name);
        t->name = NULL;

        if (t->w) {
            lex_word_free(t->w);
            t->w = NULL;
        }

        if (t->scope) {
            scope_free(t->scope);
            t->scope = NULL;
        }

        free(t);
        t = NULL;
    }
}

static type_abbrev_t type_abbrevs[] =
    {
        {"int", "i"},
        {"void", "v"},

        {"char", "c"},
        {"float", "f"},
        {"double", "d"},

        {"int8_t", "b"},
        {"int16_t", "w"},
        {"int32_t", "i"},
        {"int64_t", "q"},

        {"uint8_t", "ub"},
        {"uint16_t", "uw"},
        {"uint32_t", "ui"},
        {"uint64_t", "uq"},

        {"intptr_t", "p"},
        {"uintptr_t", "up"},
        {"funcptr", "fp"},

        {"string", "s"},
};

const char *type_find_abbrev(const char *name) {
    type_abbrev_t *t;

    int i;
    int n = sizeof(type_abbrevs) / sizeof(type_abbrevs[0]);

    for (i = 0; i < n; i++) {
        t = &type_abbrevs[i];

        if (!strcmp(t->name, name))
            return t->abbrev;
    }

    return NULL;
}
