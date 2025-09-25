#include"3ac.h"
#include"function.h"
#include"basic_block.h"
#include"utils_graph.h"

static mc_3ac_operator_t mc_3ac_operators[] = {
	{OP_CALL, 			"call"},// 函数调用

	{OP_ARRAY_INDEX, 	"array_index"},// 访问 a[i]
	{OP_POINTER,        "pointer"},// 指针操作 p->field

	{OP_TYPE_CAST, 	    "cast"},// 

	{OP_LOGIC_NOT, 		"logic_not"},
	{OP_BIT_NOT,        "not"},
	{OP_NEG, 			"neg"},
	{OP_POSITIVE, 		"positive"},

	{OP_INC,            "inc"},// 前置 自增 ++x
	{OP_DEC,            "dec"},// 前置 自减 --x

	{OP_INC_POST,       "inc_post"},// 后置自增 x++
	{OP_DEC_POST,       "dec_post"},// 后置自减 x--

	{OP_DEREFERENCE,	"dereference"},// 解引用 *p
	{OP_ADDRESS_OF, 	"address_of"},// 取地址(&x)

	{OP_MUL, 			"mul"},// 乘法 a * b
	{OP_DIV, 			"div"},// 除法 a / b
	{OP_MOD,            "mod"},// 取余 a % b

	{OP_ADD, 			"add"},// 加法
	{OP_SUB, 			"sub"},// 减法

	{OP_SHL,            "shl"},// 左移 a << b
	{OP_SHR,            "shr"},// 右移 a >> b

	{OP_BIT_AND,        "and"},// 按位与 (a & b)
	{OP_BIT_OR,         "or"},// 按位或 (a | b)

	{OP_EQ, 			"eq"},// 相等(==)
	{OP_NE,             "neq"},// 不等(!=)
	{OP_GT, 			"gt"},// 大于(>)
	{OP_LT, 			"lt"},// 小于(<)
	{OP_GE, 			"ge"},// 大于等于(>=)
	{OP_LE, 			"le"},// 小于等于(<=)

	{OP_ASSIGN,         "assign"},// x = y
	{OP_ADD_ASSIGN,     "+="},// x+=y
	{OP_SUB_ASSIGN,     "-="},// x-=y
	{OP_MUL_ASSIGN,     "*="},// x*=y
	{OP_DIV_ASSIGN,     "/="},// x/=y
	{OP_MOD_ASSIGN,     "%="},// x%=y
	{OP_SHL_ASSIGN,     "<<="},// x <<= y
	{OP_SHR_ASSIGN,     ">>="},// x >>= y
	{OP_AND_ASSIGN,     "&="},// x &= y
	{OP_OR_ASSIGN,      "|="},// x |= y

	{OP_VA_START,       "va_start"},// 宏 IR
	{OP_VA_ARG,         "va_arg"},
	{OP_VA_END,         "va_end"},

	{OP_VLA_ALLOC,      "vla_alloc"},// 分配变长数组
	{OP_VLA_FREE,       "vla_free"},// 释放变长数组

	{OP_RETURN,			 "return"},// 从函数返回
	{OP_GOTO,			 "jmp"},// 无条件跳转

	{OP_3AC_TEQ,         "teq"},// 测试相等(test eq)
	{OP_3AC_CMP,         "cmp"},// 比较(cmp)

	{OP_3AC_LEA,         "lea"},// 取地址(Load Effective Address, 类似汇编 lea)

	{OP_3AC_SETZ,        "setz"},// 等于则置位
	{OP_3AC_SETNZ,       "setnz"},// 不等则置位
	{OP_3AC_SETGT,       "setgt"},// 大于置位
	{OP_3AC_SETGE,       "setge"},// 
	{OP_3AC_SETLT,       "setlt"},
	{OP_3AC_SETLE,       "setle"},// 小于置位

	{OP_3AC_SETA,        "seta"},
	{OP_3AC_SETAE,       "setae"},
	{OP_3AC_SETB,        "setb"},
	{OP_3AC_SETBE,       "setbe"},

	{OP_3AC_JZ,          "jz"},  // 如果等于零跳转
	{OP_3AC_JNZ,         "jnz"},// 如果不等于零跳转
	{OP_3AC_JGT,         "jgt"},// 如果大于跳转
	{OP_3AC_JGE,         "jge"},// 如果大于等于跳转
	{OP_3AC_JLT,         "jlt"},// 如果小于跳转
	{OP_3AC_JLE,         "jle"},// 如果小于等于跳转

	{OP_3AC_JA,          "ja"},// 无符号大于跳转
	{OP_3AC_JAE,         "jae"},// 无符号大于等于跳转
	{OP_3AC_JB,          "jb"},// 无符号小于跳转
	{OP_3AC_JBE,         "jbe"},// 无符号小于等于跳转

	{OP_3AC_DUMP,        "core_dump"},// 打印 IR 栈或核心数据
	{OP_3AC_NOP,         "nop"},// 空操作 (No Operation)
	{OP_3AC_END,         "end"},// 程序结束

	{OP_3AC_PUSH,        "push"},
	{OP_3AC_POP,         "pop"},
	{OP_3AC_SAVE,        "save"},
	{OP_3AC_LOAD,        "load"},
	{OP_3AC_RELOAD,      "reload"},
	{OP_3AC_RESAVE,      "resave"},

	{OP_3AC_PUSH_RETS,   "push rets"},
	{OP_3AC_POP_RETS,    "pop  rets"},
	{OP_3AC_MEMSET,      "memset"},

	{OP_3AC_INC,         "inc3"},
	{OP_3AC_DEC,         "dec3"},// 

	{OP_3AC_ASSIGN_DEREFERENCE,	    "dereference="},// *p=x
	{OP_3AC_ASSIGN_ARRAY_INDEX,	    "array_index="},// a[i]=x
	{OP_3AC_ASSIGN_POINTER,	        "pointer="},// p->field = x

	{OP_3AC_ADDRESS_OF_ARRAY_INDEX, "&array_index"},// &a[i]
	{OP_3AC_ADDRESS_OF_POINTER,     "&pointer"},// &p->field
};

mc_3ac_operator_t*	mc_3ac_find_operator(const int type)
{
	int i;
	for (i = 0; i < sizeof(mc_3ac_operators) / sizeof(mc_3ac_operators[0]); i++) {
		if (type == mc_3ac_operators[i].type) {
			return &(mc_3ac_operators[i]);
		}
	}

	return NULL;
}

