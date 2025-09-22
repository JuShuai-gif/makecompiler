#include"ghr_elf.h"
#include"dwarf_def.h"

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	elf_context_t* elf = NULL;

	if (elf_open(&elf, "x64", "./1.elf", "rb") < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	elf_section_t* s = NULL;

	int ret = elf_read_section(elf, &s, argv[1]);
	if (ret < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	printf("s->data_len: %d\n", s->data_len);
	int i;
	for (i = 0; i < s->data_len; i++) {
		if (i > 0 && i % 4 == 0)
			printf("\n");

		unsigned char c = s->data[i];
		printf("%#02x ", c);
	}
	printf("\n\n");

	for (i = 0; i < s->data_len; i++) {
		unsigned char c = s->data[i];
		printf("%c", c);
	}
	printf("\n");

	dwarf_line_machine_t* lm = dwarf_line_machine_alloc();
	assert(lm);

	vector_t* line_results = vector_alloc();
	assert(line_results);
#if 1
	ret = dwarf_line_decode(lm, line_results, s->data, s->data_len);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	dwarf_line_machine_print(lm);

#if 1
	for (i = 0; i < line_results->size; i++) {
		dwarf_line_result_t* r = line_results->data[i];

		loge("address: %#lx, line: %u, column: %u, is_stmt: %u, basic_block: %u, end_sequence: %u\n",
				r->address, r->line, r->column, r->is_stmt, r->basic_block, r->end_sequence);
	}
#endif
#endif

	elf_close(elf);
	elf = NULL;
	printf("%s(),%d, main ok\n", __func__, __LINE__);
	return 0;
}

