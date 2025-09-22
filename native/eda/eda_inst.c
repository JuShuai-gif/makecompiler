#include"eda.h"

#define EDA_INST_SRC_CHECK() \
	if (!c->srcs || c->srcs->size != 1) \
	return -EINVAL; \
	\
	eda_context_t* eda  = ctx->priv; \
	function_t*    f    = eda->f; \
	\
	3ac_operand_t* src  = c->srcs->data[0]; \
	\
	if (!src || !src->dag_node) \
	return -EINVAL;

#define EDA_INST_DST_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
	return -EINVAL; \
	\
	eda_context_t* eda  = ctx->priv; \
	function_t*    f    = eda->f; \
	\
	3ac_operand_t* dst  = c->dsts->data[0]; \
	\
	if (!dst || !dst->dag_node) \
	return -EINVAL;

#define EDA_INST_OP2_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 1) \
		return -EINVAL; \
	\
	eda_context_t* eda  = ctx->priv; \
	function_t*    f    = eda->f; \
	\
	3ac_operand_t* dst  = c->dsts->data[0]; \
	3ac_operand_t* src  = c->srcs->data[0]; \
	\
	if (!src || !src->dag_node) \
		return -EINVAL; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (src->dag_node->var->size != dst->dag_node->var->size) {\
		loge("size: %d, %d\n", src->dag_node->var->size, dst->dag_node->var->size); \
		return -EINVAL; \
	}

#define EDA_INST_OP3_CHECK() \
	if (!c->dsts || c->dsts->size != 1) \
		return -EINVAL; \
	\
	if (!c->srcs || c->srcs->size != 2) \
		return -EINVAL; \
	\
	eda_context_t* eda  = ctx->priv; \
	function_t*    f    = eda->f; \
	\
	3ac_operand_t* dst  = c->dsts->data[0]; \
	3ac_operand_t* src0 = c->srcs->data[0]; \
	3ac_operand_t* src1 = c->srcs->data[1]; \
	\
	if (!src0 || !src0->dag_node) \
		return -EINVAL; \
	\
	if (!src1 || !src1->dag_node) \
		return -EINVAL; \
	\
	if (!dst || !dst->dag_node) \
		return -EINVAL; \
	\
	if (src0->dag_node->var->size != src1->dag_node->var->size) {\
		loge("size: %d, %d\n", src0->dag_node->var->size, src1->dag_node->var->size); \
		return -EINVAL; \
	}

#define EDA_INST_IN_CHECK(_in, _N) \
	do { \
		int i; \
		if (!(_in)->var->arg_flag) { \
			if ((_in)->n_pins != _N) \
				return -EINVAL; \
			\
			for (i = 0; i < _N; i++) { \
				if (!(_in)->pins[i]) \
					return -EINVAL; \
			} \
		} \
	} while (0)

#define EDA_PIN_ADD_INPUT(_in, _i, _ef, _p) \
		do { \
			if (!(_in)->pins[_i]) \
				(_in)->pins[_i] = _p; \
			else if (_p) \
				EDA_PIN_ADD_PIN_EF(_ef, _p, (_in)->pins[_i]); \
			else \
		        _p = (_in)->pins[_i]; \
		} while (0)

int __eda_bit_nand(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B    = f->ef->components[0];
	ScfEcomponent*  NAND = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, NAND, EDA_NAND);

	EDA_PIN_ADD_PIN(NAND, EDA_NAND_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(NAND, EDA_NAND_NEG, B, EDA_Battery_NEG);

	*in0 = NAND->pins[EDA_NAND_IN0];
	*in1 = NAND->pins[EDA_NAND_IN1];
	*out = NAND->pins[EDA_NAND_OUT];
	return 0;
}

int __eda_bit_nor(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  NOR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, NOR, EDA_NOR);

	EDA_PIN_ADD_PIN(NOR, EDA_NOR_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(NOR, EDA_NOR_NEG, B, EDA_Battery_NEG);

	*in0 = NOR->pins[EDA_NOR_IN0];
	*in1 = NOR->pins[EDA_NOR_IN1];
	*out = NOR->pins[EDA_NOR_OUT];
	return 0;
}

int __eda_bit_not(function_t* f, ScfEpin** in, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  NOT = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, NOT, EDA_NOT);

	EDA_PIN_ADD_PIN(NOT, EDA_NOT_POS, B,   EDA_Battery_POS);
	EDA_PIN_ADD_PIN(NOT, EDA_NOT_NEG, B,   EDA_Battery_NEG);
	EDA_PIN_ADD_PIN(NOT, EDA_NOT_IN,  NOT, EDA_NOT_IN);

	*in  = NOT->pins[EDA_NOT_IN];
	*out = NOT->pins[EDA_NOT_OUT];
	return 0;
}

int __eda_bit_and(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  AND = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, AND, EDA_AND);

	EDA_PIN_ADD_PIN(AND, EDA_AND_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(AND, EDA_AND_NEG, B, EDA_Battery_NEG);

	*in0 = AND->pins[EDA_AND_IN0];
	*in1 = AND->pins[EDA_AND_IN1];
	*out = AND->pins[EDA_AND_OUT];
	return 0;
}

int __eda_bit_or(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B  = f->ef->components[0];
	ScfEcomponent*  OR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, OR, EDA_OR);

	EDA_PIN_ADD_PIN(OR, EDA_OR_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(OR, EDA_OR_NEG, B, EDA_Battery_NEG);

	*in0 = OR->pins[EDA_OR_IN0];
	*in1 = OR->pins[EDA_OR_IN1];
	*out = OR->pins[EDA_OR_OUT];
	return 0;
}

int __eda_bit_if(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** in2, ScfEpin** out)
{
	ScfEcomponent*  B  = f->ef->components[0];
	ScfEcomponent*  IF = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, IF, EDA_IF);

	EDA_PIN_ADD_PIN(IF, EDA_IF_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(IF, EDA_IF_NEG, B, EDA_Battery_NEG);

	*in0 = IF->pins[EDA_IF_TRUE];
	*in1 = IF->pins[EDA_IF_COND];
	*in2 = IF->pins[EDA_IF_FALSE];
	*out = IF->pins[EDA_IF_OUT];
	return 0;
}

int __eda_bit_and2_or(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** in2, ScfEpin** in3, ScfEpin** out)
{
	ScfEcomponent*  B       = f->ef->components[0];
	ScfEcomponent*  AND2_OR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, AND2_OR, EDA_AND2_OR);

	EDA_PIN_ADD_PIN(AND2_OR, EDA_AND2_OR_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(AND2_OR, EDA_AND2_OR_NEG, B, EDA_Battery_NEG);

	*in0 = AND2_OR->pins[EDA_AND2_OR_IN0];
	*in1 = AND2_OR->pins[EDA_AND2_OR_IN1];
	*in2 = AND2_OR->pins[EDA_AND2_OR_IN2];
	*in3 = AND2_OR->pins[EDA_AND2_OR_IN3];
	*out = AND2_OR->pins[EDA_AND2_OR_OUT];
	return 0;
}

