#ifndef EDA_PB_H
#define EDA_PB_H

#include "eda.pb-c.h"

enum {
	EDA_None,
	EDA_Battery,

	EDA_Resistor,
	EDA_Capacitor,
	EDA_Inductor,

	EDA_Diode,
	EDA_NPN,
	EDA_PNP,

	EDA_Components_NB,
};

#define EDA_PIN_NONE  0
#define EDA_PIN_IN    1
#define EDA_PIN_OUT   2
#define EDA_PIN_POS   4
#define EDA_PIN_NEG   8
#define EDA_PIN_CF   16

#define EDA_V_INIT   -10001001.0
#define EDA_V_MIN    -10000000.0
#define EDA_V_MAX     10000000.0

#define EDA_V_Diode_ON  0.69
#define EDA_V_Diode_OFF 0.70

enum {
	EDA_Battery_NEG,
	EDA_Battery_POS,
	EDA_Battery_NB,
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
	EDA_PNP_B,
	EDA_PNP_E,
	EDA_PNP_C,
	EDA_PNP_NB,
};

typedef struct {
	uint64_t  type;
	uint64_t  model;
	uint64_t  pid;

	double    v;
	double    a;
	double    r;
	double    jr;
	double    uf;
	double    uh;
	double    hfe;
} edata_t;


ScfEconn*      econn__alloc();
int            econn__add_cid(ScfEconn* ec, uint64_t  cid);
int            econn__del_cid(ScfEconn* ec, uint64_t  cid);
void           econn__free(ScfEconn* ec);

ScfEline*      eline__alloc();
int            eline__add_line(ScfEline* el, ScfLine*  l);
int            eline__del_line(ScfEline* el, ScfLine*  l);

int            eline__add_pin (ScfEline* el, uint64_t  cid, uint64_t pid);
int            eline__del_pin (ScfEline* el, uint64_t  cid, uint64_t pid);

int            eline__add_conn(ScfEline* el, ScfEconn* ec);
int            eline__del_conn(ScfEline* el, ScfEconn* ec);
void           eline__free    (ScfEline* el);

ScfEpin*       epin__alloc();
int            epin__add_component(ScfEpin* pin, uint64_t cid, uint64_t pid);
int            epin__del_component(ScfEpin* pin, uint64_t cid, uint64_t pid);
void           epin__free         (ScfEpin* pin);

ScfEcomponent* ecomponent__alloc  (uint64_t type);
int            ecomponent__add_pin(ScfEcomponent* c, ScfEpin* pin);
int            ecomponent__del_pin(ScfEcomponent* c, ScfEpin* pin);
void           ecomponent__free   (ScfEcomponent* c);

ScfEfunction*  efunction__alloc        (const   char* name);
int            efunction__add_component(ScfEfunction* f, ScfEcomponent* c);
int            efunction__del_component(ScfEfunction* f, ScfEcomponent* c);

int            efunction__add_eline    (ScfEfunction* f, ScfEline* el);
int            efunction__del_eline    (ScfEfunction* f, ScfEline* el);
void           efunction__free         (ScfEfunction* f);

ScfEboard*     eboard__alloc();
int            eboard__add_function(ScfEboard* b, ScfEfunction* f);
int            eboard__del_function(ScfEboard* b, ScfEfunction* f);
void           eboard__free        (ScfEboard* b);

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
			ecomponent__free(_c); \
			_c = NULL; \
			return ret; \
		} \
		\
		for (size_t i = 0;  i < (_c)->n_pins; i++) \
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

#endif
