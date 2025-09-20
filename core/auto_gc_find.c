#include"optimizer.h"
#include"pointer_alias.h"

static int _bb_find_ds( basic_block_t* bb,  dn_status_t* ds_obj)
{
	if ( vector_find_cmp(bb->ds_freed, ds_obj,  ds_cmp_same_indexes))
		return 0;

	if ( vector_find_cmp(bb->ds_malloced, ds_obj,  ds_cmp_same_indexes))
		return 1;
	return 0;
}

static int __ds_append_index_n( dn_status_t* dst,  dn_status_t* src, int n)
{
	 dn_index_t* di;
	int j;

	assert(n <= src->dn_indexes->size);

	for (j = 0; j < n; j++) {
		di = src->dn_indexes->data[j];

		int ret =  vector_add_front(dst->dn_indexes, di);
		if (ret < 0)
			return ret;
		di->refs++;
	}

	return 0;
}

int __bb_find_ds_alias( vector_t* aliases,  dn_status_t* ds_obj,  _3ac_code_t* c,  basic_block_t* bb,  list_t* bb_list_head)
{
	 vector_t*    tmp;
	 dn_status_t* ds2;
	 dn_status_t* ds;
	 dn_index_t*  di;
	 _3ac_code_t*  c2 =  list_data( list_prev(&c->list),  _3ac_code_t, list);

	int ndi = 0;
	int ret = 0;
	int j;

	tmp =  vector_alloc();
	if (!tmp)
		return -ENOMEM;

	ds =  dn_status_clone(ds_obj);
	if (!ds) {
		 vector_free(tmp);
		return -ENOMEM;
	}

	while (1) {
		ret =  pointer_alias_ds(tmp, ds, c2, bb, bb_list_head);
		if (ret < 0) {
			if ( POINTER_NOT_INIT == ret)
				break;
			goto error;
		}

		for (j = 0; j < tmp->size; j++) {
			ds2       = tmp->data[j];

			 XCHG(ds2->dn_indexes, ds2->alias_indexes);
			 XCHG(ds2->dag_node,   ds2->alias);

			if (ds_obj->dn_indexes) {

				if (!ds2->dn_indexes) {
					ds2 ->dn_indexes =  vector_alloc();
					if (!ds2->dn_indexes) {
						ret = -ENOMEM;
						goto error;
					}
				}

				ret = __ds_append_index_n(ds2, ds_obj, ndi);
				if (ret < 0)
					goto error;
			}

			ret =  vector_add(aliases, ds2);
			if (ret < 0)
				goto error;

			 dn_status_ref(ds2);
			ds2 = NULL;
		}

		 vector_clear(tmp, ( void (*)(void*) ) dn_status_free);

		if (ds->dn_indexes) {
			assert(ds->dn_indexes->size > 0);
			di =   ds->dn_indexes->data[0];

			assert(0 ==  vector_del(ds->dn_indexes, di));

			 dn_index_free(di);
			di = NULL;

			ndi++;

			if (0 == ds->dn_indexes->size) {
				 vector_free(ds->dn_indexes);
				ds->dn_indexes = NULL;
			}
		} else
			break;
	}

	ret = 0;
error:
	 dn_status_free(ds);
	 vector_clear(tmp, ( void (*)(void*) ) dn_status_free);
	 vector_free(tmp);
	return ret;
}

static int _bb_find_ds_alias( dn_status_t* ds_obj,  _3ac_code_t* c,  basic_block_t* bb,  list_t* bb_list_head)
{
	 dn_status_t* ds_obj2;
	 dn_status_t* ds;
	 dn_index_t*  di;
	 vector_t*    aliases;
	int i;

	aliases =  vector_alloc();
	if (!aliases)
		return -ENOMEM;

	int ret = __bb_find_ds_alias(aliases, ds_obj, c, bb, bb_list_head);
	if (ret < 0)
		return ret;

	int need = 0;
	for (i = 0; i < aliases->size; i++) {
		ds =        aliases->data[i];

		 logd("ds: %#lx, ds->refs: %d\n", 0xffff & (uintptr_t)ds, ds->refs);
		if (!ds->dag_node)
			continue;

		if ( vector_find_cmp(bb->ds_malloced, ds,  ds_cmp_same_indexes)
		&& ! vector_find_cmp(bb->ds_freed,    ds,  ds_cmp_same_indexes)) {
			need = 1;
			break;
		}
	}

	if ( vector_find_cmp(bb->ds_malloced, ds_obj,  ds_cmp_same_indexes)
	&& ! vector_find_cmp(bb->ds_freed,    ds_obj,  ds_cmp_same_indexes)) {
		need = 1;
	}

	ret = need;
error:
	 vector_clear(aliases, ( void (*)(void*) )  dn_status_free);
	 vector_free (aliases);
	return ret;
}

