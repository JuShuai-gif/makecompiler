
#include "../utils_string.h" 
int main(int argc, char* argv[])
{
	string_t* s0 = string_alloc();
	string_t* s1 = string_cstr("hello, world!");
	string_t* s2 = string_clone(s1);
	printf("s0: %s\n", s0->data);
	printf("s1: %s\n", s1->data);
	printf("s2: %s\n", s2->data);

	string_copy(s0, s2);
	printf("s0: %s\n", s0->data);

	printf("s0 cmp s1: %d\n", string_cmp(s0, s1));

	string_t* s3 = string_cstr("ha ha ha!");
	printf("s3: %s\n", s3->data);

	string_cat(s0, s3);
	printf("s0: %s\n", s0->data);

	string_cat_cstr(s0, "he he he!");
	printf("s0: %s\n", s0->data);

	string_t* s4  = string_cstr("hello, world!");
	string_t* s5  = string_cstr("ll");
	vector_t* vec = vector_alloc();

	int ret = string_match_kmp(s4, s5, vec);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	int i;
	for (i = 0; i < vec->size; i++) {

		int offset = (intptr_t)vec->data[i];

		printf("i: %d, offset: %d\n", i, offset);
	}

	string_free(s0);
	string_free(s1);
	string_free(s2);
	string_free(s3);
	return 0;
}