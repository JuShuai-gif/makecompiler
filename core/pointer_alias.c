#include"optimizer.h"
#include"pointer_alias.h"

static int __bb_dfs_initeds(basic_block_t* root, dn_status_t* ds, vector_t* initeds)
{
	basic_block_t* bb;
	dn_status_t*   ds2;

	int ret;
	int i;
	int j;

	assert(!root->jmp_flag);

	root->visit_flag = 1;

	int like = dn_status_is_like(ds);

	for (i = 0; i < root->prevs->size; ++i) {
		bb =        root->prevs->data[i];

		if (bb->visit_flag)
			continue;

		for (j = 0; j < bb->dn_status_initeds->size; j++) {
			ds2       = bb->dn_status_initeds->data[j];

			if (like) {
				if (0 == ds_cmp_like_indexes(ds, ds2))
					break;
			} else {
				if (0 == ds_cmp_same_indexes(ds, ds2))
					break;
			}
		}

		if (j < bb->dn_status_initeds->size) {

			ret = vector_add(initeds, bb);
			if (ret < 0)
				return ret;

			bb->visit_flag = 1;
			continue;
		}

		ret = __bb_dfs_initeds(bb, ds, initeds);
		if ( ret < 0)
			return ret;
	}

	return 0;
}

static int __bb_dfs_check_initeds(basic_block_t* root, basic_block_t* obj)
{
	basic_block_t* bb;
	int i;

	assert(!root->jmp_flag);

	if (root == obj)
		return -1;

	if (root->visit_flag)
		return 0;

	root->visit_flag = 1;

	for (i = 0; i < root->nexts->size; ++i) {
		bb =        root->nexts->data[i];

		if (bb->visit_flag)
			continue;

		if (bb == obj)
			return -1;

		int ret = __bb_dfs_check_initeds(bb, obj);
		if ( ret < 0)
			return ret;
	}

	return 0;
}

static int _bb_dfs_initeds(list_t* bb_list_head, basic_block_t* bb, dn_status_t* ds, vector_t* initeds)
{
	basic_block_visit_flag(bb_list_head, 0);

	return __bb_dfs_initeds(bb, ds, initeds);
}

static int _bb_dfs_check_initeds(list_t* bb_list_head, basic_block_t* bb, vector_t* initeds)
{
	list_t*        l;
	basic_block_t* bb2;
	int i;

	basic_block_visit_flag(bb_list_head, 0);

	for (i = 0; i < initeds->size; i++) {
		bb2       = initeds->data[i];

		bb2->visit_flag = 1;
	}

	l   = list_head(bb_list_head);
	bb2 = list_data(l, basic_block_t, list);

	return __bb_dfs_check_initeds(bb2, bb);
}

static int _bb_pointer_initeds(vector_t* initeds, list_t* bb_list_head, basic_block_t* bb, dn_status_t* ds)
{
	dag_node_t* dn;
	variable_t* v;

	int ret = _bb_dfs_initeds(bb_list_head, bb, ds, initeds);
	if (ret < 0)
		return ret;

	if (0 == initeds->size) {
		dn = ds->dag_node;
		v  = dn->var;

		if (variable_const(v) || variable_const_string(v))
			return 0;

		if (v->arg_flag)
			return 0;

		if (v->global_flag) {
			if (v->nb_pointers > 0)
				logw("global pointer '%s' is not inited, file: %s, line: %d\n", v->w->text->data, v->w->file->data, v->w->line);
			return 0;
		}

		if (v->nb_dimentions > 0 && !ds->dn_indexes)
			return 0;

		if (dn->node->split_flag) {
			logd("dn->node->split_parent: %d, %p\n", dn->node->split_parent->type, dn->node->split_parent);

			assert(dn->node->split_parent->type == OP_CALL
				|| dn->node->split_parent->type == OP_CREATE);
			return 0;
		}

		if (ds->dn_indexes)
			return 0;

		if (v->tmp_flag)
			return 0;

		loge("pointer '%s' is not inited, tmp_flag: %d, local_flag: %d, file: %s, line: %d\n",
				v->w->text->data, v->tmp_flag, v->local_flag, v->w->file->data, v->w->line);
		return POINTER_NOT_INIT;
	}

	if (ds->dn_indexes && ds->dn_indexes->size > 0) {
		dn = ds->dag_node;
		v  = dn->var;

		if (!v->arg_flag
				&& !v->global_flag
				&& v->nb_dimentions <= 0
				&& v->nb_pointers > 0
				&& !variable_const(v) && !variable_const_string(v)) {

			dn_status_t* base;
			vector_t*    tmp;

			base = dn_status_alloc(ds->dag_node);
			if (!base)
				return -ENOMEM;

			tmp = vector_alloc();
			if (!tmp) {
				dn_status_free(base);
				return -ENOMEM;
			}

			ret = _bb_pointer_initeds(tmp, bb_list_head, bb, base);

			vector_free(tmp);
			dn_status_free(base);

			return ret;
		}
	}

	logd("initeds->size: %d\n", initeds->size);
	int i;
	for (i = 0; i < initeds->size; i++) {
		basic_block_t* bb2 = initeds->data[i];

		logd("bb2: %p, %d\n", bb2, bb2->index);
	}

	ret = _bb_dfs_check_initeds(bb_list_head, bb, initeds);
	if (ret < 0) {
		dn = ds->dag_node;
		v  = dn->var;

		if (!v->arg_flag && !v->global_flag) {
			loge("pointer '%s' is not inited, file: %s, line: %d\n", v->w->text->data, v->w->file->data, v->w->line);
			return POINTER_NOT_INIT;
		}
		return 0;
	}
	return ret;
}

