#include "eda_pack.h"

static int component_pins[EDA_Components_NB] =
{
	0, // None
	EDA_Battery_NB, // EDA_Battery

	2, // EDA_Resistor
	2, // EDA_Capacitor
	2, // EDA_Inductor

	EDA_Diode_NB,
	EDA_NPN_NB,
	EDA_PNP_NB,

	EDA_NAND_NB,
	EDA_NOR_NB,
	EDA_NOT_NB,

	EDA_AND_NB,
	EDA_OR_NB,
	EDA_XOR_NB,

	EDA_ADD_NB,

	EDA_NAND4_NB,
	EDA_AND2_OR_NB,

	EDA_IF_NB,
	EDA_MLA_NB,
	EDA_ADC_NB,

	EDA_DFF_NB,

	EDA_Signal_NB,

	EDA_OP_AMP_NB,
	2, // crystal oscillator
};

static int __diode_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_Diode_NEG == p0->id)
		return 1;
	return 0;
}

static int __npn_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_NPN_E == p0->id)
		return 1;

	if (!p1 || EDA_NPN_E == p1->id)
		return 0;
	return 1;
}

static int __npn_shared(ScfEpin* p, int flags)
{
	if (EDA_NPN_E == p->id)
		return 1;
	return 0;
}

static int __pnp_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_PNP_E != p0->id)
		return 1;

	if (!p1 || EDA_PNP_E != p1->id)
		return 0;
	return 1;
}

static int __pnp_shared(ScfEpin* p, int flags)
{
	if (EDA_PNP_E == p->id)
		return 1;
	return 0;
}

static int __ttl_nand_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_NAND_NEG == p0->id)
		return 1;
	if (flags && (EDA_NAND_IN0 == p0->id || EDA_NAND_IN1 == p0->id))
		return 1;

	if (EDA_NAND_POS == p0->id) {
		if (p1 && (EDA_NAND_IN0 == p1->id || EDA_NAND_IN1 == p1->id))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_NAND_IN0 == p0->id || EDA_NAND_IN1 == p0->id)
					&&  EDA_NAND_OUT == p1->id)
				return 0;

			if (EDA_NAND_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_nand_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_NAND_OUT == p->id)
			return 0;
	}
	return 1;
}

static int __ttl_not_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_NOT_NEG == p0->id)
		return 1;
	if (flags && EDA_NOT_IN == p0->id)
		return 1;

	if (EDA_NOT_POS == p0->id) {
		if (p1 && EDA_NOT_IN == p1->id)
			return 1;
	} else {
		if (p1) {
			if (!flags && EDA_NOT_IN == p0->id && EDA_NOT_OUT == p1->id)
				return 0;
			if (EDA_NOT_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_not_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_NOT_OUT == p->id)
			return 0;
	}
	return 1;
}

static int __ttl_add_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_ADD_NEG == p0->id)
		return 1;
	if (flags && (EDA_ADD_IN0 == p0->id || EDA_ADD_IN1 == p0->id))
		return 1;

	if (EDA_ADD_POS == p0->id) {
		if (p1 && (EDA_ADD_IN0 == p1->id || EDA_ADD_IN1 == p1->id))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_ADD_IN0 == p0->id || EDA_ADD_IN1 == p0->id)
					&& (EDA_ADD_OUT == p1->id || EDA_ADD_CF == p1->id))
				return 0;

			if (EDA_ADD_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_add_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_ADD_OUT == p->id || EDA_ADD_CF == p->id)
			return 0;
	}
	return 1;
}

static int __ttl_nand4_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_NAND4_NEG == p0->id)
		return 1;
	if (flags && (EDA_NAND4_IN0 <= p0->id && p0->id <= EDA_NAND4_IN3))
		return 1;

	if (EDA_NAND4_POS == p0->id) {
		if (p1 && (EDA_NAND4_IN0 <= p0->id && p0->id <= EDA_NAND4_IN3))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_NAND4_IN0 <= p0->id && p0->id <= EDA_NAND4_IN3)
					&&  EDA_NAND4_OUT == p1->id)
				return 0;

			if (EDA_NAND4_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_nand4_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_NAND4_OUT == p->id)
			return 0;
	}
	return 1;
}

static int __ttl_if_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_IF_NEG == p0->id)
		return 1;
	if (flags && (EDA_IF_TRUE <= p0->id && p0->id <= EDA_IF_FALSE))
		return 1;

	if (EDA_IF_POS == p0->id) {
		if (p1 && (EDA_IF_TRUE <= p0->id && p0->id <= EDA_IF_FALSE))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_IF_TRUE <= p0->id && p0->id <= EDA_IF_FALSE)
					&&  EDA_IF_OUT  == p1->id)
				return 0;

			if (EDA_IF_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_if_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_IF_OUT == p->id)
			return 0;
	}
	return 1;
}

