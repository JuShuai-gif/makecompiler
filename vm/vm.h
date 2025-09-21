#ifndef VM_H
#define VM_H

#include"elf.h"
#include<dlfcn.h>

#if 0
#define NAJA_PRINTF   printf
#else
#define NAJA_PRINTF
#endif

#define NAJA_REG_FP   28
#define NAJA_REG_LR   29
#define NAJA_REG_SP   30

typedef struct vm_s       vm_t;
typedef struct vm_ops_s   vm_ops_t;

struct  vm_s
{
	elf_context_t*        elf;

	vector_t*             sofiles;
	vector_t*             phdrs;

	elf_phdr_t*           text;
	elf_phdr_t*           rodata;
	elf_phdr_t*           data;

	elf_phdr_t*           dynamic;
	Elf64_Rela*               jmprel;
	uint64_t                  jmprel_addr;
	uint64_t                  jmprel_size;
	Elf64_Sym*                dynsym;
	uint64_t*                 pltgot;
	uint8_t*                  dynstr;

	vm_ops_t*             ops;
	void*                     priv;
};

struct vm_ops_s
{
	const char* name;

	int  (*open )(vm_t* vm);
	int  (*close)(vm_t* vm);

	int  (*run  )(vm_t* vm, const char* path, const char* sys);
};

#define  VM_Z   0
#define  VM_NZ  1
#define  VM_GE  2
#define  VM_GT  3
#define  VM_LE  4
#define  VM_LT  5

typedef union {
	uint8_t  b[32];
	uint16_t w[16];
	uint32_t l[8];
	uint64_t q[4];
	float    f[8];
	double   d[4];
} fv256_t;

typedef struct {
	uint64_t  regs[32];
	fv256_t   fvec[32];

	uint64_t  ip;
	uint64_t  flags;

#define STACK_INC 16
	uint8_t*  stack;
	int64_t   size;

	uint64_t  _start;

} vm_naja_t;

typedef int (*naja_opcode_pt)(vm_t* vm, uint32_t inst);

int vm_open (vm_t** pvm, const char* arch);
int vm_close(vm_t*   vm);
int vm_clear(vm_t*   vm);

int vm_run  (vm_t*   vm, const char* path, const char* sys);

int naja_vm_open(vm_t* vm);
int naja_vm_close(vm_t* vm);
int naja_vm_init(vm_t* vm, const char* path, const char* sys);

#endif
