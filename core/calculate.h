#ifndef  CALCULATE_H
#define  CALCULATE_H

#include"ast.h"

typedef struct {
	const char* name;

	int			op_type;

	int			src0_type;
	int			src1_type;
	int			ret_type;

	int			(*func)( ast_t* ast,  variable_t** pret,  variable_t* src0,  variable_t* src1);

}  calculate_t;

 calculate_t*  find_base_calculate(int op_type, int src0_type, int src1_type);

#endif

