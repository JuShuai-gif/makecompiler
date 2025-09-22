#ifndef ELF_H
#define ELF_H

#include<elf.h>
#include"utils_list.h"
#include"utils_vector.h"

typedef struct elf_context_s	elf_context_t;
typedef struct elf_ops_s		elf_ops_t;

typedef struct {
	char*		name;
	uint64_t    st_size;
	Elf64_Addr  st_value;

	uint16_t    st_shndx;
	uint8_t		st_info;

	uint8_t     dyn_flag:1;
} elf_sym_t;

typedef struct {
	char*		name;

	Elf64_Addr	r_offset;
	uint64_t	r_info;
	int64_t     r_addend;
} elf_rela_t;

typedef struct {
	char*		name;

	uint32_t    index;

	uint8_t*	data;
	int			data_len;

	uint32_t	sh_type;
    uint64_t	sh_flags;
	uint64_t    sh_addralign;
	uint32_t    sh_link;
	uint32_t    sh_info;
} elf_section_t;

typedef struct {
	Elf64_Phdr  ph;

	uint64_t    addr;
	uint64_t    len;
	void*       data;
} elf_phdr_t;

struct elf_ops_s
{
	const char*		machine;

	int				(*open )(elf_context_t* elf);
	int				(*close)(elf_context_t* elf);

	int				(*add_sym   )(elf_context_t* elf, const elf_sym_t*  sym,   const char* sh_name);
	int				(*read_syms )(elf_context_t* elf,       vector_t*   syms,  const char* sh_name);
	int				(*read_relas)(elf_context_t* elf,       vector_t*   relas, const char* sh_name);
	int				(*read_phdrs)(elf_context_t* elf,       vector_t*   phdrs);

	int				(*add_section )(elf_context_t* elf, const elf_section_t*  section);
	int				(*read_section)(elf_context_t* elf,       elf_section_t** psection, const char* name);

	int				(*add_rela_section)(elf_context_t* elf, const elf_section_t* section, vector_t* relas);

	int				(*add_dyn_need)(elf_context_t* elf, const char* soname);
	int	            (*add_dyn_rela)(elf_context_t* elf, const elf_rela_t* rela);

	int				(*write_rel )(elf_context_t* elf);
	int				(*write_dyn )(elf_context_t* elf, const char* sysroot);
	int				(*write_exec)(elf_context_t* elf, const char* sysroot);
};

struct elf_context_s {

	elf_ops_t*	ops;

	void*			priv;

	FILE*           fp;
	int64_t         start;
	int64_t         end;
};

void elf_rela_free(elf_rela_t* rela);

int elf_open (elf_context_t** pelf, const char* machine, const char* path, const char* mode);
int elf_open2(elf_context_t*  elf,  const char* machine);
int elf_close(elf_context_t*  elf);

int elf_add_sym (elf_context_t* elf, const elf_sym_t*     sym, const char* sh_name);

int elf_add_section(elf_context_t* elf, const elf_section_t* section);

int	elf_add_rela_section(elf_context_t* elf, const elf_section_t* section, vector_t* relas);
int	elf_add_dyn_need(elf_context_t* elf, const char* soname);
int	elf_add_dyn_rela(elf_context_t* elf, const elf_rela_t* rela);

int elf_read_section(elf_context_t* elf, elf_section_t** psection, const char* name);

int elf_read_syms (elf_context_t* elf, vector_t* syms,  const char* sh_name);
int elf_read_relas(elf_context_t* elf, vector_t* relas, const char* sh_name);
int elf_read_phdrs(elf_context_t* elf, vector_t* phdrs);

int elf_write_rel (elf_context_t* elf);
int elf_write_dyn (elf_context_t* elf, const char* sysroot);
int elf_write_exec(elf_context_t* elf, const char* sysroot);

#endif