mc_3ac_operand_t* mc_3ac_operand_alloc()
{
	mc_3ac_operand_t* operand = calloc(1, sizeof(mc_3ac_operand_t));
	assert(operand);
	return operand;
}

void mc_3ac_operand_free(mc_3ac_operand_t* operand)
{
	if (operand) {
		free(operand);
		operand = NULL;
	}
}

mc_3ac_code_t* mc_3ac_code_alloc()
{
	mc_3ac_code_t* c = calloc(1, sizeof(mc_3ac_code_t));

	return c;
}

int mc_3ac_code_same(mc_3ac_code_t* c0, mc_3ac_code_t* c1)
{
	if (c0->op != c1->op)
		return 0;

	if (c0->dsts) {
		if (!c1->dsts)
			return 0;

		if (c0->dsts->size != c1->dsts->size)
			return 0;

		int i;
		for (i = 0; i < c0->dsts->size; i++) {
			mc_3ac_operand_t* dst0 = c0->dsts->data[i];
			mc_3ac_operand_t* dst1 = c1->dsts->data[i];

			if (dst0->dag_node) {
				if (!dst1->dag_node)
					return 0;

				if (!dag_dn_same(dst0->dag_node, dst1->dag_node))
					return 0;
			} else if (dst1->dag_node)
				return 0;
		}
	} else if (c1->dsts)
		return 0;

	if (c0->srcs) {
		if (!c1->srcs)
			return 0;

		if (c0->srcs->size != c1->srcs->size)
			return 0;

		int i;
		for (i = 0; i < c0->srcs->size; i++) {
			mc_3ac_operand_t* src0 = c0->srcs->data[i];
			mc_3ac_operand_t* src1 = c1->srcs->data[i];

			if (src0->dag_node) {
				if (!src1->dag_node)
					return 0;

				if (!dag_dn_same(src0->dag_node, src1->dag_node))
					return 0;
			} else if (src1->dag_node)
				return 0;
		}
	} else if (c1->srcs)
		return 0;
	return 1;
}

mc_3ac_code_t* _3ac_code_clone(mc_3ac_code_t* c)
{
	mc_3ac_code_t* c2 = calloc(1, sizeof(mc_3ac_code_t));
	if (!c2)
		return NULL;

	c2->op = c->op;

	if (c->dsts) {
		c2->dsts = vector_alloc();
		if (!c2->dsts) {
			_3ac_code_free(c2);
			return NULL;
		}

		int i;
		for (i = 0; i < c->dsts->size; i++) {
			mc_3ac_operand_t* dst  = c->dsts->data[i];
			mc_3ac_operand_t* dst2 = _3ac_operand_alloc();

			if (!dst2) {
				_3ac_code_free(c2);
				return NULL;
			}

			if (vector_add(c2->dsts, dst2) < 0) {
				_3ac_code_free(c2);
				return NULL;
			}

			dst2->node     = dst->node;
			dst2->dag_node = dst->dag_node;
			dst2->code     = dst->code;
			dst2->bb       = dst->bb;
		}
	}

	if (c->srcs) {
		c2->srcs = vector_alloc();
		if (!c2->srcs) {
			_3ac_code_free(c2);
			return NULL;
		}

		int i;
		for (i = 0; i < c->srcs->size; i++) {
			mc_3ac_operand_t* src  = c->srcs->data[i];
			mc_3ac_operand_t* src2 = _3ac_operand_alloc();

			if (!src2) {
				_3ac_code_free(c2);
				return NULL;
			}

			if (vector_add(c2->srcs, src2) < 0) {
				_3ac_code_free(c2);
				return NULL;
			}

			src2->node     = src->node;
			src2->dag_node = src->dag_node;
			src2->code     = src->code;
			src2->bb       = src->bb;
		}
	}

	c2->label  = c->label;
	c2->origin = c;
	return c2;
}

void mc_3ac_code_free(mc_3ac_code_t* c)
{
	int i;

	if (c) {
		if (c->dsts) {
			for (i = 0; i < c->dsts->size; i++)
				mc_3ac_operand_free(c->dsts->data[i]);
			vector_free(c->dsts);
		}

		if (c->srcs) {
			for (i = 0; i < c->srcs->size; i++)
				mc_3ac_operand_free(c->srcs->data[i]);
			vector_free(c->srcs);
		}

		if (c->active_vars) {
			int i;
			for (i = 0; i < c->active_vars->size; i++)
				dn_status_free(c->active_vars->data[i]);
			vector_free(c->active_vars);
		}

		free(c);
		c = NULL;
	}
}

mc_3ac_code_t* mc_3ac_alloc_by_src(int op_type, dag_node_t* src)
{
	mc_3ac_operator_t* mc_3ac_op = mc_3ac_find_operator(op_type);
	if (!mc_3ac_op) {
		loge("3ac operator not found\n");
		return NULL;
	}

	mc_3ac_operand_t* src0	= mc_3ac_operand_alloc();
	if (!src0)
		return NULL;
	src0->dag_node = src;

	vector_t* srcs = vector_alloc();
	if (!srcs)
		goto error1;
	if (vector_add(srcs, src0) < 0)
		goto error0;

	mc_3ac_code_t* c = mc_3ac_code_alloc();
	if (!c)
		goto error0;

	c->op	= mc_3ac_op;
	c->srcs	= srcs;
	return c;

error0:
	vector_free(srcs);
error1:
	mc_3ac_operand_free(src0);
	return NULL;
}

mc_3ac_code_t* mc_3ac_alloc_by_dst(int op_type, dag_node_t* dst)
{
	mc_3ac_operator_t* op;
	mc_3ac_operand_t*  d;
	mc_3ac_code_t*     c;

	op = mc_3ac_find_operator(op_type);
	if (!op) {
		loge("3ac operator not found\n");
		return NULL;
	}

	d = mc_3ac_operand_alloc();
	if (!d)
		return NULL;
	d->dag_node = dst;

	c = mc_3ac_code_alloc();
	if (!c) {
		mc_3ac_operand_free(d);
		return NULL;
	}

	c->dsts = vector_alloc();
	if (!c->dsts) {
		mc_3ac_operand_free(d);
		mc_3ac_code_free(c);
		return NULL;
	}

	if (vector_add(c->dsts, d) < 0) {
		mc_3ac_operand_free(d);
		mc_3ac_code_free(c);
		return NULL;
	}

	c->op = op;
	return c;
}

