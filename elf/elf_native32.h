#ifndef ELF_NATIVE32_H
#define ELF_NATIVE32_H

#include"elf.h"
#include"utils_vector.h"
#include"utils_string.h"

typedef struct elf_section_s  elf_section_t;

struct elf_section_s
{
	elf_section_t*  link;
	elf_section_t*  info;

	string_t*     name;

	Elf32_Shdr        sh;

	uint64_t          offset;

	uint16_t          index;
	uint8_t*          data;
	int               data_len;
};

typedef struct {
	elf_section_t*  section;

	string_t*     name;

	Elf32_Sym         sym;

	int               index;
	uint8_t           dyn_flag:1;

} elf_sym_t;

typedef struct {
	Elf32_Ehdr        eh;

	Elf32_Shdr        sh_null;

	vector_t*     sections;
	vector_t*     phdrs;

	Elf32_Shdr        sh_symtab;
	vector_t*     symbols;

	Elf32_Shdr        sh_strtab;

	Elf32_Shdr        sh_shstrtab;
	string_t*     sh_shstrtab_data;

	vector_t*     dynsyms;
	vector_t*     dyn_needs;
	vector_t*     dyn_relas;

	elf_section_t*  interp;
	elf_section_t*  dynsym;
	elf_section_t*  dynstr;
	elf_section_t*  gnu_version;
	elf_section_t*  gnu_version_r;
	elf_section_t*  rela_plt;
	elf_section_t*  plt;
	elf_section_t*  dynamic;
	elf_section_t*  got_plt;

} elf_native_t;


int  elf32_open (elf_context_t* elf);
int  elf32_close(elf_context_t* elf);

int  elf32_add_sym         (elf_context_t* elf, const elf_sym_t*     sym, const char* sh_name);
int  elf32_add_section     (elf_context_t* elf, const elf_section_t* section);
int  elf32_add_rela_section(elf_context_t* elf, const elf_section_t* section, vector_t* relas);

int  elf32_read_shstrtab(elf_context_t* elf);
int  elf32_read_section (elf_context_t* elf, elf_section_t** psection, const char* name);

int  elf32_add_dyn_need(elf_context_t* elf, const char* soname);
int  elf32_add_dyn_rela(elf_context_t* elf, const elf_rela_t* rela);

int  elf32_read_phdrs  (elf_context_t* elf, vector_t* phdrs);
int  elf32_read_relas  (elf_context_t* elf, vector_t* relas, const char* sh_name);
int  elf32_read_syms   (elf_context_t* elf, vector_t* syms,  const char* sh_name);

int  elf32_find_sym    (elf_sym_t**   psym, Elf32_Rela* rela, vector_t* symbols);
void elf32_process_syms(elf_native_t* native, uint32_t cs_index);

int  elf32_write_sections(elf_context_t* elf);
int  elf32_write_shstrtab(elf_context_t* elf);
int  elf32_write_symtab  (elf_context_t* elf);
int  elf32_write_strtab  (elf_context_t* elf);
int  elf32_write_rel     (elf_context_t* elf, uint16_t e_machine);

int  elf32_sym_cmp(const void* v0, const void* v1);

static inline void elf_header(Elf32_Ehdr* eh,
		uint16_t    e_type,
		uint16_t    e_machine,
		Elf32_Addr  e_entry,
		Elf32_Off   e_phoff,
		uint16_t    e_phnum,
		uint16_t    e_shnum,
		uint16_t    e_shstrndx)
{
	eh->e_ident[EI_MAG0]    = ELFMAG0;
	eh->e_ident[EI_MAG1]    = ELFMAG1;
	eh->e_ident[EI_MAG2]    = ELFMAG2;
	eh->e_ident[EI_MAG3]    = ELFMAG3;
	eh->e_ident[EI_CLASS]   = ELFCLASS32;
	eh->e_ident[EI_DATA]    = ELFDATA2LSB;
	eh->e_ident[EI_VERSION] = EV_CURRENT;
	eh->e_ident[EI_OSABI]   = ELFOSABI_SYSV;

	eh->e_type      = e_type;
	eh->e_machine   = e_machine;
	eh->e_version   = EV_CURRENT;
	eh->e_entry     = e_entry;
	eh->e_ehsize    = sizeof(Elf32_Ehdr);

	eh->e_flags     = 0x5000400;

	eh->e_phoff     = e_phoff;
	eh->e_phentsize = sizeof(Elf32_Phdr);
	eh->e_phnum     = e_phnum;

	eh->e_shoff     = sizeof(Elf32_Ehdr);
	eh->e_shentsize = sizeof(Elf32_Shdr);
	eh->e_shnum     = e_shnum;
	eh->e_shstrndx  = e_shstrndx;
}

static inline void section_header(Elf32_Shdr* sh,
		uint32_t    sh_name,
        Elf32_Addr  sh_addr,
        Elf32_Off   sh_offset,
        uint64_t    sh_size,
        uint32_t    sh_link,
        uint32_t    sh_info,
        uint64_t    sh_entsize)
{
	sh->sh_name		= sh_name;
	sh->sh_addr		= sh_addr;
	sh->sh_offset	= sh_offset;
	sh->sh_size		= sh_size;
	sh->sh_link		= sh_link;
	sh->sh_info		= sh_info;
	sh->sh_entsize	= sh_entsize;
}

#endif

