#include "variable.h"
#include "type.h"
#include "function.h"

// member内存分配
member_t *member_alloc(variable_t *base) {
    // 
	member_t *m = calloc(1, sizeof(member_t));

    if (!m)
        return NULL;

    m->base = base;
    return m;
}

// 成员释放
void member_free(member_t *m) {
    if (m) {
		// 先释放 indexes
        if (m->indexes) {
			// 先清除
            vector_clear(m->indexes, (void (*)(void *))free);
            // 再释放
			vector_free(m->indexes);
        }
		// 最后释放本体
        free(m);
    }
}

// 计算成员偏移量
int member_offset(member_t *m) {
	// 如果没有索引（比如只是变量本身，不取数组元素），偏移为 0
    if (!m->indexes)
        return 0;

	// 基础变量（比如 arr、a 等）
    variable_t *base = m->base;

	// 当前处理的索引操作
    index_t *idx;

	// 当前正在处理的维度
    int dim = 0;
	// 最终偏移量（字节数）
    int offset = 0;
    int i;
    int j;

	// 遍历所有索引操作
    for (i = 0; i < m->indexes->size; i++) {
		// 当前索引
        idx = m->indexes->data[i];

		// 如果 idx->member 不为空，说明这是结构体成员访问
        if (idx->member) {
			 // 加上结构体成员的偏移量
            offset += idx->member->offset;
			// 更新 base，进入这个成员作为新的基准
            base = idx->member;
            dim = 0;
            continue;
        }

		// 如果是数组下标访问，要求 index >= 0
        assert(idx->index >= 0);
		// 进入下一维度
        dim++;
		// 计算当前维度之后所有维度的容量
        int capacity = 1;
        for (j = dim; j < base->nb_dimentions; j++) {
            capacity *= base->dimentions[j].num;
        }

		// 乘以单个元素的大小，得到这一维度单位步长（字节数）
        capacity *= base->size;
		// 累加偏移量：下标 × 步长
        offset += capacity * idx->index;
    }

	// 返回最终计算的偏移量
    return offset;
}

// 成员增加索引
int member_add_index(member_t *m, variable_t *member, int index) {
	// 如果
    if (!m)
        return -1;

    if (!m->indexes) {
        m->indexes = vector_alloc();
        if (!m->indexes)
            return -ENOMEM;
    }

    index_t *idx = calloc(1, sizeof(index_t));
    if (!idx)
        return -ENOMEM;

    if (member)
        idx->member = member;
    else
        idx->index = index;

    if (vector_add(m->indexes, idx) < 0) {
        free(idx);
        return -ENOMEM;
    }

    return 0;
}

// 申请一个变量类型，所需词元、类型
variable_t*	variable_alloc(lex_word_t* w, type_t* t)
{
	// 先申请一个变量空间
	variable_t* v = calloc(1, sizeof(variable_t));
	if (!v)
		return NULL;

	v->refs = 1;
	v->type = t->type;

	v->const_flag  = t->node.const_flag;
	v->nb_pointers = t->nb_pointers;
	v->func_ptr    = t->func_ptr;

	// 指针层数
	if (v->nb_pointers > 0)
		v->size = sizeof(void*);
	else
		v->size = t->size;

	if (v->nb_pointers > 1)
		v->data_size = sizeof(void*);
	else
		v->data_size = t->size;

	v->offset = t->offset;

	if (w) {
		// 克隆一个词元
		v->w = lex_word_clone(w);
		if (!v->w) {
			free(v);
			return NULL;
		}

		// 
		if (lex_is_const(w)) {
			v->const_flag         = 1;
			v->const_literal_flag = 1;

			switch (w->type) {
				case LEX_WORD_CONST_CHAR:
					v->data.u32 = w->data.u32;
					break;

				case LEX_WORD_CONST_STRING:
					v->data.s = string_clone(w->data.s);
					if (!v->data.s) {
						lex_word_free(v->w);
						free(v);
						return NULL;
					}
					break;

				case LEX_WORD_CONST_INT:
					v->data.i = w->data.i;
					break;
				case LEX_WORD_CONST_U32:
					v->data.u32 = w->data.u32;
					break;
				case LEX_WORD_CONST_FLOAT:
					v->data.f = w->data.f;
					break;
				case LEX_WORD_CONST_DOUBLE:
					v->data.d = w->data.d;
					break;
				case LEX_WORD_CONST_COMPLEX:
					v->data.z = w->data.z;
					break;

				case LEX_WORD_CONST_I64:
					v->data.i64 = w->data.i64;
					break;
				case LEX_WORD_CONST_U64:
					v->data.u64 = w->data.u64;
					break;
				default:
					break;
			};
		}
	}

	return v;
}