mc_3ac_code_t* mc_3ac_jmp_code(int type, label_t* l, node_t* err)
{
	mc_3ac_operand_t* dst;
	mc_3ac_code_t*    c;

	c = mc_3ac_code_alloc();
	if (!c)
		return NULL;

	c->op    = mc_3ac_find_operator(type);
	c->label = l;

	c->dsts  = vector_alloc();
	if (!c->dsts) {
		mc_3ac_code_free(c);
		return NULL;
	}

	dst = mc_3ac_operand_alloc();
	if (!dst) {
		mc_3ac_code_free(c);
		return NULL;
	}

	if (vector_add(c->dsts, dst) < 0) {
		mc_3ac_operand_free(dst);
		mc_3ac_code_free(c);
		return NULL;
	}

	return c;
}

static void mc_3ac_print_node(node_t* node)
{
	if (type_is_var(node->type)) {
		if (node->var->w) {
			printf("v_%d_%d/%s/%#lx",
					node->var->w->line, node->var->w->pos, node->var->w->text->data, 0xffff & (uintptr_t)node->var);
		} else {
			printf("v_%#lx", 0xffff & (uintptr_t)node->var);
		}
	} else if (type_is_operator(node->type)) {
		if (node->result) {
			if (node->result->w) {
				printf("v_%d_%d/%s/%#lx",
						node->result->w->line, node->result->w->pos, node->result->w->text->data, 0xffff & (uintptr_t)node->result);
			} else
				printf("v/%#lx", 0xffff & (uintptr_t)node->result);
		}
	} else if (FUNCTION == node->type) {
		printf("f_%d_%d/%s",
				node->w->line, node->w->pos, node->w->text->data);
	} else {
		loge("node: %p\n", node);
		loge("node->type: %d\n", node->type);
		assert(0);
	}
}

static void mc_3ac_print_dag_node(dag_node_t* dn)
{
	if (type_is_var(dn->type)) {
		if (dn->var->w) {
			printf("v_%d_%d/%s_%#lx ",
					dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
					0xffff & (uintptr_t)dn);
		} else {
			printf("v_%#lx", 0xffff & (uintptr_t)dn->var);
		}
	} else if (type_is_operator(dn->type)) {
		mc_3ac_operator_t* op = mc_3ac_find_operator(dn->type);
		if (dn->var && dn->var->w)
			printf("v_%d_%d/%s_%#lx ",
					dn->var->w->line, dn->var->w->pos, dn->var->w->text->data,
					0xffff & (uintptr_t)dn);
		else
			printf("v_%#lx/%s ", 0xffff & (uintptr_t)dn->var, op->name);
	} else {
		//printf("type: %d, v_%#lx\n", dn->type, 0xffff & (uintptr_t)dn->var);
		//assert(0);
	}
}

void mc_3ac_code_print(mc_3ac_code_t* c, list_t* sentinel)
{
	mc_3ac_operand_t* src;
	mc_3ac_operand_t* dst;

	int i;

	printf("%s  ", c->op->name);

	if (c->dsts) {

		for (i  = 0; i < c->dsts->size; i++) {
			dst =        c->dsts->data[i];

			if (dst->dag_node)
				mc_3ac_print_dag_node(dst->dag_node);

			else if (dst->node)
				mc_3ac_print_node(dst->node);

			else if (dst->code) {
				if (&dst->code->list != sentinel) {
					printf(": ");
					mc_3ac_code_print(dst->code, sentinel);
				}
			} else if (dst->bb)
				printf(" bb: %p, index: %d ", dst->bb, dst->bb->index);

			if (i < c->dsts->size - 1)
				printf(", ");
		}
	}

	if (c->srcs) {
	
		for (i  = 0; i < c->srcs->size; i++) {
			src =        c->srcs->data[i];

			if (0 == i && c->dsts && c->dsts->size > 0)
				printf("; ");

			if (src->dag_node)
				mc_3ac_print_dag_node(src->dag_node);

			else if (src->node)
				mc_3ac_print_node(src->node);

			else if (src->code)
				assert(0);

			if (i < c->srcs->size - 1)
				printf(", ");
		}
	}

	printf("\n");
}

static int mc_3ac_code_to_dag(mc_3ac_code_t* c, list_t* dag, int nb_operands0, int nb_operands1)
{
	mc_3ac_operand_t* dst;
	mc_3ac_operand_t* src;
	dag_node_t*    dn;

	int ret;
	int i;
	int j;

	if (c->dsts) {
		for (j  = 0; j < c->dsts->size; j++) {
			dst =        c->dsts->data[j];

			if (!dst || !dst->node)
				continue;

			ret = dag_get_node(dag, dst->node, &dst->dag_node);
			if (ret < 0)
				return ret;
		}
	}

	if (!c->srcs)
		return 0;

	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		if (!src || !src->node)
			continue;

		ret = dag_get_node(dag, src->node, &src->dag_node);
		if (ret < 0)
			return ret;

		if (!c->dsts)
			continue;

		for (j  = 0; j < c->dsts->size; j++) {
			dst =        c->dsts->data[j];

			if (!dst || !dst->dag_node)
				continue;

			dn = dst->dag_node;

			if (!dn->childs || dn->childs->size < nb_operands0) {

				ret = dag_node_add_child(dn, src->dag_node);
				if (ret < 0)
					return ret;
				continue;
			}

			int k;
			for (k = 0; k < dn->childs->size && k < nb_operands1; k++) {

				if (src->dag_node == dn->childs->data[k])
					break;
			}

			if (k == dn->childs->size) {
				loge("i: %d, c->op: %s, dn->childs->size: %d, c->srcs->size: %d\n",
						i, c->op->name, dn->childs->size, c->srcs->size);
				mc_3ac_code_print(c, NULL);
				return -1;
			}
		}
	}

	return 0;
}

