#ifndef SCF_EDA_H
#define SCF_EDA_H

#include"scf_native.h"
#include"scf_eda_pack.h"

typedef struct {

	scf_function_t*     f;

} scf_eda_context_t;

typedef int	(*eda_inst_handler_pt)(scf_native_t* ctx, scf_3ac_code_t* c);

eda_inst_handler_pt  scf_eda_find_inst_handler(const int op_type);

int scf_eda_open  (scf_native_t* ctx, const char* arch);
int scf_eda_close (scf_native_t* ctx);
int scf_eda_select(scf_native_t* ctx);

int __eda_bit_nand(scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_nor (scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_not (scf_function_t* f, ScfEpin** in,  ScfEpin** out);
int __eda_bit_and (scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);
int __eda_bit_or  (scf_function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out);

#define EDA_PIN_ADD_CONN(_ef, _dst, _p) \
	do { \
		if (_dst) \
			EDA_PIN_ADD_PIN_EF(_ef, _dst, _p); \
		else \
			_dst = _p; \
	} while (0)

static inline int eda_variable_size(scf_variable_t* v)
{
	if (v->nb_dimentions + v->nb_pointers > 0)
		return 64;

	if (v->type >= SCF_STRUCT)
		return 64;

	if (SCF_VAR_BIT == v->type || SCF_VAR_I1 == v->type)
		return 1;
	if (SCF_VAR_U2 == v->type || SCF_VAR_I2 == v->type)
		return 2;
	if (SCF_VAR_U3 == v->type || SCF_VAR_I3 == v->type)
		return 3;
	if (SCF_VAR_U4 == v->type || SCF_VAR_I4 == v->type)
		return 4;

	return v->size << 3;
}

static inline int eda_find_argv_index(scf_function_t* f, scf_variable_t* v)
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
