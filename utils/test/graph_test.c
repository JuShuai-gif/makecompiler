#include "../utils_graph.h"

int main(){
    graph_t* g = graph_alloc();

    graph_node_t* n0 = graph_node_alloc();
    graph_node_t* n1 = graph_node_alloc();
    graph_node_t* n2 = graph_node_alloc();
    graph_node_t* n3 = graph_node_alloc();
    graph_node_t* n4 = graph_node_alloc();
    graph_node_t* n5 = graph_node_alloc();

    graph_make_edge(n1,n2);
    graph_make_edge(n2,n3);
    graph_make_edge(n3,n0);

    graph_add_node(g,n0);
    graph_add_node(g,n3);
    graph_add_node(g,n2);
    graph_add_node(g,n1);

    graph_make_edge(n0,n1);
    graph_make_edge(n1,n0);

    int nb_colors = 2;
    vector_t* colors = vector_alloc();
    int i;
    for (i = 0; i < nb_colors; i++)
        vector_add(colors,(void*)(intptr_t)(i+1));

    graph_kcolor(g,colors);

    graph_node_print(n0);
    graph_node_print(n1);
    graph_node_print(n2);
    graph_node_print(n3);

    return 0;
    




}


