int mc_3ac_code_to_dag(mc_3ac_code_t* c, list_t* dag)
{
	mc_3ac_operand_t* src;
	mc_3ac_operand_t* dst;

	int ret = 0;
	int i;

	if (type_is_assign(c->op->type)) {

		src = c->srcs->data[0];
		dst = c->dsts->data[0];

		ret = dag_get_node(dag, src->node, &src->dag_node);
		if (ret < 0)
			return ret;

		ret = dag_get_node(dag, dst->node, &dst->dag_node);
		if (ret < 0)
			return ret;

		if (OP_ASSIGN == c->op->type && src->dag_node == dst->dag_node)
			return 0;

		dag_node_t* dn_src;
		dag_node_t* dn_parent;
		dag_node_t* dn_assign;
		variable_t* v_assign = NULL;

		if (dst->node->parent)
			v_assign = _operand_get(dst->node->parent);

		dn_assign = dag_node_alloc(c->op->type, v_assign, NULL);

		list_add_tail(dag, &dn_assign->list);

		dn_src = src->dag_node;

		if (dn_src->parents && dn_src->parents->size > 0 && !variable_may_malloced(dn_src->var)) {
			dn_parent        = dn_src->parents->data[dn_src->parents->size - 1];

			if (OP_ASSIGN == dn_parent->type) {

				assert(2 == dn_parent->childs->size);
				dn_src    = dn_parent->childs->data[1];
			}
		}

		ret = dag_node_add_child(dn_assign, dst->dag_node);
		if (ret < 0)
			return ret;

		return dag_node_add_child(dn_assign, dn_src);

	} else if (type_is_assign_array_index(c->op->type)
			|| type_is_assign_dereference(c->op->type)
			|| type_is_assign_pointer(c->op->type)) {

		dag_node_t* assign;

		assert(c->srcs);

		assign = dag_node_alloc(c->op->type, NULL, NULL);
		if (!assign)
			return -ENOMEM;
		list_add_tail(dag, &assign->list);

		for (i  = 0; i < c->srcs->size; i++) {
			src =        c->srcs->data[i];

			ret = dag_get_node(dag, src->node, &src->dag_node);
			if (ret < 0)
				return ret;

			ret = dag_node_add_child(assign, src->dag_node);
			if (ret < 0)
				return ret;
		}

	} else if (OP_VLA_ALLOC == c->op->type) {

		dst = c->dsts->data[0];
		ret = dag_get_node(dag, dst->node, &dst->dag_node);
		if (ret < 0)
			return ret;

		dag_node_t* alloc = dag_node_alloc(c->op->type, NULL, NULL);
		if (!alloc)
			return -ENOMEM;
		list_add_tail(dag, &alloc->list);

		ret = dag_node_add_child(alloc, dst->dag_node);
		if (ret < 0)
			return ret;

		for (i  = 0; i < c->srcs->size; i++) {
			src =        c->srcs->data[i];

			ret = dag_get_node(dag, src->node, &src->dag_node);
			if (ret < 0)
				return ret;

			ret = dag_node_add_child(alloc, src->dag_node);
			if (ret < 0)
				return ret;
		}
	} else if (OP_3AC_CMP == c->op->type
			|| OP_3AC_TEQ == c->op->type
			|| OP_3AC_DUMP == c->op->type) {

		dag_node_t* dn_cmp = dag_node_alloc(c->op->type, NULL, NULL);

		list_add_tail(dag, &dn_cmp->list);

		if (c->srcs) {
			int i;
			for (i  = 0; i < c->srcs->size; i++) {
				src = c->srcs->data[i];

				ret = dag_get_node(dag, src->node, &src->dag_node);
				if (ret < 0)
					return ret;

				ret = dag_node_add_child(dn_cmp, src->dag_node);
				if (ret < 0)
					return ret;
			}
		}
	} else if (OP_3AC_SETZ  == c->op->type
			|| OP_3AC_SETNZ == c->op->type
			|| OP_3AC_SETLT == c->op->type
			|| OP_3AC_SETLE == c->op->type
			|| OP_3AC_SETGT == c->op->type
			|| OP_3AC_SETGE == c->op->type) {

		assert(c->dsts && 1 == c->dsts->size);
		dst = c->dsts->data[0];

		dag_node_t* dn_setcc = dag_node_alloc(c->op->type, NULL, NULL);
		list_add_tail(dag, &dn_setcc->list);

		ret = dag_get_node(dag, dst->node, &dst->dag_node);
		if (ret < 0)
			return ret;

		ret = dag_node_add_child(dn_setcc, dst->dag_node);
		if (ret < 0)
			return ret;

	} else if (OP_INC == c->op->type
			|| OP_DEC == c->op->type
			|| OP_INC_POST == c->op->type
			|| OP_DEC_POST == c->op->type
			|| OP_3AC_INC  == c->op->type
			|| OP_3AC_DEC  == c->op->type) {
		src = c->srcs->data[0];

		assert(src->node->parent);

		variable_t* v_parent  = _operand_get(src->node->parent);
		dag_node_t* dn_parent = dag_node_alloc(c->op->type, v_parent, NULL);

		list_add_tail(dag, &dn_parent->list);

		ret = dag_get_node(dag, src->node, &src->dag_node);
		if (ret < 0)
			return ret;

		ret = dag_node_add_child(dn_parent, src->dag_node);
		if (ret < 0)
			return ret;

		if (c->dsts) {
			dst = c->dsts->data[0];
			assert(dst->node);
			dst->dag_node = dn_parent;
		}

	} else if (OP_TYPE_CAST == c->op->type) {

		src = c->srcs->data[0];
		dst = c->dsts->data[0];

		ret = dag_get_node(dag, src->node, &src->dag_node);
		if (ret < 0)
			return ret;

		ret = dag_get_node(dag, dst->node, &dst->dag_node);
		if (ret < 0)
			return ret;

		if (!dag_node_find_child(dst->dag_node, src->dag_node)) {

			ret = dag_node_add_child(dst->dag_node, src->dag_node);
			if (ret < 0)
				return ret;
		}

	} else if (OP_RETURN == c->op->type) {

		if (c->srcs) {
			dag_node_t* dn = dag_node_alloc(c->op->type, NULL, NULL);

			list_add_tail(dag, &dn->list);

			for (i  = 0; i < c->srcs->size; i++) {
				src =        c->srcs->data[i];

				ret = dag_get_node(dag, src->node, &src->dag_node);
				if (ret < 0)
					return ret;

				ret = dag_node_add_child(dn, src->dag_node);
				if (ret < 0)
					return ret;
			}
		}
	} else if (type_is_jmp(c->op->type)) {

		logd("c->op: %d, name: %s\n", c->op->type, c->op->name);

	} else {
		int n_operands0   = -1;
		int n_operands1   = -1;

		if (c->dsts) {
			dst = c->dsts->data[0];

			assert(dst->node->op);

			n_operands0 = dst->node->op->nb_operands;
			n_operands1 = n_operands0;

			switch (c->op->type) {
				case OP_ARRAY_INDEX:
				case OP_3AC_ADDRESS_OF_ARRAY_INDEX:
				case OP_VA_START:
				case OP_VA_ARG:
					n_operands0 = 3;
					break;

				case OP_3AC_ADDRESS_OF_POINTER:
				case OP_VA_END:
					n_operands0 = 2;
					break;

				case OP_CALL:
					n_operands0 = c->srcs->size;
					break;
				default:
					break;
			};
		}

		return mc_3ac_code_to_dag(c, dag, n_operands0, n_operands1);
	}

	return 0;
}

