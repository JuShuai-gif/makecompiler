#ifndef ELF_NAJA_H
#define ELF_NAJA_H

#include"ghr_elf.h"
#include"elf_native.h"
#include"utils_vector.h"
#include"utils_string.h"

int __naja_elf_add_dyn (elf_native_t* naja, const char* sysroot);
int __naja_elf_post_dyn(elf_native_t* naja, uint64_t rx_base, uint64_t rw_base, elf_section_t* cs);

int __naja_elf_write_phdr   (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint32_t nb_phdrs);
int __naja_elf_write_interp (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint64_t len);
int __naja_elf_write_text   (elf_context_t* elf, uint64_t rx_base, uint64_t offset, uint64_t len);
int __naja_elf_write_rodata (elf_context_t* elf, uint64_t r_base,  uint64_t offset, uint64_t len);
int __naja_elf_write_data   (elf_context_t* elf, uint64_t rw_base, uint64_t offset, uint64_t len);
int __naja_elf_write_dynamic(elf_context_t* elf, uint64_t rw_base, uint64_t offset, uint64_t len);

#endif

