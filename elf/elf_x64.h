#ifndef ELF_X64_H
#define ELF_X64_H

#include"ghr_elf.h"
#include"elf_native.h"

int  __x64_elf_add_dyn (elf_native_t* x64, const char* sysroot);
void __x64_elf_post_dyn(elf_native_t* x64, uint64_t rx_base, uint64_t rw_base, elf_section_t* cs);

int  __x64_so_add_dyn(elf_native_t* x64, const char* sysroot);

int __x64_elf_write_phdr   (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint32_t nb_phdrs);
int __x64_elf_write_interp (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint64_t len);
int __x64_elf_write_text   (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint64_t len);
int __x64_elf_write_rodata (elf_context_t* elf, uint64_t r_base,  uint64_t offset, uint64_t len);
int __x64_elf_write_data   (elf_context_t* elf, uint64_t rw_base, uint64_t offset, uint64_t len);
int __x64_elf_write_dynamic(elf_context_t* elf, uint64_t rw_base, uint64_t offset, uint64_t len);

#endif