static int __eda_bit_xor(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  XOR = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, XOR, EDA_XOR);

	EDA_PIN_ADD_PIN(XOR, EDA_XOR_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(XOR, EDA_XOR_NEG, B, EDA_Battery_NEG);

	*in0 = XOR->pins[EDA_XOR_IN0];
	*in1 = XOR->pins[EDA_XOR_IN1];
	*out = XOR->pins[EDA_XOR_OUT];
	return 0;
}

static int __eda_bit_add(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** out, ScfEpin** cf)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  ADD = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, ADD, EDA_ADD);

	EDA_PIN_ADD_PIN(ADD, EDA_ADD_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(ADD, EDA_ADD_NEG, B, EDA_Battery_NEG);

	ADD->pins[EDA_ADD_CF]->flags = EDA_PIN_CF;

	*in0 = ADD->pins[EDA_ADD_IN0];
	*in1 = ADD->pins[EDA_ADD_IN1];
	*out = ADD->pins[EDA_ADD_OUT];
	*cf  = ADD->pins[EDA_ADD_CF];
	return 0;
}

static int __eda_bit_adc(function_t* f, ScfEpin** in0, ScfEpin** in1, ScfEpin** in2, ScfEpin** out, ScfEpin** cf)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  ADC = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, ADC, EDA_ADC);

	EDA_PIN_ADD_PIN(ADC, EDA_ADC_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(ADC, EDA_ADC_NEG, B, EDA_Battery_NEG);

	ADC->pins[EDA_ADC_CF]->flags = EDA_PIN_CF;

	*in0 = ADC->pins[EDA_ADC_IN0];
	*in1 = ADC->pins[EDA_ADC_IN1];
	*in2 = ADC->pins[EDA_ADC_CI];
	*out = ADC->pins[EDA_ADC_OUT];
	*cf  = ADC->pins[EDA_ADC_CF];
	return 0;
}