static int __ttl_mla_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_MLA_NEG == p0->id)
		return 1;
	if (flags && (EDA_MLA_IN0 <= p0->id && p0->id <= EDA_MLA_IN3))
		return 1;

	if (EDA_MLA_POS == p0->id) {
		if (p1 && (EDA_MLA_IN0 <= p1->id && p1->id <= EDA_MLA_IN3))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_MLA_IN0 <= p0->id && p0->id <= EDA_MLA_IN3)
					&& (EDA_MLA_OUT == p1->id && p1->id <= EDA_MLA_CF))
				return 0;

			if (EDA_MLA_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_mla_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_MLA_OUT == p->id && p->id <= EDA_MLA_CF)
			return 0;
	}
	return 1;
}

static int __ttl_adc_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_ADC_NEG == p0->id)
		return 1;
	if (flags && (EDA_ADC_CI <= p0->id && p0->id <= EDA_ADC_IN1))
		return 1;

	if (EDA_ADC_POS == p0->id) {
		if (p1 && (EDA_ADC_CI <= p1->id && p1->id <= EDA_ADC_IN1))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_ADC_CI  <= p0->id && p0->id <= EDA_ADC_IN1)
					&& (EDA_ADC_OUT == p1->id && p1->id <= EDA_ADC_CF))
				return 0;

			if (EDA_ADC_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __ttl_adc_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_ADC_OUT == p->id && p->id <= EDA_ADC_CF)
			return 0;
	}
	return 1;
}

static int __741_op_amp_path_off(ScfEpin* p0, ScfEpin* p1, int flags)
{
	if (EDA_OP_AMP_NEG == p0->id)
		return 1;

	if (EDA_OP_AMP_POS == p0->id) {
		if (p1 && (EDA_OP_AMP_IN == p1->id || EDA_OP_AMP_INVERT == p1->id))
			return 1;
	} else {
		if (p1) {
			if (!flags
					&& (EDA_OP_AMP_IN == p0->id || EDA_OP_AMP_INVERT == p0->id)
					&&  EDA_OP_AMP_OUT == p1->id)
				return 0;

			if (EDA_OP_AMP_NEG != p1->id)
				return 1;
		}
	}
	return 0;
}

static int __741_op_amp_shared(ScfEpin* p, int flags)
{
	if (!flags) {
		if (EDA_OP_AMP_OUT == p->id)
			return 0;
	}
	return 1;
}

static ScfEops __diode_ops =
{
	__diode_path_off,
	NULL,
};

static ScfEops __npn_ops =
{
	__npn_path_off,
	__npn_shared,
};

static ScfEops __pnp_ops =
{
	__pnp_path_off,
	__pnp_shared,
};

static ScfEops __ttl_nand_ops =
{
	__ttl_nand_path_off,
	__ttl_nand_shared,
};

static ScfEops __ttl_gate_ops =
{
	__ttl_nand_path_off,
	__ttl_nand_shared,
};

static ScfEops __ttl_not_ops =
{
	__ttl_not_path_off,
	__ttl_not_shared,
};

static ScfEops __ttl_add_ops =
{
	__ttl_add_path_off,
	__ttl_add_shared,
};

static ScfEops __ttl_nand4_ops =
{
	__ttl_nand4_path_off,
	__ttl_nand4_shared,
};
static ScfEops __ttl_and2_or_ops =
{
	__ttl_nand4_path_off,
	__ttl_nand4_shared,
};

static ScfEops __ttl_if_ops =
{
	__ttl_if_path_off,
	__ttl_if_shared,
};

static ScfEops __ttl_mla_ops =
{
	__ttl_mla_path_off,
	__ttl_mla_shared,
};

static ScfEops __ttl_adc_ops =
{
	__ttl_adc_path_off,
	__ttl_adc_shared,
};

static ScfEops __741_op_amp_ops =
{
	__741_op_amp_path_off,
	__741_op_amp_shared,
};

