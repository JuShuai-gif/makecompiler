#ifndef RISC_REG_H
#define RISC_REG_H

#include"native.h"
#include"risc_util.h"

#define RISC_COLOR(type, id, mask)   ((type) << 24 | (id) << 16 | (mask))
#define RISC_COLOR_TYPE(c)           ((c) >> 24)
#define RISC_COLOR_ID(c)             (((c) >> 16) & 0xff)
#define RISC_COLOR_MASK(c)           ((c) & 0xffff)
//#define RISC_COLOR_CONFLICT(c0, c1)  ( (c0) >> 16 == (c1) >> 16 && (c0) & (c1) & 0xffff )

#define RISC_COLOR_BYTES(c) \
	({ \
	     int n = 0;\
	     intptr_t minor = (c) & 0xffff; \
	     while (minor) { \
	         minor &= minor - 1; \
	         n++;\
	     } \
	     n;\
	 })

#define RISC_SELECT_REG_CHECK(pr, dn, c, f, load_flag) \
	do {\
		int ret = risc_select_reg(pr, dn, c, f, load_flag); \
		if (ret < 0) { \
			loge("\n"); \
			return ret; \
		} \
		assert(dn->color > 0); \
	} while (0)

#define RISC_ABI_CALLER_SAVES_MAX 32
#define RISC_ABI_RET_MAX          8

int                 risc_save_var (dag_node_t* dn, _3ac_code_t* c, function_t* f);

int                 risc_save_var2(dag_node_t* dn, register_t* r, _3ac_code_t* c, function_t* f);

int                 risc_save_reg (register_t* r,  _3ac_code_t* c, function_t* f);

int                 risc_load_const(register_t* r, dag_node_t* dn, _3ac_code_t* c, function_t* f);
int                 risc_load_reg  (register_t* r, dag_node_t* dn, _3ac_code_t* c, function_t* f);

int                 risc_select_reg(register_t** preg, dag_node_t* dn, _3ac_code_t* c, function_t* f, int load_flag);

int                 risc_select_free_reg(register_t** preg, _3ac_code_t* c, function_t* f, int is_float);

int                 risc_dereference_reg(sib_t* sib, dag_node_t* base, dag_node_t* member, _3ac_code_t* c, function_t* f);

int                 risc_pointer_reg(sib_t* sib, dag_node_t* base, dag_node_t* member, _3ac_code_t* c, function_t* f);

int                 risc_array_index_reg(sib_t* sib, dag_node_t* base, dag_node_t* index, dag_node_t* scale, _3ac_code_t* c, function_t* f);

void                risc_call_rabi(int* p_nints, int* p_nfloats, _3ac_code_t* c, function_t* f);

#endif