static void mc_3ac_filter_jmp(list_t* h, mc_3ac_code_t* c)
{
	list_t*        l2   = NULL;
	mc_3ac_code_t*    c2   = NULL;
	mc_3ac_operand_t* dst0 = c->dsts->data[0];
	mc_3ac_operand_t* dst1 = NULL;

	for (l2 = &dst0->code->list; l2 != list_sentinel(h); ) {

		c2 = list_data(l2, mc_3ac_code_t, list);

		if (OP_GOTO == c2->op->type) {
			dst0 = c2->dsts->data[0];
			l2   = &dst0->code->list;
			continue;
		}

		if (OP_3AC_NOP == c2->op->type) {
			l2 = list_next(l2);
			continue;
		}

		if (!type_is_jmp(c2->op->type)) {

			dst0       = c->dsts->data[0];
			dst0->code = c2;

			c2->basic_block_start = 1;
			c2->jmp_dst_flag      = 1;
			break;
		}
#if 0
		if (OP_GOTO == c->op->type) {
			c->op        = c2->op;

			dst0 = c ->dsts->data[0];
			dst1 = c2->dsts->data[0];

			dst0->code = dst1->code;

			l2 = &dst1->code->list;
			continue;
		}
#endif
		logw("c: %s, c2: %s\n", c->op->name, c2->op->name);
		dst0       = c->dsts->data[0];
		dst0->code = c2;
		c2->basic_block_start = 1;
		c2->jmp_dst_flag      = 1;
		break;
	}

	l2	= list_next(&c->list);
	if (l2 != list_sentinel(h)) {
		c2 = list_data(l2, mc_3ac_code_t, list);
		c2->basic_block_start = 1;
	}
	c->basic_block_start = 1;
}

static int mc_3ac_filter_dst_teq(list_t* h, mc_3ac_code_t* c)
{
	mc_3ac_code_t*	   setcc2 = NULL;
	mc_3ac_code_t*	   setcc3 = NULL;
	mc_3ac_code_t*	   setcc  = NULL;
	mc_3ac_code_t*	   jcc    = NULL;
	mc_3ac_operand_t* dst0   = c->dsts->data[0];
	mc_3ac_operand_t* dst1   = NULL;
	mc_3ac_code_t*    teq    = dst0->code;
	list_t*        l;

	int jmp_op;

	if (list_prev(&c->list) == list_sentinel(h))
		return 0;
	setcc = list_data(list_prev(&c->list),   mc_3ac_code_t, list);

	if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETZ == setcc->op->type)
			|| (OP_3AC_JZ  == c->op->type && OP_3AC_SETNZ == setcc->op->type)
			|| (OP_3AC_JGT == c->op->type && OP_3AC_SETLE == setcc->op->type)
			|| (OP_3AC_JGE == c->op->type && OP_3AC_SETLT == setcc->op->type)
			|| (OP_3AC_JLT == c->op->type && OP_3AC_SETGE == setcc->op->type)
			|| (OP_3AC_JLE == c->op->type && OP_3AC_SETGT == setcc->op->type))
		jmp_op = OP_3AC_JZ;

	else if ((OP_3AC_JNZ == c->op->type && OP_3AC_SETNZ == setcc->op->type)
			|| (OP_3AC_JZ  == c->op->type && OP_3AC_SETZ  == setcc->op->type)
			|| (OP_3AC_JGT == c->op->type && OP_3AC_SETGT == setcc->op->type)
			|| (OP_3AC_JGE == c->op->type && OP_3AC_SETGE == setcc->op->type)
			|| (OP_3AC_JLT == c->op->type && OP_3AC_SETLT == setcc->op->type)
			|| (OP_3AC_JLE == c->op->type && OP_3AC_SETLE == setcc->op->type))
		jmp_op = OP_3AC_JNZ;
	else
		return 0;

	setcc2 = setcc;

	while (teq && OP_3AC_TEQ == teq->op->type) {

		assert(setcc->dsts && 1 == setcc->dsts->size);
		assert(teq->srcs   && 1 == teq  ->srcs->size);

		mc_3ac_operand_t* src   = teq  ->srcs->data[0];
		mc_3ac_operand_t* dst   = setcc->dsts->data[0];
		variable_t*    v_teq = _operand_get(src->node);
		variable_t*    v_set = _operand_get(dst->node);

		if (v_teq != v_set)
			return 0;

		for (l  = list_next(&teq->list); l != list_sentinel(h); l = list_next(l)) {
			jcc = list_data(l, mc_3ac_code_t, list);

			if (type_is_jmp(jcc->op->type))
				break;
		}
		if (l == list_sentinel(h))
			return 0;

		if (OP_3AC_JZ == jmp_op) {

			if (OP_3AC_JZ == jcc->op ->type) {

				dst0 = c  ->dsts->data[0];
				dst1 = jcc->dsts->data[0];

				dst0->code = dst1->code;

			} else if (OP_3AC_JNZ == jcc->op->type) {

				l = list_next(&jcc->list);
				if (l == list_sentinel(h))
					return 0;

				dst0       = c->dsts->data[0];
				dst0->code = list_data(l, mc_3ac_code_t, list);
			} else
				return 0;

		} else if (OP_3AC_JNZ == jmp_op) {

			if (OP_3AC_JNZ == jcc-> op->type) {

				dst0 = c  ->dsts->data[0];
				dst1 = jcc->dsts->data[0];

				dst0->code = dst1->code;

			} else if (OP_3AC_JZ == jcc->op->type) {

				l = list_next(&jcc->list);
				if (l == list_sentinel(h))
					return 0;

				dst0       = c->dsts->data[0];
				dst0->code = list_data(l, mc_3ac_code_t, list);
			} else
				return 0;
		}
		teq = dst0->code;

		if (list_prev(&jcc->list) == list_sentinel(h))
			return 0;
		setcc = list_data(list_prev(&jcc->list), mc_3ac_code_t, list);

		if (type_is_setcc(setcc->op->type)) {

			setcc3 = mc_3ac_code_clone(setcc);
			if (!setcc3)
				return -ENOMEM;
			setcc3->op = setcc2->op;

			list_add_tail(&c->list, &setcc3->list);
		}

		if ((OP_3AC_JNZ == jcc->op->type && OP_3AC_SETZ  == setcc->op->type)
		 || (OP_3AC_JZ  == jcc->op->type && OP_3AC_SETNZ == setcc->op->type))

			jmp_op = OP_3AC_JZ;

		else if ((OP_3AC_JNZ == jcc->op->type && OP_3AC_SETNZ == setcc->op->type)
			  || (OP_3AC_JZ  == jcc->op->type && OP_3AC_SETZ  == setcc->op->type))

			jmp_op = OP_3AC_JNZ;
		else
			return 0;
	}

	return 0;
}

