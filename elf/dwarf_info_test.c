#include"elf.h"
#include"dwarf_def.h"

int main(int argc, char* argv[])
{
	elf_context_t* elf = NULL;

	if (elf_open(&elf, "x64", "/home/yu/my/1.o", "rb") < 0) {
		loge("\n");
		return -1;
	}

	elf_section_t* s_abbrev = NULL;
	elf_section_t* s_str    = NULL;
	elf_section_t* s_info   = NULL;

	int ret;
#if 1
	ret = elf_read_section(elf, &s_abbrev, ".debug_abbrev");
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	ret = elf_read_section(elf, &s_str, ".debug_str");
	if (ret < 0) {
		loge("\n");
		return -1;
	}
#endif
	ret = elf_read_section(elf, &s_info, ".debug_info");
	if (ret < 0) {
		loge("\n");
		return -1;
	}
	loge("s_info->data_len: %d\n", s_info->data_len);
#if 1
	vector_t* abbrevs = vector_alloc();
	vector_t* infos   = vector_alloc();

	ret = dwarf_abbrev_decode(abbrevs, s_abbrev->data, s_abbrev->data_len);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	string_t debug_str;
	debug_str.data = s_str->data;
	debug_str.len  = s_str->data_len;
	debug_str.capacity = -1;

	dwarf_info_header_t info_header;

	ret = dwarf_info_decode(infos, abbrevs, &debug_str, s_info->data, s_info->data_len, &info_header);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

	printf("header length:  %u\n", info_header.length);
	printf("header version: %u\n", info_header.version);
	printf("header offset:  %u\n", info_header.offset);
	printf("header address_size: %u\n", info_header.address_size);

//	dwarf_info_print(infos);
	string_t* encode_str = string_alloc();

	ret = dwarf_info_encode(infos, abbrevs, &debug_str, encode_str, &info_header);
	if (ret < 0) {
		loge("\n");
		return -1;
	}

#if 1
	printf("s->data_len: %d\n", s_info->data_len);
	int i;
	for (i = 0; i < s_info->data_len; i++) {
		if (i > 0 && i % 4 == 0)
			printf("\n");

		unsigned char c = s_info->data[i];
		printf("%#02x ", c);
	}
	printf("\n\n");
#endif

	printf("encode_str->len: %ld\n", encode_str->len);
	string_print_bin(encode_str);
	printf("\n\n");

#endif
	elf_close(elf);
	elf = NULL;
	logi("main ok\n");
	return 0;
}

