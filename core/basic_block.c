#include"basic_block.h"
#include"dag.h"
#include"3ac.h"
#include"pointer_alias.h"

 basic_block_t*  basic_block_alloc()
{
	 basic_block_t* bb = calloc(1, sizeof( basic_block_t));
	if (!bb)
		return NULL;

	 list_init(&bb->list);
	 list_init(&bb->dag_list_head);
	 list_init(&bb->code_list_head);
	 list_init(&bb->save_list_head);

	bb->prevs =  vector_alloc();
	if (!bb->prevs)
		goto error_prevs;

	bb->nexts =  vector_alloc();
	if (!bb->nexts)
		goto error_nexts;

	bb->entry_dn_delivery =  vector_alloc();
	if (!bb->entry_dn_delivery)
		goto error_entry_delivery;

	bb->entry_dn_inactives =  vector_alloc();
	if (!bb->entry_dn_inactives)
		goto error_entry_inactive;

	bb->entry_dn_actives =  vector_alloc();
	if (!bb->entry_dn_actives)
		goto error_entry_active;

	bb->exit_dn_actives  =  vector_alloc();
	if (!bb->exit_dn_actives)
		goto error_exit;

	bb->dn_updateds =  vector_alloc();
	if (!bb->dn_updateds)
		goto error_updateds;

	bb->dn_loads =  vector_alloc();
	if (!bb->dn_loads)
		goto error_loads;

	bb->dn_saves =  vector_alloc();
	if (!bb->dn_saves)
		goto error_saves;

	bb->dn_colors_entry =  vector_alloc();
	if (!bb->dn_colors_entry)
		goto error_colors_entry;

	bb->dn_colors_exit =  vector_alloc();
	if (!bb->dn_colors_exit)
		goto error_colors_exit;

	bb->dn_status_initeds =  vector_alloc();
	if (!bb->dn_status_initeds)
		goto error_dn_status_initeds;

	bb->dn_pointer_aliases =  vector_alloc();
	if (!bb->dn_pointer_aliases)
		goto error_dn_pointer_aliases;

	bb->entry_dn_aliases =  vector_alloc();
	if (!bb->entry_dn_aliases)
		goto error_entry_dn_aliases;

	bb->exit_dn_aliases =  vector_alloc();
	if (!bb->exit_dn_aliases)
		goto error_exit_dn_aliases;

	bb->dn_reloads =  vector_alloc();
	if (!bb->dn_reloads)
		goto error_reloads;

	bb->dn_resaves =  vector_alloc();
	if (!bb->dn_resaves)
		goto error_resaves;

	bb->ds_malloced =  vector_alloc();
	if (!bb->ds_malloced)
		goto error_malloced;

	bb->ds_freed =  vector_alloc();
	if (!bb->ds_freed)
		goto error_freed;

	return bb;

error_freed:
	 vector_free(bb->ds_malloced);
error_malloced:
	 vector_free(bb->dn_resaves);
error_resaves:
	 vector_free(bb->dn_reloads);
error_reloads:
	 vector_free(bb->exit_dn_aliases);
error_exit_dn_aliases:
	 vector_free(bb->entry_dn_aliases);
error_entry_dn_aliases:
	 vector_free(bb->dn_pointer_aliases);
error_dn_pointer_aliases:
	 vector_free(bb->dn_status_initeds);
error_dn_status_initeds:
	 vector_free(bb->dn_colors_exit);
error_colors_exit:
	 vector_free(bb->dn_colors_entry);
error_colors_entry:
	 vector_free(bb->dn_saves);
error_saves:
	 vector_free(bb->dn_loads);
error_loads:
	 vector_free(bb->dn_updateds);
error_updateds:
	 vector_free(bb->exit_dn_actives);
error_exit:
	 vector_free(bb->entry_dn_actives);
error_entry_active:
	 vector_free(bb->entry_dn_inactives);
error_entry_inactive:
	 vector_free(bb->entry_dn_delivery);
error_entry_delivery:
	 vector_free(bb->nexts);
error_nexts:
	 vector_free(bb->prevs);
error_prevs:
	free(bb);
	return NULL;
}

 basic_block_t*  basic_block_jcc( basic_block_t* to,  function_t* f, int jcc)
{
	 basic_block_t* bb;
	 _3ac_operand_t* dst;
	 _3ac_code_t*    c;

	bb =  basic_block_alloc();
	if (!bb)
		return NULL;

	c =  _3ac_jmp_code(jcc, NULL, NULL);
	if (!c) {
		 basic_block_free(bb);
		return NULL;
	}

	 list_add_tail(&bb->code_list_head, &c->list);

	if ( vector_add(f->jmps, c) < 0) {
		 basic_block_free(bb);
		return NULL;
	}

	dst     = c->dsts->data[0];
	dst->bb = to;

	c->basic_block = bb;

	bb->jmp_flag = 1;
	return bb;
}

