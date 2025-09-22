#include "utils_graph.h"
#include "x64.h"


static intptr_t _x64_color_select(graph_node_t* node, vector_t* colors)
{
	x64_rcg_node_t* rn = node->data;

	if (!rn->dag_node) {
		assert(rn->reg);
		return rn->reg->color;
	}

	int i;
	for (i = 0; i < colors->size; i++) {
		intptr_t c     = (intptr_t)(colors->data[i]);
		int      bytes = X64_COLOR_BYTES(c);
		uint32_t type  = X64_COLOR_TYPE(c);

		if (bytes == x64_variable_size(rn->dag_node->var)
				&& type == variable_float(rn->dag_node->var))
			return c;
	}

	return 0;
}

static int _x64_color_del(vector_t* colors, intptr_t color)
{
	int i = 0;

	while (i < colors->size) {
		intptr_t c = (intptr_t)(colors->data[i]);

		if (X64_COLOR_CONFLICT(c, color)) {
			int ret = vector_del(colors, (void*)c);
			if (ret < 0)
				return ret;
		} else {
			//logi("c: %ld, color: %ld\n", c, color);
			i++;
		}
	}

	return 0;
}

static int _x64_kcolor_check(graph_t* graph)
{
	int i;
	for (i = 0; i < graph->nodes->size; i++) {

		graph_node_t* node = graph->nodes->data[i];
		x64_rcg_node_t*   rn   = node->data;

		if (rn->reg)
			assert(node->color > 0);

		if (0 == node->color)
			return -1;
	}

	return 0;
}

static int _x64_kcolor_delete(graph_t* graph, int k, vector_t* deleted_nodes)
{
	while (graph->nodes->size > 0) {

		int nb_deleted = 0;
		int i = 0;
		while (i < graph->nodes->size) {

			graph_node_t* node = graph->nodes->data[i];
			x64_rcg_node_t*   rn   = node->data;

			logd("graph->nodes->size: %d, neighbors: %d, k: %d, node: %p, rn->reg: %p\n",
					graph->nodes->size, node->neighbors->size, k, node, rn->reg);

			if (!rn->dag_node) {
				assert(rn->reg);
				assert(node->color > 0);
				i++;
				continue;
			}

			if (node->neighbors->size >= k) {
				i++;
				continue;
			}

			logd("graph->nodes->size: %d, neighbors: %d, k: %d, node: %p, rn->reg: %p\n",
					graph->nodes->size, node->neighbors->size, k, node, rn->reg);

			if (0 != graph_delete_node(graph, node)) {
				loge("graph_delete_node\n");
				return -1;
			}

			if (0 != vector_add(deleted_nodes, node)) {
				loge("graph_delete_node\n");
				return -1;
			}
			nb_deleted++;
		}

		if (0 == nb_deleted)
			break;
	}

	return 0;
}

static int _x64_kcolor_fill(graph_t* graph, int k, vector_t* colors,
		vector_t* deleted_nodes)
{
	logd("graph->nodes->size: %d\n", graph->nodes->size);
	int i;
	int j;
	vector_t* colors2 = NULL;

	for (i = deleted_nodes->size - 1; i >= 0; i--) {
		graph_node_t* node = deleted_nodes->data[i];
		x64_rcg_node_t*   rn   = node->data; 

		if (node->neighbors->size >= k)
			assert(rn->reg);

		colors2 = vector_clone(colors);
		if (!colors2)
			return -ENOMEM;

		logd("node: %p, neighbor: %d, k: %d\n", node, node->neighbors->size, k);

		for (j = 0; j < node->neighbors->size; j++) {
			graph_node_t* neighbor = node->neighbors->data[j];

			if (neighbor->color > 0) {
				int ret = _x64_color_del(colors2, neighbor->color);
				if (ret < 0)
					goto error;

				if (X64_COLOR_CONFLICT(node->color, neighbor->color)) {
					logd("node: %p, neighbor: %p, color: %#lx:%#lx\n", node, neighbor, node->color, neighbor->color);
					logd("node: %p, dn: %p, reg: %p\n", node, rn->dag_node, rn->reg);
					//assert(!rn->reg);
					assert(rn->dag_node);
					node->color = 0;
				}
			}

			logd("node: %p, neighbor: %p, color: %ld\n", node, neighbor, neighbor->color);

			if (0 != vector_add(neighbor->neighbors, node))
				goto error;
		}

		assert(colors2->size >= 0);

		if (0 == node->color) {
			node->color = _x64_color_select(node, colors2);
			if (0 == node->color) {
				node->color = -1;
				logd("colors2->size: %d\n", colors2->size);
			}
		}

		if (0 != vector_add(graph->nodes, node))
			goto error;

		vector_free(colors2);
		colors2 = NULL;
	}

	return 0;

error:
	vector_free(colors2);
	colors2 = NULL;
	return -1;
}