static int mc_3ac_filter_prev_teq(list_t* h, mc_3ac_code_t* c, mc_3ac_code_t* teq)
{
	mc_3ac_code_t*	setcc3 = NULL;
	mc_3ac_code_t*	setcc2 = NULL;
	mc_3ac_code_t*	setcc  = NULL;
	mc_3ac_code_t*	jcc    = NULL;
	mc_3ac_code_t*	jmp    = NULL;
	list_t*     l;

	int jcc_type;

	if (list_prev(&teq->list) == list_sentinel(h))
		return 0;

	setcc    = list_data(list_prev(&teq->list), mc_3ac_code_t, list);
	jcc_type = -1;

	if (OP_3AC_JZ == c->op->type) {

		switch (setcc->op->type) {
			case OP_3AC_SETZ:
				jcc_type = OP_3AC_JNZ;
				break;

			case OP_3AC_SETNZ:
				jcc_type = OP_3AC_JZ;
				break;

			case OP_3AC_SETGT:
				jcc_type = OP_3AC_JLE;
				break;

			case OP_3AC_SETGE:
				jcc_type = OP_3AC_JLT;
				break;

			case OP_3AC_SETLT:
				jcc_type = OP_3AC_JGE;
				break;

			case OP_3AC_SETLE:
				jcc_type = OP_3AC_JGT;
				break;

			case OP_3AC_SETA:
				jcc_type = OP_3AC_JBE;
				break;
			case OP_3AC_SETAE:
				jcc_type = OP_3AC_JB;
				break;

			case OP_3AC_SETB:
				jcc_type = OP_3AC_JAE;
				break;
			case OP_3AC_SETBE:
				jcc_type = OP_3AC_JA;
				break;
			default:
				return 0;
				break;
		};
	} else if (OP_3AC_JNZ == c->op->type) {

		switch (setcc->op->type) {
			case OP_3AC_SETZ:
				jcc_type = OP_3AC_JZ;
				break;

			case OP_3AC_SETNZ:
				jcc_type = OP_3AC_JNZ;
				break;

			case OP_3AC_SETGT:
				jcc_type = OP_3AC_JGT;
				break;

			case OP_3AC_SETGE:
				jcc_type = OP_3AC_JGE;
				break;

			case OP_3AC_SETLT:
				jcc_type = OP_3AC_JLT;
				break;

			case OP_3AC_SETLE:
				jcc_type = OP_3AC_JLE;
				break;

			case OP_3AC_SETA:
				jcc_type = OP_3AC_JA;
				break;
			case OP_3AC_SETAE:
				jcc_type = OP_3AC_JAE;
				break;

			case OP_3AC_SETB:
				jcc_type = OP_3AC_JB;
				break;
			case OP_3AC_SETBE:
				jcc_type = OP_3AC_JBE;
				break;
			default:
				return 0;
				break;
		};
	} else
		return 0;

	assert(setcc->dsts && 1 == setcc->dsts->size);
	assert(teq->srcs   && 1 == teq  ->srcs->size);

	mc_3ac_operand_t* src   = teq  ->srcs->data[0];
	mc_3ac_operand_t* dst0  = setcc->dsts->data[0];
	mc_3ac_operand_t* dst1  = NULL;
	variable_t*    v_teq = _operand_get(src ->node);
	variable_t*    v_set = _operand_get(dst0->node);

	if (v_teq != v_set)
		return 0;

#define MC_3AC_JCC_ALLOC(j, cc) \
	do { \
		j = mc_3ac_code_alloc(); \
		if (!j) \
			return -ENOMEM; \
		\
		j->dsts = vector_alloc(); \
		if (!j->dsts) { \
			mc_3ac_code_free(j); \
			return -ENOMEM; \
		} \
		\
		mc_3ac_operand_t* dst0 = mc_3ac_operand_alloc(); \
		if (!dst0) { \
			mc_3ac_code_free(j); \
			return -ENOMEM; \
		} \
		\
		if (vector_add(j->dsts, dst0) < 0) { \
			mc_3ac_code_free(j); \
			mc_3ac_operand_free(dst0); \
			return -ENOMEM; \
		} \
		j->op = mc_3ac_find_operator(cc); \
		assert(j->op); \
	} while (0)

	mc_3ac_JCC_ALLOC(jcc, jcc_type);
	dst0 = jcc->dsts->data[0];
	dst1 = c  ->dsts->data[0];
	dst0->code = dst1->code;
	list_add_front(&setcc->list, &jcc->list);

	l = list_prev(&c->list);
	if (l != list_sentinel(h)) {

		setcc2 = list_data(l, mc_3ac_code_t, list);

		if (type_is_setcc(setcc2->op->type)) {

			setcc3 = mc_3ac_code_clone(setcc2);
			if (!setcc3)
				return -ENOMEM;
			setcc3->op = setcc->op;

			list_add_tail(&jcc->list, &setcc3->list);
		}
	}

	l = list_next(&c->list);
	if (l == list_sentinel(h)) {
		mc_3ac_filter_jmp(h, jcc);
		return 0;
	}

	mc_3ac_JCC_ALLOC(jmp, OP_GOTO);
	dst0 = jmp->dsts->data[0];
	dst0->code = list_data(l, mc_3ac_code_t, list);
	list_add_front(&jcc->list, &jmp->list);

	mc_3ac_filter_jmp(h, jcc);
	mc_3ac_filter_jmp(h, jmp);

	return 0;
}