static int _auto_gc_find_argv_in( basic_block_t* cur_bb,  _3ac_code_t* c)
{
	 _3ac_operand_t* src;
	 dag_node_t*    dn;
	 variable_t*    v;

	int i;
	for (i  = 1; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		dn  = src->dag_node;

		while (dn) {
			if ( OP_TYPE_CAST == dn->type)
				dn = dn->childs->data[0];

			else if ( OP_EXPR == dn->type)
				dn = dn->childs->data[0];

			else if ( OP_POINTER == dn->type)
				dn = dn->childs->data[0];
			else
				break;
		}

		v = dn->var;

		if (v->nb_pointers + v->nb_dimentions + (v->type >=  STRUCT) < 2)
			continue;

		 logd("v: %s\n", v->w->text->data);

		if ( vector_add_unique(cur_bb->entry_dn_actives, dn) < 0)
			return -ENOMEM;
	}

	return 0;
}

static int _auto_gc_bb_next_find( basic_block_t* bb, void* data,  vector_t* queue)
{
	 basic_block_t* next_bb;
	 dn_status_t*   ds;
	 dn_status_t*   ds2;

	int count = 0;
	int ret;
	int j;

	for (j = 0; j < bb->nexts->size; j++) {
		next_bb   = bb->nexts->data[j];

		int k;
		for (k = 0; k < bb->ds_malloced->size; k++) {
			ds =        bb->ds_malloced->data[k];

			if ( vector_find_cmp(bb->ds_freed, ds,  ds_cmp_same_indexes))
				continue;

			if ( vector_find_cmp(next_bb->ds_freed, ds,  ds_cmp_same_indexes))
				continue;

			ds2 =  vector_find_cmp(next_bb->ds_malloced, ds,  ds_cmp_like_indexes);
			if (ds2) {
				uint32_t tmp = ds2->ret_flag;

				ds2->ret_flag |= ds->ret_flag;

				if (tmp != ds2->ret_flag) {
					 logd("*** ds2: %#lx, ret_index: %d, ret_flag: %d, ds: %#lx, ret_index: %d, ret_flag: %d\n",
							0xffff & (uintptr_t)ds2, ds2->ret_index, ds2->ret_flag,
							0xffff & (uintptr_t)ds,   ds->ret_index,  ds->ret_flag);

					count++;
					ds2->ret_index = ds->ret_index;
				}
				continue;
			}

			ds2 =  dn_status_clone(ds);
			if (!ds2)
				return -ENOMEM;

			ret =  vector_add(next_bb->ds_malloced, ds2);
			if (ret < 0) {
				 dn_status_free(ds2);
				return ret;
			}
			++count;
		}

		ret =  vector_add(queue, next_bb);
		if (ret < 0)
			return ret;
	}
	return count;
}

static int _bfs_sort_function( vector_t* fqueue,  vector_t* functions)
{
	 function_t* fmalloc = NULL;
	 function_t* f;
	 function_t* f2;
	int i;
	int j;

	for (i = 0; i < functions->size; i++) {
		f  =        functions->data[i];

		f->visited_flag = 0;

		if (!fmalloc && !strcmp(f->node.w->text->data, " _auto_malloc"))
			fmalloc = f;
	}

	if (!fmalloc)
		return 0;

	int ret =  vector_add(fqueue, fmalloc);
	if (ret < 0)
		return ret;

	for (i = 0; i < fqueue->size; i++) {
		f  =        fqueue->data[i];

		if (f->visited_flag)
			continue;

		 logd("f: %p, %s\n", f, f->node.w->text->data);

		f->visited_flag = 1;

		for (j = 0; j < f->caller_functions->size; j++) {
			f2 =        f->caller_functions->data[j];

			if (f2->visited_flag)
				continue;

			ret =  vector_add(fqueue, f2);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}
