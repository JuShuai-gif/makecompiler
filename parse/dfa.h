#ifndef DFA_H
#define DFA_H

#include "utils_vector.h"
#include "utils_list.h"

enum dfa_retvals {
    DFA_REPEATED = -3,

    DFA_ERROR = -1,

    DFA_OK = 0,
    DFA_NEXT_SYNTAX = 1,
    DFA_NEXT_WORD = 2,
    DFA_CONTINUE = 3,
    DFA_SWITCH_TO = 4,
};

typedef struct dfa_s dfa_t;
typedef struct dfa_node_s dfa_node_t;
typedef struct dfa_ops_s dfa_ops_t;
typedef struct dfa_module_s dfa_module_t;
typedef struct dfa_hook_s dfa_hook_t;

typedef int (*dfa_is_pt)(dfa_t *dfa, void *word);
typedef int (*dfa_action_pt)(dfa_t *dfa, vector_t *words, void *data);

enum dfa_hook_types {

    DFA_HOOK_PRE = 0,
    DFA_HOOK_POST,
    DFA_HOOK_END,

    DFA_HOOK_NB
};

struct dfa_hook_s {
    dfa_hook_t *next;
    dfa_node_t *node;
};

struct dfa_node_s {
    char *name;

    dfa_is_pt is;
    dfa_action_pt action;

    vector_t *childs;

    int refs;

    int module_index;
};

struct dfa_s {
    vector_t *nodes;
    vector_t *syntaxes;

    //	 vector_t*       words;

    dfa_hook_t *hooks[DFA_HOOK_NB];

    void *priv;

    dfa_ops_t *ops;
};

struct dfa_ops_s {
    const char *name;

    void *(*pop_word)(dfa_t *dfa);
    int (*push_word)(dfa_t *dfa, void *word);
    void (*free_word)(void *word);
};

struct dfa_module_s {
    const char *name;

    int (*init_module)(dfa_t *dfa);
    int (*init_syntax)(dfa_t *dfa);

    int (*fini_module)(dfa_t *dfa);

    int index;
};

static inline int dfa_is_entry(dfa_t *dfa, void *word) {
    return 1;
}

static inline int dfa_action_entry(dfa_t *dfa, vector_t *words, void *data) {
    return words->size > 0 ? DFA_CONTINUE : DFA_NEXT_WORD;
}

static inline int dfa_action_next(dfa_t *dfa, vector_t *words, void *data) {
    return DFA_NEXT_WORD;
}

#define DFA_MODULE_NODE(dfa, module, node, is, action)                            \
    {                                                                             \
        char str[256];                                                            \
        snprintf(str, sizeof(str) - 1, "%s_%s", dfa_module_##module.name, #node); \
        dfa_node_t *node = dfa_node_alloc(str, is, action);                       \
        if (!node) {                                                              \
            printf("%s(),%d, error: \n", __func__, __LINE__);                     \
            return -1;                                                            \
        }                                                                         \
        node->module_index = dfa_module_##module.index;                           \
        dfa_add_node(dfa, node);                                                  \
    }

#define DFA_GET_MODULE_NODE(dfa, module, name, node)          \
    dfa_node_t *node = dfa_find_node(dfa, #module "_" #name); \
    if (!node) {                                              \
        printf("%s(),%d, error: \n", __func__, __LINE__);     \
        return -1;                                            \
    }

#define DFA_MODULE_ENTRY(dfa, module) \
    DFA_MODULE_NODE(dfa, module, entry, dfa_is_entry, dfa_action_entry)

#define DFA_PUSH_HOOK(dfa_node, type)                                          \
    ({                                                                         \
        dfa_node_t *dn = (dfa_node);                                           \
        if (!dn || !dn->is) {                                                  \
            printf("%s(), %d, error: invalid dfa node\n", __func__, __LINE__); \
            return DFA_ERROR;                                                  \
        }                                                                      \
        dfa_hook_t *h = calloc(1, sizeof(dfa_hook_t));                         \
        if (!h) {                                                              \
            printf("%s(), %d, error: \n", __func__, __LINE__);                 \
            return DFA_ERROR;                                                  \
        }                                                                      \
        h->node = dn;                                                          \
        h->next = dfa->hooks[type];                                            \
        dfa->hooks[type] = h;                                                  \
        h;                                                                     \
    })

dfa_node_t *dfa_node_alloc(const char *name, dfa_is_pt is, dfa_action_pt action);
void dfa_node_free(dfa_node_t *node);

int dfa_open(dfa_t **pdfa, const char *name, void *priv);
void dfa_close(dfa_t *dfa);

int dfa_add_node(dfa_t *dfa, dfa_node_t *node);
dfa_node_t *dfa_find_node(dfa_t *dfa, const char *name);

int dfa_node_add_child(dfa_node_t *parent, dfa_node_t *child);

int dfa_parse_word(dfa_t *dfa, void *word, void *data);

void dfa_del_hook(dfa_hook_t **pp, dfa_hook_t *sentinel);
void dfa_del_hook_by_name(dfa_hook_t **pp, const char *name);

#endif