void mc_3ac_list_print(list_t* h)
{
	mc_3ac_code_t* c;
	list_t*	    l;

	for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {

		c  = list_data(l, mc_3ac_code_t, list);

		mc_3ac_code_print(c, NULL);
	}
}

static int mc_3ac_find_basic_block_start(list_t* h)
{
	int			  start = 0;
	list_t*	  l;

	for (l = list_head(h); l != list_sentinel(h); l = list_next(l)) {

		mc_3ac_code_t* c  = list_data(l, mc_3ac_code_t, list);

		list_t*		l2 = NULL;
		mc_3ac_code_t*	c2 = NULL;

		if (!start) {
			c->basic_block_start = 1;
			start = 1;
		}
#if 0
		if (type_is_assign_dereference(c->op->type)) {

			l2	= list_next(&c->list);
			if (l2 != list_sentinel(h)) {
				c2 = list_data(l2, 3ac_code_t, list);
				c2->basic_block_start = 1;
			}

			c->basic_block_start = 1;
			continue;
		}
		if (OP_DEREFERENCE == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (OP_CALL == c->op->type) {

			l2	= list_next(&c->list);
			if (l2 != list_sentinel(h)) {
				c2 = list_data(l2, 3ac_code_t, list);
				c2->basic_block_start = 1;
			}

//			c->basic_block_start = 1;
			continue;
		}
#endif

#if 0
		if (OP_RETURN == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}
#endif
		if (OP_3AC_DUMP == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}

		if (OP_3AC_END == c->op->type) {
			c->basic_block_start = 1;
			continue;
		}

		if (OP_3AC_CMP == c->op->type
				|| OP_3AC_TEQ == c->op->type) {

			for (l2 = list_next(&c->list); l2 != list_sentinel(h); l2 = list_next(l2)) {

				c2  = list_data(l2, mc_3ac_code_t, list);

				if (type_is_setcc(c2->op->type))
					continue;

				if (OP_3AC_TEQ == c2->op->type)
					continue;

				if (type_is_jmp(c2->op->type))
					c->basic_block_start = 1;
				break;
			}
			continue;
		}

		if (type_is_jmp(c->op->type)) {

			mc_3ac_operand_t* dst0 = c->dsts->data[0];

			assert(dst0->code);

			// filter 1st expr of logic op, such as '&&', '||'
			if (OP_3AC_TEQ == dst0->code->op->type) {

				int ret = mc_3ac_filter_dst_teq(h, c);
				if (ret < 0)
					return ret;
			}

			for (l2 = list_prev(&c->list); l2 != list_sentinel(h); l2 = list_prev(l2)) {

				c2  = list_data(l2, mc_3ac_code_t, list);

				if (type_is_setcc(c2->op->type))
					continue;

				// filter 2nd expr of logic op, such as '&&', '||'
				if (OP_3AC_TEQ == c2->op->type) {

					int ret = mc_3ac_filter_prev_teq(h, c, c2);
					if (ret < 0)
						return ret;
				}
				break;
			}

			mc_3ac_filter_jmp(h, c);
		}
	}
#if 1
	for (l = list_head(h); l != list_sentinel(h); ) {

		mc_3ac_code_t*    c    = list_data(l, mc_3ac_code_t, list);

		list_t*        l2   = NULL;
		mc_3ac_code_t*    c2   = NULL;
		mc_3ac_operand_t* dst0 = NULL;

		if (OP_3AC_NOP == c->op->type) {
			assert(!c->jmp_dst_flag);

			l = list_next(l);

			list_del(&c->list);
			mc_3ac_code_free(c);
			c = NULL;
			continue;
		}

		if (OP_GOTO != c->op->type) {
			l = list_next(l);
			continue;
		}
		assert(!c->jmp_dst_flag);

		for (l2 = list_next(&c->list); l2 != list_sentinel(h); ) {

			c2  = list_data(l2, mc_3ac_code_t, list);

			if (c2->jmp_dst_flag)
				break;

			l2 = list_next(l2);

			list_del(&c2->list);
			mc_3ac_code_free(c2);
			c2 = NULL;
		}

		l    = list_next(l);
		dst0 = c->dsts->data[0];

		if (l == &dst0->code->list) {
			list_del(&c->list);
			mc_3ac_code_free(c);
			c = NULL;
		}
	}
#endif
	return 0;
}

static int mc_3ac_split_basic_blocks(list_t* h, function_t* f)
{
	list_t*	l;
	basic_block_t* bb = NULL;

	for (l = list_head(h); l != list_sentinel(h); ) {

		mc_3ac_code_t* c = list_data(l, mc_3ac_code_t, list);

		l = list_next(l);

		if (c->basic_block_start) {

			bb = basic_block_alloc();
			if (!bb)
				return -ENOMEM;

			bb->index = f->nb_basic_blocks++;
			list_add_tail(&f->basic_block_list_head, &bb->list);

			c->basic_block = bb;

			if (OP_3AC_CMP == c->op->type
					|| OP_3AC_TEQ == c->op->type) {

				mc_3ac_operand_t* src;
				mc_3ac_code_t*    c2;
				list_t*	       l2;
				node_t*        e;
				int i;

				for (l2 = list_next(&c->list); l2 != list_sentinel(h); l2 = list_next(l2)) {

					c2  = list_data(l2, mc_3ac_code_t, list);

					if (type_is_setcc(c2->op->type))
						continue;

					if (OP_3AC_TEQ == c2->op->type)
						continue;

					if (type_is_jmp(c2->op->type)) {
						bb->cmp_flag = 1;

						if (c->srcs) {

							for (i = 0; i < c->srcs->size; i++) {
								src       = c->srcs->data[i];
								e         = src->node;

								while (e && OP_EXPR == e->type)
									e = e->nodes[0];
								assert(e);

								if (OP_DEREFERENCE == e->type)
									bb->dereference_flag = 1;
							}
						}
					}
					break;
				}

				list_del(&c->list);
				list_add_tail(&bb->code_list_head, &c->list);
				continue;
			}

			list_del(&c->list);
			list_add_tail(&bb->code_list_head, &c->list);

			if (OP_CALL == c->op->type) {
				bb->call_flag = 1;
				continue;
			}

			if (OP_RETURN == c->op->type) {
				bb->ret_flag = 1;
				continue;
			}

			if (OP_3AC_DUMP == c->op->type) {
				bb->dump_flag = 1;
				continue;
			}

			if (OP_3AC_END == c->op->type) {
				bb->end_flag = 1;
				continue;
			}

			if (type_is_assign_dereference(c->op->type)
					|| OP_DEREFERENCE == c->op->type) {
				bb->dereference_flag = 1;
				continue;
			}

			if (type_is_assign_array_index(c->op->type)) {
				bb->array_index_flag = 1;
				continue;
			}

			if (OP_VA_START      == c->op->type
					|| OP_VA_ARG == c->op->type
					|| OP_VA_END == c->op->type) {
				bb->varg_flag = 1;
				continue;
			}

			if (type_is_jmp(c->op->type)) {
				bb->jmp_flag = 1;

				if (type_is_jcc(c->op->type))
					bb->jcc_flag = 1;

				int ret = vector_add_unique(f->jmps, c);
				if (ret < 0)
					return ret;
			}
		} else {
			assert(bb);
			c->basic_block = bb;

			if (type_is_assign_dereference(c->op->type) || OP_DEREFERENCE == c->op->type)
				bb->dereference_flag = 1;

			else if (type_is_assign_array_index(c->op->type))
				bb->array_index_flag = 1;

			else if (OP_CALL == c->op->type)
				bb->call_flag = 1;

			else if (OP_RETURN == c->op->type)
				bb->ret_flag = 1;

			else if (OP_VLA_ALLOC == c->op->type) {
				bb->vla_flag = 1;
				f ->vla_flag = 1;

			} else if (OP_VA_START == c->op->type
					|| OP_VA_ARG == c->op->type
					|| OP_VA_END == c->op->type)
				bb->varg_flag = 1;

			list_del(&c->list);
			list_add_tail(&bb->code_list_head, &c->list);
		}
	}

	return 0;
}

static int mc_3ac_connect_basic_blocks(function_t* f)
{
	int i;
	int ret;

	list_t*	l;
	list_t* sentinel = list_sentinel(&f->basic_block_list_head);

	for (l = list_head(&f->basic_block_list_head); l != sentinel; l = list_next(l)) {

		basic_block_t* current_bb = list_data(l, basic_block_t, list);
		basic_block_t* prev_bb    = NULL;
		basic_block_t* next_bb    = NULL;
		list_t*        l2         = list_prev(l);

		if (current_bb->jmp_flag)
			continue;

		if (l2 != sentinel) {
			prev_bb = list_data(l2, basic_block_t, list);

			if (!prev_bb->jmp_flag) {
				ret = basic_block_connect(prev_bb, current_bb);
				if (ret < 0)
					return ret;
			}
		}

		l2 = list_next(l);
		if (l2 == sentinel)
			continue;

		next_bb = list_data(l2, basic_block_t, list);

		if (!next_bb->jmp_flag) {
			ret = basic_block_connect(current_bb, next_bb);
			if (ret < 0)
				return ret;
		}
	}

	for (i = 0; i < f->jmps->size; i++) {
		mc_3ac_code_t*    c    = f->jmps->data[i];
		mc_3ac_operand_t* dst0 = c->dsts->data[0];
		mc_3ac_code_t*    dst  = dst0->code;

		basic_block_t* current_bb = c->basic_block;
		basic_block_t* dst_bb     = dst->basic_block;
		basic_block_t* prev_bb    = NULL;
		basic_block_t* next_bb    = NULL;

		dst0->bb   = dst_bb;
		dst0->code = NULL;

		for (l = list_prev(&current_bb->list); l != sentinel; l = list_prev(l)) {

			prev_bb = list_data(l, basic_block_t, list);

			if (!prev_bb->jmp_flag)
				break;

			if (!prev_bb->jcc_flag) {
				prev_bb = NULL;
				break;
			}
			prev_bb = NULL;
		}

		if (prev_bb) {
			ret = basic_block_connect(prev_bb, dst_bb);
			if (ret < 0)
				return ret;
		} else
			continue;

		if (!current_bb->jcc_flag)
			continue;

		for (l = list_next(&current_bb->list); l != sentinel; l = list_next(l)) {

			next_bb = list_data(l, basic_block_t, list);

			if (!next_bb->jmp_flag)
				break;

			if (!next_bb->jcc_flag) {
				next_bb = NULL;
				break;
			}
			next_bb = NULL;
		}

		if (next_bb) {
			ret = basic_block_connect(prev_bb, next_bb);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int mc_3ac_split_basic_blocks(list_t* list_head_3ac, function_t* f)
{
	int ret = mc_3ac_find_basic_block_start(list_head_3ac);
	if (ret < 0)
		return ret;

	ret = mc_3ac_split_basic_blocks(list_head_3ac, f);
	if (ret < 0)
		return ret;

	return mc_3ac_connect_basic_blocks(f);
}

mc_3ac_code_t* mc_3ac_code_NN(int op_type, node_t** dsts, int nb_dsts, node_t** srcs, int nb_srcs)
{
	mc_3ac_operator_t* op = mc_3ac_find_operator(op_type);
	if (!op) {
		loge("\n");
		return NULL;
	}

	mc_3ac_operand_t* operand;
	mc_3ac_code_t*    c;
	vector_t*      vsrc = NULL;
	vector_t*      vdst = NULL;
	node_t*        node;

	int i;

	if (srcs) {
		vsrc = vector_alloc();
		for (i = 0; i < nb_srcs; i++) {

			operand = mc_3ac_operand_alloc();

			node    = srcs[i];

			while (node && OP_EXPR == node->type)
				node = node->nodes[0];

			operand->node = node;

			vector_add(vsrc, operand);
		}
	}

	if (dsts) {
		vdst = vector_alloc();
		for (i = 0; i < nb_dsts; i++) {

			operand = mc_3ac_operand_alloc();

			node    = dsts[i];

			while (node && OP_EXPR == node->type)
				node = node->nodes[0];

			operand->node = node;

			vector_add(vdst, operand);
		}
	}

	c       = mc_3ac_code_alloc();
	c->op   = op;
	c->dsts = vdst;
	c->srcs = vsrc;
	return c;
}