void  basic_block_free( basic_block_t* bb)
{
	if (bb) {
		// this bb's DAG nodes freed here
		 list_clear(&bb->dag_list_head,  dag_node_t, list,  dag_node_free);

		 list_clear(&bb->code_list_head,  _3ac_code_t, list,  _3ac_code_free);
		 list_clear(&bb->save_list_head,  _3ac_code_t, list,  _3ac_code_free);

		// DAG nodes were freed by ' list_clear' above, so only free this vector
		if (bb->var_dag_nodes)
			 vector_free(bb->var_dag_nodes);

		if (bb->prevs)
			 vector_free(bb->prevs);

		if (bb->nexts)
			 vector_free(bb->nexts);
	}
}

 bb_group_t*  bb_group_alloc()
{
	 bb_group_t* bbg = calloc(1, sizeof( bb_group_t));
	if (!bbg)
		return NULL;

	bbg->body =  vector_alloc();
	if (!bbg->body) {
		free(bbg);
		return NULL;
	}

	return bbg;
}

void  bb_group_free( bb_group_t* bbg)
{
	if (bbg) {
		if (bbg->body)
			 vector_free(bbg->body);

		free(bbg);
	}
}

int  basic_block_connect( basic_block_t* prev_bb,  basic_block_t* next_bb)
{
	int ret =  vector_add_unique(prev_bb->nexts, next_bb);
	if (ret < 0)
		return ret;

	return  vector_add_unique(next_bb->prevs, prev_bb);
}

void  basic_block_print( basic_block_t* bb,  list_t* sentinel)
{
	if (bb) {
#define  BB_PRINT(h) \
		do { \
			 _3ac_code_t* c; \
			 list_t*     l; \
			for (l =  list_head(&bb->h); l !=  list_sentinel(&bb->h); l =  list_next(l)) { \
				c  =  list_data(l,  _3ac_code_t, list); \
				 _3ac_code_print(c, sentinel); \
			} \
		} while (0)

		 BB_PRINT(code_list_head);
		 BB_PRINT(save_list_head);
	}
}

static void __bb_group_print( bb_group_t* bbg)
{
	 basic_block_t* bb;
	int i;

	printf("\033[34mentries:\033[0m\n");
	if (bbg->entries) {
		for (i = 0; i < bbg->entries->size; i++) {
			bb =        bbg->entries->data[i];

			printf("%p, %d\n", bb, bb->index);
		}
	}

	printf("\033[35mbody:\033[0m\n");
	for (i = 0; i < bbg->body->size; i++) {
		bb =        bbg->body->data[i];

		printf("%p, %d\n", bb, bb->index);
	}

	printf("\033[36mexits:\033[0m\n");
	if (bbg->exits) {
		for (i = 0; i < bbg->exits->size; i++) {
			bb =        bbg->exits->data[i];

			printf("%p, %d\n", bb, bb->index);
		}
	}
}

void  bb_group_print( bb_group_t* bbg)
{
	printf("\033[33mbbg: %p, loop_layers: %d\033[0m\n", bbg, bbg->loop_layers);

	__bb_group_print(bbg);

	printf("\n");
}

void  bb_loop_print( bb_group_t* loop)
{
	 basic_block_t* bb;
	 bb_group_t*    bbg;

	int k;

	if (loop->loop_childs) {
		for (k = 0; k < loop->loop_childs->size; k++) {
			bbg       = loop->loop_childs->data[k];

			 bb_loop_print(bbg);
		}
	}

	printf("\033[33mloop:  %p, loop_layers: %d\033[0m\n", loop, loop->loop_layers);

	__bb_group_print(loop);

	if (loop->loop_childs) {
		printf("childs: %d\n", loop->loop_childs->size);

		for (k = 0; k <   loop->loop_childs->size; k++)
			printf("%p ", loop->loop_childs->data[k]);
		printf("\n");
	}

	if (loop->loop_parent)
		printf("parent: %p\n", loop->loop_parent);

	printf("\n");
}

