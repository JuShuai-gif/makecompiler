#include "utils_graph.h"
#include "utils_vector.h"
#include <assert.h>
#include <stdint.h>

graph_t *graph_alloc() {
    graph_t *graph = calloc(1, sizeof(graph_t));

    if (!graph) {
        return NULL;
    }

    // 创建一个基础的 node
    graph->nodes = vector_alloc();

    // 分配不成功
    if (!graph->nodes) {
        free(graph);
        graph = NULL;
        return NULL;
    }
    return graph;
}

// 图释放
void graph_free(graph_t *graph) {
    if (graph) {
        int i;
        for (i = 0; i < graph->nodes->size; i++) {
            // 这里释放的是数组中数据本身
            graph_node_t *node = graph->nodes->data[i];

            graph_node_free(node);
        }

        vector_free(graph->nodes);
        free(graph);
        graph = NULL;
    }
}

// 图节点分配
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

// 释放节点
void graph_node_free(graph_node_t *node) {
    if (node) {
        // 先释放邻居
        vector_free(node->neighbors);
        // 在释放自己
        free(node);
        node = NULL;
    }
}

// 图节点打印
void graph_node_print(graph_node_t *node) {
    if (node) {
        // 打印节点信息和颜色
        printf("node: %p, color: %d\n", node, (int)node->color);
        int j;
        // 打印邻居信息
        for (j = 0; j < node->neighbors->size; j++) {
            graph_node_t *neighbor = node->neighbors->data[j];
            printf("neighbor: %p, color: %d\n", neighbor, (int)neighbor->color);
        }
        printf("\n");
    }
}

// 将两个节点联立
int graph_make_edge(graph_node_t *node1, graph_node_t *node2) {
    if (!node1 || !node2) {
        return -EINVAL;
    }

    return vector_add(node1->neighbors, node2);
}

// 删除一个节点
int graph_delete_node(graph_t *graph, graph_node_t *node) {
    if (!graph || !node) {
        return -EINVAL;
    }

    graph_node_t *neighbor;

    int j;
    for (j = 0; j < node->neighbors->size; j++) {
        neighbor = node->neighbors->data[j];

        logd("node %p, neighbor: %p, %d\n", node, neighbor, neighbor->neighbors->size);

        // 删除这个节点
        // neighbor->neighbors 是 vector_t 类型，为什么可以删除node类型，node是graph_node_t类型
        vector_del(neighbor->neighbors, node);
    }

    if (0 != vector_del(graph->nodes, node)) {
        return -1;
    }
    return 0;
}

// 增加节点
int graph_add_node(graph_t *graph, graph_node_t *node) {
    int j;
    for (j = 0; j < node->neighbors->size; j++) {
        // 先获取要添加节点的邻居的数据节点信息
        graph_node_t *neighbor = node->neighbors->data[j];
        // 如果图的节点里面有上面这个节点信息
        if (vector_find(graph->nodes, neighbor)) {
            // 邻居中包含这个节点信息
            if (!vector_find(neighbor->neighbors, node)) {
                // 如果都没有，则添加这个节点
                if (0 != vector_add(neighbor->neighbors, node)) {
                    return -1;
                }
            } else
                logw("node %p already in neighbor: %p\n", node, neighbor);
        } else
            logw("neighbor: %p of node: %p not in graph\n", neighbor, node);
    }

    // 在图中添加这个节点
    if (0 != vector_add(graph->nodes, node)) {
        return -1;
    }
    return 0;
}