static int __eda_teq(function_t* f, dag_node_t* in, ScfEpin** res)
{
	ScfEpin* prev = NULL;
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* po;

	int N = eda_variable_size(in->var);
	int ret;
	int i;

	for (i = 0; i + 1 < N; i += 2) {
		ret = __eda_bit_nor(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(in, i,     f->ef, p0);
		EDA_PIN_ADD_INPUT(in, i + 1, f->ef, p1);

		if (!prev)
			prev = po;
		else
			EDA_PIN_ADD_PIN_EF(f->ef, prev, po);

		if (in->var->arg_flag) {
			if (!in->var->r_pins[i]) {
				in ->var->r_pins[i] = in->pins[i];

				in->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[i]->io_lid = i;
			}

			if (!in->var->r_pins[i + 1]) {
				in ->var->r_pins[i + 1] = in->pins[i + 1];

				in->pins[i + 1]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[i + 1]->io_lid = i + 1;
			}
		}
	}

	if (N & 0x1) {
		ret = __eda_bit_not(f, &p0, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(in, N - 1, f->ef, p0);

		if (!prev)
			prev = po;
		else
			EDA_PIN_ADD_PIN_EF(f->ef, prev, po);

		if (in->var->arg_flag) {
			if (!in->var->r_pins[N - 1]) {
				in ->var->r_pins[N - 1] = in->pins[N - 1];

				in->pins[N - 1]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[N - 1]->io_lid = N - 1;
			}
		}
	}

	ret = __eda_bit_not(f, &po, res);
	if (ret < 0)
		return ret;
	EDA_PIN_ADD_PIN_EF(f->ef, po, prev);
	return 0;
}

static int _eda_inst_logic_not_handler(native_t* ctx, 3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;
	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	eda_context_t* eda  = ctx->priv;
	function_t*    f    = eda->f;

	3ac_operand_t* dst  = c->dsts->data[0];
	3ac_operand_t* src  = c->srcs->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;
	if (!dst || !dst->dag_node)
		return -EINVAL;

	dag_node_t* in  = src->dag_node;
	dag_node_t* out = dst->dag_node;

	ScfEpin* res = NULL;
	ScfEpin* p0  = NULL;
	ScfEpin* p1  = NULL;
	ScfEpin* p2  = NULL;

	int N = eda_variable_size(in->var);
	int i;

	EDA_INST_IN_CHECK(in, N);

	in ->n_pins = N;
	out->n_pins = eda_variable_size(out->var);

	int ret = __eda_teq(f, in, &res);
	if (ret < 0)
		return ret;

	for (i = 0; i < out->n_pins; i++) {
		if (i > 0) {
			ret = __eda_bit_not(f, &p0, &p1);
			if (ret < 0)
				return ret;

			ret = __eda_bit_not(f, &p2, &(out->pins[i]));
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p0, res);
			EDA_PIN_ADD_PIN_EF(f->ef, p1, p2);
		} else
			out->pins[i] = res;
	}
	return 0;
}

static int _eda_inst_bit_not_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP2_CHECK()

	dag_node_t* in  = src->dag_node;
	dag_node_t* out = dst->dag_node;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in ->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {
		ScfEpin* pi = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_not(f, &pi, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in, i, f->ef, pi);

		if (in->var->arg_flag) {
			if (!in->var->r_pins[i]) {
				in ->var->r_pins[i] = in->pins[i];

				in->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_bit_and_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0 = NULL;
		ScfEpin* p1 = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_and(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_bit_or_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0 = NULL;
		ScfEpin* p1 = NULL;
		ScfEpin* po = NULL;

		int ret = __eda_bit_or(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = po;
	}

	return 0;
}

static int _eda_inst_shl_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* src = src0->dag_node;
	dag_node_t* sht = src1->dag_node;
	dag_node_t* out = dst ->dag_node;
	ScfEcomponent*  B   = f->ef->components[0];

	int i;
	int j;
	int N = eda_variable_size(src->var);

	EDA_INST_IN_CHECK(src, N);
	EDA_INST_IN_CHECK(sht, N);

	src->n_pins = N;
	sht->n_pins = N;
	out->n_pins = N;

	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* tmp[EDA_MAX_BITS];

	for (j = 0; j < N && j < 6; j++) {
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;

		int ret = __eda_bit_not(f, &p1, &p2);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(sht, j, f->ef, p1);

		for (i = 0; i < (1 << j); i++) {
			ScfEpin* p00 = NULL;
			ScfEpin* p01 = NULL;
			ScfEpin* po = NULL;

			ret = __eda_bit_and(f, &p00, &p01, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p01, p2);

			if (j > 0)
				EDA_PIN_ADD_PIN_EF(f->ef, p00, res[i]);
			else
				EDA_PIN_ADD_INPUT(src, i, f->ef, p00);

			tmp[i] = po;
		}

		for ( ; i < N; i++) {
			ScfEpin* p00 = NULL;
			ScfEpin* p01 = NULL;
			ScfEpin* p10 = NULL;
			ScfEpin* p11 = NULL;
			ScfEpin* po  = NULL;
/*
x = sht[j]
y = src[i - (1 << j)]
z = src[i]

out[i] =     (x & y) |   (~x & z)
       =  ~(~(x & y) &  ~(~x & z))
       = (x NAND y) NAND (~x NAND z)
 */
			ret = __eda_bit_and2_or(f, &p00, &p10, &p01, &p11, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p10, p2);
			EDA_PIN_ADD_PIN_EF(f->ef, p11, p1);

			if (j > 0) {
				EDA_PIN_ADD_PIN_EF(f->ef, p00, res[i]);
				EDA_PIN_ADD_PIN_EF(f->ef, p01, res[i - (1 << j)]);
			} else {
				EDA_PIN_ADD_INPUT(src, i,            f->ef, p00);
				EDA_PIN_ADD_INPUT(src, i - (1 << j), f->ef, p01);
			}

			tmp[i] = po;
		}

		for (i = 0; i < N; i++) {
			res[i] = tmp[i];
			tmp[i] = NULL;

			res[i]->flags |= EDA_PIN_SHIFT;
		}
	}

	for (i = 0; i < N; i++) {
		if (src->var->arg_flag) {
			if (!src->var->r_pins[i]) {
				src ->var->r_pins[i] = src->pins[i];

				src->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				src->pins[i]->io_lid = i;
			}
		}

		if (sht->var->arg_flag) {
			if (!sht->var->r_pins[i]) {
				sht ->var->r_pins[i] = sht->pins[i];

				sht->pins[i]->flags |= EDA_PIN_IN;
				sht->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = res[i];
	}

	return 0;
}

static int __eda_shr(function_t* f, dag_node_t* src, int shift, int sign, ScfEpin** cur, ScfEpin** res, int i, int N)
{
	ScfEpin* tmp[EDA_MAX_BITS];

	ScfEpin* p00 = NULL;
	ScfEpin* p10 = NULL;
	ScfEpin* p01 = NULL;
	ScfEpin* p11 = NULL;

	ScfEpin* po2 = NULL;
	ScfEpin* po  = NULL;

	int ret;
	int j;

	for (j = i ; j < N - (1 << shift); j++) {

		ret = __eda_bit_if(f, &p00, &p10, &p01, &po);
		if (ret < 0)
			return ret;
		if (*cur)
			EDA_PIN_ADD_PIN_EF(f->ef, p10, *cur);
		else
			*cur = p10;

		if (!src)
			EDA_PIN_ADD_PIN_EF(f->ef, p01, res[j + (1 << shift)]);
		else
			EDA_PIN_ADD_INPUT(src, j + (1 << shift), f->ef, p01);

		if (!src)
			EDA_PIN_ADD_PIN_EF(f->ef, p00, res[j]);
		else
			EDA_PIN_ADD_INPUT(src, j, f->ef, p00);

		tmp[j] = po;
	}

	for ( ; j < N; j++) {

		if (sign && N - 1 == j) {
			ret = __eda_bit_not(f, &p00, &p10);
			if (ret < 0)
				return ret;

			ret = __eda_bit_not(f, &po2, &po);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, po2, p10);

		} else {
			ret = __eda_bit_and(f, &p00, &p10, &po);
			if (ret < 0)
				return ret;
			if (*cur)
				EDA_PIN_ADD_PIN_EF(f->ef, p10, *cur);
			else
				*cur = p10;

			if (sign) {
				ret = __eda_bit_or(f, &p01, &p11, &po2);
				if (ret < 0)
					return ret;
				EDA_PIN_ADD_PIN_EF(f->ef, p11, po);

				if (!src)
					EDA_PIN_ADD_PIN_EF(f->ef, p01, res[N - 1]);
				else
					EDA_PIN_ADD_INPUT(src, N - 1, f->ef, p01);

				po = po2;
			}
		}

		if (!src)
			EDA_PIN_ADD_PIN_EF(f->ef, p00, res[j]);
		else
			EDA_PIN_ADD_INPUT(src, j, f->ef, p00);

		tmp[j] = po;
	}

	for (j = i; j < N; j++) {
		res[j] = tmp[j];

		res[j]->flags |= EDA_PIN_SHIFT;
		res[j]->io_lid = j;
	}

	return 0;
}

static int _eda_inst_shr_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* src = src0->dag_node;
	dag_node_t* sht = src1->dag_node;
	dag_node_t* out = dst ->dag_node;
	ScfEcomponent*  B   = f->ef->components[0];

	int i;
	int j;
	int N    = eda_variable_size  (src->var);
	int sign = variable_signed(src->var);

	EDA_INST_IN_CHECK(src, N);
	EDA_INST_IN_CHECK(sht, N);

	src->n_pins = N;
	sht->n_pins = N;
	out->n_pins = N;

	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* tmp[EDA_MAX_BITS];

	for (j = 0; j < N && j < 6; j++) {
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;

		int ret = __eda_bit_not(f, &p1, &p2); // if sht[j] == 0 keep current bit, else select high bit.
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_INPUT(sht, j, f->ef, p1);
/*
x = sht[j]
y = src[i + (1 << j)]
z = src[i]

out[i] =     (x & y) |   (~x & z)
       =  ~(~(x & y) &  ~(~x & z))
       = (x NAND y) NAND (~x NAND z)
 */
		if (j > 0)
			ret = __eda_shr(f, NULL, j, sign, &p2, res, 0, N);
		else
			ret = __eda_shr(f, src, j, sign, &p2, res, 0, N);

		if (ret < 0)
			return ret;
	}

	for (i = 0; i < N; i++) {
		if (src->var->arg_flag) {
			if (!src->var->r_pins[i]) {
				src ->var->r_pins[i] = src->pins[i];

				src->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				src->pins[i]->io_lid = i;
			}
		}

		if (sht->var->arg_flag) {
			if (!sht->var->r_pins[i]) {
				sht ->var->r_pins[i] = sht->pins[i];

				sht->pins[i]->flags |= EDA_PIN_IN;
				sht->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = res[i];
	}

	return 0;
}

static int _eda_inst_add_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	ScfEcomponent*  B   = f->ef->components[0];
	ScfEpin*        pc  = NULL;

	int i;
	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* p2  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			int ret = __eda_bit_adc(f, &p0, &p1, &p2, &res, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p2, pc);

		} else {
			int ret = __eda_bit_add(f, &p0, &p1, &res, &cf);
			if (ret < 0)
				return ret;
		}

		EDA_PIN_ADD_INPUT(in0, i, f->ef, p0);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, p1);

		pc         = cf; // carry flag
		cf->flags |= EDA_PIN_CF;

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}

		out->pins[i] = res; // result
	}

	return 0;
}

