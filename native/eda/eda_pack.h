#ifndef EDA_PACK_H
#define EDA_PACK_H

#include "pack.h"

enum {
	EDA_None,
	EDA_Battery,

	EDA_Resistor,
	EDA_Capacitor,
	EDA_Inductor,

	EDA_Diode,
	EDA_NPN,
	EDA_PNP,

	EDA_NAND,
	EDA_NOR,
	EDA_NOT,

	EDA_AND,
	EDA_OR,
	EDA_XOR,

	EDA_ADD,

	EDA_NAND4,
	EDA_AND2_OR,
	EDA_IF,
	EDA_MLA,

	EDA_ADC,

	EDA_DFF, // D flip flop

	EDA_Signal,

	EDA_OP_AMP,

	EDA_Crystal,

	EDA_Components_NB,
};

#define EDA_PIN_NONE   0
#define EDA_PIN_IN     1
#define EDA_PIN_OUT    2
#define EDA_PIN_POS    4
#define EDA_PIN_NEG    8
#define EDA_PIN_CF     16
#define EDA_PIN_BORDER 32
#define EDA_PIN_SHIFT  64
#define EDA_PIN_IN0    128
#define EDA_PIN_DIV0   256
#define EDA_PIN_KEY    512
#define EDA_PIN_DELAY  1024
#define EDA_PIN_CK     2048
#define EDA_PIN_GND    4096

#define EDA_V_INIT   -10001001.0
#define EDA_V_MIN    -10000000.0
#define EDA_V_MAX     10000000.0

#define EDA_V_Diode_ON  0.58
#define EDA_V_Diode_OFF 0.55

#define EDA_V_NPN_ON    0.70
#define EDA_V_NPN_OFF   0.61

#define EDA_V_PNP_ON    EDA_V_NPN_ON
#define EDA_V_PNP_OFF   EDA_V_NPN_OFF

enum {
	EDA_Battery_NEG,
	EDA_Battery_POS,
	EDA_Battery_NB,
};

#define EDA_Signal_SIN  0
#define EDA_Signal_DC   1
enum {
	EDA_Signal_NEG,
	EDA_Signal_POS,
	EDA_Signal_NB,
};

enum {
	EDA_Diode_NEG,
	EDA_Diode_POS,
	EDA_Diode_NB,
};

enum {
	EDA_Status_ON,
	EDA_Status_OFF,
	EDA_Path_OFF,
	EDA_Path_TO,
};

enum {
	EDA_NPN_B,
	EDA_NPN_E,
	EDA_NPN_C,
	EDA_NPN_NB,
};

enum {
	EDA_PNP_B  = EDA_NPN_B,
	EDA_PNP_E  = EDA_NPN_E,
	EDA_PNP_C  = EDA_NPN_C,
	EDA_PNP_NB = EDA_NPN_NB,
};

#define EDA_TTL_DELAY 1
enum {
	EDA_NOT_NEG,
	EDA_NOT_POS,

	EDA_NOT_IN,
	EDA_NOT_OUT,

	EDA_NOT_NB,
};

enum {
	EDA_NAND_NEG,
	EDA_NAND_POS,

	EDA_NAND_IN0,
	EDA_NAND_IN1,
	EDA_NAND_OUT,

	EDA_NAND_NB,
};

enum {
	EDA_NOR_NEG = EDA_NAND_NEG,
	EDA_NOR_POS = EDA_NAND_POS,

	EDA_NOR_IN0 = EDA_NAND_IN0,
	EDA_NOR_IN1 = EDA_NAND_IN1,
	EDA_NOR_OUT = EDA_NAND_OUT,

	EDA_NOR_NB,
};

enum {
	EDA_AND_NEG = EDA_NAND_NEG,
	EDA_AND_POS = EDA_NAND_POS,

	EDA_AND_IN0 = EDA_NAND_IN0,
	EDA_AND_IN1 = EDA_NAND_IN1,
	EDA_AND_OUT = EDA_NAND_OUT,

