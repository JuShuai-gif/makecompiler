#include "utils_graph.h"

graph_t* graph_alloc(){
    graph_t* graph = calloc(1,sizeof(graph_t));

    if (!graph)
    {
        return NULL;
    }

    graph->nodes = vector_alloc();

    if (!graph->nodes)
    {
        free(graph);
        graph = NULL;
        return NULL;
    }
    return graph;
}

void graph_gree(graph_t* graph){
    if (graph)
    {
        int i;
        for (i = 0; i < graph->nodes->size; i++)
        {
            graph_node_t* node = graph->nodes->data[i];

            graph_node_free(node);
        }

        vector_free(graph->nodes);
        free(graph);
        graph = NULL:
    }    
}


graph_node_t* graph_node_alloc(){
    graph_node_t* node = calloc(1,sizeof(graph_node_t));
    if (!node)
        return NULL;
    node->neighbors = vector_alloc();
    if (!node->neighbors)
    {
        free(node);
        node = NULL;
        return NULL;
    }
    return node;
}

void graph_node_free(graph_node_t* node){
    if (node)
    {
        vector_free(node->neighbors);
        free(node);
        node = NULL;
    }
}

void graph_node_print(graph_node_t* node){
    if (node)
    {
        printf("node: %p, color: %d\n", node, (int)node->color);
        int j;
        for (j = 0; j < node->neighbors->size; j++)
        {
            graph_node_t* neighbor = node->neighbors->data[j];
            printf("neighbor: %p, color: %d\n", neighbor, (int)neighbor->color);
        }
        printf("\n");
    }
}




