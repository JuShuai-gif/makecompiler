#ifndef ELF_LINK_H
#define ELF_LINK_H

#include"elf.h"
#include"utils_string.h"
#include<ar.h>

typedef struct {
	elf_context_t* elf;

	string_t*      name;

	int                text_idx;
	int                rodata_idx;
	int                data_idx;

	int                abbrev_idx;
	int                info_idx;
	int                line_idx;
	int                str_idx;

	string_t*      text;
	string_t*      rodata;
	string_t*      data;

	string_t*      debug_abbrev;
	string_t*      debug_info;
	string_t*      debug_line;
	string_t*      debug_str;

	vector_t*      syms;

	vector_t*      text_relas;
	vector_t*      data_relas;

	vector_t*      debug_line_relas;
	vector_t*      debug_info_relas;

	vector_t*      dyn_syms;
	vector_t*      rela_plt;
	vector_t*      dyn_needs;

} elf_file_t;

#define ELF_FILE_SHNDX(member) ((void**)&((elf_file_t*)0)->member - (void**)&((elf_file_t*)0)->text + 1)

typedef struct {
	string_t*      name;
	uint32_t           offset;

} ar_sym_t;

typedef struct {

	vector_t*      symbols;
	vector_t*      files;

	FILE*              fp;

} ar_file_t;

int elf_file_close(elf_file_t* ef, void (*rela_free)(void*), void (*sym_free)(void*));

int elf_link(vector_t* objs, vector_t* afiles, vector_t* sofiles, const char* sysroot, const char* arch, const char* out, int dyn_flag);

#endif