// 变量克隆
variable_t*	variable_clone(variable_t* v)
{
	// 先申请一个空间
	variable_t* v2 = calloc(1, sizeof(variable_t));
	if (!v2)
		return NULL;

	v2->refs = 1;

	if (v->w) {
		v2->w = lex_word_clone(v->w);
		if (!v2->w) {
			variable_free(v2);
			return NULL;
		}
	}

	v2->nb_pointers = v->nb_pointers;
	v2->func_ptr    = v->func_ptr;

	if (v->nb_dimentions > 0) {
		v2->dimentions = calloc(v->nb_dimentions, sizeof(int));
		if (!v2->dimentions) {
			variable_free(v2);
			return NULL;
		}

		memcpy(v2->dimentions, v->dimentions, sizeof(int) * v->nb_dimentions);

		v2->nb_dimentions = v->nb_dimentions;
		v2->dim_index     = v->dim_index;
		v2->capacity      = v->capacity;
	}

	v2->size      = v->size;
	v2->data_size = v->data_size;
	v2->offset    = v->offset;

	v2->bit_offset = v->bit_offset;
	v2->bit_size   = v->bit_size;

	if (variable_is_struct(v) || variable_is_array(v)) {

		int size = variable_size(v);

		if (v->data.p) {
			v2->data.p = malloc(size);
			if (!v2->data.p) {
				variable_free(v2);
				return NULL;
			}

			memcpy(v2->data.p, v->data.p, size);
		}

	} else if (variable_const_string(v)) {

		v2->data.s = string_clone(v->data.s);
		if (!v2->data.s) {
			variable_free(v2);
			return NULL;
		}
	} else
		memcpy(&v2->data, &v->data, sizeof(v->data));

	v2->type = v->type;

	v2->const_literal_flag = v->const_literal_flag;
	v2->const_flag         = v->const_flag;
	v2->static_flag        = v->static_flag;
	v2->tmp_flag           = v->tmp_flag;
	v2->local_flag         = v->local_flag;
	v2->global_flag        = v->global_flag;
	v2->member_flag        = v->member_flag;
	v2->arg_flag           = v->arg_flag;
	v2->auto_gc_flag       = v->auto_gc_flag;

	return v2;
}

// 增加一个引用
variable_t*	variable_ref(variable_t* v)
{
	v->refs++;
	return v;
}

// 变量释放
void variable_free(variable_t* v)
{
	if (v) {
		if (--v->refs > 0)
			return;

		assert(0 == v->refs);

		if (v->signature) {
			string_free(v->signature);
			v->signature = NULL;
		}

		if (v->dimentions) {
			free(v->dimentions);
			v->dimentions = NULL;
		}

		if (variable_is_struct(v) || variable_is_array(v)) {
			if (v->data.p) {
				free(v->data.p);
				v->data.p = NULL;
			}

		} else if (variable_const_string(v)) {
			if (v->data.s) {
				string_free(v->data.s);
				v->data.s = NULL;
			}
		}

		if (v->w) {
			lex_word_free(v->w);
			v->w = NULL;
		}

		free(v);
		v = NULL;
	}
}

void variable_add_array_dimention(variable_t* array, int num, expr_t* vla)
{
	assert(array);

	void* p = realloc(array->dimentions, sizeof(dimention_t) * (array->nb_dimentions + 1));
	assert(p);

	array->dimentions = p;
	array->dimentions[array->nb_dimentions].num = num;
	array->dimentions[array->nb_dimentions].vla = vla;
	array->nb_dimentions++;
}