/**
 * 从图中删除所有“度数 < k”的节点，并把删除的节点存放到 deleted_nodes 里。
 * 如果一轮删除有节点被删除，就继续下一轮（因为邻居被删后可能出现新的度数 < k 的节点）。
 * 直到一轮中没有节点被删除为止。
 *
 * 参数:
 *   graph          - 输入图
 *   k              - k 值，阈值
 *   deleted_nodes  - 存放被删除节点的 vector
 *
 * 返回值:
 *   0  - 成功
 *  -1  - 出错
 *
这个函数在做的是 图着色算法（graph coloring）里的“简化阶段”，通常用于 寄存器分配 或 图染色问题：

1、遍历图中的所有节点；

2、删除所有度数 < k 的节点（这些节点以后一定能被安全染色，因为它们的邻居最多 k-1 种颜色，总能找到一个颜色可用）；

3、每次删除一个节点后，更新图（邻居度数减少），继续循环；

4、如果一轮中没有节点被删，就说明剩下的图中所有节点度数都 ≥ k，此时停止。
 */
static int _kcolor_delete(graph_t *graph, int k, vector_t *deleted_nodes) {
    // 只要图中还有节点，就尝试删除
    while (graph->nodes->size > 0) {
        int nb_deleted = 0; // 本轮删除的节点数
        int i = 0;

        // 遍历当前图中的所有节点
        while (i < graph->nodes->size) {
            graph_node_t *node = graph->nodes->data[i];

            // 如果节点的度数 >= k，说明暂时不能删除，跳过
            if (node->neighbors->size >= k) {
                i++;
                continue;
            }

            // 度数 < k，可以删除该节点
            if (0 != graph_delete_node(graph, node)) {
                loge("scf_graph_delete_node\n"); // 打印错误日志
                return -1;                       // 删除失败
            }

            // 把删除的节点加入 deleted_nodes 记录下来
            if (0 != vector_add(deleted_nodes, node)) {
                loge("scf_graph_delete_node\n"); // 打印错误日志
                return -1;                       // 插入失败
            }
            nb_deleted++;
            // 注意：这里没有 i++，因为删除节点后 graph->nodes->data[i] 已经被更新，
            // 下一次循环还需要检查当前位置的新节点。
        }

        // 如果这一轮没有删除任何节点，就停止
        if (0 == nb_deleted) {
            break;
        }
    }

    return 0;
}

/**
 * _kcolor_fill
 *
 * 作用：
 *   在执行完 _kcolor_delete 之后，需要“回填”被删除的节点，并为它们分配颜色。
 *   这个过程相当于着色算法的逆过程：从删除序列里倒着取出节点，把它放回图中，
 *   给它选择一个合法颜色（不与邻居冲突）。
 *
 * 参数：
 *   graph         - 图结构（删除后剩余的子图）
 *   colors        - 初始可用颜色集合（大小 = k）
 *   deleted_nodes - 在 _kcolor_delete 中删除的节点集合（栈，后进先出）
 *
 * 返回值：
 *   0  - 成功
 *  -1  - 出错
 */
static int _kcolor_fill(graph_t *graph, vector_t *colors, vector_t *deleted_nodes) {
    int k = colors->size; // k 值 = 颜色数量
    int i;
    int j;
    vector_t *colors2 = NULL; // 每个节点的临时颜色集合

    // 从删除序列的末尾开始（逆序回填节点）
    for (i = deleted_nodes->size - 1; i >= 0; i--) {
        graph_node_t *node = deleted_nodes->data[i];

        // 保证该节点的度数 < k（否则说明算法逻辑出错）
        assert(node->neighbors->size < k);

        // 克隆一份可用颜色集合
        colors2 = vector_clone(colors);

        if (!colors2)
            return -ENOMEM; // 内存分配失败

        logd("node: %p, neighbor: %d, k: %d\n", node, node->neighbors->size, k);

        // 遍历该节点的所有邻居
        for (j = 0; j < node->neighbors->size; j++) {
            graph_node_t *neighbor = node->neighbors->data[j];

            // 如果邻居已经有颜色，则从候选集合中移除该颜色
            if (neighbor->color > 0) {
                vector_del(colors2, (void *)neighbor->color);
            }

            // 把当前节点也加到邻居的邻居列表里（恢复图结构）
            if (0 != vector_add(neighbor->neighbors, node)) {
                goto error; // 出错跳转
            }
        }

        // 保证至少还有两个候选颜色（>=2，避免无解）
        assert(colors2->size >= 2);

        // 把节点放回图的节点列表中
        if (0 != vector_add(graph->nodes, node)) {
            goto error;
        }

        // 释放临时颜色集合
        vector_free(colors2);
        colors2 = NULL;
    }
    return 0;

error:
    vector_free(colors2);
    colors2 = NULL;
    return -1;
}