static int __eda_sub(function_t* f, ScfEpin** a, ScfEpin** b, ScfEpin** res, ScfEpin** cf, int N)
{
	ScfEcomponent*  B  = f->ef->components[0];
	ScfEcomponent*  R0 = NULL;
	ScfEpin*        pc = B->pins[EDA_Battery_POS];

	int i;
	for (i = 0; i < N; i++) {
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;
		ScfEpin* p3 = NULL;

		int ret = __eda_bit_not(f, &b[i], &p1);
		if (ret < 0)
			return ret;

		ret = __eda_bit_adc(f, &a[i], &p2, &p3, &res[i], cf);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p2, p1);
		EDA_PIN_ADD_PIN_EF(f->ef, p3, pc);

		pc         = *cf;
		pc->flags |= EDA_PIN_CF;
	}
	return 0;
}

static int _eda_inst_sub_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	ScfEpin* a[EDA_MAX_BITS];
	ScfEpin* b[EDA_MAX_BITS];
	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* cf;

	int ret = __eda_sub(f, a, b, res, &cf, N);
	if (ret < 0)
		return ret;

	int i;
	for (i = 0; i < N; i++) {
		EDA_PIN_ADD_INPUT(in0, i, f->ef, a[i]);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, b[i]);

		out->pins[i] = res[i];

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}
	}
	return 0;
}

static int __eda_bit_mla(function_t* f, ScfEpin** a, ScfEpin** b, ScfEpin** c, ScfEpin** d, ScfEpin** out, ScfEpin** cf)
{
	ScfEcomponent*  B   = f->ef->components[0];
	ScfEcomponent*  MLA = NULL;

	EDA_INST_ADD_COMPONENT(f->ef, MLA, EDA_MLA);

	EDA_PIN_ADD_PIN(MLA, EDA_MLA_POS, B, EDA_Battery_POS);
	EDA_PIN_ADD_PIN(MLA, EDA_MLA_NEG, B, EDA_Battery_NEG);

	*a   = MLA->pins[EDA_MLA_IN0];
	*b   = MLA->pins[EDA_MLA_IN1];
	*c   = MLA->pins[EDA_MLA_IN2];
	*d   = MLA->pins[EDA_MLA_IN3];
	*out = MLA->pins[EDA_MLA_OUT];
	*cf  = MLA->pins[EDA_MLA_CF];
	return 0;
}

static int _eda_inst_mul_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	ScfEpin*  adds[EDA_MAX_BITS] = {NULL};
	ScfEpin*  cfs [EDA_MAX_BITS] = {NULL};

	int n_adds = 0;
	int n_cfs  = 0;

	int N = eda_variable_size(in0->var);
	int i;

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	for (i = 0; i < N; i++) {

		ScfEpin* p0j = NULL;
		ScfEpin* p0k = NULL;
		ScfEpin* p1j = NULL;
		ScfEpin* p1k = NULL;
		ScfEpin* res = NULL;
		ScfEpin* cf  = NULL;

		if (0 == i) {
			int ret = __eda_bit_and(f, &p0j, &p1k, &res);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, 0, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in1, 0, f->ef, p1k);

			out->pins[0] = res;

			if (in0->var->arg_flag) {
				if (!in0->var->r_pins[i]) {
					in0 ->var->r_pins[i] = in0->pins[i];

					in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
					in0->pins[i]->io_lid = i;
				}
			}

			if (in1->var->arg_flag) {
				if (!in1->var->r_pins[i]) {
					in1 ->var->r_pins[i] = in1->pins[i];

					in1->pins[i]->flags |= EDA_PIN_IN;
					in1->pins[i]->io_lid = i;
				}
			}
			continue;
		}

		int j = 0;
		int k = i;

		while (j < k) {
			int ret = __eda_bit_mla(f, &p0j, &p1k, &p0k, &p1j, &adds[n_adds], &cfs[n_cfs]);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, j, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in0, k, f->ef, p0k);

			EDA_PIN_ADD_INPUT(in1, j, f->ef, p1j);
			EDA_PIN_ADD_INPUT(in1, k, f->ef, p1k);

			cfs[n_cfs]->flags |= EDA_PIN_CF;

			in0->pins[j]->flags |= EDA_PIN_IN0;
			in0->pins[k]->flags |= EDA_PIN_IN0;

			n_adds++;
			n_cfs++;

			j++;
			k--;
		}

		if (j == k) {
			int ret = __eda_bit_and(f, &p0j, &p1k, &adds[n_adds]);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_INPUT(in0, j, f->ef, p0j);
			EDA_PIN_ADD_INPUT(in1, j, f->ef, p1k);

			in0->pins[j]->flags |= EDA_PIN_IN0;

			n_adds++;
		}

		while (n_adds > 1) {
			j = 0;
			k = n_adds - 1;

			while (j < k) {
				p0j = NULL;
				p1k = NULL;

				int ret = __eda_bit_add(f, &p0j, &p1k, &res, &cfs[n_cfs]);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p0j, adds[j]);
				EDA_PIN_ADD_PIN_EF(f->ef, p1k, adds[k]);

				cfs[n_cfs++]->flags |= EDA_PIN_CF;

				adds[j] = res;

				j++;
				k--;
			}

			if (j == k)
				n_adds = j + 1;
			else
				n_adds = j;
			logd("i: %d, n_adds: %d, j: %d, k: %d\n\n", i, n_adds, j, k);
		}

		out->pins[i] = adds[0];

		for (j = 0; j < n_cfs; j++)
			adds[j] = cfs[j];

		n_adds = n_cfs;
		n_cfs  = 0;

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}
	}

	return 0;
}

/* x = a / b
---------------------------------------------
(a[0] + 2a[1] + 4a[2]) = (b[0] + 2b[1] + 4b[2])(x[0] + 2x[1] + 4x[2])
---------------------------------------------
shift right until b[0] == 1

a[0] =  x[0] % 2
a[1] = (x[1] + b[1]x[0]) % 2
a[2] = (x[2] + b[1]x[1] + b[2]x[0]) % 2

x[0] = a[0]
x[1] = a[1] - b[1]x[0]
----------------------------------------------
 */
