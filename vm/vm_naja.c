#include"vm.h"

static const char* somaps[][3] =
{
	{"x64", "/lib/ld-linux-aarch64.so.1",  "/lib64/ld-linux-x86-64.so.2"},
	{"x64", "libc.so.6",                   "/lib/x86_64-linux-gnu/libc.so.6"},
};

typedef int (*dyn_func_pt)(uint64_t r0,
		uint64_t r1,
		uint64_t r2,
		uint64_t r3,
		uint64_t r4,
		uint64_t r5,
		uint64_t r6,
		uint64_t r7,
		double   d0,
		double   d1,
		double   d2,
		double   d3,
		double   d4,
		double   d5,
		double   d6,
		double   d7);

int naja_vm_open(vm_t* vm)
{
	if (!vm)
		return -EINVAL;

	vm_naja_t* naja = calloc(1, sizeof(vm_naja_t));
	if (!naja)
		return -ENOMEM;

	vm->priv = naja;
	return 0;
}

int naja_vm_close(vm_t* vm)
{
	if (vm) {
		if (vm->priv) {
			free(vm->priv);
			vm->priv = NULL;
		}
	}

	return 0;
}

static int naja_vm_dynamic_link(vm_t* vm)
{
	vm_naja_t* naja = vm->priv;
	dyn_func_pt    f    = NULL;

	int64_t  sp = naja->regs[NAJA_REG_SP];

	uint64_t lr  = *(uint64_t*)(naja->stack - (sp +  8));
	uint64_t r16 = *(uint64_t*)(naja->stack - (sp + 16));

	logw("sp: %ld, r16: %#lx, lr: %#lx, vm->jmprel_size: %ld\n", sp, r16, lr, vm->jmprel_size);

	if (r16  > (uint64_t)vm->data->data) {
		r16 -= (uint64_t)vm->data->data;
		r16 +=           vm->data->addr;
	}

	logw("r16: %#lx, text: %p, rodata: %p, data: %p\n", r16, vm->text->data, vm->rodata->data, vm->data->data);

	int i;
	for (i = 0; i < vm->jmprel_size / sizeof(Elf64_Rela); i++) {

		if (r16  == vm->jmprel[i].r_offset) {

			int   j     = ELF64_R_SYM(vm->jmprel[i].r_info);
			char* fname = vm->dynstr + vm->dynsym[j].st_name;

			logw("j: %d, %s\n", j, fname);

			int k;
			for (k = 0; k < vm->sofiles->size; k++) {

				f = dlsym(vm->sofiles->data[k], fname);
				if (f)
					break;
			}

			if (f) {
				int64_t offset = vm->jmprel[i].r_offset - vm->data->addr;

				if (offset < 0 || offset > vm->data->len) {
					loge("\n");
					return -1;
				}

				logw("f: %p\n", f);

				*(void**)(vm->data->data + offset) = f;

				naja->regs[0] = f(naja->regs[0],
						naja->regs[1],
						naja->regs[2],
						naja->regs[3],
						naja->regs[4],
						naja->regs[5],
						naja->regs[6],
						naja->regs[7],
						naja->fvec[0].d[0],
						naja->fvec[1].d[0],
						naja->fvec[2].d[0],
						naja->fvec[3].d[0],
						naja->fvec[4].d[0],
						naja->fvec[5].d[0],
						naja->fvec[6].d[0],
						naja->fvec[7].d[0]);

				naja->regs[NAJA_REG_SP] += 16;
				return 0;
			}
			break;
		}
	}

	return -1;
}