static ScfEdata  component_datas[] =
{
	{EDA_None,       0,                   0, 0, 0,    0,   0,   0, 0, 0,   NULL, NULL, NULL},
	{EDA_Battery,    0,                   0, 5, 0, 1e-9, 1e9,   0, 0, 0,   NULL, NULL, NULL},
	{EDA_Signal,     0,                   0, 0, 0, 1e-9, 1e9,   0, 0, 0,   NULL, NULL, NULL},

	{EDA_Resistor,   0,                   0, 0, 0,  1e4,   0,   0, 0, 0,   NULL, NULL, NULL},
	{EDA_Capacitor,  0,                   0, 0, 0,   10, 0.1,   0, 0, 0,   NULL, NULL, NULL},
	{EDA_Inductor,   0,                   0, 0, 0,   10,   0, 1e3, 0, 0,   NULL, NULL, NULL},

	{EDA_Diode,      0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__diode_ops, NULL, "./cpk/9013.txt"},
	{EDA_NPN,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__npn_ops,   NULL, "./cpk/9013.txt"},
	{EDA_PNP,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__pnp_ops,   NULL, "./cpk/9012.txt"},

	{EDA_NAND,       0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_nand_ops, "./cpk/ttl_nand.cpk", NULL},
	{EDA_NOT,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_not_ops,  "./cpk/ttl_not.cpk",  NULL},
	{EDA_AND,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_gate_ops, "./cpk/ttl_and.cpk",  NULL},
	{EDA_OR,         0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_gate_ops, "./cpk/ttl_or.cpk",   NULL},
	{EDA_NOR,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_gate_ops, "./cpk/ttl_nor.cpk",  NULL},
	{EDA_XOR,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_gate_ops, "./cpk/ttl_xor.cpk",  NULL},
	{EDA_DFF,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_gate_ops, "./cpk/ttl_dff.cpk",  NULL},

	{EDA_ADD,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_add_ops,  "./cpk/ttl_add.cpk",  NULL},
	{EDA_ADC,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_adc_ops,  "./cpk/ttl_adc.cpk",  NULL},

	{EDA_NAND4,      0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_nand4_ops,   "./cpk/ttl_nand4.cpk",   NULL},
	{EDA_AND2_OR,    0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_and2_or_ops, "./cpk/ttl_and2_or.cpk", NULL},
	{EDA_IF,         0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_if_ops,      "./cpk/ttl_if.cpk",      NULL},
	{EDA_MLA,        0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_mla_ops,     "./cpk/ttl_mla.cpk",     NULL},

	{EDA_NOT,        EDA_TTL_DELAY,   0, 0, 0,    0,   0,   0, 0, 0,   &__ttl_not_ops,     "./cpk/ttl_not_delay.cpk", NULL},

	{EDA_OP_AMP,     0,                   0, 0, 0,    0,   0,   0, 0, 0,   &__741_op_amp_ops, "./cpk/op_amp_f007.cpk",   NULL},

	{EDA_Crystal,    0,                   0, 0, 0,    0,   0,   0, 0, 1e7, NULL,              "./cpk/crystal.cpk",       NULL},
};

static ScfEdata  pin_datas[] =
{
	{EDA_None,       0,                   0, 0, 0,    0,   0,   0,   0, 0, NULL, NULL, NULL},

	{EDA_Diode,      0,   EDA_Diode_NEG, 0, 0,  750,   0,   0,   0, 0, NULL, NULL, NULL},

	{EDA_NPN,        0,       EDA_NPN_B, 0, 0,  750,   0,   0,   0, 0, NULL, NULL, NULL},
	{EDA_NPN,        0,       EDA_NPN_C, 0, 0,    3,   0,   0, 200, 0, NULL, NULL, NULL},

	{EDA_PNP,        0,       EDA_PNP_B, 0, 0,  750,   0,   0,   0, 0, NULL, NULL, NULL},
	{EDA_PNP,        0,       EDA_PNP_C, 0, 0,    3,   0,   0, 200, 0, NULL, NULL, NULL},
};

static ScfEdata* _pin_find_data(const uint64_t type, const uint64_t model, const uint64_t pid)
{
	ScfEdata* ed;

	int i;
	for (i = 0; i < sizeof(pin_datas) / sizeof(pin_datas[0]); i++) {
		ed =              &pin_datas[i];

		if (ed->type == type && ed->model == model && ed->pid == pid)
			return ed;
	}

	return NULL;
}

ScfEdata* ecomponent__find_data(const uint64_t type, const uint64_t model)
{
	ScfEdata* ed;

	int i;
	for (i = 0; i < sizeof(component_datas) / sizeof(component_datas[0]); i++) {
		ed =              &component_datas[i];

		if (ed->type == type && ed->model == model)
			return ed;
	}

	return NULL;
}

ScfEconn* econn__alloc()
{
	ScfEconn* ec = calloc(1, sizeof(ScfEconn));

	return ec;
}

