#include "optimizer.h"
#include "pointer_alias.h"

#define BB_SPLIT(bb0, bb1) \
	do { \
		bb1 = NULL; \
		int ret = basic_block_split(bb0, &bb1); \
		if (ret < 0) \
		    return ret; \
		bb1->dereference_flag = bb0->dereference_flag; \
		bb1->ret_flag         = bb0->ret_flag; \
		bb0->ret_flag         = 0; \
		list_add_front(&bb0->list, &bb1->list); \
	} while (0)

static int _optimize_split_call_bb(basic_block_t* bb,list_t* bb_list_head){
    basic_block_t* cur_bb = bb;
    basic_block_t* bb2;
    _3ac_code_t* c;
    list_t* l;

    int split_flag = 0;
    
    
	for (l = list_head(&bb->code_list_head); l != list_sentinel(&bb->code_list_head); ) {

		c  = list_data(l,_3ac_code_t, list);
		l  = list_next(l);

		if (split_flag) {
			split_flag = 0;

			BB_SPLIT(cur_bb, bb2);
			cur_bb = bb2;
		}

		if (cur_bb != bb) {
			list_del(&c->list);
			list_add_tail(&cur_bb->code_list_head, &c->list);

			c->basic_block = cur_bb;
		}

		if (OP_CALL == c->op->type) {
			split_flag = 1;

			if (list_prev(&c->list) != list_sentinel(&cur_bb->code_list_head)) {
		        BB_SPLIT(cur_bb, bb2);

				cur_bb->call_flag = 0;

				list_del(&c->list);
				list_add_tail(&bb2->code_list_head, &c->list);

				c->basic_block = bb2;

				cur_bb = bb2;
			}

			cur_bb->call_flag = 1;
		}
	}

	return 0;
    

}








