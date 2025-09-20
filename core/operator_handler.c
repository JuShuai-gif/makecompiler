#include "operator_handler.h"

operator_handler_t *operator_handler_alloc(int type, operator_handler_pt func) {
    operator_handler_t *h = calloc(1, sizeof(operator_handler_t));
    assert(h);

    list_init(&h->list);

    h->type = type;

    h->func = func;
    return h;
}

void operator_handler_free(operator_handler_t *h) {
    if (h) {
        free(h);
        h = NULL;
    }
}