/**
 * _kcolor_find_nor_neighbor
 *
 * 作用：
 *   在图中寻找一对“非邻居节点”(non-neighbor)，即彼此之间没有边相连的两个节点。
 *   条件：node0 的度数 <= k，node1 与 node0 互相不是邻居。
 *   找到后通过 pp0, pp1 返回这两个节点。
 *
 * 参数：
 *   graph - 输入图
 *   k     - 阈值，要求 node0 的度数 <= k
 *   pp0   - 输出参数，返回找到的第一个节点
 *   pp1   - 输出参数，返回找到的第二个节点
 *
 * 返回值：
 *   0  - 找到一对非邻居节点
 *  -1  - 没有找到
 *
 * 这个函数一般出现在 图着色优化(寄存器分配)中，用来做 合并
 */
static int _kcolor_find_nor_neighbor(graph_t *graph, int k, graph_node_t **pp0, graph_node_t **pp1) {
    // 保证节点数至少有 k 个，否则无意义
    assert(graph->nodes->size >= k);

    int i;
    // 遍历所有节点，尝试作为 node0
    for (i = 0; i < graph->nodes->size; i++) {
        graph_node_t *node0 = graph->nodes->data[i];

        // 如果 node0 的度数 > k，跳过（我们只考虑度数 <= k 的节点）
        if (node0->neighbors->size > k) {
            continue;
        }

        graph_node_t *node1 = NULL;
        int j;
        // 在 node0 后面的节点中寻找一个非邻居 node1
        for (j = i + 1; j < graph->nodes->size; j++) {
            node1 = graph->nodes->data[j];

            // 判断 node0 和 node1 是否相邻
            if (!vector_find(node0->neighbors, node1)) {
                // 如果 node0 的邻居里找不到 node1，
                // 那么 node1 的邻居里也一定找不到 node0（对称性）
                assert(!vector_find(node1->neighbors, node0));
                break; // 找到了合适的 node1
            }
            node1 = NULL; // 如果相邻，则清空，继续找下一个
        }

        // 如果找到了非邻居
        if (node1) {
            *pp0 = node0;
            *pp1 = node1;
            return 0; // 成功返回
        }
    }
    return -1; // 没有找到任何非邻居对
}

// 在图中找到邻居数量最多的节点，并返回该节点
static graph_node_t *_max_neighbors(graph_t *graph) {
    graph_node_t *node_max = NULL; // 用来存放当前找到的“邻居数最多的节点”

    int max = 0; // 当前最大邻居数
    int i;

    // 遍历图中所有节点
    for (i = 0; i < graph->nodes->size; i++) {
        graph_node_t *node = graph->nodes->data[i]; // 获取第 i 个节点

        // 如果 node_max 还没赋值，或者当前节点的邻居数比已知最大值还大
        if (!node_max || max < node->neighbors->size) {
            node_max = node;             // 更新最大邻居节点
            max = node->neighbors->size; // 更新最大邻居数
        }
    }
    // 打印日志，输出最大邻居数
    logi("max_neighbors: %d\n", max);
    // 返回邻居数量最多的节点
    return node_max;
}