int naja_vm_init(vm_t* vm, const char* path, const char* sys)
{
	if (!vm || !path)
		return -EINVAL;

	if (vm->elf)
		vm_clear(vm);

	if (vm->priv)
		memset(vm->priv, 0, sizeof(vm_naja_t));
	else {
		vm->priv = calloc(1, sizeof(vm_naja_t));
		if (!vm->priv)
			return -ENOMEM;
	}

	if (vm->phdrs)
		vector_clear(vm->phdrs, (void (*)(void*) )free);
	else {
		vm->phdrs = vector_alloc();
		if (!vm->phdrs)
			return -ENOMEM;
	}

	if (vm->sofiles)
		vector_clear(vm->phdrs, (void (*)(void*) )dlclose);
	else {
		vm->sofiles = vector_alloc();
		if (!vm->sofiles)
			return -ENOMEM;
	}

	int ret = elf_open(&vm->elf, "naja", path, "rb");
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	ret = elf_read_phdrs(vm->elf, vm->phdrs);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	elf_phdr_t* ph;
	int i;
	for (i = 0; i < vm->phdrs->size; i++) {
		ph =        vm->phdrs->data[i];

		if (PT_LOAD ==  ph->ph.p_type) {

			ph->addr = (ph->ph.p_vaddr + ph->ph.p_memsz) & ~(ph->ph.p_align - 1);
			ph->len  = (ph->ph.p_vaddr + ph->ph.p_memsz) - ph->addr;

			ph->data = calloc(1, ph->len);
			if (!ph->data)
				return -ENOMEM;

			fseek(vm->elf->fp, 0, SEEK_SET);

			ret = fread(ph->data, ph->len, 1, vm->elf->fp);
			if (1 != ret) {
				loge("\n");
				return -1;
			}

			logi("i: %d, ph->p_offset: %#lx, ph->p_filesz: %#lx\n", i, ph->ph.p_offset, ph->ph.p_filesz);

			logi("i: %d, ph->addr: %#lx, ph->len: %#lx, %#lx, ph->flags: %#x\n", i, ph->addr, ph->len, ph->ph.p_memsz, ph->ph.p_flags);

			if ((PF_X | PF_R) == ph->ph.p_flags)
				vm->text   =  ph;

			else if ((PF_W | PF_R) == ph->ph.p_flags)
				vm->data   = ph;

			else if (PF_R == ph->ph.p_flags)
				vm->rodata =  ph;
			else {
				loge("\n");
				return -1;
			}

		} else if (PT_DYNAMIC == ph->ph.p_type) {
			ph->addr = ph->ph.p_vaddr;
			ph->len  = ph->ph.p_memsz;

			vm->dynamic = ph;

			logi("ph->addr: %#lx, ph->len: %#lx, %#lx, ph->p_offset: %#lx\n", ph->addr, ph->len, ph->ph.p_memsz, ph->ph.p_offset);
		}
	}

	logi("\n\n");

	if (vm->dynamic) {
		Elf64_Dyn* d = (Elf64_Dyn*)(vm->data->data + vm->dynamic->ph.p_offset);

		vm->jmprel = NULL;
		for (i = 0; i < vm->dynamic->ph.p_filesz / sizeof(Elf64_Dyn); i++) {

			switch (d[i].d_tag) {

				case DT_STRTAB:
					logi("dynstr: %#lx\n", d[i].d_un.d_ptr);
					vm->dynstr = d[i].d_un.d_ptr - vm->text->addr + vm->text->data;
					break;

				case DT_SYMTAB:
					logi("dynsym: %#lx\n", d[i].d_un.d_ptr);
					vm->dynsym = (Elf64_Sym*)(d[i].d_un.d_ptr - vm->text->addr + vm->text->data);
					break;

				case DT_JMPREL:
					logi("JMPREL: %#lx\n", d[i].d_un.d_ptr);
					vm->jmprel      = (Elf64_Rela*)(d[i].d_un.d_ptr - vm->text->addr + vm->text->data);
					vm->jmprel_addr = d[i].d_un.d_ptr;
					break;

				case DT_PLTGOT:
					logi("PLTGOT: %#lx\n", d[i].d_un.d_ptr);
					vm->pltgot = (uint64_t*)(d[i].d_un.d_ptr - vm->data->addr + vm->data->data);
					break;

				default:
					break;
			};
		}

		for (i = 0; i < vm->dynamic->ph.p_filesz / sizeof(Elf64_Dyn); i++) {

			if (DT_NEEDED == d[i].d_tag) {

				uint8_t* name = d[i].d_un.d_ptr + vm->dynstr;

				int j;
				for (j = 0; j < sizeof(somaps) / sizeof(somaps[0]); j++) {

					if (!strcmp(somaps[j][0], sys)
							&& !strcmp(somaps[j][1], name)) {
						name  = somaps[j][2];
						break;
					}
				}

				logi("needed: %s\n", name);

				void* so = dlopen(name, RTLD_LAZY);
				if (!so) {
					loge("dlopen error, so: %s\n", name);
					return -1;
				}

				if (vector_add(vm->sofiles, so) < 0) {
					dlclose(so);
					return -ENOMEM;
				}
			}
		}

		vm->pltgot[2] = (uint64_t)naja_vm_dynamic_link;
	}

	return 0;
}

static int __naja_add(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm14 = (inst >> 5) & 0x3fff;

		naja->regs[rd]  = naja->regs[rs0] + uimm14;

		NAJA_PRINTF("add    r%d, r%d, %lu\n", rd, rs0, uimm14);
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		switch (SH) {
			case 0:
				naja->regs[rd] = naja->regs[rs0] + (naja->regs[rs1] << uimm9);

				NAJA_PRINTF("add    r%d, r%d, r%d << %lu\n", rd, rs0, rs1, uimm9);
				break;

			case 1:
				naja->regs[rd] = naja->regs[rs0] + (naja->regs[rs1] >> uimm9);

				NAJA_PRINTF("add    r%d, r%d, r%d LSR %lu\n", rd, rs0, rs1, uimm9);
				break;
			default:
				naja->regs[rd] = naja->regs[rs0] + (((int64_t)naja->regs[rs1]) >> uimm9);

				NAJA_PRINTF("add    r%d, r%d, r%d ASR %lu\n", rd, rs0, rs1, uimm9);
				break;
		};
	}

	naja->ip += 4;
	return 0;
}

static int __naja_fadd(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;

	naja->fvec[rd].d[0]  = naja->fvec[rs0].d[0] + naja->fvec[rs1].d[0];

	NAJA_PRINTF("fadd   d%d, d%d, d%d\n", rd, rs0, rs1);

	naja->ip += 4;
	return 0;
}

static int __naja_fsub(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;

	naja->fvec[rd].d[0]  = naja->fvec[rs0].d[0] - naja->fvec[rs1].d[0];

	if (31 == rd) {
		double d = naja->fvec[rd].d[0];

		if (d > 0.0)
			naja->flags = 0x4;
		else if (d < 0.0)
			naja->flags = 0x2;
		else
			naja->flags = 0x1;

		NAJA_PRINTF("fcmp   d%d, d%d\n", rs0, rs1);
	} else
		NAJA_PRINTF("fsub   r%d, r%d, r%d\n", rd, rs0, rs1);

	naja->ip += 4;
	return 0;
}

static int __naja_sub(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm14 = (inst >> 5) & 0x3fff;

		naja->regs[rd]  = naja->regs[rs0] - uimm14;

		NAJA_PRINTF("sub    r%d, r%d, %lu\n", rd, rs0, uimm14);
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		switch (SH) {
			case 0:
				naja->regs[rd] = naja->regs[rs0] - (naja->regs[rs1] << uimm9);

				NAJA_PRINTF("sub    r%d, r%d, r%d << %lu\n", rd, rs0, rs1, uimm9);
				break;

			case 1:
				naja->regs[rd] = naja->regs[rs0] - (naja->regs[rs1] >> uimm9);

				NAJA_PRINTF("sub    r%d, r%d, r%d LSR %lu\n", rd, rs0, rs1, uimm9);
				break;
			default:
				naja->regs[rd] = naja->regs[rs0] - (((int64_t)naja->regs[rs1]) >> uimm9);

				NAJA_PRINTF("sub    r%d, r%d, r%d ASR %lu\n", rd, rs0, rs1, uimm9);
				break;
		}
	}

	if (31 == rd) { // cmp
		int ret = naja->regs[rd];

		if (0 == ret)
			naja->flags = 0x1;
		else if (ret > 0)
			naja->flags = 0x4;
		else
			naja->flags = 0x2;
	}

	naja->ip += 4;
	return 0;
}