static int __eda_div(function_t* f, ScfEpin** a, ScfEpin** b, ScfEpin** x, int N)
{
	ScfEpin*  adds[EDA_MAX_BITS] = {NULL};
	ScfEpin*  cfs [EDA_MAX_BITS] = {NULL};

	int n_adds = 0;
	int n_cfs  = 0;
	int i;
	int j;
	int k;

	x[0] = a[0];

	for (i = 1; i < N; i++) {
		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* res = NULL;

		for (j = 1; j <= i; j++) {

			int ret = __eda_bit_and(f, &p0, &p1, &adds[n_adds]);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p0, b[j]);
			EDA_PIN_ADD_PIN_EF(f->ef, p1, x[i - j]);
			n_adds++;
		}

		while (n_adds > 1) {
			j = 0;
			k = n_adds - 1;

			while (j < k) {
				int ret;

				if (i < N - 1)
					ret = __eda_bit_add(f, &p0, &p1, &res, &cfs[n_cfs]);
				else
					ret = __eda_bit_xor(f, &p0, &p1, &res);
				if (ret < 0)
					return ret;
				EDA_PIN_ADD_PIN_EF(f->ef, p0, adds[j]);
				EDA_PIN_ADD_PIN_EF(f->ef, p1, adds[k]);

				if (i < N - 1)
					cfs[n_cfs++]->flags |= EDA_PIN_CF;

				adds[j] = res;

				j++;
				k--;
			}

			if (j == k)
				n_adds = j + 1;
			else
				n_adds = j;
		}

		int ret = __eda_bit_xor(f, &p0, &p1, &x[i]);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p0, a[i]);
		EDA_PIN_ADD_PIN_EF(f->ef, p1, adds[0]);

		if (i < N - 1) {
			ret = __eda_bit_and(f, &p0, &p1, &cfs[n_cfs]);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p0, x[i]);
			EDA_PIN_ADD_PIN_EF(f->ef, p1, adds[0]);

			cfs[n_cfs++]->flags |= EDA_PIN_CF;
		}

		for (j = 0; j < n_cfs; j++)
			adds[j] = cfs[j];

		n_adds = n_cfs;
		n_cfs  = 0;
	}

	return 0;
}

static int _eda_inst_div_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP3_CHECK()

	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;
	dag_node_t* out = dst ->dag_node;

	int N    = eda_variable_size  (in0->var);
	int sign = variable_signed(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;
	out->n_pins = N;

	ScfEpin* res[EDA_MAX_BITS] = {NULL};
	ScfEpin* sh0[EDA_MAX_BITS] = {NULL};
	ScfEpin* sh1[EDA_MAX_BITS] = {NULL};
	ScfEpin* mask = NULL;

	ScfEpin* high;
	ScfEpin* cur;
	ScfEpin* pm0;
	ScfEpin* pm1;
	ScfEpin* pm;

	int ret;
	int i;
	for (i = 0; i < N - 1; i++) {

		if (mask) {
			ret = __eda_bit_or(f, &pm0, &pm1, &cur);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, pm0, mask);
			EDA_PIN_ADD_INPUT(in1, i, f->ef, pm1);
		}

		if (i > 0)
			ret = __eda_shr(f, NULL, 0, sign, &cur, sh0, 0, N);
		else
			ret = __eda_shr(f, in0, 0, sign, &cur, sh0, 0, N);
		if (ret < 0)
			return ret;

		if (i > 0)
			ret = __eda_shr(f, NULL, 0, sign, &cur, sh1, 1, N);
		else
			ret = __eda_shr(f, in1, 0, sign, &cur, sh1, 1, N);
		if (ret < 0)
			return ret;

		if (!mask)
			EDA_PIN_ADD_INPUT(in1, i, f->ef, cur);
		mask = cur;
	}

	if (1 == N) {
		ret = __eda_bit_and(f, &sh0[0], &sh1[0], &res[0]);
		if (ret < 0)
			return ret;

		EDA_PIN_ADD_INPUT(in0, 0, f->ef, sh0[0]);
		EDA_PIN_ADD_INPUT(in1, 0, f->ef, sh1[0]);

		sh1[0]->flags |= EDA_PIN_DIV0;
	} else {
		ret = __eda_bit_or(f, &pm0, &pm1, &cur);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, pm0, mask);
		EDA_PIN_ADD_INPUT(in1, N - 1, f->ef, pm1);

		cur->flags |= EDA_PIN_DIV0;
	}

	ret = __eda_div(f, sh0, sh1, res, N);
	if (ret < 0)
		return ret;

	for (i = 0; i < N; i++) {
		out->pins[i] = res[i];

		out->pins[i]->flags |= EDA_PIN_OUT;
		out->pins[i]->io_lid = i;

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}
	}
	return 0;
}

static int __eda_loop_var(function_t* f, dag_node_t* in, int i, ScfEpin* cond)
{
	ScfEcomponent* B = f->ef->components[0];
	ScfEcomponent* IF;
	ScfEcomponent* IF2;
	ScfEcomponent* DFF;

	EDA_INST_ADD_COMPONENT(f->ef, IF,  EDA_IF);
	EDA_INST_ADD_COMPONENT(f->ef, IF2, EDA_IF);
	EDA_INST_ADD_COMPONENT(f->ef, DFF, EDA_DFF);

	EDA_PIN_ADD_PIN(IF,  EDA_IF_POS,  B,   EDA_Battery_POS);
	EDA_PIN_ADD_PIN(IF2, EDA_IF_POS,  B,   EDA_Battery_POS);
	EDA_PIN_ADD_PIN(DFF, EDA_DFF_POS, B,   EDA_Battery_POS);

	EDA_PIN_ADD_PIN(IF,  EDA_IF_NEG,  B,   EDA_Battery_NEG);
	EDA_PIN_ADD_PIN(IF2, EDA_IF_NEG,  B,   EDA_Battery_NEG);
	EDA_PIN_ADD_PIN(DFF, EDA_DFF_NEG, B,   EDA_Battery_NEG);

	EDA_PIN_ADD_PIN(IF,  EDA_IF_OUT,   DFF, EDA_DFF_IN);
	EDA_PIN_ADD_PIN(IF,  EDA_IF_FALSE, IF2, EDA_IF_OUT);
	EDA_PIN_ADD_PIN(IF2, EDA_IF_FALSE, DFF, EDA_DFF_OUT);

	assert(cond);
	EDA_PIN_ADD_PIN_EF(f->ef, IF2->pins[EDA_IF_COND], cond);

	IF ->pins[EDA_IF_COND]->flags = EDA_PIN_DELAY;
	DFF->pins[EDA_DFF_CK ]->flags = EDA_PIN_CK;

	in->var->w_pins[i] = IF2->pins[EDA_IF_TRUE];
	in->var->DFFs[i]   = DFF;

	EDA_PIN_ADD_INPUT(in, i, f->ef, IF->pins[EDA_IF_TRUE]);

	in->pins[i] = DFF->pins[EDA_DFF_OUT];
	return 0;
}

