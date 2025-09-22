#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include"eda_pb.h"
#include"def.h"


int main()
{
	ScfEcomponent* r      = ecomponent__alloc(EDA_None);
	ScfEcomponent* d0     = ecomponent__alloc(EDA_None);
	ScfEcomponent* d1     = ecomponent__alloc(EDA_None);

	ScfEpin*       rp0    = epin__alloc();
	ScfEpin*       rp1    = epin__alloc();

	ScfEpin*       d0p0   = epin__alloc();
	ScfEpin*       d0p1   = epin__alloc();

	ScfEpin*       d1p0   = epin__alloc();
	ScfEpin*       d1p1   = epin__alloc();

	ScfEpin*       rps[]  = {rp0,  rp1};
	ScfEpin*       d0ps[] = {d0p0, d0p1};
	ScfEpin*       d1ps[] = {d1p0, d1p1};

	int64_t c[] = {1, 2};
	int64_t b[] = {0, 2};
	int64_t a[] = {0, 1};

	epin__add_component(rp1, 1, EDA_Diode_NEG);
	epin__add_component(rp1, 2, EDA_Diode_NEG);

	epin__add_component(d0p1, 0, 0);
	epin__add_component(d0p1, 2, EDA_Diode_NEG);

	epin__add_component(d1p1, 0, 0);
	epin__add_component(d1p1, 1, EDA_Diode_NEG);

	r->id      = 0;
	r->type    = EDA_Resistor;
	ecomponent__add_pin(r, rp0);
	ecomponent__add_pin(r, rp1);

	d0->id     = 1;
	d0->type   = EDA_Diode;
	ecomponent__add_pin(d0, d0p0);
	ecomponent__add_pin(d0, d0p1);

	d1->id     = 2;
	d1->type   = EDA_Diode;
	ecomponent__add_pin(d1, d1p0);
	ecomponent__add_pin(d1, d1p1);

	ScfEfunction* f = efunction__alloc("test");
	efunction__add_component(f, r);
	efunction__add_component(f, d0);
	efunction__add_component(f, d1);

	ScfEboard* board = eboard__alloc();
	eboard__add_function(board, f);

	size_t rlen  = ecomponent__get_packed_size(r);
	size_t d0len = ecomponent__get_packed_size(d0);
	size_t d1len = ecomponent__get_packed_size(d1);
	size_t flen  = efunction__get_packed_size(f);
	size_t blen  = eboard__get_packed_size(board);

	printf("rlen: %ld, d0len: %ld, d1len: %ld, flen: %ld, blen: %ld\n", rlen, d0len, d1len, flen, blen);

	uint8_t pb[1024];

	eboard__pack(board, pb);

	ScfEboard* p = eboard__unpack(NULL, blen, pb);

	printf("p: %p\n", p);
	size_t i;
	size_t j;
	size_t k;
	size_t l;

	for (i = 0; i < p->n_functions; i++) {
		ScfEfunction* pf = p->functions[i];

		printf("f: %s\n", pf->name);

		for (l = 0; l < pf->n_components; l++) {
			ScfEcomponent* pc = pf->components[l];

			printf("i: %ld, pc: %p, id: %ld, cid: %ld, n_pins: %ld\n", i, pc, pc->id, pc->type, pc->n_pins);

			for (j = 0; j < pc->n_pins; j++) {
				ScfEpin* pp = pc->pins[j];

				printf("j: %ld, pp: %p, n_tos: %ld, cid: %ld, pid: %ld\n", j, pp, pp->n_tos, pp->cid, pp->id);

				for (k = 0; k + 1 < pp->n_tos; k += 2) {
					printf("k: %ld, cid: %ld, pid: %ld\n", k, pp->tos[k], pp->tos[k + 1]);
				}
			}
			printf("\n");
		}
		printf("\n\n");
	}

	eboard__free(board);

	eboard__free_unpacked(p, NULL);
	return 0;
}