static int _bb_pointer_initeds_leak(vector_t* initeds, list_t* bb_list_head, basic_block_t* bb, dn_status_t* ds)
{
	dag_node_t* dn;
	variable_t* v;

	int ret = _bb_dfs_initeds(bb_list_head, bb, ds, initeds);
	if (ret < 0)
		return ret;

	if (0 == initeds->size) {
		dn = ds->dag_node;
		v  = dn->var;

		if (variable_const(v) || variable_const_string(v))
			return 0;

		if (v->arg_flag)
			return 0;

		if (v->global_flag) {
			if (v->nb_pointers > 0)
				logw("global pointer '%s' is not inited, file: %s, line: %d\n", v->w->text->data, v->w->file->data, v->w->line);
			return 0;
		}

		if (v->nb_dimentions > 0 && !ds->dn_indexes)
			return 0;

		if (dn->node->split_flag) {
			assert(dn->node->split_parent->type == OP_CALL
				|| dn->node->split_parent->type == OP_CREATE);
			return 0;
		}

		if (ds->dn_indexes)
			return 0;

		loge("pointer '%s' is not inited, file: %s, line: %d\n", v->w->text->data, v->w->file->data, v->w->line);
		return POINTER_NOT_INIT;
	}

	if (ds->dn_indexes && ds->dn_indexes->size > 0) {

		dn_status_t* base;
		vector_t*    tmp;

		base = dn_status_alloc(ds->dag_node);
		if (!base)
			return -ENOMEM;

		tmp = vector_alloc();
		if (!tmp) {
			dn_status_free(base);
			return -ENOMEM;
		}

		ret = _bb_pointer_initeds_leak(tmp, bb_list_head, bb, base);

		vector_free(tmp);
		dn_status_free(base);

		return ret;
	}

	return 0;
}

static int _find_aliases_from_initeds(vector_t* aliases, vector_t* initeds, dn_status_t* ds_pointer)
{
	basic_block_t* bb2;
	dn_status_t*   ds;
	dn_status_t*   ds2;

	int i;
	int j;

	for (i = 0; i < initeds->size; i++) {
		bb2       = initeds->data[i];

		for (j = 0; j < bb2->dn_status_initeds->size; j++) {
			ds =        bb2->dn_status_initeds->data[j];

			if (ds_cmp_like_indexes(ds, ds_pointer))
				continue;

			if (vector_find_cmp(aliases, ds, ds_cmp_alias))
				continue;

			ds2 = dn_status_clone(ds);
			if (!ds2)
				return -ENOMEM;

			int ret = vector_add(aliases, ds2);
			if (ret < 0) {
				dn_status_free(ds2);
				return ret;
			}
		}
	}

	return 0;
}

static int _bb_pointer_aliases(vector_t* aliases, list_t* bb_list_head, basic_block_t* bb, dn_status_t* ds_pointer)
{
	vector_t* initeds = vector_alloc();
	if (!initeds)
		return -ENOMEM;

	int ret = _bb_pointer_initeds(initeds, bb_list_head, bb, ds_pointer);
	if (ret < 0) {
		vector_free(initeds);
		return ret;
	}

	ret = _find_aliases_from_initeds(aliases, initeds, ds_pointer);

	vector_free(initeds);
	return ret;
}

static int _bb_pointer_aliases_leak(vector_t* aliases, list_t* bb_list_head, basic_block_t* bb, dn_status_t* ds_pointer)
{
	vector_t* initeds = vector_alloc();
	if (!initeds)
		return -ENOMEM;

	int ret = _bb_pointer_initeds_leak(initeds, bb_list_head, bb, ds_pointer);
	if (ret < 0) {
		vector_free(initeds);
		return ret;
	}

	ret = _find_aliases_from_initeds(aliases, initeds, ds_pointer);

	vector_free(initeds);
	return ret;
}

static int _find_aliases_from_current(vector_t* aliases, _3ac_code_t* c, basic_block_t* bb, dn_status_t* ds_pointer)
{
	dn_status_t* ds;
	dn_status_t* ds2;
	_3ac_code_t*  c2;
	list_t*      l2;

	for (l2 = &c->list; l2 != list_sentinel(&bb->code_list_head); l2 = list_prev(l2)) {

		c2  = list_data(l2, _3ac_code_t, list);

		if (!c2->dn_status_initeds)
			continue;

		ds2 = vector_find_cmp(c2->dn_status_initeds, ds_pointer, ds_cmp_same_indexes);
		if (!ds2)
			continue;

		ds = dn_status_null(); 
		if (!ds)
			return -ENOMEM;

		int ret = ds_copy_alias(ds, ds2);
		if (ret < 0) {
			dn_status_free(ds);
			return ret;
		}

		ret = vector_add(aliases, ds);
		if (ret < 0) {
			dn_status_free(ds);
			return ret;
		}

		return 1;
	}

	return 0;
}