void variable_get_array_member(variable_t* array, int index, variable_t* member)
{
	assert(array);
	assert(member);
	assert(array->type == member->type);
	assert(array->size == member->size);
	assert(index >= 0 && index < array->capacity);

	memcpy(&(member->data.i), array->data.p + index * member->size, member->size);
}

void variable_set_array_member(variable_t* array, int index, variable_t* member)
{
	assert(array);
	assert(member);
	assert(array->type == member->type);
	assert(array->size == member->size);
	assert(index >= 0 && index < array->capacity);

	memcpy(array->data.p + index * member->size, &(member->data.i), member->size);
}

// 变量信息打印
void variable_print(variable_t* v)
{
	assert(v);

	if (v->nb_pointers > 0) {
		printf("print var: name: %s, type: %d, value: %p\n", v->w->text->data, v->type, v->data.p);
		return;
	}

	if (VAR_CHAR == v->type || VAR_INT == v->type)
		printf("print var: name: %s, type: %d, value: %d\n", v->w->text->data, v->type, v->data.i);

	else if (VAR_DOUBLE == v->type)
		printf("print var: name: %s, type: %d, value: %lg\n", v->w->text->data, v->type, v->data.d);
	else
		printf("print var: name: %s, type: %d\n", v->w->text->data, v->type);
}

// 变量类型是否相同
int variable_same_type(variable_t* v0, variable_t* v1)
{
	if (v0) {
		if (!v1)
			return 0;

		if (v0->type != v1->type)
			return 0;

		if (v0->nb_pointers != v1->nb_pointers)
			return 0;

		if (v0->nb_dimentions != v1->nb_dimentions)
			return 0;

		int i;
		for (i = 0; i < v0->nb_dimentions; i++) {
			if (v0->dimentions[i].num != v1->dimentions[i].num)
				return 0;
		}

		if (FUNCTION_PTR == v0->type) {
			assert(v0->func_ptr);
			assert(v1->func_ptr);

			if (!function_same_type(v0->func_ptr, v1->func_ptr))
				return 0;
		}
	} else {
		if (v1)
			return 0;
	}

	return 1;
}

// 变量类型是否相似
int variable_type_like(variable_t* v0, variable_t* v1)
{
	if (v0) {
		if (!v1)
			return 0;

		if (v0->type != v1->type)
			return 0;

		if (variable_nb_pointers(v0) != variable_nb_pointers(v1))
			return 0;

		if (FUNCTION_PTR == v0->type) {

			if (v0->func_ptr && v1->func_ptr) {

				if (!function_same_type(v0->func_ptr, v1->func_ptr))
					return 0;
			} else {
				logw("common function ptr!\n");
			}
		}
	} else {
		if (v1)
			return 0;
	}

	return 1;
}

// 有符号扩展
void variable_sign_extend(variable_t* v, int bytes)
{
	if (bytes <= v->size)
		return;

	bytes = bytes > 8 ? 8 : bytes;

	v->data.u64 = sign_extend(v->data.u64, v->size << 3);

	v->size = bytes;
}

// 0 扩展
void variable_zero_extend(variable_t* v, int bytes)
{
	if (bytes <= v->size)
		return;

	bytes = bytes > 8 ? 8 : bytes;

	v->data.u64 = zero_extend(v->data.u64, v->size << 3);

	v->size = bytes;
}

// 变量扩展
void variable_extend_bytes(variable_t* v, int bytes)
{
	if (type_is_signed(v->type))
		variable_sign_extend(v, bytes);
	else
		variable_zero_extend(v, bytes);
}

// 
void variable_extend_std(variable_t* v, variable_t* std)
{
	if (type_is_signed(v->type))
		variable_sign_extend(v, std->size);
	else
		variable_zero_extend(v, std->size);

	v->type = std->type;
}

// 变量大小
int variable_size(variable_t* v)
{
	if (0 == v->nb_dimentions)
		return v->size;

	assert(v->nb_dimentions > 0);

	if (v->vla_flag)
		return sizeof(void*);

	int capacity = 1;
	int j;

	for (j = 0; j < v->nb_dimentions; j++) {

		if (v->dimentions[j].num < 0) {
			loge("\n");
			return -EINVAL;
		}

		capacity *= v->dimentions[j].num;
	}
	v->capacity = capacity;

	return capacity * v->size;
}