	EDA_AND_NB,
};

enum {
	EDA_OR_NEG = EDA_NAND_NEG,
	EDA_OR_POS = EDA_NAND_POS,

	EDA_OR_IN0 = EDA_NAND_IN0,
	EDA_OR_IN1 = EDA_NAND_IN1,
	EDA_OR_OUT = EDA_NAND_OUT,

	EDA_OR_NB,
};

enum {
	EDA_XOR_NEG = EDA_NAND_NEG,
	EDA_XOR_POS = EDA_NAND_POS,

	EDA_XOR_IN0 = EDA_NAND_IN0,
	EDA_XOR_IN1 = EDA_NAND_IN1,
	EDA_XOR_OUT = EDA_NAND_OUT,

	EDA_XOR_NB,
};

enum {
	EDA_OP_AMP_NEG = EDA_NAND_NEG,
	EDA_OP_AMP_POS = EDA_NAND_POS,

	EDA_OP_AMP_IN     = EDA_NAND_IN0,
	EDA_OP_AMP_INVERT = EDA_NAND_IN1,
	EDA_OP_AMP_OUT    = EDA_NAND_OUT,

	EDA_OP_AMP_NB,
};

enum {
	EDA_DFF_NEG = EDA_NAND_NEG,
	EDA_DFF_POS = EDA_NAND_POS,

	EDA_DFF_IN  = EDA_NAND_IN0,
	EDA_DFF_CK  = EDA_NAND_IN1,
	EDA_DFF_OUT = EDA_NAND_OUT,

	EDA_DFF_NB,
};

enum {
	EDA_ADD_NEG = EDA_NAND_NEG,
	EDA_ADD_POS = EDA_NAND_POS,

	EDA_ADD_IN0 = EDA_NAND_IN0,
	EDA_ADD_IN1 = EDA_NAND_IN1,
	EDA_ADD_OUT = EDA_NAND_OUT,
	EDA_ADD_CF,

	EDA_ADD_NB,
};

enum {
	EDA_ADC_NEG,
	EDA_ADC_POS,

	EDA_ADC_CI,
	EDA_ADC_IN0,
	EDA_ADC_IN1,
	EDA_ADC_OUT,
	EDA_ADC_CF,

	EDA_ADC_NB,
};

enum {
	EDA_NAND4_NEG,
	EDA_NAND4_POS,

	EDA_NAND4_IN0,
	EDA_NAND4_IN1,
	EDA_NAND4_IN2,
	EDA_NAND4_IN3,
	EDA_NAND4_OUT,

	EDA_NAND4_NB,
};

enum {
	EDA_AND2_OR_NEG = EDA_NAND4_NEG,
	EDA_AND2_OR_POS = EDA_NAND4_POS,

	EDA_AND2_OR_IN0 = EDA_NAND4_IN0,
	EDA_AND2_OR_IN1 = EDA_NAND4_IN1,
	EDA_AND2_OR_IN2 = EDA_NAND4_IN2,
	EDA_AND2_OR_IN3 = EDA_NAND4_IN3,
	EDA_AND2_OR_OUT = EDA_NAND4_OUT,

	EDA_AND2_OR_NB,
};

enum {
	EDA_IF_NEG,
	EDA_IF_POS,

	EDA_IF_TRUE,
	EDA_IF_COND,
	EDA_IF_FALSE,
	EDA_IF_OUT,

	EDA_IF_NB,
};

enum {
	EDA_MLA_NEG,
	EDA_MLA_POS,

	EDA_MLA_IN0,
	EDA_MLA_IN1,
	EDA_MLA_IN2,
	EDA_MLA_IN3,

	EDA_MLA_OUT,
	EDA_MLA_CF,

	EDA_MLA_NB,
};