static int _eda_inst_inc_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	basic_block_t* bb = c->basic_block;
	dag_node_t*    in = src->dag_node;
	ScfEcomponent*     B  = f->ef->components[0];
	ScfEpin*           pc = NULL;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in->n_pins = N;

// (x | y) & ~(x & y) == ~y when x = 1
	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			int ret = __eda_bit_add(f, &p0, &p1, &res, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p1, pc);
		} else {
			int ret = __eda_bit_not(f, &p0, &res);
			if (ret < 0)
				return ret;

			cf = p0;
		}

		if (bb->loop_flag) {
			if (!in->var->DFFs[i]) {
				int ret = __eda_loop_var(f, in, i, bb->mask_pin);
				if (ret < 0)
					return ret;
			}
		}
		EDA_PIN_ADD_INPUT(in, i, f->ef, p0);

		pc         = cf;
		cf->flags |= EDA_PIN_CF;

		if (in->var->arg_flag) {
			if (!in->var->r_pins[i]) {
				in ->var->r_pins[i] = in->pins[i];

				in->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[i]->io_lid = i;
			}
		}

		in->pins[i] = res;
	}

	return 0;
}

static int _eda_inst_dec_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	dag_node_t* in = src->dag_node;
	ScfEpin*        pc = NULL;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);

	in->n_pins = N;
// y-- == y - 1 == y + (-1) == y + 0xff...ff, every bit add 1

// x + y == (x | y) & ~(x & y) == ~y when x = 1
	for (i = 0; i < N; i++) {

		ScfEpin* p0  = NULL;
		ScfEpin* p1  = NULL;
		ScfEpin* cf  = NULL;
		ScfEpin* res = NULL;

		if (i > 0) {
			ScfEpin* p2  = NULL;
			ScfEpin* not = NULL;
			ScfEpin* cf0 = NULL;
			ScfEpin* cf1 = NULL;
			ScfEpin* cf2 = NULL;
			ScfEpin* cf3 = NULL;

			int ret = __eda_bit_not(f, &cf0, &not); // ~cf0 == cf0 + 1
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, cf0, pc);

			ret = __eda_bit_add(f, &p0, &p1, &res, &cf1);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p1, not);

			ret = __eda_bit_or(f, &cf2, &cf3, &cf);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, cf2, cf0);
			EDA_PIN_ADD_PIN_EF(f->ef, cf3, cf1);

		} else {
			int ret = __eda_bit_not(f, &p0, &res);
			if (ret < 0)
				return ret;

			cf = p0;
		}

		EDA_PIN_ADD_INPUT(in, i, f->ef, p0);

		pc         = cf;
		cf->flags |= EDA_PIN_CF;

		if (in->var->arg_flag) {
			if (!in->var->r_pins[i]) {
				in ->var->r_pins[i] = in->pins[i];

				in->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in->pins[i]->io_lid = i;
			}
		}

		in->pins[i] = res;
	}

	return 0;
}

static int _eda_inst_assign_array_index_handler(native_t* ctx, 3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 4)
		return -EINVAL;

	eda_context_t*  eda    = ctx->priv;
	function_t*     f      = eda->f;

	3ac_operand_t*  base   = c->srcs->data[0];
	3ac_operand_t*  index  = c->srcs->data[1];
	3ac_operand_t*  scale  = c->srcs->data[2];
	3ac_operand_t*  src    = c->srcs->data[3];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!src || !src->dag_node)
		return -EINVAL;

	return -EINVAL;
}

static int _eda_inst_array_index_handler(native_t* ctx, 3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 3)
		return -EINVAL;

	eda_context_t*  eda    = ctx->priv;
	function_t*     f      = eda->f;

	3ac_operand_t*  dst    = c->dsts->data[0];
	3ac_operand_t*  base   = c->srcs->data[0];
	3ac_operand_t*  index  = c->srcs->data[1];
	3ac_operand_t*  scale  = c->srcs->data[2];

	if (!base || !base->dag_node)
		return -EINVAL;

	if (!index || !index->dag_node)
		return -EINVAL;

	if (!scale || !scale->dag_node)
		return -EINVAL;

	if (!c->instructions) {
		c->instructions = vector_alloc();
		if (!c->instructions)
			return -ENOMEM;
	}

	variable_t*     vd  = dst  ->dag_node->var;
	variable_t*     vb  = base ->dag_node->var;
	variable_t*     vi  = index->dag_node->var;
	variable_t*     vs  = scale->dag_node->var;

	return -EINVAL;
}

static int _eda_inst_teq_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	basic_block_t* bb  = c->basic_block;
	dag_node_t*    in  = src->dag_node;
	ScfEpin*           res = NULL;

	int ret = __eda_teq(f, in, &res);
	if (ret < 0)
		return ret;

	EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[EDA_FLAG_ZERO], res);
	return 0;
}