static int __naja_mul(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rs2 = (inst >> 10) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int S   = (inst >> 18) & 0x1;
	int opt = (inst >> 19) & 0x3;

	if (S)
		naja->regs[rd] = (int64_t)naja->regs[rs0] * (int64_t)naja->regs[rs1];
	else
		naja->regs[rd] = naja->regs[rs0] * naja->regs[rs1];

	if (0 == opt) {
		naja->regs[rd] += naja->regs[rs2];

		NAJA_PRINTF("madd   r%d, r%d, r%d, r%d", rd, rs2, rs0, rs1);
	} else if (1 == opt) {
		naja->regs[rd]  = naja->regs[rs2] - naja->regs[rd];

		NAJA_PRINTF("msub   r%d, r%d, r%d, r%d", rd, rs2, rs0, rs1);
	} else {
		if (S)
			NAJA_PRINTF("smul   r%d, r%d, r%d", rd, rs0, rs1);
		else
			NAJA_PRINTF("mul    r%d, r%d, r%d", rd, rs0, rs1);
	}

	naja->ip += 4;
	return 0;
}

static int __naja_fmul(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rs2 = (inst >> 10) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int opt = (inst >> 18) & 0x3;

	naja->fvec[rd].d[0] = naja->fvec[rs0].d[0] * naja->fvec[rs1].d[0];

	if (0 == opt) {
		naja->fvec[rd].d[0] += naja->fvec[rs2].d[0];

		NAJA_PRINTF("fmadd   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);

	} else if (1 == opt) {
		naja->fvec[rd].d[0]  = naja->fvec[rs2].d[0] - naja->fvec[rd].d[0];

		NAJA_PRINTF("fmsub   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	} else
		NAJA_PRINTF("fmul    d%d, d%d, d%d", rd, rs0, rs1);

	naja->ip += 4;
	return 0;
}

static int __naja_fdiv(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rs2 = (inst >> 10) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int opt = (inst >> 18) & 0x3;

	naja->fvec[rd].d[0] = naja->fvec[rs0].d[0] / naja->fvec[rs1].d[0];

	if (0 == opt) {
		naja->fvec[rd].d[0] += naja->fvec[rs2].d[0];

		NAJA_PRINTF("fdadd   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);

	} else if (1 == opt) {
		naja->fvec[rd].d[0]  = naja->fvec[rs2].d[0] - naja->fvec[rd].d[0];

		NAJA_PRINTF("fdsub   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	} else
		NAJA_PRINTF("fdiv    d%d, d%d, d%d", rd, rs0, rs1);

	naja->ip += 4;
	return 0;
}

static int __naja_div(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rs2 = (inst >> 10) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int S   = (inst >> 18) & 0x1;
	int opt = (inst >> 19) & 0x3;

	if (S)
		naja->regs[rd] = (int64_t)naja->regs[rs0] / (int64_t)naja->regs[rs1];
	else
		naja->regs[rd] = naja->regs[rs0] / naja->regs[rs1];

	if (0 == opt) {
		naja->regs[rd] += naja->regs[rs2];

		NAJA_PRINTF("fdadd   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);

	} else if (1 == opt) {
		naja->regs[rd]  = naja->regs[rs2] - naja->regs[rd];

		NAJA_PRINTF("dsub   r%d, r%d, r%d, r%d", rd, rs2, rs0, rs1);
	} else {
		if (S)
			NAJA_PRINTF("sdiv   r%d, r%d, r%d", rd, rs0, rs1);
		else
			NAJA_PRINTF("div    r%d, r%d, r%d", rd, rs0, rs1);
	}

	naja->ip += 4;
	return 0;
}

static int __naja_mem(vm_t* vm, int64_t addr, uint8_t** pdata, int64_t* poffset)
{
	vm_naja_t* naja   = vm->priv;
	uint8_t*       data   = NULL;
	int64_t        offset = 0;

	if (addr >= (int64_t)vm->data->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->data->data + vm->data->len) {
			loge("\n");
			return -1;
		}

	} else if (addr >= (int64_t)vm->rodata->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->rodata->data + vm->rodata->len) {
			loge("\n");
			return -1;
		}

	} else if (addr >= (int64_t)vm->text->data) {
		data  = (uint8_t*)addr;

		if (addr >= (int64_t)vm->text->data + vm->text->len) {
			loge("\n");
			return -1;
		}

	} else if (addr  >= 0x800000) {
		data   = vm->data->data;
		offset = addr - vm->data->addr;

		if (offset >= vm->data->len) {
			loge("offset: %#lx, vm->data->len: %ld\n", offset, vm->data->len);
			return -1;
		}

	} else if (addr >= 0x600000) {
		data   = vm->rodata->data;
		offset = addr - vm->rodata->addr;

		if (offset >= vm->rodata->len) {
			loge("\n");
			return -1;
		}

	} else if (addr >= 0x400000) {
		data   = vm->text->data;
		offset = addr - vm->text->addr;

		if (offset >= vm->text->len) {
			loge("\n");
			return -1;
		}

	} else {
		data   = naja->stack;
		offset = addr;
	}

	*poffset = offset;
	*pdata   = data;
	return 0;
}

static int __naja_fstr_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s13 = (inst >>  5) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	logd("rd: %d, rb: %d, s13: %d, SH: %d\n", rd, rb, s13, SH);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << SH;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			loge("offset0: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			*(float*)(data + offset) = naja->fvec[rd].d[0];

			NAJA_PRINTF("fstr   f%d, [r%d, %d]\n", rd, rb, s13 << 2);
			break;

		case 3:
			*(double*)(data + offset) = naja->fvec[rd].d[0];

			NAJA_PRINTF("fstr    d%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_fpush(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset >= 0);

		offset += 1 << SH;

		if (naja->size < offset) {
			data = realloc(naja->stack, offset + STACK_INC);
			if (!data)
				return -ENOMEM;

			naja->stack = data;
			naja->size  = offset + STACK_INC;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			*(float*)(data + offset) = naja->fvec[rd].d[0];

			NAJA_PRINTF("fpush   f%d, [r%d]\n", rd, rb);
			break;

		case 3:
			*(double*)(data + offset) = naja->fvec[rd].d[0];

			NAJA_PRINTF("fpush   d%d, [r%d]\n", rd, rb);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	naja->regs[rb] -= 1 << SH;

	naja->ip += 4;
	return 0;
}

static int __naja_fldr_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s13 = (inst >>  5) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	logd("rd: %d, rb: %d, s13: %d, SH: %d\n", rd, rb, s13, SH);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << SH;

	if (data   == naja->stack) {
		offset = -offset;

		logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset > 0);

		if (naja->size < offset) {
			loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			naja->fvec[rd].d[0] = *(float*)(data + offset);

			NAJA_PRINTF("fldr    f%d, [r%d, %d], rd: %lg, rb: %ld, %p\n", rd, rb, s13 << 2, naja->fvec[rd].d[0], naja->regs[rb], data + offset);
			break;
		case 3:
			naja->fvec[rd].d[0] = *(double*)(data + offset);

			NAJA_PRINTF("fldr    d%d, [r%d, %d], rd: %lg, rb: %ld, %p\n", rd, rb, s13 << 3, naja->fvec[rd].d[0], naja->regs[rb], data + offset);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_fpop(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	logd("rd: %d, rb: %d, SH: %d\n", rd, rb, SH);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset = -offset;

		logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset > 0);

		if (naja->size < offset) {
			loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			naja->fvec[rd].d[0] = *(float*)(data + offset);

			NAJA_PRINTF("fldr    f%d, [r%d], rd: %lg, rb: %ld, %p\n", rd, rb, naja->fvec[rd].d[0], naja->regs[rb], data + offset);
			break;
		case 3:
			naja->fvec[rd].d[0] = *(double*)(data + offset);

			NAJA_PRINTF("fldr    d%d, [r%d], rd: %lg, rb: %ld, %p\n", rd, rb, naja->fvec[rd].d[0], naja->regs[rb], data + offset);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	naja->regs[rb] += 1 << SH;

	naja->ip += 4;
	return 0;
}

static int __naja_ldr_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;
	int s13 = (inst >>  5) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	logd("rd: %d, rb: %d, s13: %d, SH: %d\n", rd, rb, s13, SH);

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << SH;

	if (data   == naja->stack) {
		offset = -offset;

		logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset >= 0);

		if (naja->size < offset) {
			loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			if (s) {
				naja->regs[rd] = *(int8_t*)(data + offset);

				NAJA_PRINTF("ldrsb  r%d, [r%d, %d]\n", rd, rb, s13);
			} else {
				naja->regs[rd] = *(uint8_t*)(data + offset);

				NAJA_PRINTF("ldrb   r%d, [r%d, %d]\n", rd, rb, s13);
			}
			break;

		case 1:
			if (s) {
				naja->regs[rd] = *(int16_t*)(data + offset);

				NAJA_PRINTF("ldrsw  r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			} else {
				naja->regs[rd] = *(uint16_t*)(data + offset);

				NAJA_PRINTF("ldrw   r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			}
			break;

		case 2:
			if (s) {
				naja->regs[rd] = *(int32_t*)(data + offset);

				NAJA_PRINTF("ldrsl  r%d, [r%d, %d]\n", rd, rb, s13 << 2);
			} else {
				naja->regs[rd] = *(uint32_t*)(data + offset);

				NAJA_PRINTF("ldrl   r%d, [r%d, %d],  %ld, %p\n", rd, rb, s13 << 2, naja->regs[rd], data + offset);
			}
			break;

		default:
			naja->regs[rd] = *(uint64_t*)(data + offset);

			NAJA_PRINTF("ldr    r%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_pop(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset  = -offset;

		logd("offset0: %ld, size: %ld\n", offset, naja->size);
		assert(offset > 0);

		if (naja->size < offset) {
			loge("offset: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			if (s) {
				naja->regs[rd] = *(int8_t*)(data + offset);

				NAJA_PRINTF("popsb  r%d, [r%d]\n", rd, rb);
			} else {
				naja->regs[rd] = *(uint8_t*)(data + offset);

				NAJA_PRINTF("popb   r%d, [r%d]\n", rd, rb);
			}
			break;

		case 1:
			if (s) {
				naja->regs[rd] = *(int16_t*)(data + offset);

				NAJA_PRINTF("popsw  r%d, [r%d]\n", rd, rb);
			} else {
				naja->regs[rd] = *(uint16_t*)(data + offset);

				NAJA_PRINTF("popw   r%d, [r%d]\n", rd, rb);
			}
			break;

		case 2:
			if (s) {
				naja->regs[rd] = *(int32_t*)(data + offset);

				NAJA_PRINTF("popsl  r%d, [r%d]\n", rd, rb);
			} else {
				naja->regs[rd] = *(uint32_t*)(data + offset);

				NAJA_PRINTF("popl   r%d, [r%d],  %ld, %p\n", rd, rb, naja->regs[rd], data + offset);
			}
			break;

		default:
			naja->regs[rd] = *(uint64_t*)(data + offset);

			NAJA_PRINTF("popq   r%d, [r%d]\n", rd, rb);
			break;
	};

	naja->regs[rb] += 1 << SH;

	naja->ip += 4;
	return 0;
}

static int __naja_ldr_sib(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int ri  = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;
	int u8  = (inst >> 10) & 0xff;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += (naja->regs[ri] << u8);

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset >= 0);

		if (naja->size < offset) {
			loge("\n");
			return -1;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			if (s) {
				NAJA_PRINTF("ldrsb r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(int8_t*)(data + offset);
			} else {
				NAJA_PRINTF("ldrb  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(uint8_t*)(data + offset);
			}
			break;

		case 1:
			if (s) {
				NAJA_PRINTF("ldrsw r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(int16_t*)(data + offset);
			} else {
				NAJA_PRINTF("ldrw  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(uint16_t*)(data + offset);
			}
			break;

		case 2:
			if (s) {
				NAJA_PRINTF("ldrsl r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(int32_t*)(data + offset);
			} else {
				NAJA_PRINTF("ldrl  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

				naja->regs[rd] = *(uint32_t*)(data + offset);
			}
			break;
		default:
			NAJA_PRINTF("ldr   r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			naja->regs[rd] = *(uint64_t*)(data + offset);
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_fldr_sib(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int ri  = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int u8  = (inst >> 10) & 0xff;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += naja->regs[ri] << u8;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			loge("\n");
			return -1;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			NAJA_PRINTF("fldr  f%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			naja->fvec[rd].d[0] = *(float*)(data + offset);
			break;

		case 3:
			NAJA_PRINTF("fldr  d%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			naja->fvec[rd].d[0] = *(double*)(data + offset);
			break;
		default:
			loge("\n");
			return -1;
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_fstr_sib(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int ri  = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int u8  = (inst >> 10) & 0xff;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += naja->regs[ri] << u8;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			loge("\n");
			return -1;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 2:
			NAJA_PRINTF("fstr  f%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(float*)(data + offset) = naja->fvec[rd].d[0];
			break;

		case 3:
			NAJA_PRINTF("fstr  d%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(double*)(data + offset) = naja->fvec[rd].d[0];
			break;
		default:
			loge("\n");
			return -1;
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_str_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s13 = (inst >>  5) & 0x1fff;

	if (s13  & 0x1000)
		s13 |= 0xffffe000;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += s13 << SH;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset > 0);

		if (naja->size < offset) {
			loge("offset0: %ld, size: %ld\n", offset, naja->size);
			return -EINVAL;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			*(uint8_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("strb   r%d, [r%d, %d]\n", rd, rb, s13);
			break;

		case 1:
			*(uint16_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("strw   r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			break;

		case 2:
			*(uint32_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("strl   r%d, [r%d, %d],  s13: %d, %d, %p\n", rd, rb, s13 << 2, s13, *(uint32_t*)(data + offset), data + offset);
			break;

		default:
			*(uint64_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("str    r%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_push(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	if (data   == naja->stack) {
		offset = -offset;

		logd("offset0: %ld, size: %ld\n", offset, naja->size);

		assert(offset >= 0);

		offset += 1 << SH;

		if (naja->size < offset) {
			data = realloc(naja->stack, offset + STACK_INC);
			if (!data)
				return -ENOMEM;

			naja->stack = data;
			naja->size  = offset + STACK_INC;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			*(uint8_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("pushb  r%d, [r%d]\n", rd, rb);
			break;

		case 1:
			*(uint16_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("pushw  r%d, [r%d]\n", rd, rb);
			break;

		case 2:
			*(uint32_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("pushl  r%d, [r%d], %d, %p\n", rd, rb, *(uint32_t*)(data + offset), data + offset);
			break;

		default:
			*(uint64_t*)(data + offset) = naja->regs[rd];

			NAJA_PRINTF("pushq  r%d, [r%d]\n", rd, rb);
			break;
	};

	naja->regs[rb] -= 1 << SH;

	naja->ip += 4;
	return 0;
}

static int __naja_str_sib(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int ri  = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int u8  = (inst >> 10) & 0xff;

	int64_t  addr   = naja->regs[rb];
	int64_t  offset = 0;
	uint8_t* data   = NULL;

	int ret = __naja_mem(vm, addr, &data, &offset);
	if (ret < 0)
		return ret;

	offset += naja->regs[ri] << u8;

	if (data   == naja->stack) {
		offset = -offset;

		assert(offset >= 0);

		if (naja->size < offset) {
			loge("\n");
			return -1;
		}

		offset -= 1 << SH;
	}

	switch (SH) {
		case 0:
			NAJA_PRINTF("strb  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(uint8_t*)(data + offset) = naja->regs[rd];
			break;

		case 1:
			NAJA_PRINTF("strw  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(uint16_t*)(data + offset) = naja->regs[rd];
			break;

		case 2:
			NAJA_PRINTF("strl  r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(uint32_t*)(data + offset) = naja->regs[rd];
			break;

		default:
			NAJA_PRINTF("str   r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);

			*(uint64_t*)(data + offset) = naja->regs[rd];
			break;
	};

	naja->ip += 4;
	return 0;
}

static int __naja_and(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm14 = (inst >> 5) & 0x3fff;

		naja->regs[rd]  = naja->regs[rs0] & uimm14;
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		if (0 == SH)
			naja->regs[rd]  = naja->regs[rs0] & (naja->regs[rs1] << uimm9);
		else if (1 == SH)
			naja->regs[rd]  = naja->regs[rs0] & (naja->regs[rs1] >> uimm9);
		else
			naja->regs[rd]  = naja->regs[rs0] & (((int64_t)naja->regs[rs1]) >> uimm9);
	}

	if (31 == rd)
		naja->flags = !naja->regs[rd];

	naja->ip += 4;
	return 0;
}

static int __naja_or(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm14 = (inst >> 5) & 0x3fff;

		naja->regs[rd]  = naja->regs[rs0] | uimm14;
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		if (0 == SH)
			naja->regs[rd]  = naja->regs[rs0] | (naja->regs[rs1] << uimm9);
		else if (1 == SH)
			naja->regs[rd]  = naja->regs[rs0] | (naja->regs[rs1] >> uimm9);
		else
			naja->regs[rd]  = naja->regs[rs0] | (((int64_t)naja->regs[rs1]) >> uimm9);
	}

	naja->ip += 4;
	return 0;
}

static int __naja_jmp_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	naja->ip += simm26 << 2;
	NAJA_PRINTF("jmp    %#lx\n", naja->ip);
	return 0;
}

static int __naja_call_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	naja->regs[NAJA_REG_LR] = naja->ip + 4;

	naja->ip += simm26 << 2;

	NAJA_PRINTF("call   %#lx\n", naja->ip);
	return 0;
}

static int __naja_jmp_reg(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	if (inst & 0x1) {

		int cc  = (inst >> 1) & 0xf;
		int s21 = (inst >> 5) & 0x1fffff;

		if (s21  & 0x100000)
			s21 |= 0xffe00000;

		s21 <<= 2;

		if (0 == (cc & naja->flags))
			naja->ip  += s21;
		else
			naja->ip  += 4;

		if (0 == cc)
			NAJA_PRINTF("jz     %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (1 == cc)
			NAJA_PRINTF("jnz    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (2 == cc)
			NAJA_PRINTF("jge    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (3 == cc)
			NAJA_PRINTF("jgt    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (4 == cc)
			NAJA_PRINTF("jle    %#lx, flags: %#lx\n", naja->ip, naja->flags);

		else if (5 == cc)
			NAJA_PRINTF("jlt    %#lx, flags: %#lx\n", naja->ip, naja->flags);
		else {
			loge("\n");
			return -EINVAL;
		}

	} else {
		int rd = (inst >> 21) & 0x1f;

		if (naja_vm_dynamic_link == (void*)naja->regs[rd]) {

			NAJA_PRINTF("\033[36mjmp    r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

			int ret = naja_vm_dynamic_link(vm);
			if (ret < 0) {
				loge("\n");
				return ret;
			}

			naja->ip = naja->regs[NAJA_REG_LR];

		} else if (naja->regs[rd] < vm->text->addr
				|| naja->regs[rd] > vm->text->addr + vm->text->len) {

			NAJA_PRINTF("\033[36mjmp    r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

			dyn_func_pt f = (void*)naja->regs[rd];

			naja->regs[0] = f(naja->regs[0],
					naja->regs[1],
					naja->regs[2],
					naja->regs[3],
					naja->regs[4],
					naja->regs[5],
					naja->regs[6],
					naja->regs[7],
					naja->fvec[0].d[0],
					naja->fvec[1].d[0],
					naja->fvec[2].d[0],
					naja->fvec[3].d[0],
					naja->fvec[4].d[0],
					naja->fvec[5].d[0],
					naja->fvec[6].d[0],
					naja->fvec[7].d[0]);

			naja->ip = naja->regs[NAJA_REG_LR];
		} else {
			naja->ip = naja->regs[rd];

			NAJA_PRINTF("jmp    r%d, %#lx\n", rd, naja->regs[rd]);
		}
	}

	return 0;
}


static int __naja_call_reg(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;

	naja->regs[NAJA_REG_LR]   = naja->ip + 4;

	if (naja_vm_dynamic_link == (void*)naja->regs[rd]) {

		NAJA_PRINTF("\033[36mcall  r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

		int ret = naja_vm_dynamic_link(vm);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

		naja->ip = naja->regs[NAJA_REG_LR];

	} else if (naja->regs[rd] < vm->text->addr
			|| naja->regs[rd] > vm->text->addr + vm->text->len) {

		NAJA_PRINTF("\033[36mcall  r%d, %#lx@plt\033[0m\n", rd, naja->regs[rd]);

		dyn_func_pt f = (void*)naja->regs[rd];

		naja->regs[0]  = f(naja->regs[0],
				naja->regs[1],
				naja->regs[2],
				naja->regs[3],
				naja->regs[4],
				naja->regs[5],
				naja->regs[6],
				naja->regs[7],
				naja->fvec[0].d[0],
				naja->fvec[1].d[0],
				naja->fvec[2].d[0],
				naja->fvec[3].d[0],
				naja->fvec[4].d[0],
				naja->fvec[5].d[0],
				naja->fvec[6].d[0],
				naja->fvec[7].d[0]);

		naja->ip = naja->regs[NAJA_REG_LR];
	} else {
		NAJA_PRINTF("call  r%d, %#lx\n", rd, naja->regs[rd]);
		naja->ip = naja->regs[rd];
	}

	return 0;
}

static int __naja_adrp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd  = (inst >> 21) & 0x1f;
	int s21 =  inst & 0x1fffff;

	if (s21  & 0x100000)
		s21 |= ~0x1fffff;

	naja->regs[rd] = (naja->ip + ((int64_t)s21 << 14)) & ~0x3fffULL;

	if (naja->regs[rd] >= 0x800000)
		naja->regs[rd]  = naja->regs[rd] - vm->data->addr + (uint64_t)vm->data->data;

	else if (naja->regs[rd] >= 0x600000)
		naja->regs[rd]  = naja->regs[rd] - vm->rodata->addr + (uint64_t)vm->rodata->data;

	else if (naja->regs[rd] >= 0x400000)
		naja->regs[rd]  = naja->regs[rd] - vm->text->addr + (uint64_t)vm->text->data;

	NAJA_PRINTF("adrp   r%d, [rip, %d],  %#lx\n", rd, s21, naja->regs[rd]);

	naja->ip += 4;
	return 0;
}

static int __naja_ret(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	naja->ip   =  naja->regs[NAJA_REG_LR];
	int64_t sp = -naja->regs[NAJA_REG_SP];

	assert (sp >= 0);

	if (naja->size > sp + STACK_INC) {

		void* p = realloc(naja->stack, sp + STACK_INC);
		if (!p) {
			loge("\n");
			return -ENOMEM;
		}

		naja->stack = p;
		naja->size  = sp + STACK_INC;
	}

	NAJA_PRINTF("ret,   %#lx, sp: %ld, stack->size: %ld\n", naja->ip, sp, naja->size);
	return 0;
}

static int __naja_setcc(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;
	int cc = (inst >>  1) & 0xf;

	naja->regs[rd] = 0 == (cc & naja->flags);

	if (VM_Z == cc)
		NAJA_PRINTF("setz   r%d\n", rd);

	else if (VM_NZ == cc)
		NAJA_PRINTF("setnz  r%d\n", rd);

	else if (VM_GE == cc)
		NAJA_PRINTF("setge  r%d\n", rd);

	else if (VM_GT == cc)
		NAJA_PRINTF("setgt  r%d\n", rd);

	else if (VM_LT == cc)
		NAJA_PRINTF("setlt  r%d\n", rd);

	else if (VM_LE == cc)
		NAJA_PRINTF("setle  r%d\n", rd);
	else {
		loge("inst: %#x\n", inst);
		return -EINVAL;
	}

	naja->ip  += 4;
	return 0;
}

static int __naja_mov(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;
	int opt = (inst >> 16) & 0x3;

	if (0 == opt) {
		int rs0 =  inst       & 0x1f;
		int rs1 = (inst >> 5) & 0x1f;

		if (0 == SH) {
			naja->regs[rd]  = naja->regs[rs0] << naja->regs[rs1];

			NAJA_PRINTF("mov    r%d, r%d LSL r%d\n", rd, rs0, rs1);

		} else if (1 == SH) {
			naja->regs[rd]  = naja->regs[rs0] >> naja->regs[rs1];

			NAJA_PRINTF("mov    r%d, r%d LSR r%d\n", rd, rs0, rs1);
		} else {
			naja->regs[rd]  = (int64_t)naja->regs[rs0] >> naja->regs[rs1];

			NAJA_PRINTF("mov    r%d, r%d ASR r%d\n", rd, rs0, rs1);
		}

	} else if (1 == opt) {
		int rs0 =  inst       & 0x1f;
		int u11 = (inst >> 5) & 0x7ff;

		if (0 == SH) {
			naja->regs[rd]  = naja->regs[rs0] << u11;

			if (0 == u11)
				NAJA_PRINTF("mov    r%d, r%d\n", rd, rs0);
			else
				NAJA_PRINTF("mov    r%d, r%d LSL %d\n", rd, rs0, u11);

		} else if (1 == SH) {
			naja->regs[rd]  = naja->regs[rs0] >> u11;

			NAJA_PRINTF("mov    r%d, r%d LSR %d\n", rd, rs0, u11);
		} else {
			naja->regs[rd]  = (int64_t)naja->regs[rs0] >> u11;

			NAJA_PRINTF("mov    r%d, r%d ASR %d\n", rd, rs0, u11);
		}

	} else if (2 == opt) {
		int rs = inst & 0x1f;

		switch (SH) {
			case 0:
				if (s) {
					NAJA_PRINTF("movsb  r%d, r%d\n", rd, rs);

					naja->regs[rd] = (int8_t)naja->regs[rs];
				} else {
					naja->regs[rd] = (uint8_t)naja->regs[rs];

					NAJA_PRINTF("movzb  r%d, r%d\n", rd, rs);
				}
				break;

			case 1:
				if (s) {
					NAJA_PRINTF("movsw  r%d, r%d\n", rd, rs);

					naja->regs[rd] = (int16_t)naja->regs[rs];
				} else {
					naja->regs[rd] = (uint16_t)naja->regs[rs];

					NAJA_PRINTF("movzw  r%d, r%d\n", rd, rs);
				}
				break;

			case 2:
				if (s) {
					NAJA_PRINTF("movsl  r%d, r%d\n", rd, rs);

					naja->regs[rd] = (int32_t)naja->regs[rs];
				} else {
					naja->regs[rd] = (uint32_t)naja->regs[rs];

					NAJA_PRINTF("movzl  r%d, r%d\n", rd, rs);
				}
				break;
			default:
				if (s) {
					NAJA_PRINTF("NEG    r%d, r%d\n", rd, rs);

					naja->regs[rd] = -naja->regs[rs];
				} else {
					naja->regs[rd] = ~naja->regs[rs];

					NAJA_PRINTF("NOT    r%d, r%d\n", rd, rs);
				}
				break;
		};

	} else {
		uint64_t u16 = inst & 0xffff;

		if (s) {
			naja->regs[rd] = ~u16;

			NAJA_PRINTF("mvn    r%d, %#lx\n", rd, u16);
		} else {
			if (0 == SH) {
				NAJA_PRINTF("mov    r%d, %#lx\n", rd, u16);

				naja->regs[rd] = u16;
			} else {
				naja->regs[rd] |= u16 << SH;

				NAJA_PRINTF("mov    r%d, %#lx << %d\n", rd, u16, 16 << SH);
			}
		}
	}

	naja->ip += 4;
	return 0;
}

static int __naja_fmov(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs  =  inst & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;
	int opt = (inst >> 16) & 0x3;

	if (0 == opt) {
		if (2 == SH) {
			naja->fvec[rd].d[0] = naja->fvec[rs].d[0];

			NAJA_PRINTF("fss2sd   d%d, f%d\n", rd, rs);

		} else if (3 == SH) {
			naja->fvec[rd].d[0] = naja->fvec[rs].d[0];

			NAJA_PRINTF("fsd2ss   f%d, d%d\n", rd, rs);
		} else {
			loge("\n");
			return -EINVAL;
		}

	} else if (1 == opt) {
		if (s) {
			naja->regs[rd] = (int64_t)naja->fvec[rs].d[0];

			NAJA_PRINTF("cvtss2si r%d, f%d\n", rd, rs);
		} else {
			naja->regs[rd] = (uint64_t)naja->fvec[rs].d[0];

			NAJA_PRINTF("cvtss2ui r%d, f%d\n", rd, rs);
		}

	} else if (2 == opt) {

		naja->fvec[rd].d[0] = (double)naja->regs[rs];

		NAJA_PRINTF("cvtsi2ss f%d, r%d\n", rd, rs);

	} else {
		if (s) {
			NAJA_PRINTF("fneg     d%d, d%d\n", rd, rs);

			naja->fvec[rd].d[0] = -naja->fvec[rs].d[0];
		} else {
			naja->fvec[rd].d[0] =  naja->fvec[rs].d[0];

			NAJA_PRINTF("fmov     d%d, d%d\n", rd, rs);
		}
	}

	naja->ip += 4;
	return 0;
}

static naja_opcode_pt  naja_opcodes[64] =
{
	__naja_add,      // 0
	__naja_sub,      // 1
	__naja_mul,      // 2
	__naja_div,      // 3

	__naja_ldr_disp, // 4
	__naja_pop,      // 5

	__naja_str_disp, // 6
	__naja_push,     // 7

	__naja_and,      // 8
	__naja_or,       // 9
	__naja_jmp_disp, // 10
	__naja_jmp_reg,  // 11
	__naja_setcc,    // 12
	__naja_ldr_sib,  // 13
	__naja_str_sib,  // 14
	__naja_mov,      // 15

	__naja_fadd,     // 16
	__naja_fsub,     // 17
	__naja_fmul,     // 18
	__naja_fdiv,     // 19

	__naja_fldr_disp,// 20
	__naja_fpop,     // 21

	__naja_fstr_disp,// 22
	__naja_fpush,    // 23

	NULL,            // 24
	NULL,            // 25
	__naja_call_disp,// 26
	__naja_call_reg, // 27
	NULL,            // 28
	__naja_fldr_sib, // 29
	__naja_fstr_sib, // 30
	__naja_fmov,     // 31

	NULL,            // 32
	NULL,            // 33
	NULL,            // 34
	NULL,            // 35
	NULL,            // 36
	NULL,            // 37
	NULL,            // 38
	NULL,            // 39
	NULL,            // 40
	NULL,            // 41
	__naja_adrp,     // 42
	NULL,            // 43
	NULL,            // 44
	NULL,            // 45
	NULL,            // 46
	NULL,            // 47

	NULL,            // 48
	NULL,            // 49
	NULL,            // 50
	NULL,            // 51
	NULL,            // 52
	NULL,            // 53
	NULL,            // 54
	NULL,            // 55
	__naja_ret,      // 56
	NULL,            // 57
	NULL,            // 58
	NULL,            // 59
	NULL,            // 60
	NULL,            // 61
	NULL,            // 62
	NULL,            // 63
};

static void __naja_vm_exit()
{
}

static int __naja_vm_run(vm_t* vm, const char* path, const char* sys)
{
	vm_naja_t* naja = vm->priv;
	Elf64_Ehdr     eh;
	Elf64_Shdr     sh;

	fseek(vm->elf->fp, 0, SEEK_SET);

	int ret  = fread(&eh, sizeof(Elf64_Ehdr), 1, vm->elf->fp);
	if (ret != 1)
		return -1;

	if (vm->jmprel) {
		fseek(vm->elf->fp, eh.e_shoff, SEEK_SET);

		int i;
		for (i = 0; i < eh.e_shnum; i++) {

			ret = fread(&sh, sizeof(Elf64_Shdr), 1, vm->elf->fp);
			if (ret != 1)
				return -1;

			if (vm->jmprel_addr == sh.sh_addr) {
				vm->jmprel_size  = sh.sh_size;
				break;
			}
		}

		if (i == eh.e_shnum) {
			loge("\n");
			return -1;
		}
	}

	naja->stack  = calloc(STACK_INC, sizeof(uint64_t));
	if (!naja->stack)
		return -ENOMEM;

	naja->size   = STACK_INC;
	naja->_start = eh.e_entry;
	naja->ip     = eh.e_entry;

	naja->regs[NAJA_REG_LR] = (uint64_t)__naja_vm_exit;

	int n = 0;
	while ((uint64_t)__naja_vm_exit != naja->ip) {

		int64_t offset = naja->ip - vm->text->addr;

		if (offset >= vm->text->len) {
			loge("naja->ip: %#lx, %p\n", naja->ip, __naja_vm_exit);
			return -1;
		}

		uint32_t inst = *(uint32_t*)(vm->text->data + offset);

		naja_opcode_pt pt = naja_opcodes[(inst >> 26) & 0x3f];

		if (!pt) {
			loge("inst: %d, %#x\n", (inst >> 26) & 0x3f, inst);
			return -EINVAL;
		}

		NAJA_PRINTF("%4d, %#lx: ", n++, naja->ip);
		ret = pt(vm, inst);
		if (ret < 0) {
			loge("\n");
			return ret;
		}

//		usleep(10 * 1000);
	}

	logw("r0: %ld, sizeof(fv256_t): %ld\n", naja->regs[0], sizeof(fv256_t));
	return naja->regs[0];
}

static int naja_vm_run(vm_t* vm, const char* path, const char* sys)
{
	int ret = naja_vm_init(vm, path, sys);
	if (ret < 0) {
		loge("\n");
		return ret;
	}

	return __naja_vm_run(vm, path, sys);
}

vm_ops_t  vm_ops_naja =
{
	.name  = "naja",
	.open  = naja_vm_open,
	.close = naja_vm_close,
	.run   = naja_vm_run,
};