typedef struct {
	uint64_t  type;
	uint64_t  model;
	uint64_t  pid;

	double    v;
	double    a;
	double    r;

	double    uf;
	double    uh;
	double    hfe;
	double    Hz;

	void*     ops;
	char*     cpk;
	char*     va_curve;
} ScfEdata;

typedef struct {
	PACK_DEF_VAR(double, Vb);
	PACK_DEF_VAR(double, Ib);
	PACK_DEF_VAR(double, Vc);
} ScfEcurve;

PACK_TYPE(ScfEcurve)
PACK_INFO_VAR(ScfEcurve, Vb),
PACK_INFO_VAR(ScfEcurve, Ib),
PACK_INFO_VAR(ScfEcurve, Vc),
PACK_END(ScfEcurve)

typedef struct eops_s        ScfEops;
typedef struct epin_s        ScfEpin;
typedef struct eline_s       ScfEline;
typedef struct ecomponent_s  ScfEcomponent;
typedef struct efunction_s   ScfEfunction;
typedef struct eboard_s      ScfEboard;

struct eops_s
{
	int (*off   )(ScfEpin* p0, ScfEpin* p1, int flags);

	int (*shared)(ScfEpin* p,  int flags);
};

typedef struct {
	PACK_DEF_VAR(int, x0);
	PACK_DEF_VAR(int, y0);
	PACK_DEF_VAR(int, x1);
	PACK_DEF_VAR(int, y1);
	PACK_DEF_OBJ(ScfEline, el);
} ScfLine;

PACK_TYPE(ScfLine)
PACK_INFO_VAR(ScfLine, x0),
PACK_INFO_VAR(ScfLine, y0),
PACK_INFO_VAR(ScfLine, x1),
PACK_INFO_VAR(ScfLine, y1),
PACK_END(ScfLine)

struct epin_s
{
	PACK_DEF_VAR(uint64_t, id);
	PACK_DEF_VAR(uint64_t, cid);
	PACK_DEF_VAR(int64_t,  lid);
	PACK_DEF_VAR(uint64_t, flags);
	PACK_DEF_VARS(uint64_t, tos);
	PACK_DEF_VAR(uint64_t, c_lid);

	PACK_DEF_VAR(int64_t, io_lid);

	PACK_DEF_VAR(int64_t, ic_lid);
	PACK_DEF_OBJ(ScfEcomponent, c);

	PACK_DEF_VAR(double, v);
	PACK_DEF_VAR(double, a);

	PACK_DEF_VAR(double, r);
	PACK_DEF_VAR(double, uf);
	PACK_DEF_VAR(double, uh);
	PACK_DEF_VAR(double, hfe);

	PACK_DEF_VAR(double, dr);

	PACK_DEF_VAR(double, sr);
	PACK_DEF_VAR(double, pr);

	PACK_DEF_VAR(uint64_t, path);

	PACK_DEF_VAR(int, x);
	PACK_DEF_VAR(int, y);

	PACK_DEF_VAR(int, n_diodes);

	PACK_DEF_VAR(uint8_t, vflag);
	PACK_DEF_VAR(uint8_t, pflag);
	PACK_DEF_VAR(uint8_t, vconst);
	PACK_DEF_VAR(uint8_t, aconst);
};

PACK_TYPE(ScfEpin)
PACK_INFO_VAR(ScfEpin, id),
PACK_INFO_VAR(ScfEpin, cid),
PACK_INFO_VAR(ScfEpin, lid),
PACK_INFO_VAR(ScfEpin, flags),
PACK_INFO_VARS(ScfEpin, tos, uint64_t),
PACK_INFO_VAR(ScfEpin, c_lid),

PACK_INFO_VAR(ScfEpin, io_lid),
PACK_INFO_VAR(ScfEpin, ic_lid),

PACK_INFO_VAR(ScfEpin, v),
PACK_INFO_VAR(ScfEpin, a),