int econn__add_cid(ScfEconn* ec, uint64_t cid)
{
	if (!ec)
		return -EINVAL;

	for (size_t i = 0; i < ec->n_cids; i++) {

		if (ec->cids[i] == cid)
			return 0;
	}

	uint64_t* p = realloc(ec->cids, sizeof(uint64_t) * (ec->n_cids + 1));
	if (!p)
		return -ENOMEM;

	ec->cids = p;
	ec->cids[ec->n_cids++] = cid;
	return 0;
}

int econn__del_cid(ScfEconn* ec, uint64_t cid)
{
	if (!ec)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i < ec->n_cids; i++) {

		if (ec->cids[i] == cid) {

			for (j = i + 1;  j  < ec->n_cids; j++)
				ec->cids[j - 1] = ec->cids[j];

			ec->n_cids--;

			p = realloc(ec->cids, sizeof(uint64_t) * ec->n_cids);
			if (!p && ec->n_cids)
				return -ENOMEM;

			ec->cids = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEline* eline__alloc()
{
	ScfEline* l = calloc(1, sizeof(ScfEline));

	return l;
}

int eline__add_line(ScfEline* el, ScfLine*  l)
{
	if (!el || !l)
		return -EINVAL;

	for (long i = 0; i < el->n_lines; i++) {

		if (el->lines[i] == l)
			return 0;
	}

	void* p = realloc(el->lines, sizeof(ScfLine*) * (el->n_lines + 1));
	if (!p)
		return -ENOMEM;

	el->lines = p;
	el->lines[el->n_lines++] = l;
	return 0;
}

int eline__del_line(ScfEline* el, ScfLine* l)
{
	if (!el || !l)
		return -EINVAL;

	void*  p;
	long   i;
	long   j;

	for (i = 0; i < el->n_lines; i++) {

		if (el->lines[i] == l) {

			for (j = i + 1;  j  < el->n_lines; j++)
				el->lines[j - 1] = el->lines[j];

			el->n_lines--;

			p = realloc(el->lines, sizeof(ScfLine*) * el->n_lines);
			if (!p && el->n_lines > 0)
				return -ENOMEM;

			el->lines = p;
			return 0;
		}
	}

	return -EINVAL;
}

int eline__add_pin(ScfEline* el, uint64_t  cid, uint64_t pid)
{
	if (!el)
		return -EINVAL;

	for (size_t i = 0; i + 1 < el->n_pins; i += 2) {

		if (el->pins[i] == cid && el->pins[i + 1] == pid)
			return 0;
	}

	uint64_t* p = realloc(el->pins, sizeof(uint64_t) * (el->n_pins + 2));
	if (!p)
		return -ENOMEM;

	el->pins = p;
	el->pins[el->n_pins++] = cid;
	el->pins[el->n_pins++] = pid;
	return 0;
}

int eline__del_pin(ScfEline* el, uint64_t  cid, uint64_t pid)
{
	if (!el)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i + 1 < el->n_pins; i += 2) {

		if (el->pins[i] == cid && el->pins[i + 1] == pid) {

			for (j = i + 2;  j  < el->n_pins; j++)
				el->pins[j - 2] = el->pins[j];

			el->n_pins -= 2;

			p = realloc(el->pins, sizeof(uint64_t) * el->n_pins);
			if (!p && el->n_pins > 0)
				return -EINVAL;

			el->pins = p;
			return 0;
		}
	}

	return -EINVAL;
}

int eline__add_conn(ScfEline* el, ScfEconn* ec)
{
	if (!el || !ec)
		return -EINVAL;

	for (size_t i = 0; i < el->n_conns; i++) {

		if (el->conns[i] == ec)
			return 0;
	}

	void* p = realloc(el->conns, sizeof(ScfEconn*) * (el->n_conns + 1));
	if (!p)
		return -ENOMEM;

	el->conns = p;
	el->conns[el->n_conns++] = ec;
	return 0;
}

int eline__del_conn(ScfEline* el, ScfEconn* ec)
{
	if (!el || !ec)
		return -EINVAL;

	void*   p;
	size_t  i;
	size_t  j;

	for (i = 0; i < el->n_conns; i++) {

		if (el->conns[i] == ec) {

			for (j = i + 1;  j  < el->n_conns; j++)
				el->conns[j - 1] = el->conns[j];

			el->n_conns--;

			p = realloc(el->conns, sizeof(ScfEconn*) * el->n_conns);
			if (!p && el->n_conns > 0)
				return -EINVAL;

			el->conns = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEpin* epin__alloc()
{
	ScfEpin* p = calloc(1, sizeof(ScfEpin));
	if (p)
		p->lid = -1;

	return p;
}

int epin__add_component(ScfEpin* pin, uint64_t cid, uint64_t pid)
{
	if (!pin)
		return -EINVAL;

	for (size_t i = 0; i + 1 < pin->n_tos; i += 2) {

		if (pin->tos[i] == cid && pin->tos[i + 1] == pid)
			return 0;
	}

	uint64_t* p = realloc(pin->tos, sizeof(uint64_t) * (pin->n_tos + 2));
	if (!p)
		return -ENOMEM;

	pin->tos = p;
	pin->tos[pin->n_tos++] = cid;
	pin->tos[pin->n_tos++] = pid;
	return 0;
}

int epin__del_component(ScfEpin* pin, uint64_t cid, uint64_t pid)
{
	if (!pin)
		return -EINVAL;

	uint64_t* p;
	size_t    i;
	size_t    j;

	for (i = 0; i + 1 < pin->n_tos; i += 2) {

		if (pin->tos[i] == cid && pin->tos[i + 1] == pid) {

			for (j = i + 2;  j  < pin->n_tos; j++)
				pin->tos[j - 2] = pin->tos[j];

			pin->n_tos -= 2;

			p = realloc(pin->tos, sizeof(uint64_t) * pin->n_tos);
			if (!p && pin->n_tos > 0)
				return -ENOMEM;

			pin->tos = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEcomponent* ecomponent__alloc(uint64_t type)
{
	ScfEcomponent* c;
	ScfEdata*      ed;

	if (type >= EDA_Components_NB)
		return NULL;

	c = calloc(1, sizeof(ScfEcomponent));
	if (!c)
		return NULL;

	c->type      = type;
	c->mirror_id = -1;

	ed = ecomponent__find_data(c->type, c->model);
	if (ed) {
		c->v   = ed->v;
		c->a   = ed->a;
		c->r   = ed->r;
		c->uf  = ed->uf;
		c->uh  = ed->uh;
		c->Hz  = ed->Hz;
		c->ops = ed->ops;
	}

	int i;
	for (i = 0; i < component_pins[type]; i++) {

		ScfEpin* pin = epin__alloc();
		if (!pin) {
			ScfEcomponent_free(c);
			return NULL;
		}

		pin->id     = i;
		pin->ic_lid = -1;

		logd("pin %p, id: %ld, ic_lid: %ld\n", pin, pin->id, pin->ic_lid);

		if (ecomponent__add_pin(c, pin) < 0) {
			ScfEcomponent_free(c);
			ScfEpin_free(pin);
			return NULL;
		}

		ed = _pin_find_data(c->type, c->model, pin->id);
		if (ed) {
			pin->v   = ed->v;
			pin->a   = ed->a;
			pin->r   = ed->r;
			pin->uf  = ed->uf;
			pin->uh  = ed->uh;
			pin->hfe = ed->hfe;
		}
	}

	return c;
}

int ecomponent__add_curve(ScfEcomponent* c, ScfEcurve* curve)
{
	if (!c || !curve)
		return -EINVAL;

	void* p = realloc(c->curves, sizeof(ScfEcurve*) * (c->n_curves + 1));
	if (!p)
		return -ENOMEM;

	c->curves = p;
	c->curves[c->n_curves++] = curve;
	return 0;
}

int ecomponent__add_pin(ScfEcomponent* c, ScfEpin* pin)
{
	if (!c || !pin)
		return -EINVAL;

	void* p = realloc(c->pins, sizeof(ScfEpin*) * (c->n_pins + 1));
	if (!p)
		return -ENOMEM;

	c->pins = p;
	c->pins[c->n_pins++] = pin;
	return 0;
}

int ecomponent__del_pin(ScfEcomponent* c, ScfEpin* pin)
{
	if (!c)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < c->n_pins; i++) {

		if (c->pins[i] == pin) {

			for (j = i + 1; j  < c->n_pins; j++)
				c->pins[j - 1] = c->pins[j];

			c->n_pins--;

			p = realloc(c->pins, sizeof(ScfEpin*) * c->n_pins);
			if (!p && c->n_pins > 0)
				return -ENOMEM;

			c->pins = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEfunction*  efunction__alloc(const char* name)
{
	ScfEfunction* f = calloc(1, sizeof(ScfEfunction));
	if (!f)
		return NULL;

	f->n_name = strlen(name) + 1;

	f->name = strdup(name);
	if (!f->name) {
		free(f);
		return NULL;
	}

	return f;
}

int efunction__add_component(ScfEfunction* f, ScfEcomponent* c)
{
	if (!f || !c)
		return -EINVAL;

	void* p = realloc(f->components, sizeof(ScfEcomponent*) * (f->n_components + 1));
	if (!p)
		return -ENOMEM;

	f->components = p;
	f->components[f->n_components++] = c;
	return 0;
}

static int __efunction__del_component(ScfEfunction* f, ScfEcomponent* c)
{
	ScfEcomponent* c2;
	ScfEline*      el;
	ScfEpin*       p;

	long  i;
	long  j;
	long  k;

	for (i = 0; i < f->n_elines; i++) {
		el =        f->elines[i];

		for (j = 0; j + 1 < el->n_pins; ) {

			if (el->pins[j] == c->id)
				assert(0 == eline__del_pin(el, c->id, el->pins[j + 1]));
			else {
				if (el->pins[j] > c->id)
					el->pins[j]--;

				j += 2;
			}
		}
	}

	for (i = 0; i < f->n_components; i++) {
		c2 =        f->components[i];

		if (c2 == c)
			continue;

		if (c2->id > c->id)
			c2->id--;

		for (j = 0; j < c2->n_pins; j++) {
			p  =        c2->pins[j];

			for (k = 0; k + 1 < p->n_tos; ) {

				if (p->tos[k] == c->id)
					assert(0 == epin__del_component(p, c->id, p->tos[k + 1]));
				else {
					if (p->tos[k] > c->id)
						p->tos[k]--;

					k += 2;
				}
			}

			if (p->cid > c->id)
				p->cid--;
		}
	}
}

int efunction__del_component(ScfEfunction* f, ScfEcomponent* c)
{
	if (!f || !c)
		return -EINVAL;

	void* p;
	long  i;
	long  j;

	for (i = 0; i < f->n_components; i++) {

		if (f->components[i] == c) {
			__efunction__del_component(f, c);

			for (j = i + 1;  j < f->n_components; j++)
				f->components[j - 1] = f->components[j];

			f->n_components--;

			p = realloc(f->components, sizeof(ScfEcomponent*) * f->n_components);
			if (!p && f->n_components > 0)
				return -EINVAL;

			f->components = p;
			return 0;
		}
	}

	return -EINVAL;
}

int efunction__add_eline(ScfEfunction* f, ScfEline* el)
{
	if (!f || !el)
		return -EINVAL;

	void* p = realloc(f->elines, sizeof(ScfEline*) * (f->n_elines + 1));
	if (!p)
		return -ENOMEM;

	f->elines = p;
	f->elines[f->n_elines++] = el;
	return 0;
}

int efunction__del_eline(ScfEfunction* f, ScfEline* el)
{
	if (!f || !el)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < f->n_elines; i++) {

		if (f->elines[i] == el) {

			for (j = i + 1;  j < f->n_elines; j++)
				f->elines[j - 1] = f->elines[j];

			f->n_elines--;

			p = realloc(f->elines, sizeof(ScfEline*) * f->n_elines);
			if (!p && f->n_elines > 0)
				return -ENOMEM;

			f->elines = p;
			return 0;
		}
	}

	return -EINVAL;
}

ScfEboard* eboard__alloc()
{
	ScfEboard* b = calloc(1, sizeof(ScfEboard));

	return b;
}

int eboard__add_function(ScfEboard* b, ScfEfunction* f)
{
	if (!b || !f)
		return -EINVAL;

	void* p = realloc(b->functions, sizeof(ScfEfunction*) * (b->n_functions + 1));
	if (!p)
		return -ENOMEM;

	b->functions = p;
	b->functions[b->n_functions++] = f;
	return 0;
}

int eboard__del_function(ScfEboard* b, ScfEfunction* f)
{
	if (!b)
		return -EINVAL;

	size_t   i;
	size_t   j;
	void*    p;

	for (i = 0; i < b->n_functions; i++) {

		if (b->functions[i] == f) {

			for (j = i + 1;  j < b->n_functions; j++)
				b->functions[j - 1] = b->functions[j];

			b->n_functions--;

			p = realloc(b->functions, sizeof(ScfEfunction*) * b->n_functions);
			if (!p && b->n_functions > 0)
				return -ENOMEM;

			b->functions = p;
			return 0;
		}
	}

	return -EINVAL;
}

static int epin_cmp(const void* v0, const void* v1)
{
	const uint64_t* t0 = v0;
	const uint64_t* t1 = v1;

	if (t0[0] < t1[0])
		return -1;

	if (t0[0] > t1[0])
		return 1;

	if (t0[1] < t1[1])
		return -1;

	if (t0[1] > t1[1])
		return 1;
	return 0;
}

int pins_same_line(ScfEfunction* f)
{
	ScfEcomponent* c;
	ScfEline*      el;
	ScfEline*      el2;
	ScfEpin*       p;
	ScfEpin*       p2;

	long i;
	long j;
	long k;
	long m;
	long n;

	for (i = 0; i < f->n_components; i++) {
		c         = f->components[i];
		c->pf     = f;

		for (j = 0; j < c->n_pins; j++) {
			p         = c->pins[j];
			p->c      = c;

			qsort(p->tos, p->n_tos / 2, sizeof(uint64_t) * 2, epin_cmp);

			for (k = 0; k + 1 < p->n_tos; k += 2) {
				if (p->tos[k] == c->id)
					logw("c%ldp%ld connect to its own pin %ld\n", c->id, p->id, p->tos[k + 1]);
			}

			for (k = 0; k < f->n_elines; k++) {
				el        = f->elines[k];

				for (m = 0; m + 1 < el->n_pins; m += 2) {

					if (el->pins[m] == p->cid && el->pins[m + 1] == p->id)
						goto next;
				}

				m = 0;
				n = 0;
				while (m + 1 < el->n_pins && n + 1 < p->n_tos) {

					if (el->pins[m] < p->tos[n])
						m += 2;
					else if (el->pins[m] > p->tos[n])
						n += 2;

					else if (el->pins[m + 1] < p->tos[n + 1])
						m += 2;
					else if (el->pins[m + 1] > p->tos[n + 1])
						n += 2;

					else {
						if (eline__add_pin(el, p->cid, p->id) < 0)
							return -ENOMEM;

						p ->lid    = el->id;
						p ->c_lid  = el->id;
						el->flags |= p->flags;

						if (p->flags & (EDA_PIN_IN | EDA_PIN_OUT | EDA_PIN_SHIFT))
							el->io_lid = p->io_lid;
						goto next;
					}
				}
			}

			el = eline__alloc();
			if (!el)
				return -ENOMEM;
			el->id = f->n_elines;

			if (efunction__add_eline(f, el) < 0) {
				ScfEline_free(el);
				return -ENOMEM;
			}

			if (eline__add_pin(el, p->cid, p->id) < 0)
				return -ENOMEM;

			p ->lid    = el->id;
			p ->c_lid  = el->id;
			el->flags |= p->flags;

			if (p->flags & (EDA_PIN_IN | EDA_PIN_OUT | EDA_PIN_SHIFT))
				el->io_lid = p->io_lid;
next:
			for (n = 0; n + 1 < p->n_tos; n += 2) {

				p2 = f->components[p->tos[n]]->pins[p->tos[n + 1]];

				if (p2->cid >  p->cid
				|| (p2->cid == p->cid && p2->id > p->id))
					break;

				el2 = f->elines[p2->lid];

				if (el2 == el)
					continue;

				if (el2->id < el->id)
					XCHG(el2, el);

				for (m = 0; m + 1 < el2->n_pins; m += 2) {
					p2 = f->components[el2->pins[m]]->pins[el2->pins[m + 1]];

					if (eline__add_pin(el, p2->cid, p2->id) < 0)
						return -ENOMEM;

					p2->lid    = el->id;
					p2->c_lid  = el->id;
					el->flags |= p2->flags;

					if (p2->flags & (EDA_PIN_IN | EDA_PIN_OUT | EDA_PIN_SHIFT))
						el->io_lid = p2->io_lid;
				}

				qsort(el->pins, el->n_pins / 2, sizeof(uint64_t) * 2, epin_cmp);

				el2->n_pins = 0;
			}
			p = NULL;
		}
	}

	for (i = 0; i < f->n_elines; ) {
		el        = f->elines[i];

		if (0 == el->n_pins) {
			efunction__del_eline(f, el);
			ScfEline_free(el);
			continue;
		}

		el->pf = f;
		el->c_pins = el->n_pins;
		i++;
	}

	for (i = 0; i < f->n_elines; ) {
		el        = f->elines[i];
		el->id    = i;

		int64_t eid = -1;

		for (j = 0; j + 1 < el->n_pins; j += 2) {

			c  = f->components[el->pins[j]];
			p  = c->pins      [el->pins[j + 1]];

			p->lid   = i;
			p->c_lid = i;

			if (p->ic_lid < 0)
				continue;

			if (eid < 0)
				eid = p->ic_lid;

			else if (eid != p->ic_lid) {
				loge("IC pin number set error, prev: %ld, current: %ld\n", eid, p->ic_lid);
				return -EINVAL;
			}

			logd("pin j: %ld, c%ldp%ld\n", j, el->pins[j], el->pins[j + 1]);
		}

		if (eid >= f->n_elines) {
			loge("IC pin number set error, max: %ld, current: %ld\n", f->n_elines - 1, eid);
			return -EINVAL;
		}

		logd("i: %ld, eid: %ld\n", i, eid);

		if (eid >= 0 && eid != i) {
			el->id = eid;

			for (j = 0; j + 1 < el->n_pins; j += 2) {

				c  = f->components[el->pins[j]];
				p  = c->pins      [el->pins[j + 1]];

				p->lid   = eid;
				p->c_lid = eid;
			}

			XCHG(f->elines[eid], f->elines[i]);
			continue;
		}

		i++;
	}

	return 0;
}

void efunction_find_pin(ScfEfunction* f, ScfEcomponent** pc, ScfEpin** pp, int x, int y)
{
	ScfEcomponent*  c;
	ScfEpin*        p;

	long i;
	long j;

	for (i = 0; i < f->n_components; i++) {
		c  =        f->components[i];

		logd("c%ld, c->x: %d, c->y: %d, c->w: %d, c->h: %d, x: %d, y: %d\n", c->id, c->x, c->y, c->w, c->y, x, y);
		if (x > c->x - c->w / 2
				&& x < c->x + c->w / 2
				&& y > c->y - c->h / 2
				&& y < c->y + c->h / 2) {
			*pc = c;
			*pp = NULL;

			logd("c%ld, %d,%d, x: %d, y: %d\n", c->id, c->x, c->y, x, y);
			return;
		}

		for (j = 0; j < c->n_pins; j++) {
			p  =        c->pins[j];

			if (x > p->x - 5
					&& x < p->x + 5
					&& y > p->y - 5
					&& y < p->y + 5) {
				*pc = c;
				*pp = p;

				logi("c%ldp%ld, %d,%d, x: %d, y: %d\n", c->id, p->id, p->x, p->y, x, y);
				return;
			}
		}
	}
}

int epin_in_line(int px, int py, int x0, int y0, int x1, int y1)
{
	if (x0 == x1) {
		if (x0 - 5 < px && px < x0 + 5) {

			if (y0 > y1)
				XCHG(y0, y1);

			if (y0 - 5 < py && py < y1 + 5)
				return 1;
		}
	} else if (y0 == y1) {
		if (y0 - 5 < py && py < y0 + 5) {

			if (x0 > x1)
				XCHG(x0, x1);

			if (x0 - 5 < px && px < x1 + 5)
				return 1;
		}
	} else {
		if (x0 > x1) {
			XCHG(x0, x1);
			XCHG(y0, y1);
		}

		double Y0 = y0;
		double Y1 = y1;

		if (y0 > y1) {
			Y0 = y1;
			Y1 = y0;
		}

		double k = (y1 - y0) / (double)(x1 - x0);
		double x = (py - y0  + px / k + x0 * k) / (k + 1.0 / k);
		double y = (x  - x0) * k  + y0;

		double dx = px - x;
		double dy = py - y;
		double d  = sqrt(dx * dx + dy * dy);

		if (x > x0 - 5
				&& x < x1 + 5
				&& y > Y0 - 5
				&& y < Y1 + 5
				&& d < 5) {
			logi("--- k: %lg, px: %d, py: %d, x: %lg, y: %lg, d: %lg, (%d,%d)-->(%d,%d)\n", k, px, py, x, y, d, x0, y0, x1, y1);
			return 1;
		}
	}

	return 0;
}

void efunction_find_eline(ScfEfunction* f, ScfEline** pel, ScfLine** pl, int x, int y)
{
	ScfEline*  el;
	ScfLine*   l;

	long i;
	long j;

	for (i = 0; i < f->n_elines; i++) {
		el =        f->elines[i];

		for (j = el->n_lines - 1; j >= 0; j--) {
			l  = el->lines[j];

			if (epin_in_line(x, y, l->x0, l->y0, l->x1, l->y1)) {

				logi("j: %ld, (%d, %d)-->(%d, %d), x: %d, y: %d\n", j, l->x0, l->y0, l->x1, l->y1, x, y);

				*pel = el;
				*pl  = l;
				return;
			}
		}
	}
}

long find_eline_index(ScfEfunction* f, int64_t lid)
{
	ScfEline* el;
	long      j;

	for (j = 0; j < f->n_elines; j++) {
		el        = f->elines[j];

		if (el->id == lid)
			return j;
	}

	return -1;
}