void  basic_block_print_list( list_t* h)
{
	if ( list_empty(h))
		return;

	 dn_status_t*   ds;
	 dag_node_t*    dn;
	 variable_t*    v;

	 basic_block_t* bb;
	 basic_block_t* bb2;
	 basic_block_t* last_bb  =  list_data( list_tail(h),  basic_block_t, list);
	 list_t*        sentinel =  list_sentinel(&last_bb->code_list_head);
	 list_t*        l;

	for (l =  list_head(h); l !=  list_sentinel(h); l =  list_next(l)) {
		bb =  list_data(l,  basic_block_t, list);

		printf("\033[34mbasic_block: %p, index: %d, dfo: %d, cmp_flag: %d, call_flag: %d, group: %d, loop: %d, dereference_flag: %d, ret_flag: %d\033[0m\n",
				bb, bb->index, bb->dfo, bb->cmp_flag, bb->call_flag, bb->group_flag, bb->loop_flag, bb->dereference_flag, bb->ret_flag);

		 basic_block_print(bb, sentinel);

		printf("\n");
		int i;
		if (bb->prevs) {
			for (i = 0; i < bb->prevs->size; i++) {
				bb2       = bb->prevs->data[i];

				printf("prev     : %p, index: %d\n", bb2, bb2->index);
			}
		}
		if (bb->nexts) {
			for (i = 0; i < bb->nexts->size; i++) {
				bb2       = bb->nexts->data[i];

				printf("next     : %p, index: %d\n", bb2, bb2->index);
			}
		}

		if (bb->dn_status_initeds) {
			printf("inited: \n");
			for (i = 0; i < bb->dn_status_initeds->size; i++) {
				ds =        bb->dn_status_initeds->data[i];

				 dn_status_print(ds);
			}
			printf("\n");
		}

		if (bb->ds_malloced) {
			printf("auto gc: \n");
			for (i = 0; i < bb->ds_malloced->size; i++) {
				ds =        bb->ds_malloced->data[i];

				if ( vector_find_cmp(bb->ds_freed, ds,  ds_cmp_same_indexes))
					continue;
				 dn_status_print(ds);
			}
			printf("\n");
		}

		if (bb->entry_dn_actives) {
			for (i = 0; i < bb->entry_dn_actives->size; i++) {
				dn =        bb->entry_dn_actives->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("entry active: v_%d_%d/%s_%#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		if (bb->exit_dn_actives) {
			for (i = 0; i < bb->exit_dn_actives->size; i++) {
				dn =        bb->exit_dn_actives->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("exit  active: v_%d_%d/%s_%#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		if (bb->entry_dn_aliases) {
			for (i = 0; i < bb->entry_dn_aliases->size; i++) {
				dn =        bb->entry_dn_aliases->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("entry alias:  v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
			}
		}
		if (bb->exit_dn_aliases) {
			for (i = 0; i < bb->exit_dn_aliases->size; i++) {
				dn =        bb->exit_dn_aliases->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("exit  alias:  v_%d_%d/%s, dn: %#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		if (bb->dn_updateds) {
			for (i = 0; i < bb->dn_updateds->size; i++) {
				dn =        bb->dn_updateds->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("updated:      v_%d_%d/%s_%#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		if (bb->dn_loads) {
			for (i = 0; i < bb->dn_loads->size; i++) {
				dn =        bb->dn_loads->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("loads:        v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
			}
		}

		if (bb->dn_reloads) {
			for (i = 0; i < bb->dn_reloads->size; i++) {
				dn =        bb->dn_reloads->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("reloads:      v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);
			}
		}

		if (bb->dn_saves) {
			for (i = 0; i < bb->dn_saves->size; i++) {
				dn =        bb->dn_saves->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("saves:        v_%d_%d/%s, dn: %#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		if (bb->dn_resaves) {
			for (i = 0; i < bb->dn_resaves->size; i++) {
				dn =        bb->dn_resaves->data[i];

				v  = dn->var;
				if (v && v->w)
					printf("resaves:      v_%d_%d/%s, dn: %#lx\n", v->w->line, v->w->pos, v->w->text->data, 0xffff & (uintptr_t)dn);
			}
		}

		printf("\n");
	}
}

static int _copy_to_active_vars( vector_t* active_vars,  vector_t* dag_nodes)
{
	 dag_node_t*   dn;
	 dn_status_t*  ds;
	int i;

	for (i = 0; i < dag_nodes->size; i++) {
		dn        = dag_nodes->data[i];

		ds =  vector_find_cmp(active_vars, dn,  dn_status_cmp);

		if (!ds) {
			ds =  dn_status_alloc(dn);
			if (!ds)
				return -ENOMEM;

			int ret =  vector_add(active_vars, ds);
			if (ret < 0) {
				 dn_status_free(ds);
				return ret;
			}
		}
#if 0
		ds->alias   = dn->alias;
		ds->value   = dn->value;
#endif
		ds->active  = dn->active;
		ds->inited  = dn->inited;
		ds->updated = dn->updated;
		dn->inited  = 0;
	}

	return 0;
}

static int _copy_vars_by_active( vector_t* dn_vec,  vector_t* ds_vars, int active)
{
	if (!dn_vec)
		return -EINVAL;

	if (!ds_vars)
		return 0;

	 dn_status_t* ds;
	 dag_node_t*  dn;
	int j;

	for (j = 0; j < ds_vars->size; j++) {
		ds =        ds_vars->data[j];

		dn = ds->dag_node;

		if (active == ds->active &&  dn_through_bb(dn)) {

			int ret =  vector_add_unique(dn_vec, dn);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int _copy_updated_vars( vector_t* dn_vec,  vector_t* ds_vars)
{
	if (!dn_vec)
		return -EINVAL;

	if (!ds_vars)
		return 0;

	 dn_status_t* ds;
	 dag_node_t*  dn;
	int j;

	for (j = 0; j < ds_vars->size; j++) {
		ds =        ds_vars->data[j];

		dn = ds->dag_node;

		if ( variable_const(dn->var))
			continue;

		if (!ds->updated)
			continue;

		if ( dn_through_bb(dn)) {
			int ret =  vector_add_unique(dn_vec, dn);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}

static int _bb_vars( basic_block_t* bb)
{
	 _3ac_code_t* c;
	 dag_node_t* dn;
	 list_t*     l;

	int ret = 0;

	if (!bb->var_dag_nodes) {
		bb->var_dag_nodes =  vector_alloc();
		if (!bb->var_dag_nodes)
			return -ENOMEM;
	} else
		 vector_clear(bb->var_dag_nodes, NULL);

	for (l =  list_tail(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_prev(l)) {

		c  =  list_data(l,  _3ac_code_t, list);

		if ( type_is_jmp(c->op->type))
			continue;

		 _3ac_operand_t* src;
		 _3ac_operand_t* dst;
		 variable_t*    v;
		int j;

		if (c->dsts) {
			for (j  = 0; j < c->dsts->size; j++) {
				dst =        c->dsts->data[j];

				if (!dst->dag_node)
					continue;
			
				dn  = dst->dag_node;

				if ( dn_through_bb(dn)) {
					ret =  vector_add_unique(bb->var_dag_nodes, dn);
					if (ret < 0)
						return ret;
				}
			}
		}

		if (c->srcs) {
			for (j  = 0; j < c->srcs->size; j++) {
				src =        c->srcs->data[j];

				if (!src->dag_node)
					continue;

				dn = src->dag_node;
				v  = dn->var;

				if ( type_is_operator(dn->type)
						|| ! variable_const(v)
						|| (v->nb_dimentions > 0 && ( OP_ARRAY_INDEX == c->op->type ||  type_is_assign_array_index(c->op->type)))
						) {

					ret =  vector_add_unique(bb->var_dag_nodes, dn);
					if (ret < 0)
						return ret;
				}
			}
		}
	}

	return 0;
}

int  basic_block_dag( basic_block_t* bb,  list_t* dag_list_head)
{
	 list_t*     l;
	 _3ac_code_t* c;

	for (l =  list_head(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_next(l)) {
		c  =  list_data(l,  _3ac_code_t, list);

		int ret =  _3ac_code_to_dag(c, dag_list_head);
		if (ret < 0)
			return ret;
	}
	return 0;
}

int  basic_block_vars( basic_block_t* bb,  list_t* bb_list_head)
{
	 list_t*     l;
	 _3ac_code_t* c;

	int ret = _bb_vars(bb);
	if (ret < 0) {
		 loge("\n");
		return ret;
	}

	for (l =  list_head(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_next(l)) {
		c  =  list_data(l,  _3ac_code_t, list);

		if (c->active_vars)
			 vector_clear(c->active_vars, (void (*)(void*)) dn_status_free);
		else {
			c->active_vars =  vector_alloc();
			if (!c->active_vars)
				return -ENOMEM;
		}

		ret = _copy_to_active_vars(c->active_vars, bb->var_dag_nodes);
		if (ret < 0)
			return ret;
	}

	return  basic_block_inited_vars(bb, bb_list_head);
}

static int _bb_init_pointer_aliases( dn_status_t* ds_pointer,  dag_node_t* dn_alias,  _3ac_code_t* c,  basic_block_t* bb,  list_t* bb_list_head)
{
	 dn_status_t*  ds;
	 dn_status_t*  ds2;
	 dn_status_t*  ds3;
	 vector_t*     aliases;

	int ret;
	int i;

	 ds_vector_clear_by_ds(c ->dn_status_initeds, ds_pointer);
	 ds_vector_clear_by_ds(bb->dn_status_initeds, ds_pointer);

	aliases =  vector_alloc();
	if (!aliases)
		return -ENOMEM;

	ret =  pointer_alias(aliases, dn_alias, c, bb, bb_list_head);
	if (ret < 0) {
		 loge("\n");
		 vector_free(aliases);
		return ret;
	}

	ret = 0;

	for (i = 0; i < aliases->size; i++) {
		ds = aliases->data[i];

		ds2 =  dn_status_null();
		if (!ds2) {
			ret = -ENOMEM;
			break;
		}
		ds2->inited = 1;

		ret =  ds_copy_dn(ds2, ds_pointer);
		if (ret < 0) {
			 dn_status_free(ds2);
			break;
		}

		ret =  ds_copy_alias(ds2, ds);
		if (ret < 0) {
			 dn_status_free(ds2);
			break;
		}

		ret =  vector_add(c->dn_status_initeds, ds2);
		if (ret < 0) {
			 dn_status_free(ds2);
			break;
		}

		ds3 =  dn_status_ref(ds2);

		ret =  vector_add(bb->dn_status_initeds, ds3);
		if (ret < 0) {
			 dn_status_free(ds3);
			break;
		}
	}

	 vector_clear(aliases, NULL);
	 vector_free (aliases);
	return ret;
}

static int _bb_init_var( dag_node_t* dn,  _3ac_code_t* c,  basic_block_t* bb,  list_t* bb_list_head)
{
	 dn_status_t*   ds;
	 dn_status_t*   ds2;
	 _3ac_operand_t* src;

	if (!c->dn_status_initeds) {
		c ->dn_status_initeds =  vector_alloc();

		if (!c->dn_status_initeds)
			return -ENOMEM;
	}

	if (dn->var->nb_pointers > 0) {
		int ret;

		ds =  dn_status_alloc(dn);
		if (!ds)
			return -ENOMEM;

		assert(c->srcs && 1 == c->srcs->size);
		src = c->srcs->data[0];

		ret = _bb_init_pointer_aliases(ds, src->dag_node, c, bb, bb_list_head);
		 dn_status_free(ds);
		return ret;
	}

	 DN_STATUS_GET(ds, c->dn_status_initeds, dn);
	ds->inited = 1;

	 DN_STATUS_GET(ds2, bb->dn_status_initeds, dn);
	ds2->inited = 1;
	return 0;
}

static int _bb_init_array_index( _3ac_code_t* c,  basic_block_t* bb,  list_t* bb_list_head)
{
	 _3ac_operand_t* base;
	 _3ac_operand_t* index;
	 _3ac_operand_t* src;

	 dag_node_t*    dn_base;
	 dag_node_t*    dn_index;
	 dag_node_t*    dn_src;

	 dn_status_t*   ds;
	 dn_status_t*   ds2;

	if (!c->dn_status_initeds) {
		c ->dn_status_initeds =  vector_alloc();

		if (!c->dn_status_initeds)
			return -ENOMEM;
	}

	ds =  dn_status_null(); 
	if (!ds)
		return -ENOMEM;

	ds->dn_indexes =  vector_alloc();
	if (!ds->dn_indexes) {
		 dn_status_free(ds);
		return -ENOMEM;
	}

	int type;
	switch (c->op->type) {
		case  OP_3AC_ASSIGN_ARRAY_INDEX:
			assert(4 == c->srcs->size);
			src      =  c->srcs->data[3];
			dn_src   =  src->dag_node;
			type     =   OP_ARRAY_INDEX;
			break;

		case  OP_3AC_ASSIGN_POINTER:
			assert(3 == c->srcs->size);
			src      =  c->srcs->data[2];
			dn_src   =  src->dag_node;
			type     =   OP_POINTER;
			break;
		default:
			 loge("\n");
			return -1;
			break;
	};

	base     = c->srcs->data[0];
	index    = c->srcs->data[1];

	dn_base  = base ->dag_node;
	dn_index = index->dag_node;

	int ret =  dn_status_index(ds, dn_index, type);
	if (ret < 0) {
		 loge("\n");
		return ret;
	}

	while ( OP_ARRAY_INDEX == dn_base->type
			||  OP_POINTER == dn_base->type) {

		dn_index = dn_base->childs->data[1];

		ret =  dn_status_index(ds, dn_index, dn_base->type);
		if (ret < 0) {
			 loge("\n");
			return ret;
		}

		dn_base  = dn_base->childs->data[0];
	}

	assert( type_is_var(dn_base->type));

	ds->dag_node = dn_base;
	ds->inited   = 1;

	if ( ds_is_pointer(ds) > 0)
		ret = _bb_init_pointer_aliases(ds, dn_src, c, bb, bb_list_head);
	else
		ret = 0;

	 dn_status_free(ds);
	return ret;
}

int  basic_block_inited_vars( basic_block_t* bb,  list_t* bb_list_head)
{
	 _3ac_operand_t* src;
	 _3ac_operand_t* dst;
	 dn_status_t*   ds;
	 dag_node_t*    dn;
	 dn_index_t*    di;
	 dn_index_t*    di2;
	 _3ac_code_t*    c;
	 list_t*        l;

	int ret = 0;

	for (l =  list_head(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_next(l)) {

		c  =  list_data(l,  _3ac_code_t, list);

		if (!c->active_vars)
			continue;

		if (c->dsts &&  OP_ASSIGN == c->op->type) {

			dst = c->dsts->data[0];
			dn  = dst->dag_node;

			if (( type_is_var(dn->type)
						||  OP_INC == dn->type ||  OP_INC_POST == dn->type
						||  OP_DEC == dn->type ||  OP_DEC_POST == dn->type)
					&& (dn->var->global_flag || dn->var->local_flag || dn->var->tmp_flag)) {

				 variable_t* v = dn->var;
				 logd("init: v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

				ret = _bb_init_var(dn, c, bb, bb_list_head);
				if (ret < 0) {
					 loge("\n");
					return ret;
				}
			}
		} else if ( OP_3AC_ASSIGN_ARRAY_INDEX == c->op->type
				||  OP_3AC_ASSIGN_POINTER     == c->op->type) {

			ret = _bb_init_array_index(c, bb, bb_list_head);
			if (ret < 0) {
				 loge("\n");
				 _3ac_code_print(c, NULL);
				return ret;
			}

		} else if ( OP_3AC_INC == c->op->type
				||  OP_3AC_DEC == c->op->type) {

			src = c->srcs->data[0];
			dn  = src->dag_node;

			if ( type_is_var(dn->type)
					&& (dn->var->global_flag || dn->var->local_flag || dn->var->tmp_flag)
					&&  dn->var->nb_pointers > 0) {

				 variable_t* v = dn->var;
				 logd("++/-- v_%d_%d/%s\n", v->w->line, v->w->pos, v->w->text->data);

				ret = _bb_init_var(dn, c, bb, bb_list_head);
				if (ret < 0) {
					 loge("\n");
					return ret;
				}

				 DN_STATUS_GET(ds, c->dn_status_initeds, dn);
//				 dn_status_print(ds);

				if (!ds->alias_indexes) {
					if (!v->arg_flag && !v->global_flag)
						 logw("++/-- update pointers NOT to an array, v->arg_flag: %d, file: %s, line: %d\n", v->arg_flag, v->w->file->data, v->w->line);
					continue;
				}

				assert(ds->alias_indexes->size > 0);
				di =   ds->alias_indexes->data[ds->alias_indexes->size - 1];

				if (di->refs > 1) {
					di2 =  dn_index_clone(di);
					if (!di2)
						return -ENOMEM;

					ds->alias_indexes->data[ds->alias_indexes->size - 1] = di2;
					 dn_index_free(di);
					di = di2;
				}

				if (di->index >= 0) {
					if ( OP_3AC_INC == c->op->type)
						di->index++;

					else if ( OP_3AC_DEC == c->op->type)
						di->index--;
				}
			}
		}
	}

	return 0;
}

int  basic_block_inited_by_3ac( basic_block_t* bb)
{
	 dn_status_t*   ds;
	 dn_status_t*   ds2;
	 _3ac_code_t*    c;
	 list_t*        l;

	int i;

	if (bb->dn_status_initeds)
		 vector_clear(bb->dn_status_initeds, ( void (*)(void*) ) dn_status_free);
	else {
		bb->dn_status_initeds =  vector_alloc();
		if (!bb->dn_status_initeds)
			return -ENOMEM;
	}

	for (l =  list_head(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_next(l)) {

		c  =  list_data(l,  _3ac_code_t, list);

		if (!c->dn_status_initeds)
			continue;

		for (i = 0; i < c->dn_status_initeds->size; i++) {
			ds =        c->dn_status_initeds->data[i];

			ds2 =  vector_find_cmp(bb->dn_status_initeds, ds,  ds_cmp_same_indexes);
			if (ds2)
				 vector_del(bb->dn_status_initeds, ds2);

			ds2 =  dn_status_clone(ds);
			if (!ds2)
				return -ENOMEM;

			if ( vector_add(bb->dn_status_initeds, ds2) < 0) {
				 dn_status_free(ds2);
				return -ENOMEM;
			}
		}
	}

	return 0;
}

int  basic_block_active_vars( basic_block_t* bb)
{
	 list_t*       l;
	 _3ac_code_t*   c;
	 dag_node_t*   dn;

	int i;
	int j;
	int ret;

	ret = _bb_vars(bb);
	if (ret < 0)
		return ret;

	for (i = 0; i < bb->var_dag_nodes->size; i++) {
		dn =        bb->var_dag_nodes->data[i];

		if ( dn_through_bb(dn))
			dn->active  = 1;
		else
			dn->active  = 0;

		dn->updated = 0;
	}

	for (l =  list_tail(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head); l =  list_prev(l)) {

		c  =  list_data(l,  _3ac_code_t, list);

		if ( type_is_jmp(c->op->type) ||  OP_3AC_END == c->op->type)
			continue;

		 _3ac_operand_t* src;
		 _3ac_operand_t* dst;

		if (c->dsts) {
			for (j  = 0; j < c->dsts->size; j++) {
				dst =        c->dsts->data[j];

				if ( type_is_binary_assign(c->op->type))
					dst->dag_node->active = 1;
				else
					dst->dag_node->active = 0;
#if 1
				if ( OP_3AC_LOAD != c->op->type
						&&  OP_3AC_RELOAD != c->op->type)
#endif
					dst->dag_node->updated = 1;
			}
		}

		if (c->srcs) {
			for (j  = 0; j < c->srcs->size; j++) {
				src =        c->srcs->data[j];

				assert(src->dag_node);

				if ( OP_ADDRESS_OF == c->op->type
						&&  type_is_var(src->dag_node->type))
					src->dag_node->active = 0;
				else
					src->dag_node->active = 1;

				if ( OP_INC == c->op->type
						||  OP_INC_POST == c->op->type
						||  OP_3AC_INC  == c->op->type
						||  OP_DEC      == c->op->type
						||  OP_DEC_POST == c->op->type
						||  OP_3AC_DEC  == c->op->type)
					src->dag_node->updated = 1;
			}
		}

		if (c->active_vars)
			 vector_clear(c->active_vars, (void (*)(void*)) dn_status_free);
		else {
			c->active_vars =  vector_alloc();
			if (!c->active_vars)
				return -ENOMEM;
		}

		ret = _copy_to_active_vars(c->active_vars, bb->var_dag_nodes);
		if (ret < 0)
			return ret;
	}

#if 0
	 loge("bb: %d\n", bb->index);
	for (l =  list_head(&bb->code_list_head); l !=  list_sentinel(&bb->code_list_head);
			l =  list_next(l)) {

		c =  list_data(l,  _3ac_code_t, list);

		if ( type_is_jmp(c->op->type))
			continue;
		 loge("\n");
		 _3ac_code_print(c, NULL);

		for (j = 0; j < c->active_vars->size; j++) {

			 dn_status_t* ds = c->active_vars->data[j];
			 dag_node_t*  dn = ds->dag_node;
			 variable_t*  v  = dn->var;

			if (v->w)
				 logw("v_%d_%d/%s: active: %d\n", v->w->line, v->w->pos, v->w->text->data, ds->active);
			else
				 logw("v_%#lx, active: %d\n", 0xffff & (uintptr_t)v, ds->active);
		}
		 loge("\n\n");
	}
#endif

	 vector_clear(bb->entry_dn_actives, NULL);
	 vector_clear(bb->dn_updateds,      NULL);

	if (! list_empty(&bb->code_list_head)) {

		l =  list_head(&bb->code_list_head);
		c =  list_data(l,  _3ac_code_t, list);

		ret = _copy_vars_by_active(bb->entry_dn_actives, c->active_vars, 1);
		if (ret < 0)
			return ret;

		ret = _copy_vars_by_active(bb->entry_dn_inactives, c->active_vars, 0);
		if (ret < 0)
			return ret;

		ret = _copy_updated_vars(bb->dn_updateds, c->active_vars);
		if (ret < 0)
			return ret;

		for (i = 0; i < bb->dn_updateds->size; i++) {
			dn =        bb->dn_updateds->data[i];

			if (!dn->var->global_flag)
				continue;

			ret =  vector_add_unique(bb->exit_dn_actives, dn);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int  basic_block_split( basic_block_t* bb_parent,  basic_block_t** pbb_child)
{
	 list_t*        l;
	 basic_block_t* bb_child;
	 basic_block_t* bb_next;

	int ret;
	int i;
	int j;

	bb_child =  basic_block_alloc();
	if (!bb_child)
		return -ENOMEM;

	 XCHG(bb_child->nexts, bb_parent->nexts);

	ret =  vector_add(bb_parent->nexts, bb_child);
	if (ret < 0) {
		 basic_block_free(bb_child);
		return ret;
	}

	ret =  vector_add(bb_child->prevs, bb_parent);
	if (ret < 0) {
		 basic_block_free(bb_child);
		return ret;
	}

	if (bb_parent->var_dag_nodes) {
		bb_child->var_dag_nodes =  vector_clone(bb_parent->var_dag_nodes);

		if (!bb_child->var_dag_nodes) {
			 basic_block_free(bb_child);
			return -ENOMEM;
		}
	}

	for (i = 0; i < bb_child->nexts->size; i++) {
		bb_next =   bb_child->nexts->data[i];

		for (j = 0; j < bb_next->prevs->size; j++) {

			if (bb_next->prevs->data[j] == bb_parent)
				bb_next->prevs->data[j] =  bb_child;
		}
	}

	*pbb_child = bb_child;
	return 0;
}

int  basic_block_search_bfs( basic_block_t* root,  basic_block_bfs_pt find, void* data)
{
	if (!root)
		return -EINVAL;

	 basic_block_t* bb;
	 vector_t*      queue;
	 vector_t*      checked;

	queue =  vector_alloc();
	if (!queue)
		return -ENOMEM;

	checked =  vector_alloc();
	if (!queue) {
		 vector_free(queue);
		return -ENOMEM;
	}

	int ret =  vector_add(queue, root);
	if (ret < 0)
		goto failed;

	int count = 0;
	int i     = 0;

	while (i < queue->size) {
		bb   = queue->data[i];

		int j;
		for (j = 0; j < checked->size; j++) {
			if (bb  ==  checked->data[j])
				goto next;
		}

		ret =  vector_add(checked, bb);
		if (ret < 0)
			goto failed;

		ret = find(bb, data, queue);
		if (ret < 0)
			goto failed;
		count += ret;
next:
		i++;
	}

	ret = count;
failed:
	 vector_free(queue);
	 vector_free(checked);
	return ret;
}

int  basic_block_search_dfs_prev( basic_block_t* root,  basic_block_dfs_pt find, void* data,  vector_t* results)
{
	 basic_block_t* bb;

	int i;
	int j;
	int ret;

	assert(!root->jmp_flag);

	root->visit_flag = 1;

	for (i = 0; i < root->prevs->size; ++i) {
		bb =        root->prevs->data[i];

		if (bb->visit_flag)
			continue;

		ret = find(bb, data);
		if (ret < 0)
			return ret;

		if (ret > 0) {
			ret =  vector_add(results, bb);
			if (ret < 0)
				return ret;

			bb->visit_flag = 1;
			continue;
		}

		ret =  basic_block_search_dfs_prev(bb, find, data, results);
		if ( ret < 0)
			return ret;
	}

	return 0;
}

int  basic_block_loads_saves( basic_block_t* bb,  list_t* bb_list_head)
{
	 dag_node_t* dn;
	 list_t*     l = &bb->list;

	int ret;
	int i;

	for (i = 0; i < bb->entry_dn_actives->size; i++) {
		dn =        bb->entry_dn_actives->data[i];

		if (dn->var->extra_flag)
			continue;

		if ( vector_find(bb->entry_dn_aliases, dn)
				|| dn->var->tmp_flag)
			ret =  vector_add_unique(bb->dn_reloads, dn);
		else
			ret =  vector_add_unique(bb->dn_loads, dn);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < bb->exit_dn_actives->size; i++) {
		dn =        bb->exit_dn_actives->data[i];

		if (! vector_find(bb->dn_updateds, dn)) {

			if (l !=  list_head(bb_list_head) || !dn->var->arg_flag)
				continue;
		}

		if ( vector_find(bb->exit_dn_aliases, dn)
				|| dn->var->tmp_flag)
			ret =  vector_add_unique(bb->dn_resaves, dn);
		else
			ret =  vector_add_unique(bb->dn_saves, dn);

		if (ret < 0)
			return ret;
	}

	for (i = 0; i < bb->exit_dn_aliases->size; i++) {
		dn =        bb->exit_dn_aliases->data[i];

		if (! vector_find(bb->dn_updateds, dn)) {

			if (l !=  list_head(bb_list_head) || !dn->var->arg_flag)
				continue;
		}

		ret =  vector_add_unique(bb->dn_resaves, dn);
		if (ret < 0)
			return ret;
	}

	return 0;
}

void  basic_block_mov_code( basic_block_t* to,  list_t* start,  basic_block_t* from)
{
	 list_t*     l;
	 _3ac_code_t* c;

	for (l = start; l !=  list_sentinel(&from->code_list_head); ) {

		c  =  list_data(l,  _3ac_code_t, list);
		l  =  list_next(l);

		 list_del(&c->list);
		 list_add_tail(&to->code_list_head, &c->list);

		c->basic_block = to;
	}
}

void  basic_block_add_code( basic_block_t* bb,  list_t* h)
{
	 _3ac_code_t* c;
	 list_t*     l;

	for (l =  list_head(h); l !=  list_sentinel(h); ) {

		c  =  list_data(l,  _3ac_code_t, list);
		l  =  list_next(l);

		 list_del(&c->list);
		 list_add_tail(&bb->code_list_head, &c->list);

		c->basic_block = bb;
	}
}

 bb_group_t*  basic_block_find_min_loop( basic_block_t* bb,  vector_t* loops)
{
	 bb_group_t* loop;
	int i;

	for (i = 0; i < loops->size; i++) {
		loop      = loops->data[i];

		if ( vector_find(loop->body, bb)) {

			if (!loop->loop_childs || loop->loop_childs->size <= 0)
				return loop;

			return  basic_block_find_min_loop(bb, loop->loop_childs);
		}
	}
	return NULL;
}