static int _eda_inst_cmp_handler(native_t* ctx, 3ac_code_t* c)
{
	if (!c->srcs || c->srcs->size != 2)
		return -EINVAL;
	eda_context_t* eda  = ctx->priv;
	function_t*    f    = eda->f;
	basic_block_t* bb   = c->basic_block;
	3ac_operand_t* src0 = c->srcs->data[0];
	3ac_operand_t* src1 = c->srcs->data[1];

	if (!src0 || !src0->dag_node)
		return -EINVAL;
	if (!src1 || !src1->dag_node)
		return -EINVAL;
	dag_node_t* in0 = src0->dag_node;
	dag_node_t* in1 = src1->dag_node;

	int N = eda_variable_size(in0->var);

	EDA_INST_IN_CHECK(in0, N);
	EDA_INST_IN_CHECK(in1, N);

	in0->n_pins = N;
	in1->n_pins = N;

	ScfEpin* a[EDA_MAX_BITS];
	ScfEpin* b[EDA_MAX_BITS];
	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* cf;

	int ret = __eda_sub(f, a, b, res, &cf, N);
	if (ret < 0)
		return ret;

	int i;
	for (i = 0; i < N; i++) {
		EDA_PIN_ADD_INPUT(in0, i, f->ef, a[i]);
		EDA_PIN_ADD_INPUT(in1, i, f->ef, b[i]);

		if (in0->var->arg_flag) {
			if (!in0->var->r_pins[i]) {
				in0 ->var->r_pins[i] = in0->pins[i];

				in0->pins[i]->flags |= EDA_PIN_IN | EDA_PIN_IN0;
				in0->pins[i]->io_lid = i;
			}
		}

		if (in1->var->arg_flag) {
			if (!in1->var->r_pins[i]) {
				in1 ->var->r_pins[i] = in1->pins[i];

				in1->pins[i]->flags |= EDA_PIN_IN;
				in1->pins[i]->io_lid = i;
			}
		}
	}

	ScfEpin* prev = NULL;
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* po;

	for (i = 0; i + 1 < N; i += 2) {
		ret = __eda_bit_nor(f, &p0, &p1, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p0, res[i]);
		EDA_PIN_ADD_PIN_EF(f->ef, p1, res[i + 1]);

		if (prev)
			EDA_PIN_ADD_PIN_EF(f->ef, po, prev);
		else
			prev = po;
	}

	if (N & 0x1) {
		ret = __eda_bit_not(f, &p0, &po);
		if (ret < 0)
			return ret;
		EDA_PIN_ADD_PIN_EF(f->ef, p0, res[N - 1]);

		if (prev)
			EDA_PIN_ADD_PIN_EF(f->ef, po, prev);
		else
			prev = po;
	}

	ret = __eda_bit_not(f, &p0, &po);
	if (ret < 0)
		return ret;
	EDA_PIN_ADD_PIN_EF(f->ef, p0, prev);

	EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[EDA_FLAG_ZERO], po);
	EDA_PIN_ADD_CONN(f->ef, bb->flag_pins[EDA_FLAG_SIGN], res[N - 1]);
	return 0;
}

static int _eda_inst_setz_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setnz_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setgt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setge_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setlt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_setle_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_eq_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_ne_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_gt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_ge_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_lt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_le_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int __eda_cast(function_t* f, dag_node_t* in, ScfEpin** res, int N)
{
	ScfEpin* zero = NULL;

	int M    = eda_variable_size(in->var);
	int sign = variable_signed(in->var);
	int ret;
	int i;

	for (i = 0; i < N; i++) {
		ScfEpin* p0 = NULL;
		ScfEpin* p1 = NULL;
		ScfEpin* p2 = NULL;
		ScfEpin* po = NULL;

		if (i == M && !sign && !zero) {

			ret = __eda_bit_xor(f, &p0, &p1, &zero);
			if (ret < 0)
				return ret;
			EDA_PIN_ADD_PIN_EF(f->ef, p0, zero);
			EDA_PIN_ADD_INPUT(in, M - 1, f->ef, p0);
			EDA_PIN_ADD_INPUT(in, M - 1, f->ef, p1);

			res[i] = zero;
		} else {
			if (!in->pins[i]) {
				ret = __eda_bit_not(f, &p0, &p1);
				if (ret < 0)
					return ret;

				ret = __eda_bit_not(f, &p2, &po);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p2, p1);

				in->pins[i] = p0;
				res[i]      = po;

			} else if (i < M)
				res[i] = in->pins[i];
			else if (sign)
				res[i] = in->pins[M - 1];
			else
				res[i] = zero;
		}
	}

	for (i = 0; i < M; i++) {
		if (in->pins[i] && in->var->arg_flag) {

			if (!in->var->r_pins[i]) {
				in ->var->r_pins[i] = in->pins[i];

				in->pins[i]->flags |= EDA_PIN_IN;

				int j = eda_find_argv_index(f, in->var);
				if (0 == j)
					in->pins[i]->flags |= EDA_PIN_IN0;

				in->pins[i]->io_lid = j;
			}
		}
	}
	return 0;
}

static int _eda_inst_cast_handler(native_t* ctx, 3ac_code_t* c)
{
	if (!c->dsts || c->dsts->size != 1)
		return -EINVAL;

	if (!c->srcs || c->srcs->size != 1)
		return -EINVAL;

	eda_context_t* eda = ctx->priv;
	function_t*    f   = eda->f;
	3ac_operand_t* src = c->srcs->data[0];
	3ac_operand_t* dst = c->dsts->data[0];

	if (!src || !src->dag_node)
		return -EINVAL;

	if (!dst || !dst->dag_node)
		return -EINVAL;

	dag_node_t* in  = src->dag_node;
	dag_node_t* out = dst->dag_node;

	int M = eda_variable_size(in->var);
	int N = eda_variable_size(out->var);
	int i;

	EDA_INST_IN_CHECK(in, M);

	in ->n_pins = M;
	out->n_pins = N;

	return __eda_cast(f, in, out->pins, N);
}

static int _eda_inst_return_handler(native_t* ctx, 3ac_code_t* c)
{
	eda_context_t*  eda = ctx->priv;
	function_t*     f   = eda->f;
	basic_block_t*  bb  = c->basic_block;

	3ac_operand_t*  src = NULL;
	3ac_operand_t*  dst = NULL;
	dag_node_t*     in  = NULL;
	variable_t*     v   = NULL;

	if (!c->srcs || c->srcs->size < 1) {
		loge("\n");
		return -EINVAL;
	}

	if (!f->rets || f->rets->size < c->srcs->size) {
		loge("\n");
		return -EINVAL;
	}

	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* p2;
	ScfEpin* po;

	int i;
	int j;
	for (i  = 0; i < c->srcs->size; i++) {
		src =        c->srcs->data[i];

		v   = f->rets->data[i];
		in  = src->dag_node;

		v->n_pins = eda_variable_size(v);

		int ret = __eda_cast(f, in, res, v->n_pins);
		if (ret < 0)
			return ret;

		for (j = 0; j < v->n_pins; j++) {

			if (bb->mask_pin) {
				ret = __eda_bit_and(f, &p0, &p1, &po);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p0, res[j]);
				EDA_PIN_ADD_PIN_EF(f->ef, p1, bb->mask_pin);

				logi("bb->index: %d, mask_pin: c%ldp%ld\n", bb->index, bb->mask_pin->cid, bb->mask_pin->id);
				res[j] = po;
			}

			if (v->w_pins[j]) {
				ret = __eda_bit_or(f, &p0, &p1, &po);
				if (ret < 0)
					return ret;

				EDA_PIN_ADD_PIN_EF(f->ef, p0, v->w_pins[j]);
				EDA_PIN_ADD_PIN_EF(f->ef, p1, res[j]);

				res[j] = po;
			}

			if (v->w_pins[j])
				v->w_pins[j]->flags &= ~EDA_PIN_OUT;

			v->w_pins[j] = res[j];

			v->w_pins[j]->flags |= EDA_PIN_OUT;
			v->w_pins[j]->io_lid = j;
		}
	}

	return 0;
}

