#ifndef UTILS_GRAPH_H
#define UTILS_GRAPH_H

#include "utils_vector.h"

// 节点,是由一串邻居、颜色、数据组成的
typedef struct{
    vector_t* neighbors;
    intptr_t color;
    void* data;
}graph_node_t;

// 图里面只有一个数据，vector_t的指针
typedef struct 
{
    vector_t* nodes;
}graph_t;


graph_t* graph_alloc();
void graph_free(graph_t* graph);

graph_node_t* graph_node_alloc();
void graph_node_free(graph_node_t* node);
void graph_node_print(graph_node_t* node);

int graph_make_edge(graph_node_t* graph,graph_node_t* node2);

int graph_delete_node(graph_t* graph,graph_node_t* node);
int graph_add_node(graph_t* graph,graph_node_t* node);

// color = 0, not colored
// color > 0, color of node
// color < 0, node should be saved to memory
int graph_kcolor(graph_t* graph,vector_t* colors);

#endif