// 对图进行 k-着色 (Graph Coloring)，
// colors 表示可用颜色集合（长度就是 k）
// 返回 0 表示成功，负数表示失败
int graph_kcolor(graph_t *graph, vector_t *colors) {
    // 参数检查：必须保证图存在，颜色集合存在，且至少有 1 种颜色
    if (!graph || !colors || 0 == colors->size) {
        return -EINVAL; // 无效参数
    }

    int k = colors->size; // 颜色数量
    int ret = -1;         // 返回值，默认失败

    vector_t *colors2 = NULL;                 // 用于拷贝 colors 的临时变量
    vector_t *deleted_nodes = vector_alloc(); // 存放被删除的节点集合

    if (!deleted_nodes) {
        return -ENOMEM; // 内存分配失败
    }

    logd("graph->nodes->size: %d, k: %d\n", graph->nodes->size, k);

    // 第一步：删除所有度数 < k 的节点，存入 deleted_nodes
    ret = _kcolor_delete(graph, k, deleted_nodes);

    if (ret < 0) {
        goto error;
    }

    // 如果删光了所有节点，说明图可以直接着色
    if (0 == graph->nodes->size) {
        // 回溯阶段：为被删除的节点分配颜色
        ret = _kcolor_fill(graph, colors, deleted_nodes);
        if (ret < 0) {
            goto error;
        }

        // 清理
        vector_free(deleted_nodes);

        deleted_nodes = NULL;

        logd("graph->nodes->size: %d\n", graph->nodes->size);
        return 0;
    }

    // 这里说明图还剩下节点
    assert(graph->nodes->size > 0);
    assert(graph->nodes->size >= k);

    graph_node_t *node0 = NULL;
    graph_node_t *node1 = NULL;

    // 第二步：尝试找到一对非邻居节点（度数 <= k），把它们染成同色
    if (0 == _kcolor_find_nor_neighbor(graph, k, &node0, &node1)) {
        assert(node0);
        assert(node1);

        // 把 node0 和 node1 染成同一个颜色
        node0->color = (intptr_t)(colors->data[0]);
        node1->color = node0->color;

        // 删除这两个节点
        ret = graph_delete_node(graph, node0);
        if (ret < 0)
            goto error;

        ret = graph_delete_node(graph, node1);
        if (ret < 0)
            goto error;

        // 拷贝一份 colors，作为递归调用的颜色集合
        assert(!colors2);
        colors2 = vector_clone(colors);
        if (!colors2) {
            ret = -ENOMEM;
            goto error;
        }

        // 从 colors2 删除刚分配给 node0/node1 的颜色
        ret = vector_del(colors2, (void *)node0->color);
        if (ret < 0)
            goto error;

        // 递归继续给剩下的图着色
        ret = graph_kcolor(graph, colors2);
        if (ret < 0)
            goto error;

        // 回溯：把 node0 和 node1 加回图
        ret = graph_add_node(graph, node0);
        if (ret < 0)
            goto error;

        ret = graph_add_node(graph, node1);
        if (ret < 0)
            goto error;

        vector_free(colors2);
        colors2 = NULL;
    } else {
        // 第三步：没找到合适的非邻居节点
        // 选一个度数最大的节点，先删除它，递归处理剩余图
        graph_node_t *node_max = _max_neighbors(graph);
        assert(node_max);

        ret = graph_delete_node(graph, node_max);
        if (ret < 0)
            goto error;
        node_max->color = -1; // 标记为未着色

        ret = graph_kcolor(graph, colors);
        if (ret < 0)
            goto error;

        // 回溯：把 node_max 加回图
        ret = graph_add_node(graph, node_max);
        if (ret < 0)
            goto error;
    }

    // 最后一步：给之前删除的节点回溯分配颜色
    ret = _kcolor_fill(graph, colors, deleted_nodes);
    if (ret < 0)
        goto error;

    vector_free(deleted_nodes);
    deleted_nodes = NULL;

    logd("graph->nodes->size: %d\n", graph->nodes->size);
    return 0;

error:
    // 出错时释放资源
    if (colors2)
        vector_free(colors2);

    vector_free(deleted_nodes);
    return ret;
}
