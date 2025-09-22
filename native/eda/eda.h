#ifndef EDA_H
#define EDA_H

#include"native.h"
#include"eda_pack.h"

typedef struct {

	function_t*     f;

} eda_context_t;

typedef int	(*eda_inst_handler_pt)(native_t* ctx, _3ac_code_t* c);

eda_inst_handler_pt  eda_find_inst_handler(const int op_type);

int eda_open  (native_t* ctx, const char* arch);
int eda_close (native_t* ctx);
int eda_select(native_t* ctx);

int __eda_bit_nand(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_nor (function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_not (function_t* f, ScfEpin** in,  ScfEpin** out);
int __eda_bit_and (function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_or  (function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);

#define EDA_PIN_ADD_CONN(_ef, _dst, _p) \
	do { \
		if (_dst) \
			EDA_PIN_ADD_PIN_EF(_ef, _dst, _p); \
		else \
			_dst = _p; \
	} while (0)

static inline int eda_variable_size(variable_t* v)
{
	if (v->nb_dimentions + v->nb_pointers > 0)
		return 64;

	if (v->type >= STRUCT)
		return 64;

	if (VAR_BIT == v->type || VAR_I1 == v->type)
		return 1;
	if (VAR_U2 == v->type || VAR_I2 == v->type)
		return 2;
	if (VAR_U3 == v->type || VAR_I3 == v->type)
		return 3;
	if (VAR_U4 == v->type || VAR_I4 == v->type)
		return 4;

	return v->size << 3;
}

static inline int eda_find_argv_index(function_t* f, variable_t* v)
{
	int i;
	if (f->argv) {
		for (i = 0; i < f->argv->size; i++) {
			if (v == f->argv->data[i])
				return i;
		}
	}

	return -1;
}

#endif