PACK_INFO_VAR(ScfEpin, r),
PACK_INFO_VAR(ScfEpin, uf),
PACK_INFO_VAR(ScfEpin, uh),
PACK_INFO_VAR(ScfEpin, hfe),

PACK_INFO_VAR(ScfEpin, dr),
PACK_INFO_VAR(ScfEpin, sr),
PACK_INFO_VAR(ScfEpin, pr),

PACK_INFO_VAR(ScfEpin, path),
PACK_INFO_VAR(ScfEpin, x),
PACK_INFO_VAR(ScfEpin, y),
PACK_INFO_VAR(ScfEpin, n_diodes),

PACK_INFO_VAR(ScfEpin, vflag),
PACK_INFO_VAR(ScfEpin, pflag),
PACK_INFO_VAR(ScfEpin, vconst),
PACK_INFO_VAR(ScfEpin, aconst),
PACK_END(ScfEpin)

typedef struct {
	PACK_DEF_VAR(uint64_t, lid);
	PACK_DEF_VARS(uint64_t, cids);
} ScfEconn;

PACK_TYPE(ScfEconn)
PACK_INFO_VAR(ScfEconn, lid),
PACK_INFO_VARS(ScfEconn, cids, uint64_t),
PACK_END(ScfEconn)

struct eline_s
{
	PACK_DEF_VAR(uint64_t, id);
	PACK_DEF_VARS(uint64_t, pins);
	PACK_DEF_VAR(uint64_t, c_pins);
	PACK_DEF_VAR(uint64_t, flags);
	PACK_DEF_VAR(int64_t, color);
	PACK_DEF_VAR(int64_t, io_lid);

	PACK_DEF_OBJ(ScfEfunction, pf);

	PACK_DEF_OBJS(ScfEconn, conns);
	PACK_DEF_OBJS(ScfLine, lines);

	PACK_DEF_VAR(double, v);
	PACK_DEF_VAR(double, ain);
	PACK_DEF_VAR(double, aout);
	PACK_DEF_VAR(uint8_t, vconst);
	PACK_DEF_VAR(uint8_t, aconst);
	PACK_DEF_VAR(uint8_t, vflag);
	PACK_DEF_VAR(uint8_t, open_flag);
};

PACK_TYPE(ScfEline)
PACK_INFO_VAR(ScfEline, id),
PACK_INFO_VARS(ScfEline, pins, uint64_t),
PACK_INFO_VAR(ScfEline, c_pins),
PACK_INFO_VAR(ScfEline, flags),
PACK_INFO_VAR(ScfEline, color),
PACK_INFO_VAR(ScfEline, io_lid),

PACK_INFO_OBJS(ScfEline, conns, ScfEconn),
PACK_INFO_OBJS(ScfEline, lines, ScfLine),

PACK_INFO_VAR(ScfEline, v),
PACK_INFO_VAR(ScfEline, ain),
PACK_INFO_VAR(ScfEline, aout),
PACK_INFO_VAR(ScfEline, vconst),
PACK_INFO_VAR(ScfEline, aconst),
PACK_INFO_VAR(ScfEline, vflag),
PACK_END(ScfEline)

struct ecomponent_s
{
	PACK_DEF_VAR(uint64_t, id);
	PACK_DEF_VAR(uint64_t, type);
	PACK_DEF_VAR(uint64_t, model);
	PACK_DEF_OBJS(ScfEpin, pins);

	PACK_DEF_OBJS(ScfEcurve, curves);

	PACK_DEF_OBJ(ScfEfunction, pf);
	PACK_DEF_OBJ(ScfEfunction, f);
	PACK_DEF_OBJ(ScfEops,      ops);

	PACK_DEF_VAR(double, v);
	PACK_DEF_VAR(double, a);

	PACK_DEF_VAR(double, dr);

	PACK_DEF_VAR(double, r);
	PACK_DEF_VAR(double, uf);
	PACK_DEF_VAR(double, uh);

	PACK_DEF_VAR(double, Hz);

