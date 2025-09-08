#ifndef VARIABLE_H
#define VARIABLE_H

#include "core_types.h"
#include "lex_word.h"

// 表示数组维度信息
typedef struct
{
    expr_t* vla;// 
    int num;
}dimention_t;

struct variable_s
{
    int refs;

    int type;
    lex_word_t* w;

    int nb_pointers;
    function_t* func_ptr;

    dimention_t* dimentions;
    int nb_dimentions;
    int dim_index;
    int capacity;

    int size;
    int data_size;

    int bit_offsets;
    int bit_size;

    int n_pins;

    Epin* r_pins[EDA_MAX_BITS];
    Epin* w_pins[EDA_MAX_BITS];
    Ecomponent* DFFs[EDA_MAX_BITS];

    int offset;
    int bp_offsets;
    int sp_offset;
    int ds_offset;
    register_t* rabi;

    union 
    {
        int32_t i;
        uint32_t u32;
        int64_t i64;
        uint64_t u64;
        float f;
        double d;
        complex_t z;
        string_t* s;
        void* p;
    }data;

    string_t* signature;

    uint32_t const_literal_flag:1;
    uint32_t const_flag:1;
    uint32_t static_flag:1;
    uint32_t extern_flag:1;
    uint32_t extra_flag:1;

    uint32_t tmp_flag:1;
    uint32_t local_flag:1;
    uint32_t global_flag:1;
    uint32_t member_flag:1;

    uint32_t vla_flag:1;
    uint32_t arg_flag:1;
    uint32_t auto_gc_flag:1;
};

struct index_s
{
    variable_t* member;
    int index;
};


struct member_s
{
    variable_t* base;
    vector_t* indexes;
};

member_t* member_alloc(variable_t* base);
void member_free(member_t* m);
int member_offset(member_t* m);

int member_add_index(member_t* m,variable_t* member,int index);

variable_t* variable_alloc(lex_word_t* w,type_t* t);
variable_t* variable_clone(variable_t* var);
variable_t* variable_ref(variable_t* var);
void variable_free(variable_t* var);

void variable_print(variable_t* var);

void variable_add_array_dimention(variable_t* array,int index,variable_t* member);

void variable_set_array_member(variable_t* array,int index,variable_t* member);
void variable_get_array_member(variable_t* array,int index,variable_t* member);

int variable_same_type(variable_t* v0,variable_t* v1);
int variable_type_like(variable_t* v0,variable_t* v1);

void variable_sign_extend(variable_t* v,int bytes);
void variable_zero_extend(variable_t* v,int bytes);

void variable_extend_std(variable_t* v,variable_t* std);

void variable_extend_bytes(variable_t* v,int bytes);

int variable_size(variable_t* v);

static inline int variable_const(variable_t* v){
    if (FUNCTION_PTR == v->type)
        return v->const_literal_flag;
    
    if (v->nb_pointers + v->nb_dimentions > 0)
        return v->const_literal_flag && !v->vla_flag;
    
    return v->const_flag && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

static inline int variable_const_integer(variable_t* v){
    return type_is_integer(v->type) && v->const_flag && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

static inline int variable_const_string(variable_t* v){
    return VAR_CHAR == v->type
        && v->const_flag
        && v->const_literal_flag
        && 1 == v->nb_pointers
        && 0 == v->nb_dimentions;
}

static inline int variable_string(variable_t* v){
    return VAR_CHAR == v->type && 1 == v->nb_pointers + v->nb_dimentions;
}

static inline int variable_float(variable_t* v){
    return type_is_float(v->type) && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}


static inline int variable_integer(variable_t* v)
{
	return type_is_integer(v->type) || v->nb_pointers > 0 || v->nb_dimentions > 0;
}

static inline int variable_signed(variable_t* v)
{
	return type_is_signed(v->type) && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

static inline int variable_unsigned(variable_t* v)
{
	return type_is_unsigned(v->type) || v->nb_pointers > 0 || v->nb_dimentions > 0;
}

static inline int variable_nb_pointers(variable_t* v)
{
	return v->nb_pointers + v->nb_dimentions;
}

static inline int variable_is_struct(variable_t* v)
{
	return v->type >= STRUCT && 0 == v->nb_pointers && 0 == v->nb_dimentions;
}

static inline int variable_is_struct_pointer(variable_t* v)
{
	return v->type >= STRUCT && 1 == v->nb_pointers && 0 == v->nb_dimentions;
}

static inline int variable_is_array(variable_t* v)
{
	return v->nb_dimentions > 0;
}

static inline int variable_may_malloced(variable_t* v)
{
	if (v->nb_dimentions > 0)
		return 0;

	if (FUNCTION_PTR == v->type) {
		if (v->nb_pointers > 1)
			return 1;
		return 0;
	}

	return v->nb_pointers > 0;
}

#endif













