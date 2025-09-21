#include"elf.h"
#include"dwarf_def.h"

int main(int argc, char* argv[])
{
	if (argc < 2) {
		loge("\n");
		return -1;
	}

	elf_context_t* elf = NULL;

	if (elf_open(&elf, "x64", "/home/yu/my/1.o", "rb") < 0) {
		loge("\n");
		return -1;
	}

	elf_section_t* s = NULL;

	int ret = elf_read_section(elf, &s, argv[1]);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	printf("s->data_len: %d\n", s->data_len);
	int i;
	for (i = 0; i < s->data_len; i++) {
		if (i > 0 && i % 10 == 0)
			printf("\n");

		unsigned char c = s->data[i];
		printf("%#02x ", c);
	}
	printf("\n\n");

	vector_t* abbrev_results = vector_alloc();

	ret = dwarf_abbrev_decode(abbrev_results, s->data, s->data_len);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	dwarf_abbrev_print(abbrev_results);

	string_t* abbrev = string_alloc();
	ret = dwarf_abbrev_encode(abbrev_results, abbrev);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	if (abbrev->len != s->data_len) {
		loge("\n");
		return -1;
	}

	for (i = 0; i < abbrev->len; i++) {
		if (i > 0 && i % 10 == 0)
			printf("\n");

		unsigned char c  = abbrev->data[i];
		unsigned char c2 = s->data[i];
		printf("%#02x ", c);

		if (c != c2) {
			loge("\n");
			return -1;
		}
	}
	printf("\n\n");

	elf_close(elf);
	elf = NULL;
	logi("main ok\n");
	return 0;
}