	PACK_DEF_VAR(int64_t, mirror_id);

	PACK_DEF_VAR(int64_t, count);
	PACK_DEF_VAR(int64_t, color);
	PACK_DEF_VAR(int, status);
	PACK_DEF_VAR(int, x);
	PACK_DEF_VAR(int, y);
	PACK_DEF_VAR(int, w);
	PACK_DEF_VAR(int, h);
	PACK_DEF_VAR(uint8_t, vflag);
	PACK_DEF_VAR(uint8_t, lock);
};

PACK_TYPE(ScfEcomponent)
PACK_INFO_VAR(ScfEcomponent, id),
PACK_INFO_VAR(ScfEcomponent, type),
PACK_INFO_VAR(ScfEcomponent, model),
PACK_INFO_OBJS(ScfEcomponent, pins,   ScfEpin),

PACK_INFO_OBJS(ScfEcomponent, curves, ScfEcurve),

PACK_INFO_VAR(ScfEcomponent, v),
PACK_INFO_VAR(ScfEcomponent, a),

PACK_INFO_VAR(ScfEcomponent, dr),

PACK_INFO_VAR(ScfEcomponent, r),
PACK_INFO_VAR(ScfEcomponent, uf),
PACK_INFO_VAR(ScfEcomponent, uh),
PACK_INFO_VAR(ScfEcomponent, Hz),

PACK_INFO_VAR(ScfEcomponent, mirror_id),

PACK_INFO_VAR(ScfEcomponent, count),
PACK_INFO_VAR(ScfEcomponent, color),
PACK_INFO_VAR(ScfEcomponent, status),
PACK_INFO_VAR(ScfEcomponent, x),
PACK_INFO_VAR(ScfEcomponent, y),
PACK_INFO_VAR(ScfEcomponent, w),
PACK_INFO_VAR(ScfEcomponent, h),
PACK_INFO_VAR(ScfEcomponent, vflag),
PACK_INFO_VAR(ScfEcomponent, lock),
PACK_END(ScfEcomponent)

struct efunction_s
{
	PACK_DEF_VARS(uint8_t, name);
	PACK_DEF_OBJS(ScfEcomponent, components);
	PACK_DEF_OBJS(ScfEline,      elines);

	PACK_DEF_OBJ(ScfEcomponent, IC);

	PACK_DEF_VAR(int, x);
	PACK_DEF_VAR(int, y);
	PACK_DEF_VAR(int, w);
	PACK_DEF_VAR(int, h);
};

PACK_TYPE(ScfEfunction)
PACK_INFO_VARS(ScfEfunction, name,       uint8_t),
PACK_INFO_OBJS(ScfEfunction, components, ScfEcomponent),
PACK_INFO_OBJS(ScfEfunction, elines,     ScfEline),
PACK_INFO_VAR(ScfEfunction,  x),
PACK_INFO_VAR(ScfEfunction,  y),
PACK_INFO_VAR(ScfEfunction,  w),
PACK_INFO_VAR(ScfEfunction,  h),
PACK_END(ScfEfunction)

struct eboard_s
{
	PACK_DEF_OBJS(ScfEfunction, functions);
};

PACK_TYPE(ScfEboard)
PACK_INFO_OBJS(ScfEboard, functions, ScfEfunction),
PACK_END(ScfEboard)


ScfEconn*      econn__alloc();
int            econn__add_cid(ScfEconn* ec, uint64_t  cid);
int            econn__del_cid(ScfEconn* ec, uint64_t  cid);

ScfEline*      eline__alloc();
int            eline__add_line(ScfEline* el, ScfLine*  l);
int            eline__del_line(ScfEline* el, ScfLine*  l);

int            eline__add_pin (ScfEline* el, uint64_t  cid, uint64_t pid);
int            eline__del_pin (ScfEline* el, uint64_t  cid, uint64_t pid);

