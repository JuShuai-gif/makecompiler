#include "utils_graph.h"
#include "utils_vector.h"
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <stdint.h>

graph_t *graph_alloc() {
    graph_t *graph = calloc(1, sizeof(graph_t));

    if (!graph) {
        return NULL;
    }

    graph->nodes = vector_alloc();

    if (!graph->nodes) {
        free(graph);
        graph = NULL;
        return NULL;
    }
    return graph;
}

void graph_gree(graph_t *graph) {
    if (graph) {
        int i;
        for (i = 0; i < graph->nodes->size; i++) {
            graph_node_t *node = graph->nodes->data[i];

            graph_node_free(node);
        }

        vector_free(graph->nodes);
        free(graph);
        graph = NULL;
    }
}

graph_node_t *graph_node_alloc() {
    graph_node_t *node = calloc(1, sizeof(graph_node_t));
    if (!node)
        return NULL;
    node->neighbors = vector_alloc();
    if (!node->neighbors) {
        free(node);
        node = NULL;
        return NULL;
    }
    return node;
}

void graph_node_free(graph_node_t *node) {
    if (node) {
        vector_free(node->neighbors);
        free(node);
        node = NULL;
    }
}

void graph_node_print(graph_node_t *node) {
    if (node) {
        printf("node: %p, color: %d\n", node, (int)node->color);
        int j;
        for (j = 0; j < node->neighbors->size; j++) {
            graph_node_t *neighbor = node->neighbors->data[j];
            printf("neighbor: %p, color: %d\n", neighbor, (int)neighbor->color);
        }
        printf("\n");
    }
}

int graph_make_edge(graph_node_t *node1, graph_node_t *node2) {
    if (!node1 || !node2) {
        return -EINVAL;
    }

    return vector_add(node1->neighbors, node2);
}

int graph_delete_node(graph_t *graph, graph_node_t *node) {
    if (!graph || !node) {
        return -EINVAL;
    }

    graph_node_t *neighbor;

    int j;
    for (j = 0; j < node->neighbors->size; j++) {
        neighbor = node->neighbors->data[j];

        logd("node %p, neighbor: %p, %d\n", node, neighbor, neighbor->neighbors->size);

        vector_del(neighbor->neighbors, node);
    }

    if (0 != vector_del(graph->nodes, node)) {
        return -1;
    }
    return 0;
}

int graph_add_node(graph_t *graph, graph_node_t *node) {
    int j;
    for (j = 0; j < node->neighbors->size; j++) {
        graph_node_t *neighbor = node->neighbors->data[j];

        if (vector_find(graph->nodes, neighbor)) {
            if (!vector_find(neighbor->neighbors, node)) {
                if (0 != vector_add(neighbor->neighbors, node)) {
                    return -1;
                }
            } else
                logw("node %p already in neighbor: %p\n", node, neighbor);
        } else
            logw("neighbor: %p of node: %p not in graph\n", neighbor, node);
    }

    if (0 != vector_add(graph->nodes, node)) {
        return -1;
    }
    return 0;
}

static int _kcolor_delete(graph_t *graph, int k, vector_t *deleted_nodes) {
    while (graph->nodes->size > 0) {
        int nb_deleted = 0;
        int i = 0;
        while (i < graph->nodes->size) {
            graph_node_t *node = graph->nodes->data[i];

            if (node->neighbors->size >= k) {
                i++;
                continue;
            }

            if (0 != graph_delete_node(graph, node)) {
                loge("scf_graph_delete_node\n");
                return -1;
            }

            if (0 != vector_add(deleted_nodes, node)) {
                loge("scf_graph_delete_node\n");
                return -1;
            }
            nb_deleted++;
        }
        if (0 == nb_deleted) {
            break;
        }
    }

    return 0;
}

static int _kcolor_fill(graph_t *graph, vector_t *colors, vector_t *deleted_nodes) {
    int k = colors->size;
    int i;
    int j;
    vector_t *colors2 = NULL;

    for (i = deleted_nodes->size - 1; i >= 0; i--) {
        graph_node_t *node = deleted_nodes->data[i];

        assert(node->neighbors->size < k);

        colors2 = vector_clone(colors);

        if (!colors2)
            return -ENOMEM;

        logd("node: %p, neighbor: %d, k: %d\n", node, node->neighbors->size, k);

        for (j = 0; j < node->neighbors->size; j++) {
            graph_node_t *neighbor = node->neighbors->data[j];

            if (neighbor->color > 0) {
                vector_del(colors2, (void *)neighbor->color);
            }

            if (0 != vector_add(neighbor->neighbors, node)) {
                goto error;
            }
        }

        assert(colors2->size >= 2);

        if (0 != vector_add(graph->nodes, node)) {
            goto error;
        }

        vector_free(colors2);
        colors2 = NULL;
    }
    return 0;

error:
    vector_free(colors2);
    colors2 = NULL;
    return -1;
}