static int _eda_inst_nop_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_end_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_goto_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jz_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jnz_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jgt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jge_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jlt_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_jle_handler(native_t* ctx, 3ac_code_t* c)
{
	return -EINVAL;
}

static int _eda_inst_load_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_reload_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_save_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_SRC_CHECK()

	basic_block_t*  bb  = c->basic_block;
	dag_node_t*     in  = src->dag_node;

	if (!bb->loop_flag)
		return 0;

	int i;
	int N = eda_variable_size(in->var);

	EDA_INST_IN_CHECK(in, N);
	in->n_pins = N;

	for (i = 0; i < N; i++) {
		assert(in->var->w_pins[i]);

		EDA_PIN_ADD_PIN_EF(f->ef, in->var->w_pins[i], in->pins[i]);
	}

	return 0;
}

static int _eda_inst_assign_handler(native_t* ctx, 3ac_code_t* c)
{
	EDA_INST_OP2_CHECK()

	basic_block_t*  bb  = c->basic_block;
	dag_node_t*     in  = src->dag_node;
	dag_node_t*     out = dst->dag_node;
	variable_t*     v   = out->var;

	int i;
	int M = eda_variable_size(in->var);
	int N = eda_variable_size(out->var);

//	EDA_INST_IN_CHECK(in, M);

	in ->n_pins = M;
	out->n_pins = N;
	v->n_pins   = N;

	ScfEpin* res[EDA_MAX_BITS];
	ScfEpin* p0;
	ScfEpin* p1;
	ScfEpin* p2;
	ScfEpin* po;

	int ret = __eda_cast(f, in, res, N);
	if (ret < 0)
		return ret;

	for (i = 0; i < N; i++) {
		if (bb->mask_pin) {
			ret = __eda_bit_and(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p0, res[i]);
			EDA_PIN_ADD_PIN_EF(f->ef, p1, bb->mask_pin);

			res[i] = po;
		}

		if (v->w_pins[i]) {
			ret = __eda_bit_or(f, &p0, &p1, &po);
			if (ret < 0)
				return ret;

			EDA_PIN_ADD_PIN_EF(f->ef, p0, v->w_pins[i]);
			EDA_PIN_ADD_PIN_EF(f->ef, p1, res[i]);

			res[i] = po;
		}

		if (v->w_pins[i])
			v->w_pins[i]->flags &= ~EDA_PIN_OUT;

		v->w_pins[i] = res[i];

		v->w_pins[i]->flags |= EDA_PIN_OUT;
		v->w_pins[i]->io_lid = i;

		out->pins[i] = v->w_pins[i];
	}

	return 0;
}

static int _eda_inst_and_assign_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static int _eda_inst_or_assign_handler(native_t* ctx, 3ac_code_t* c)
{
	return 0;
}

static eda_inst_handler_pt eda_inst_handlers[] =
{
	[OP_ARRAY_INDEX] =  _eda_inst_array_index_handler,

	[OP_TYPE_CAST]   =  _eda_inst_cast_handler,
	[OP_LOGIC_NOT] 	 =  _eda_inst_logic_not_handler,
	[OP_BIT_NOT]     =  _eda_inst_bit_not_handler,

	[OP_3AC_INC]     =  _eda_inst_inc_handler,
	[OP_3AC_DEC]     =  _eda_inst_dec_handler,

	[OP_BIT_AND]     =  _eda_inst_bit_and_handler,
	[OP_BIT_OR]      =  _eda_inst_bit_or_handler,

	[OP_SHL]         =  _eda_inst_shl_handler,
	[OP_SHR]         =  _eda_inst_shr_handler,

	[OP_ADD]         =  _eda_inst_add_handler,
	[OP_SUB]         =  _eda_inst_sub_handler,
	[OP_MUL]         =  _eda_inst_mul_handler,
	[OP_DIV]         =  _eda_inst_div_handler,

	[OP_3AC_TEQ]     =  _eda_inst_teq_handler,
	[OP_3AC_CMP]     =  _eda_inst_cmp_handler,

	[OP_3AC_SETZ]    =  _eda_inst_setz_handler,
	[OP_3AC_SETNZ]   =  _eda_inst_setnz_handler,
	[OP_3AC_SETGT]   =  _eda_inst_setgt_handler,
	[OP_3AC_SETGE]   =  _eda_inst_setge_handler,
	[OP_3AC_SETLT]   =  _eda_inst_setlt_handler,
	[OP_3AC_SETLE]   =  _eda_inst_setle_handler,

	[OP_EQ] 		 =  _eda_inst_eq_handler,
	[OP_NE] 		 =  _eda_inst_ne_handler,
	[OP_GT] 		 =  _eda_inst_gt_handler,
	[OP_GE]          =  _eda_inst_ge_handler,
	[OP_LT]          =  _eda_inst_lt_handler,
	[OP_LE]          =  _eda_inst_le_handler,

	[OP_ASSIGN] 	 =  _eda_inst_assign_handler,

	[OP_AND_ASSIGN]  =  _eda_inst_and_assign_handler,
	[OP_OR_ASSIGN]   =  _eda_inst_or_assign_handler,

	[OP_RETURN] 	 =  _eda_inst_return_handler,
	[OP_GOTO] 		 =  _eda_inst_goto_handler,

	[OP_3AC_JZ] 	 =  _eda_inst_jz_handler,
	[OP_3AC_JNZ] 	 =  _eda_inst_jnz_handler,
	[OP_3AC_JGT] 	 =  _eda_inst_jgt_handler,
	[OP_3AC_JGE] 	 =  _eda_inst_jge_handler,
	[OP_3AC_JLT] 	 =  _eda_inst_jlt_handler,
	[OP_3AC_JLE] 	 =  _eda_inst_jle_handler,

	[OP_3AC_NOP] 	 =  _eda_inst_nop_handler,
	[OP_3AC_END] 	 =  _eda_inst_end_handler,

	[OP_3AC_SAVE]    =  _eda_inst_save_handler,
	[OP_3AC_LOAD]    =  _eda_inst_load_handler,

	[OP_3AC_RESAVE]  =  _eda_inst_save_handler,
	[OP_3AC_RELOAD]  =  _eda_inst_reload_handler,

	[OP_3AC_ASSIGN_ARRAY_INDEX] = _eda_inst_assign_array_index_handler,
};

eda_inst_handler_pt eda_find_inst_handler(const int op_type)
{
	if (op_type < 0 || op_type >= N_3AC_OPS)
		return NULL;

	return eda_inst_handlers[op_type];
}