static int _x64_kcolor_find_not_neighbor(graph_t* graph, int k, graph_node_t** pp0, graph_node_t** pp1)
{
	assert(graph->nodes->size >= k);

	graph_node_t* node0;
	graph_node_t* node1;
	x64_rcg_node_t*   rn0;
	x64_rcg_node_t*   rn1;
	dag_node_t*   dn0;
	dag_node_t*   dn1;

	int i;
	for (i = 0; i < graph->nodes->size; i++) {
		node0     = graph->nodes->data[i];

		rn0 = node0->data;
		dn0 = rn0->dag_node;

		if (!dn0) {
			assert(rn0->reg);
			assert(node0->color > 0);
			continue;
		}

		if (node0->neighbors->size > k)
			continue;

		int is_float = variable_float(dn0->var);

		node1 = NULL;
		rn1   = NULL;

		int j;
		for (j = i + 1; j < graph->nodes->size; j++) {
			node1         = graph->nodes->data[j];

			rn1 = node1->data;
			dn1 = rn1->dag_node;

			if (!dn1) {
				assert(rn1->reg);
				assert(node1->color > 0);
				node1 = NULL;
				continue;
			}

			if (is_float != variable_float(dn1->var)) {
				node1 = NULL;
				continue;
			}

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

static graph_node_t* _x64_max_neighbors(graph_t* graph)
{
	graph_node_t* node_max = NULL;
	int max = 0;
	int i;
	for (i = 0; i < graph->nodes->size; i++) {
		graph_node_t* node = graph->nodes->data[i];
		x64_rcg_node_t*   rn   = node->data;

		if (rn->reg) {
			assert(node->color > 0);
			continue;
		}

		if (!node_max || max < node->neighbors->size) {
			node_max = node;
			max      = node->neighbors->size;
		}
	}

	x64_rcg_node_t*   rn   = node_max->data;

	if (rn->dag_node->var->w)
		logd("max_neighbors: %d, %s\n", max, rn->dag_node->var->w->text->data);
	else
		logd("max_neighbors: %d, v_%p\n", max, rn->dag_node->var);

	return node_max;
}

static void _x64_kcolor_process_conflict(graph_t* graph)
{
	int i;
	int j;
	int ret;

	for (i = 0; i < graph->nodes->size - 1; i++) {

		graph_node_t* gn0 = graph->nodes->data[i];
		x64_rcg_node_t*   rn0 = gn0->data;

		logd("i: %d, rn0->dag_node: %p, rn0->reg: %p\n", i, rn0->dag_node, rn0->reg);
		if (0 == gn0->color)
			continue;

		for (j = i + 1; j < graph->nodes->size; j++) {

			graph_node_t* gn1 = graph->nodes->data[j];
			x64_rcg_node_t*   rn1 = gn1->data;

			logd("j: %d, rn1->dag_node: %p, rn1->reg: %p\n", j, rn1->dag_node, rn1->reg);
			if (0 == gn1->color)
				continue;

			if (!X64_COLOR_CONFLICT(gn0->color, gn1->color))
				continue;

			if (!rn0->dag_node) {
				if (!rn1->dag_node) {
					continue;
				}

				rn1->reg    = NULL;
				rn1->OpCode = NULL;
				gn1->color  = 0;

			} else if (!rn1->dag_node) {
				rn0->reg    = NULL;
				rn0->OpCode = NULL;
				gn0->color  = 0;

			} else {
				int nb_parents0 = rn0->dag_node->parents ? rn0->dag_node->parents->size : 0;
				int nb_parents1 = rn1->dag_node->parents ? rn1->dag_node->parents->size : 0;

				if (nb_parents0 > nb_parents1) {
					rn1->reg    = NULL;
					rn1->OpCode = NULL;
					gn1->color  = 0;
				} else {
					rn0->reg    = NULL;
					rn0->OpCode = NULL;
					gn0->color  = 0;
				}
			}
		}
	}
}

static int _x64_graph_kcolor(graph_t* graph, int k, vector_t* colors)
{
	int ret = -1;
	vector_t* colors2 = NULL;

	vector_t* deleted_nodes = vector_alloc();
	if (!deleted_nodes)
		return -ENOMEM;

	logd("graph->nodes->size: %d, k: %d\n", graph->nodes->size, k);

	ret = _x64_kcolor_delete(graph, k, deleted_nodes);
	if (ret < 0)
		goto error;

	if (0 == _x64_kcolor_check(graph)) {
		logd("graph->nodes->size: %d\n", graph->nodes->size);

		ret = _x64_kcolor_fill(graph, k, colors, deleted_nodes);
		if (ret < 0)
			goto error;

		vector_free(deleted_nodes);
		deleted_nodes = NULL;

		logd("graph->nodes->size: %d\n", graph->nodes->size);
		return 0;
	}

	assert(graph->nodes->size > 0);
	assert(graph->nodes->size >= k);

	graph_node_t* node_max = NULL;
	graph_node_t* node0    = NULL;
	graph_node_t* node1    = NULL;

	if (0 == _x64_kcolor_find_not_neighbor(graph, k, &node0, &node1)) {
		assert(node0);
		assert(node1);

		x64_rcg_node_t* rn0 = node0->data;
		x64_rcg_node_t* rn1 = node1->data;

		assert(!colors2);
		colors2 = vector_clone(colors);
		if (!colors2) {
			ret = -ENOMEM;
			goto error;
		}

		int reg_size0 = x64_variable_size(rn0->dag_node->var);
		int reg_size1 = x64_variable_size(rn1->dag_node->var);

		if (reg_size0 > reg_size1) {

			node0->color = _x64_color_select(node0, colors2);
			if (0 == node0->color)
				goto overflow;

			intptr_t type  = X64_COLOR_TYPE(node0->color);
			intptr_t id    = X64_COLOR_ID(node0->color);
			intptr_t mask  = (1 << reg_size1) - 1;
			node1->color   = X64_COLOR(type, id, mask);

			ret = _x64_color_del(colors2, node0->color);
			if (ret < 0) {
				loge("\n");
				goto error;
			}

			assert(!vector_find(colors2, (void*)node1->color));

		} else {
			node1->color = _x64_color_select(node1, colors2);
			if (0 == node1->color)
				goto overflow;

			intptr_t type  = X64_COLOR_TYPE(node1->color);
			intptr_t id    = X64_COLOR_ID(node1->color);
			intptr_t mask  = (1 << reg_size0) - 1;

			node0->color   = X64_COLOR(type, id, mask);

			ret = _x64_color_del(colors2, node1->color);
			if (ret < 0) {
				loge("\n");
				goto error;
			}

			assert(!vector_find(colors2, (void*)node0->color));
		}

		ret = graph_delete_node(graph, node0);
		if (ret < 0)
			goto error;

		ret = graph_delete_node(graph, node1);
		if (ret < 0)
			goto error;

		ret = x64_graph_kcolor(graph, k - 1, colors2);
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
overflow:
		node_max = _x64_max_neighbors(graph);
		assert(node_max);

		ret = graph_delete_node(graph, node_max);
		if (ret < 0)
			goto error;
		node_max->color = -1;

		ret = x64_graph_kcolor(graph, k, colors);
		if (ret < 0)
			goto error;

		ret = graph_add_node(graph, node_max);
		if (ret < 0)
			goto error;
	}

	ret = _x64_kcolor_fill(graph, k, colors, deleted_nodes);
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

int x64_graph_kcolor(graph_t* graph, int k, vector_t* colors)
{
	if (!graph || !colors || 0 == colors->size) {
		loge("\n");
		return -EINVAL;
	}

	_x64_kcolor_process_conflict(graph);

	return _x64_graph_kcolor(graph, k, colors);
}