int pointer_alias_ds_leak(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	int ret = _find_aliases_from_current(aliases, c, bb, ds_pointer);
	if (ret < 0)
		return ret;

	if (ret > 0)
		return 0;

	return _bb_pointer_aliases_leak(aliases, bb_list_head, bb, ds_pointer);
}

int _bb_copy_aliases(basic_block_t* bb, dag_node_t* dn_pointer, dag_node_t* dn_dereference, vector_t* aliases)
{
	dag_node_t*   dn_alias;
	dn_status_t*  ds;
	dn_status_t*  ds2;

	int ret;
	int i;

	for (i = 0; i < aliases->size; i++) {
		ds        = aliases->data[i];

		dn_alias  = ds->alias;

//		variable_t* v = dn_alias->var;
//		logw("dn_alias: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

		if (DN_ALIAS_VAR == ds->alias_type) {

			if (dn_through_bb(dn_alias)) {

				ret = vector_add_unique(bb->entry_dn_aliases, dn_alias);
				if (ret < 0)
					return ret;
			}
		}

		ds2 = dn_status_null();
		if (!ds2)
			return -ENOMEM;

		if (ds_copy_alias(ds2, ds) < 0) {
			dn_status_free(ds2);
			return -ENOMEM;
		}

		ret = vector_add(bb->dn_pointer_aliases, ds2);
		if (ret < 0) {
			dn_status_free(ds2);
			return ret;
		}

		ds2->dag_node     = dn_pointer;
		ds2->dereference  = dn_dereference;
	}

	return 0;
}

