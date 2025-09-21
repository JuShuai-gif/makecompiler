#ifndef OBJECT_H
#define OBJECT_H

#include"utils_atomic.h"

typedef struct {
	atomic_t  refs;
	int           size;
	uint8_t       data[0];
} object_t;

void* malloc(int size);

void  freep(void** pp, void (*release)(void* objdata));

void  free_array (void** pp, int size, int nb_pointers, void (*release)(void* objdata));
void  freep_array(void** pp, int nb_pointers, void (*release)(void* objdata));

void  ref (void* data);

#endif