static int _kcolor_find_nor_neighbor(graph_t *graph, int k, graph_node_t **pp0, graph_node_t **pp1) {
    assert(graph->nodes->size >= k);

    int i;
    for (i = 0; i < graph->nodes->size; i++) {
        graph_node_t *node0 = graph->nodes->data[i];

        if (node0->neighbors->size > k) {
            continue;
        }

        graph_node_t *node1 = NULL;
        int j;
        for (j = i + 1; j < graph->nodes->size; j++) {
            node1 = graph->nodes->data[j];

            if (!vector_find(node0->neighbors, node1)) {
                assert(!vector_find(node1->neighbors, node0));
                break;
            }
            node1 = NULL;
        }

        if (node1) {
            *pp0 = node0;
            *pp1 = node1;
            return 0;
        }
    }
    return -1;
}

static graph_node_t *_max_neighbors(graph_t *graph) {
    graph_node_t *node_max = NULL;

    int max = 0;
    int i;
    for (i = 0; i < graph->nodes->size; i++) {
        graph_node_t *node = graph->nodes->data[i];

        if (!node_max || max < node->neighbors->size) {
            node_max = node;
            max = node->neighbors->size;
        }
    }

    logi("max_neighbors: %d\n", max);

    return node_max;
}

int graph_graph_kcolor(graph_t *graph, vector_t *colors) {
    if (!graph || !colors || 0 == colors->size) {
        return -EINVAL;
    }

    int k = colors->size;
    int ret = -1;

    vector_t *colors2 = NULL;
    vector_t *deleted_nodes = vector_alloc();

    if (!deleted_nodes) {
        return -ENOMEM;
    }

    logd("graph->nodes->size: %d, k: %d\n", graph->nodes->size, k);

    ret = _kcolor_delete(graph, k, deleted_nodes);

    if (ret < 0) {
        goto error;
    }

    if (0 == graph->nodes->size) {
        ret = _kcolor_fill(graph, colors, deleted_nodes);
        if (ret < 0) {
            goto error;
        }

        vector_free(deleted_nodes);

        deleted_nodes = NULL;

        logd("graph->nodes->size: %d\n", graph->nodes->size);
        return 0;
    }

    assert(graph->nodes->size > 0);
    assert(graph->nodes->size >= k);

    graph_node_t *node0 = NULL;
    graph_node_t *node1 = NULL;

    if (0 == _kcolor_find_nor_neighbor(graph, k, &node0, &node1)) {
        assert(node0);
        assert(node1);

        node0->color = (intptr_t)(colors->data[0]);
        node1->color = node0->color;

        ret = graph_delete_node(graph, node0);

        ret = graph_delete_node(graph, node0);
        if (ret < 0)
            goto error;

        ret = graph_delete_node(graph, node1);
        if (ret < 0)
            goto error;

        assert(!colors2);
        colors2 = vector_clone(colors);
        if (!colors2) {
            ret = -ENOMEM;
            goto error;
        }

        ret = vector_del(colors2, (void *)node0->color);
        if (ret < 0)
            goto error;

        ret = graph_kcolor(graph, colors2);
        if (ret < 0)
            goto error;

        ret = graph_add_node(graph, node0);
        if (ret < 0)
            goto error;

        ret = graph_add_node(graph, node1);
        if (ret < 0)
            goto error;

        vector_free(colors2);
        colors2 = NULL;
    } else {
        graph_node_t *node_max = _max_neighbors(graph);
        assert(node_max);

        ret = graph_delete_node(graph, node_max);
        if (ret < 0)
            goto error;
        node_max->color = -1;

        ret = graph_kcolor(graph, colors);
        if (ret < 0)
            goto error;

        ret = graph_add_node(graph, node_max);
        if (ret < 0)
            goto error;
    }

    ret = _kcolor_fill(graph, colors, deleted_nodes);
    if (ret < 0)
        goto error;

    vector_free(deleted_nodes);
    deleted_nodes = NULL;

    logd("graph->nodes->size: %d\n", graph->nodes->size);
    return 0;

error:
    if (colors2)
        vector_free(colors2);

    vector_free(deleted_nodes);
    return ret;
}
