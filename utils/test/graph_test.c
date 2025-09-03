#include"../utils_stack.h"

int main()
{
	stack_t* s = stack_alloc();

	int i;
	for (i = 0; i < 64; i++) {
		printf("i: %d, s->size: %d, s->capacity: %d\n", i, s->size, s->capacity);
		stack_push(s, (void*)i);
	}

	for (i = 0; i < 64; i++) {
		int j = (int)stack_pop(s);
		printf("i: %d, j: %d, s->size: %d, s->capacity: %d\n", i, j, s->size, s->capacity);
	}

	int j = (int)stack_pop(s);
	printf("i: %d, j: %d, s->size: %d, s->capacity: %d\n", i, j, s->size, s->capacity);
}