int _bb_copy_aliases2(basic_block_t* bb, vector_t* aliases)
{
	dag_node_t*   dn_alias;
	dn_status_t*  ds;
	dn_status_t*  ds2;

	int ret;
	int i;

	for (i = 0; i < aliases->size; i++) {
		ds        = aliases->data[i];
		dn_alias  = ds->alias;

		variable_t* v = ds->dag_node->var;
		logd("dn_pointer: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

		if (DN_ALIAS_VAR == ds->alias_type) {

			if (dn_through_bb(dn_alias)) {

				ret = vector_add_unique(bb->entry_dn_aliases, dn_alias);
				if (ret < 0)
					return ret;
			}
		}

		ds2 = dn_status_clone(ds);
		if (!ds2)
			return -ENOMEM;

		ret = vector_add(bb->dn_pointer_aliases, ds2);
		if (ret < 0) {
			dn_status_free(ds2);
			return ret;
		}

		ds2 = dn_status_clone(ds);
		if (!ds2)
			return -ENOMEM;

		ret = vector_add(bb->dn_status_initeds, ds2);
		if (ret < 0) {
			dn_status_free(ds2);
			return ret;
		}
	}
	return 0;
}

int pointer_alias_ds(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	int ret = _find_aliases_from_current(aliases, c, bb, ds_pointer);
	if (ret < 0)
		return ret;

	if (ret > 0)
		return 0;

	return _bb_pointer_aliases(aliases, bb_list_head, bb, ds_pointer);
}

static int _bb_op_aliases(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t*  ds  = NULL;
	dn_status_t* _ds  = NULL;
	dn_status_t* _ds2 = NULL;
	vector_t*    aliases2;

	if (!c->dn_status_initeds) {
		c->dn_status_initeds = vector_alloc();

		if (!c->dn_status_initeds)
			return -ENOMEM;
	}

	DN_STATUS_GET(_ds,  c ->dn_status_initeds, ds_pointer->dag_node);
	DN_STATUS_GET(_ds2, bb->dn_status_initeds, ds_pointer->dag_node);

	aliases2 = vector_alloc();
	if (!aliases2)
		return -ENOMEM;

	int ret = pointer_alias(aliases2, ds_pointer->dag_node, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		goto error;
	}

	assert(aliases2->size > 0);

	ds = aliases2->data[0];
	ret = ds_copy_alias(_ds, ds);
	if (ret < 0)
		goto error;

	ret = ds_copy_alias(_ds2, ds);
	if (ret < 0)
		goto error;

	if (!vector_find(aliases, _ds)) {

		ret = vector_add(aliases, _ds);
		if (ret < 0)
			goto error;

		dn_status_ref(_ds);
	}

	ret = 0;
error:
	vector_clear(aliases2, ( void (*)(void*) )dn_status_free);
	vector_free(aliases2);
	return ret;
}

int _dn_status_alias_dereference(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head);

static int _bb_dereference_aliases(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds  = NULL;
	dn_status_t* ds2 = NULL;

	int count = 0;
	int ret;
	int i;

	for (i = 0; i < bb->dn_pointer_aliases->size; i++) {
		ds =        bb->dn_pointer_aliases->data[i];

		if (ds_pointer->dag_node != ds->dereference)
			continue;

#define DS_POINTER_FROM_ALIAS(dst, src) \
		do { \
			dst = dn_status_null(); \
			if (!dst) \
			return -ENOMEM; \
			\
			ret = ds_copy_alias(dst, src); \
			if (ret < 0) { \
				dn_status_free(dst); \
				return ret; \
			} \
			\
			dst->dag_node      = dst->alias; \
			dst->dn_indexes    = dst->alias_indexes; \
			dst->alias         = NULL; \
			dst->alias_indexes = NULL; \
		} while (0)

		DS_POINTER_FROM_ALIAS(ds2, ds);

		ret = _dn_status_alias_dereference(aliases, ds2, c, bb, bb_list_head);

		dn_status_free(ds2);
		ds2 = NULL;

		if (ret < 0) {
			loge("\n");
			return ret;
		}

		++count;
	}

	if (count > 0)
		return 0;

	assert(ds_pointer->dag_node->childs && 1 == ds_pointer->dag_node->childs->size);

	vector_t* aliases2 = vector_alloc();
	if (!aliases2)
		return -ENOMEM;

	ds_pointer->dag_node = ds_pointer->dag_node->childs->data[0];

	ret = _dn_status_alias_dereference(aliases2, ds_pointer, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		goto error;
	}

	for (i = 0; i < aliases2->size; i++) {
		ds =        aliases2->data[i];

		DS_POINTER_FROM_ALIAS(ds2, ds);

		ret = _dn_status_alias_dereference(aliases, ds2, c, bb, bb_list_head);

		dn_status_free(ds2);
		ds2 = NULL;

		if (ret < 0) {
			loge("\n");
			goto error;
		}
	}

	ret = 0;
error:
	vector_clear(aliases2, ( void (*)(void*) )dn_status_free);
	vector_free (aliases2);
	return ret;
}

int _dn_status_alias_dereference(vector_t* aliases, dn_status_t* ds_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dag_node_t*  dn = ds_pointer->dag_node;
	dn_status_t* ds = NULL;
	_3ac_code_t*  c2;
	list_t*      l2;

	assert(ds_pointer);
	assert(ds_pointer->dag_node);

//	logw("dn_pointer: \n");
//	dn_status_print(ds_pointer);

	if (OP_DEREFERENCE == dn->type)
		return _bb_dereference_aliases(aliases, ds_pointer, c, bb, bb_list_head);

	if (!type_is_var(dn->type)
			&& OP_INC != dn->type && OP_INC_POST != dn->type
			&& OP_DEC != dn->type && OP_DEC_POST != dn->type)

		return _bb_op_aliases(aliases, ds_pointer, c, bb, bb_list_head);


	variable_t* v = dn->var;

	if (v->arg_flag) {
		logd("arg: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
		return 0;
	} else if (v->global_flag) {
		logd("global: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
		return 0;
	} else if (FUNCTION_PTR == v->type) {
		logd("funcptr: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
		return 0;
	}

	for (l2 = &c->list; l2 != list_sentinel(&bb->code_list_head); l2 = list_prev(l2)) {

		c2  = list_data(l2, _3ac_code_t, list);

		if (!c2->dn_status_initeds)
			continue;

		ds = vector_find_cmp(c2->dn_status_initeds, ds_pointer, ds_cmp_like_indexes);

		if (ds && ds->alias) {

			if (vector_find(aliases, ds))
				return 0;

			if (vector_add(aliases, ds) < 0)
				return -ENOMEM;

			dn_status_ref(ds);
			return 0; 
		}
	}

	return _bb_pointer_aliases(aliases, bb_list_head, bb, ds_pointer);
}

int __alias_dereference(vector_t* aliases, dag_node_t* dn_pointer, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds = dn_status_null(); 
	if (!ds)
		return -ENOMEM;
	ds->dag_node = dn_pointer;

	int ret = _dn_status_alias_dereference(aliases, ds, c, bb, bb_list_head);

	dn_status_free(ds);
	return ret;
}

static int _pointer_alias_array_index2(dn_status_t* ds, dag_node_t* dn)
{
	dag_node_t* dn_base;
	dag_node_t* dn_index;
	dn_index_t* di;
	variable_t* v;

	dn_base = dn;

	v = dn_base->var;

	while (OP_ARRAY_INDEX == dn_base->type
			|| OP_POINTER == dn_base->type) {

		dn_index = dn_base->childs->data[1];

		int ret = dn_status_alias_index(ds, dn_index, dn_base->type);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		if (OP_ARRAY_INDEX == dn_base->type) {

			assert(dn_base->childs->size >= 3);

			di           = ds->alias_indexes->data[ds->alias_indexes->size - 1];
			di->dn_scale = dn_base->childs->data[2];

			assert(di->dn_scale);
		}

		dn_base  = dn_base->childs->data[0];
	}

	if (variable_nb_pointers(dn_base->var) > 0) {

		ds->alias      = dn_base;
		ds->alias_type = DN_ALIAS_ARRAY;

	} else if (dn_base->var->type >= STRUCT) {

		ds->alias      = dn_base;
		ds->alias_type = DN_ALIAS_MEMBER;
	} else {
		loge("\n");
		return -1;
	}

	return 0;
}

static int _pointer_alias_address_of(vector_t* aliases, dag_node_t* dn_alias)
{
	dn_status_t*  ds;
	dag_node_t*   dn_child;
	variable_t*   v;

	int ret;
	int type;

	ds = dn_status_null();
	if (!ds)
		return -ENOMEM;

	dn_child = dn_alias->childs->data[0];

	switch (dn_alias->childs->size) {
		case 1:
			assert(type_is_var(dn_child->type));
			ds->alias      = dn_child;
			ds->alias_type = DN_ALIAS_VAR;

			v = ds->alias->var;
			logd("alias: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
			break;
		case 2:
		case 3:
			ds->alias_indexes = vector_alloc();
			if (!ds->alias_indexes) {
				dn_status_free(ds);
				return -ENOMEM;
			}

			if (2 == dn_alias->childs->size)
				type = OP_POINTER;
			else
				type = OP_ARRAY_INDEX;

			ret = dn_status_alias_index(ds, dn_alias->childs->data[1], type);
			if (ret < 0) {
				loge("\n");
				dn_status_free(ds);
				return ret;
			}

			if (OP_ARRAY_INDEX == dn_alias->type) {

				dn_index_t* di;

				assert(dn_alias->childs->size >= 3);

				di           = ds->alias_indexes->data[ds->alias_indexes->size - 1];
				di->dn_scale = dn_alias->childs->data[2];

				assert(di->dn_scale);
			}

			ret = _pointer_alias_array_index2(ds, dn_child);
			if (ret < 0) {
				loge("\n");
				dn_status_free(ds);
				return ret;
			}
			break;
		default:
			loge("\n");
			dn_status_free(ds);
			return -1;
			break;
	};

	ret = vector_add(aliases, ds);
	if (ret < 0) {
		dn_status_free(ds);
		return ret;
	}
	return 0;
}

static int _ds_alias_array_indexes(dn_status_t* ds, dag_node_t* dn_alias, int nb_dimentions)
{
	dn_index_t*   di;
	int i;

	assert(ds);
	assert(dn_alias);
	assert(nb_dimentions >= 1);

	ds->alias_indexes = vector_alloc();
	if (!ds->alias_indexes)
		return -ENOMEM;

	for (i = nb_dimentions - 1; i >= 0; i--) {

		di = dn_index_alloc();
		if (!di)
			return -ENOMEM;
		di->index = 0;

		int ret = vector_add(ds->alias_indexes, di);
		if (ret < 0) {
			dn_index_free(di);
			return ret;
		}
	}

	ds->alias      = dn_alias;
	ds->alias_type = DN_ALIAS_ARRAY;
	return 0;
}

static int _pointer_alias_var(vector_t* aliases, dag_node_t* dn_alias, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	variable_t*   v = dn_alias->var;
	dn_status_t*  ds;
	dn_status_t*  ds2;
	dn_index_t*   di;
	_3ac_code_t*   c2;
	list_t*       l2;

	int ret;

	logd("alias: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

	if (dn_alias->var->nb_dimentions > 0) {

		ds = dn_status_null(); 
		if (!ds)
			return -ENOMEM;

		ret = _ds_alias_array_indexes(ds, dn_alias, dn_alias->var->nb_dimentions);
		if (ret < 0) {
			dn_status_free(ds);
			return ret;
		}

		ret = vector_add(aliases, ds);
		if (ret < 0) {
			dn_status_free(ds);
			return ret;
		}
		return 0;
	} else if (dn_alias->var->nb_pointers > 0) {

		dn_status_t* ds_pointer = dn_status_null();
		if (!ds_pointer)
			return -ENOMEM;
		ds_pointer->dag_node = dn_alias;

		ret = pointer_alias_ds(aliases, ds_pointer, c, bb, bb_list_head);
		dn_status_free(ds_pointer);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		if (0 == aliases->size) {

			ds = dn_status_null();
			if (!ds)
				return -ENOMEM;

			ds->alias      = dn_alias;
			ds->alias_type = DN_ALIAS_VAR;

			ret = vector_add(aliases, ds);
			if (ret < 0) {
				dn_status_free(ds);
				return ret;
			}
		}

		return ret;
	} else if (sizeof(void*) == dn_alias->var->size || variable_const_integer(dn_alias->var)) {

		ds = dn_status_null();
		if (!ds)
			return -ENOMEM;

		ds->alias      = dn_alias;
		ds->alias_type = DN_ALIAS_VAR;

		ret = vector_add(aliases, ds);
		if (ret < 0) {
			dn_status_free(ds);
			return ret;
		}
		return ret;
	}

	loge("\n");
	return -1;
}

static int _pointer_alias_array_index(vector_t* aliases, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds;
	dn_status_t* ds2;
	dn_status_t* ds3;
	_3ac_code_t*   c2;
	list_t*       l2;

	ds2 = dn_status_null();
	if (!ds2)
		return -ENOMEM;

	ds2->alias_indexes = vector_alloc();
	if (!ds2->alias_indexes) {
		dn_status_free(ds2);
		return -ENOMEM;
	}

	int ret = _pointer_alias_array_index2(ds2, dn);
	if (ret < 0) {
		loge("\n");
		dn_status_free(ds2);
		return -1;
	}

	ds2->dag_node      = ds2->alias;
	ds2->dn_indexes    = ds2->alias_indexes;
	ds2->alias         = NULL;
	ds2->alias_indexes = NULL;

	ret = pointer_alias_ds(aliases, ds2, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		dn_status_free(ds2);
		return ret;
	}

	if (0 == aliases->size) {
		ds2->alias         = ds2->dag_node;
		ds2->alias_indexes = ds2->dn_indexes;
		ds2->dag_node      = NULL;
		ds2->dn_indexes    = NULL;

		ret = vector_add(aliases, ds2);
		if (ret < 0) {
			dn_status_free(ds2);
			return ret;
		}
	} else {
		dn_status_free(ds2);
	}
	return ret;
}

static int _pointer_alias_call(vector_t* aliases, dag_node_t* dn_alias)
{
	dn_status_t* ds = dn_status_null();
	if (!ds)
		return -ENOMEM;

	int ret = vector_add(aliases, ds);
	if (ret < 0) {
		dn_status_free(ds);
		return ret;
	}

	ds->alias      = dn_alias;
	ds->alias_type = DN_ALIAS_ALLOC;
	return 0;
}

static int _pointer_alias_dereference2(vector_t* aliases, dn_status_t* ds, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	vector_t* tmp = vector_alloc();
	if (!tmp)
		return -ENOMEM;

	int ret = pointer_alias_ds(tmp, ds, c, bb, bb_list_head);
	if (ret < 0)
		goto error;

	int j;
	for (j = 0; j < tmp->size; j++) {
		ds =        tmp->data[j];

		ret = vector_add(aliases, ds);
		if (ret < 0)
			goto error;

		dn_status_ref(ds);
	}

	ret = tmp->size;
error:
	vector_clear(tmp, (void (*)(void*))dn_status_free);
	vector_free (tmp);
	return ret;
}

static int _pointer_alias_dereference(vector_t* aliases, dag_node_t* dn_alias, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds;
	dn_status_t* ds2;
	dag_node_t*  dn_child;
	vector_t*    aliases2 = NULL;

	assert(dn_alias->childs && 1 == dn_alias->childs->size);

	dn_child = dn_alias->childs->data[0];

	aliases2 = vector_alloc();
	if (!aliases2) {
		loge("\n");
		return -ENOMEM;
	}

	int ret = pointer_alias(aliases2, dn_child, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		goto error;
	}

	int i;
	for (i = 0; i < aliases2->size; i++) {
		ds =        aliases2->data[i];

		assert(ds);

		ds2 = dn_status_null();
		if (!ds2) {
			ret = -ENOMEM;
			goto error;
		}

		ret = ds_copy_alias(ds2, ds);
		if (ret < 0) {
			dn_status_free(ds2);
			goto error;
		}

		ds2->dag_node      = ds2->alias;
		ds2->dn_indexes    = ds2->alias_indexes;
		ds2->alias         = NULL;
		ds2->alias_indexes = NULL;

		ret = _pointer_alias_dereference2(aliases, ds2, c, bb, bb_list_head);
		if (ret < 0) {
			dn_status_free(ds2);
			goto error;
		}

		if (0 == ret) {
			ds2->alias         = ds2->dag_node;
			ds2->alias_indexes = ds2->dn_indexes;
			ds2->dag_node      = NULL;
			ds2->dn_indexes    = NULL;

			ret = vector_add(aliases, ds2);
			if (ret < 0) {
				dn_status_free(ds2);
				goto error;
			}
		} else
			dn_status_free(ds2);
		ds2 = NULL;
	}

error:
	vector_clear(aliases2, (void (*)(void*))dn_status_free);
	vector_free (aliases2);
	return ret;
}

static int _pointer_alias_add(vector_t* aliases, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds;
	dn_status_t* ds2;
	dag_node_t*  dn0;
	dag_node_t*  dn1;
	dn_index_t*  di;

	assert(2 == dn->childs->size);
	dn0 = dn->childs->data[0];
	dn1 = dn->childs->data[1];

	while (OP_TYPE_CAST == dn0->type
			|| OP_EXPR  == dn0->type) {
		assert(1 == dn0->childs->size);
		dn0 = dn0->childs->data[0];
	}

	while (OP_TYPE_CAST == dn1->type
			|| OP_EXPR  == dn1->type) {
		assert(1 == dn1->childs->size);
		dn1 = dn1->childs->data[0];
	}

	di = dn_index_alloc();
	if (!di) {
		dn_status_free(ds2);
		return -ENOMEM;
	}

	if (variable_nb_pointers(dn0->var) > 0) {

		ds2 = NULL;
		int ret = ds_for_dn(&ds2, dn0);
		if (ret < 0)
			return ret;

		if (variable_const(dn1->var))
			di->index = dn1->var->data.i64;
		else
			di->index = -1;

	} else if (variable_nb_pointers(dn1->var) > 0) {

		ds2 = NULL;
		int ret = ds_for_dn(&ds2, dn1);
		if (ret < 0)
			return ret;

		if (variable_const(dn0->var))
			di->index = dn0->var->data.i64;
		else
			di->index = -1;
	} else {
		loge("dn0->nb_pointers: %d\n", dn0->var->nb_pointers);
		loge("dn1->nb_pointers: %d\n", dn1->var->nb_pointers);

		dn_status_free(ds2);
		return -1;
	}

	if (!ds2->dn_indexes) {
		ds2->dn_indexes = vector_alloc();

		if (!ds2->dn_indexes) {
			dn_index_free(di);
			dn_status_free(ds2);
			return -ENOMEM;
		}
	}

	if (vector_add(ds2->dn_indexes, di) < 0) {
		dn_index_free(di);
		dn_status_free(ds2);
		return -ENOMEM;
	}

	int i;
	for (i = ds2->dn_indexes->size - 2; i >= 0; i--)
		ds2->dn_indexes->data[i + 1] = ds2->dn_indexes->data[i];

	ds2->dn_indexes->data[0] = di;

	int ret = pointer_alias_ds(aliases, ds2, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		dn_status_free(ds2);
		return ret;
	}

	if (0 == aliases->size) {
		ds2->alias         = ds2->dag_node;
		ds2->alias_indexes = ds2->dn_indexes;
		ds2->dag_node      = NULL;
		ds2->dn_indexes    = NULL;
		ds2->alias_type    = DN_ALIAS_ARRAY;

		ret = vector_add(aliases, ds2);
		if (ret < 0) {
			loge("\n");
			dn_status_free(ds2);
			return ret;
		}
	} else
		dn_status_free(ds2);

	return ret;
}

static int _pointer_alias_sub(vector_t* aliases, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds;
	dn_status_t* ds2;
	dag_node_t*  dn0;
	dag_node_t*  dn1;
	dn_index_t*  di;

	assert(2 == dn->childs->size);
	dn0 = dn->childs->data[0];
	dn1 = dn->childs->data[1];

	while (OP_TYPE_CAST == dn0->type
			|| OP_EXPR  == dn0->type) {
		assert(1 == dn0->childs->size);
		dn0 = dn0->childs->data[0];
	}

	while (OP_TYPE_CAST == dn1->type
			|| OP_EXPR  == dn1->type) {
		assert(1 == dn1->childs->size);
		dn1 = dn1->childs->data[0];
	}

	ds2 = dn_status_null();
	if (!ds2)
		return -ENOMEM;

	ds2->dn_indexes = vector_alloc();
	if (!ds2->dn_indexes) {
		dn_status_free(ds2);
		return -ENOMEM;
	}

	di = dn_index_alloc();
	if (!di) {
		dn_status_free(ds2);
		return -ENOMEM;
	}

	if (vector_add(ds2->dn_indexes, di) < 0) {
		dn_index_free(di);
		dn_status_free(ds2);
		return -ENOMEM;
	}

	if (variable_nb_pointers(dn0->var) > 0) {
		assert(type_is_var(dn0->type));

		ds2->dag_node = dn0;

		if (variable_const(dn1->var))
			di->index = dn1->var->data.i64;
		else
			di->index = -1;
	} else {
		loge("dn0->nb_pointers: %d\n", dn0->var->nb_pointers);
		loge("dn1->nb_pointers: %d\n", dn1->var->nb_pointers);

		dn_status_free(ds2);
		return -1;
	}

	int ret = pointer_alias_ds(aliases, ds2, c, bb, bb_list_head);
	if (ret < 0) {
		loge("\n");
		dn_status_free(ds2);
		return ret;
	}

	if (0 == aliases->size) {
		ds2->alias         = ds2->dag_node;
		ds2->alias_indexes = ds2->dn_indexes;
		ds2->dag_node      = NULL;
		ds2->dn_indexes    = NULL;
		ds2->alias_type    = DN_ALIAS_ARRAY;

		ret = vector_add(aliases, ds2);
		if (ret < 0) {
			loge("\n");
			dn_status_free(ds2);
			return ret;
		}
	} else
		dn_status_free(ds2);

	return ret;
}

static int _pointer_alias_logic(vector_t* aliases, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dn_status_t* ds       = NULL;
	dn_status_t* ds2      = NULL;
	vector_t*    aliases2 = NULL;

	assert(2 == dn->childs->size);

	int ret = ds_for_dn(&ds, dn->childs->data[0]);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = ds_for_dn(&ds2, dn->childs->data[1]);
	if (ret < 0) {
		loge("\n");
		dn_status_free(ds);
		return ret;
	}

	if (ds_is_pointer(ds) > 0) {

		ret = pointer_alias_ds(aliases, ds, c, bb, bb_list_head);
		if (ret < 0) {
			loge("\n");
			goto error;
		}

		if (0 == aliases->size) {
			ds->alias         = ds->dag_node;
			ds->alias_indexes = ds->dn_indexes;
			ds->dag_node      = NULL;
			ds->dn_indexes    = NULL;
			ds->alias_type    = DN_ALIAS_VAR;

			ret = vector_add(aliases, ds);
			if (ret < 0) {
				loge("\n");
				goto error;
			}
		} else {
			dn_status_free(ds);
			ds = NULL;
		}

	} else if (ds_is_pointer(ds2) > 0) {

		aliases2 = vector_alloc();
		if (!aliases2) {
			ret = -ENOMEM;
			goto error;
		}

		ret = pointer_alias_ds(aliases2, ds2, c, bb, bb_list_head);
		if (ret < 0) {
			loge("\n");
			goto error;
		}

		if (0 == aliases2->size) {
			ds2->alias         = ds2->dag_node;
			ds2->alias_indexes = ds2->dn_indexes;
			ds2->dag_node      = NULL;
			ds2->dn_indexes    = NULL;
			ds2->alias_type    = DN_ALIAS_VAR;

			ret = vector_add(aliases, ds2);
			if (ret < 0) {
				loge("\n");
				goto error;
			}
		} else {
			dn_status_free(ds2);
			ds2 = NULL;

			int i;
			for (i = 0; i < aliases2->size; i++) {
				ds2       = aliases2->data[i];

				if (vector_add_unique(aliases, ds2) < 0) {
					ret = -ENOMEM;
					goto error;
				}
			}

			ds2 = NULL;
		}
	} else if (!variable_const(ds->dag_node->var)) {

		ds->alias         = ds->dag_node;
		ds->alias_indexes = ds->dn_indexes;
		ds->dag_node      = NULL;
		ds->dn_indexes    = NULL;
		ds->alias_type    = DN_ALIAS_VAR;

		if (vector_add_unique(aliases, ds) < 0) {
			ret = -ENOMEM;
			goto error;
		}

		ds = NULL;
	} else if (!variable_const(ds2->dag_node->var)) {

		ds2->alias         = ds2->dag_node;
		ds2->alias_indexes = ds2->dn_indexes;
		ds2->dag_node      = NULL;
		ds2->dn_indexes    = NULL;
		ds2->alias_type    = DN_ALIAS_VAR;

		if (vector_add_unique(aliases, ds2) < 0) {
			ret = -ENOMEM;
			goto error;
		}

		ds2 = NULL;
	} else {
		ds->alias         = ds->dag_node;
		ds->alias_indexes = ds->dn_indexes;
		ds->dag_node      = NULL;
		ds->dn_indexes    = NULL;
		ds->alias_type    = DN_ALIAS_VAR;

		if (vector_add_unique(aliases, ds) < 0) {
			ret = -ENOMEM;
			goto error;
		}

		ds = NULL;
	}

	return ret;
error:
	if (aliases2)
		vector_free(aliases2);

	if (ds)
		dn_status_free(ds);

	if (ds2)
		dn_status_free(ds2);
	return ret;
}

int pointer_alias(vector_t* aliases, dag_node_t* dn_alias, _3ac_code_t* c, basic_block_t* bb, list_t* bb_list_head)
{
	dag_node_t* dn_child = NULL;
	variable_t* v;

	v = dn_alias->var;
	logd("v: %d/%s\n", v->w->line, v->w->text->data);

	while (OP_TYPE_CAST == dn_alias->type) {

		assert(dn_alias->childs && 1 == dn_alias->childs->size);

		dn_alias = dn_alias->childs->data[0];
	}

	int ret;
	if (type_is_var(dn_alias->type))
		return _pointer_alias_var(aliases, dn_alias, c, bb, bb_list_head);

	switch (dn_alias->type) {

		case OP_ADDRESS_OF:
			ret = _pointer_alias_address_of(aliases, dn_alias);
			return ret;
			break;

		case OP_ARRAY_INDEX:
		case OP_POINTER:
			ret = _pointer_alias_array_index(aliases, dn_alias, c, bb, bb_list_head);
			return ret;
			break;

		case OP_CALL:
			ret = _pointer_alias_call(aliases, dn_alias);
			return ret;
			break;

		case OP_ADD:
			ret = _pointer_alias_add(aliases, dn_alias, c, bb, bb_list_head);
			return ret;
			break;

		case OP_SUB:
			ret = _pointer_alias_sub(aliases, dn_alias, c, bb, bb_list_head);
			return ret;
			break;

		case OP_BIT_AND:
		case OP_BIT_OR:
			ret = _pointer_alias_logic(aliases, dn_alias, c, bb, bb_list_head);
			return ret;
			break;

		case OP_DEREFERENCE:
			if (dn_alias->var && dn_alias->var->w) {
				v = dn_alias->var;
				logd("type: %d, v_%d_%d/%s\n", dn_alias->type, v->w->line, v->w->pos, v->w->text->data);
			} else
				logd("type: %d, v_%#lx\n", dn_alias->type, 0xffff & (uintptr_t)dn_alias->var);

			ret = _pointer_alias_dereference(aliases, dn_alias, c, bb, bb_list_head);
			return ret;
			break;
		default:
			if (dn_alias->var && dn_alias->var->w) {
				v = dn_alias->var;
				loge("type: %d, v_%d_%d/%s\n", dn_alias->type, v->w->line, v->w->pos, v->w->text->data);
			} else
				loge("type: %d, v_%#lx\n", dn_alias->type, 0xffff & (uintptr_t)dn_alias->var);
			return -1;
			break;
	}
	return 0;
}