int            eline__add_conn(ScfEline* el, ScfEconn* ec);
int            eline__del_conn(ScfEline* el, ScfEconn* ec);

ScfEpin*       epin__alloc();
int            epin__add_component(ScfEpin* pin, uint64_t cid, uint64_t pid);
int            epin__del_component(ScfEpin* pin, uint64_t cid, uint64_t pid);

ScfEcomponent* ecomponent__alloc(uint64_t type);
int            ecomponent__add_pin(ScfEcomponent* c, ScfEpin* pin);
int            ecomponent__del_pin(ScfEcomponent* c, ScfEpin* pin);
int            ecomponent__add_curve(ScfEcomponent* c, ScfEcurve* curve);
ScfEdata*      ecomponent__find_data(const uint64_t type, const uint64_t model);

ScfEfunction*  efunction__alloc        (const   char* name);
int            efunction__add_component(ScfEfunction* f, ScfEcomponent* c);
int            efunction__del_component(ScfEfunction* f, ScfEcomponent* c);
int            efunction__add_eline    (ScfEfunction* f, ScfEline* el);
int            efunction__del_eline    (ScfEfunction* f, ScfEline* el);

ScfEboard*     eboard__alloc();
int            eboard__add_function(ScfEboard* b, ScfEfunction* f);
int            eboard__del_function(ScfEboard* b, ScfEfunction* f);

int            pins_same_line  (ScfEfunction* f);

long           find_eline_index(ScfEfunction* f, int64_t lid);

void           efunction_find_pin  (ScfEfunction* f, ScfEcomponent** pc,  ScfEpin** pp, int x, int y);
void           efunction_find_eline(ScfEfunction* f, ScfEline**      pel, ScfLine** pl, int x, int y);

int            epin_in_line(int px, int py, int x0, int y0, int x1, int y1);


#define EDA_INST_ADD_COMPONENT(_ef, _c, _type) \
	do { \
		_c = ecomponent__alloc(_type); \
		if (!_c) \
			return -ENOMEM; \
		\
		(_c)->id = (_ef)->n_components; \
		\
		int ret = efunction__add_component(_ef, _c); \
		if (ret < 0) { \
			ScfEcomponent_free(_c); \
			_c = NULL; \
			return ret; \
		} \
		\
		for (long i = 0; i < (_c)->n_pins; i++) \
			(_c)->pins[i]->cid = (_c)->id; \
	} while (0)

#define EDA_PIN_ADD_COMPONENT(_pin, _cid, _pid) \
	do { \
		int ret = epin__add_component((_pin), _cid, _pid); \
		if (ret < 0) \
			return ret; \
	} while (0)

#define EDA_PIN_ADD_PIN(_c0, _pid0, _c1, _pid1) \
	do { \
		int ret = epin__add_component((_c0)->pins[_pid0], (_c1)->id, (_pid1)); \
		if (ret < 0) \
			return ret; \
		\
		ret = epin__add_component((_c1)->pins[_pid1], (_c0)->id, (_pid0)); \
		if (ret < 0) \
			return ret; \
	} while (0)

#define EDA_PIN_ADD_PIN_EF(_ef, _p0, _p1) \
	EDA_PIN_ADD_PIN((_ef)->components[(_p0)->cid], (_p0)->id, (_ef)->components[(_p1)->cid], (_p1)->id)

#define EDA_SET_MIRROR(_c0, _c1) \
	do { \
		(_c0)->mirror_id = (_c1)->id; \
		(_c1)->mirror_id = (_c0)->id; \
	} while (0)

static char* component_types[EDA_Components_NB] =
{
	"None",
	"Battery",

	"Resistor",
	"Capacitor",
	"Inductor",

	"Diode",
	"NPN",
	"PNP",

	"NAND",
	"NOR",
	"NOT",

	"AND",
	"OR",
	"XOR",

	"ADD",

	"NAND4",
	"AND2_OR",
	"IF",
	"MLA",
};
#endif
