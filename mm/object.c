#include"object.h"

#define __sys_malloc  malloc
#define __sys_calloc  calloc
#define __sys_free    free

void* malloc(int size)
{
	object_t* obj = __sys_calloc(1, sizeof(object_t) + size);
	if (!obj)
		return NULL;

	atomic_inc(&obj->refs);
	obj->size = size;

	assert((uintptr_t)obj->data % sizeof(void*) == 0);

	loge("obj: %p, obj->size: %d, size: %d\n", obj, obj->size, size);
	return obj->data;
}

void freep(void** pp, void (*release)(void* objdata))
{
	if (!pp || !*pp)
		return;

	void*         data = *pp;
	object_t* obj  = (object_t*)((uint8_t*)data - sizeof(object_t));

	loge("obj: %p, pp: %p, data: %p\n", obj, pp, data);

	if (atomic_dec_and_test(&obj->refs)) {

		if (release)
			release(obj->data);

		__sys_free(obj);
	}

	*pp = NULL;
}

void freep_array(void** pp, int nb_pointers, void (*release)(void* objdata))
{
	if (!pp || !*pp)
		return;

	assert(nb_pointers >= 1);

	object_t* obj;

	void** p = (void**) *pp;

	if (nb_pointers > 1) {
		void*  data;
		int    size;

		obj  = (object_t*)((uint8_t*)p - sizeof(object_t));

		size = obj->size / sizeof(void*);
		assert(obj->size % sizeof(void*) == 0);

		int i;
		for (i   = 0; i < size; i++) {
			data = p[i];

			if (!data)
				continue;

			obj  = (object_t*)((uint8_t*)data - sizeof(object_t));

			loge("obj: %p, pp: %p, data: %p, i: %d\n", obj, pp, data, i);

			freep_array(&p[i], nb_pointers - 1, release);
			p[i] = NULL;
		}
	}

	obj  = (object_t*)((uint8_t*)p - sizeof(object_t));

	if (atomic_dec_and_test(&obj->refs)) {

		if (release && 1 == nb_pointers)
			release(obj->data);

		loge("obj: %p, pp: %p, nb_pointers: %d\n\n", obj, pp, nb_pointers);

		__sys_free(obj);
	}

	*pp = NULL;
}

void free_array(void** pp, int size, int nb_pointers, void (*release)(void* objdata))
{
	if (!pp)
		return;

	object_t* obj;
	void*         data;

	int i;
	for (i = 0; i < size; i++) {

		data = pp[i];

		if (!data)
			continue;

		obj  = (object_t*)((uint8_t*)data - sizeof(object_t));

		if (atomic_dec_and_test(&obj->refs)) {

			loge("obj: %p, pp: %p, data: %p, i: %d, nb_pointers: %d\n", obj, pp, data, i, nb_pointers);

			if (nb_pointers > 1)

				freep_array(&pp[i], nb_pointers - 1, release);
			else {
				assert(1 == nb_pointers);

				if (release)
					release(obj->data);

				__sys_free(obj);
			}
		}

		pp[i] = NULL;
	}
}

void ref(void* data)
{
	if (!data)
		return;

	object_t* obj = (object_t*)((uint8_t*)data - sizeof(object_t));

	loge("obj: %p\n", obj);

	atomic_inc(&obj->refs);
}

#if 0
int main()
{
	int* a = malloc(sizeof(int) * 10);
	if (!a) {
		loge("\n");
		return -1;
	}

	loge("a: %p\n", a);

	int i;
	for (i = 9; i >= 0; i--)
		a[i] = i;

	for (i = 0; i < 10; i++)
		printf("%d\n", a[i]);

	free(a);
	return 0;
}

#endif
