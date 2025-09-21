#include"vm.h"

static int __naja_add(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint64_t uimm14 = (inst >> 5) & 0x3fff;
		printf("add      r%d, r%d, %lu\n", rd, rs0, uimm14);
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		switch (SH) {
			case 0:
				printf("add      r%d, r%d, r%d LSL %lu\n", rd, rs0, rs1, uimm9);
				break;
			case 1:
				printf("add      r%d, r%d, r%d LSR %lu\n", rd, rs0, rs1, uimm9);
				break;
			default:
				printf("add      r%d, r%d, r%d ASR %lu\n", rd, rs0, rs1, uimm9);
				break;
		};
	}

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

		if (31 == rd)
			printf("cmp      r%d, %lu\n", rs0, uimm14);
		else
			printf("sub      r%d, r%d, %lu\n", rd, rs0, uimm14);
	} else {
		uint64_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		switch (SH) {
			case 0:
				if (31 == rd)
					printf("cmp      r%d, r%d << %lu\n", rs0, rs1, uimm9);
				else
					printf("sub      r%d, r%d, r%d << %lu\n", rd, rs0, rs1, uimm9);
				break;

			case 1:
				if (31 == rd)
					printf("cmp      r%d, r%d LSR %lu\n", rs0, rs1, uimm9);
				else
					printf("sub      r%d, r%d, r%d LSR %lu\n", rd, rs0, rs1, uimm9);
				break;

			default:
				if (31 == rd)
					printf("cmp      r%d, r%d ASR %lu\n", rs0, rs1, uimm9);
				else
					printf("sub      r%d, r%d, r%d ASR %lu\n", rd, rs0, rs1, uimm9);
				break;
		};
	}

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

	if (S) {
		if (0 == opt)
			printf("smadd    r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else if (1 == opt)
			printf("smsub    r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else
			printf("smul     r%d, r%d, r%d\n", rd, rs0, rs1);
	} else {
		if (0 == opt)
			printf("madd     r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else if (1 == opt)
			printf("msub     r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else
			printf("mul      r%d, r%d, r%d\n", rd, rs0, rs1);
	}

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

	if (S) {
		if (0 == opt)
			printf("sdadd    r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else if (1 == opt)
			printf("sdsub    r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else
			printf("sdiv     r%d, r%d, r%d\n", rd, rs0, rs1);
	} else {
		if (0 == opt)
			printf("dadd     r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else if (1 == opt)
			printf("dsub     r%d, r%d, r%d, r%d\n", rd, rs2, rs0, rs1);
		else
			printf("div      r%d, r%d, r%d\n", rd, rs0, rs1);
	}

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

	switch (SH) {
		case 0:
			if (s)
				printf("ldrsb    r%d, [r%d, %d]\n", rd, rb, s13);
			else
				printf("ldrb     r%d, [r%d, %d]\n", rd, rb, s13);
			break;

		case 1:
			if (s)
				printf("ldrsw    r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			else
				printf("ldrw     r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			break;

		case 2:
			if (s)
				printf("ldrsl    r%d, [r%d, %d]\n", rd, rb, s13 << 2);
			else
				printf("ldrl     r%d, [r%d, %d]\n", rd, rb, s13 << 2);
			break;

		default:
			printf("ldr      r%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
	};

	return 0;
}

static int __naja_pop(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;
	int s   = (inst >> 18) & 0x1;

	switch (SH) {
		case 0:
			if (s)
				printf("popsb    r%d, [r%d]\n", rd, rb);
			else
				printf("popb     r%d, [r%d]\n", rd, rb);
			break;

		case 1:
			if (s)
				printf("popsw    r%d, [r%d]\n", rd, rb);
			else
				printf("popw     r%d, [r%d]\n", rd, rb);
			break;

		case 2:
			if (s)
				printf("popsl    r%d, [r%d]\n", rd, rb);
			else
				printf("popl     r%d, [r%d]\n", rd, rb);
			break;

		default:
			printf("popq     r%d, [r%d]\n", rd, rb);
			break;
	};

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

	switch (SH) {
		case 0:
			if (s)
				printf("ldrsb   r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			else
				printf("ldrb    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		case 1:
			if (s)
				printf("ldrsw   r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			else
				printf("ldrw    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		case 2:
			if (s)
				printf("ldrsl   r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			else
				printf("ldrl    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		default:
			printf("ldr     r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
	};

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

	switch (SH) {
		case 0:
			printf("strb     r%d, [r%d, %d]\n", rd, rb, s13);
			break;

		case 1:
			printf("strw     r%d, [r%d, %d]\n", rd, rb, s13 << 1);
			break;

		case 2:
			printf("strl     r%d, [r%d, %d]\n", rd, rb, s13 << 2);
			break;

		default:
			printf("str      r%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
	};

	return 0;
}

static int __naja_push(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	switch (SH) {
		case 0:
			printf("pushb    r%d, [r%d]\n", rd, rb);
			break;

		case 1:
			printf("pushw    r%d, [r%d]\n", rd, rb);
			break;

		case 2:
			printf("pushl    r%d, [r%d]\n", rd, rb);
			break;

		default:
			printf("pushq    r%d, [r%d]\n", rd, rb);
			break;
	};

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

	switch (SH) {
		case 0:
			printf("strb    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		case 1:
			printf("strw    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		case 2:
			printf("strl    r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;

		default:
			printf("str     r%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
	};

	return 0;
}

static int __naja_and(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint32_t uimm14 = (inst >> 5) & 0x3fff;

		if (31 == rd)
			printf("teq     r%d, %#x\n", rs0, uimm14);
		else
			printf("and     r%d, r%d, %#x\n", rd, rs0, uimm14);
	} else {
		uint32_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		switch (SH) {
			case 0:
				if (31 == rd)
					printf("teq     r%d, r%d LSL %#x\n", rs0, rs1, uimm9);
				else
					printf("and     r%d, r%d, r%d LSL %#x\n", rd, rs0, rs1, uimm9);
				break;

			case 1:
				if (31 == rd)
					printf("teq     r%d, r%d LSR %#x\n", rs0, rs1, uimm9);
				else
					printf("and     r%d, r%d, r%d LSR %#x\n", rd, rs0, rs1, uimm9);
				break;

			default:
				if (31 == rd)
					printf("teq     r%d, r%d ASR %#x\n", rs0, rs1, uimm9);
				else
					printf("and     r%d, r%d, r%d ASR %#x\n", rd, rs0, rs1, uimm9);
				break;
		};
	}

	return 0;
}

static int __naja_or(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	if (0x3 == SH) {
		uint32_t uimm14 = (inst >> 5) & 0x3fff;

		printf("or      r%d, r%d, %#x\n", rd, rs0, uimm14);
	} else {
		uint32_t uimm9 = (inst >> 10) & 0x1ff;
		int      rs1   = (inst >>  5) & 0x1f;

		if (0 == SH)
			printf("or      r%d, r%d, r%d LSL %#x\n", rd, rs0, rs1, uimm9);
		else if (1 == SH)
			printf("or      r%d, r%d, r%d LSR %#x\n", rd, rs0, rs1, uimm9);
		else
			printf("or      r%d, r%d, r%d ASR %#x\n", rd, rs0, rs1, uimm9);
	}

	return 0;
}

static int __naja_jmp_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	uint64_t ip = naja->ip + (simm26 << 2);
	printf("jmp    %#lx\n", ip);
	return 0;
}

static int __naja_call_disp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int simm26 = inst & 0x3ffffff;

	if (simm26  & 0x2000000)
		simm26 |= 0xfc000000;

	uint64_t ip = naja->ip + (simm26 << 2);
	printf("call     %#lx\n", ip);
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

		uint64_t ip = naja->ip + s21;

		if (0 == cc)
			printf("jz       %#lx\n", ip);

		else if (1 == cc)
			printf("jnz      %#lx\n", ip);

		else if (2 == cc)
			printf("jge      %#lx\n", ip);

		else if (3 == cc)
			printf("jgt      %#lx\n", ip);

		else if (4 == cc)
			printf("jle      %#lx\n", ip);

		else if (5 == cc)
			printf("jlt      %#lx\n", ip);
		else {
			loge("\n");
			return -EINVAL;
		}

	} else {
		int rd = (inst >> 21) & 0x1f;

		printf("jmp      *r%d\n", rd);
	}
	return 0;
}

static int __naja_call_reg(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;

	printf("call    r%d\n", rd);

	return 0;
}

static int __naja_adrp(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd  = (inst >> 21) & 0x1f;
	int s21 =  inst & 0x1fffff;

	if (s21  & 0x100000)
		s21 |= ~0x1fffff;

	printf("adrp     r%d, [rip, %d]\n", rd, s21);

	return 0;
}

static int __naja_ret(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	printf("ret\n");
	return 0;
}

static int __naja_setcc(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rd = (inst >> 21) & 0x1f;
	int cc = (inst >>  1) & 0xf;

	if (VM_Z == cc)
		printf("setz     r%d\n", rd);

	else if (VM_NZ == cc)
		printf("setnz    r%d\n", rd);

	else if (VM_GE == cc)
		printf("setge    r%d\n", rd);

	else if (VM_GT == cc)
		printf("setgt    r%d\n", rd);

	else if (VM_LT == cc)
		printf("setlt    r%d\n", rd);

	else if (VM_LE == cc)
		printf("setle    r%d\n", rd);
	else {
		loge("inst: %#x\n", inst);
		return -EINVAL;
	}

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

		if (0 == SH)
			printf("mov      r%d, r%d LSL r%d\n", rd, rs0, rs1);
		else if (1 == SH)
			printf("mov      r%d, r%d LSR r%d\n", rd, rs0, rs1);
		else
			printf("mov      r%d, r%d ASR r%d\n", rd, rs0, rs1);

	} else if (1 == opt) {
		int rs0 =  inst       & 0x1f;
		int u11 = (inst >> 5) & 0x7ff;

		if (0 == SH) {
			if (0 == u11)
				printf("mov      r%d, r%d\n", rd, rs0);
			else
				printf("mov      r%d, r%d LSL %d\n", rd, rs0, u11);

		} else if (1 == SH)
			printf("mov      r%d, r%d LSR %d\n", rd, rs0, u11);
		else
			printf("mov      r%d, r%d ASR %d\n", rd, rs0, u11);

	} else if (2 == opt) {
		int rs = inst & 0x1f;

		switch (SH) {
			case 0:
				if (s)
					printf("movsb    r%d, r%d\n", rd, rs);
				else
					printf("movzb    r%d, r%d\n", rd, rs);
				break;

			case 1:
				if (s)
					printf("movsw    r%d, r%d\n", rd, rs);
				else
					printf("movzw    r%d, r%d\n", rd, rs);
				break;

			case 2:
				if (s)
					printf("movsl    r%d, r%d\n", rd, rs);
				else
					printf("movzl    r%d, r%d\n", rd, rs);
				break;
			default:
				if (s)
					printf("NEG      r%d, r%d\n", rd, rs);
				else
					printf("NOT      r%d, r%d\n", rd, rs);
				break;
		};

	} else {
		uint64_t u16 = inst & 0xffff;

		if (s)
			printf("mvn      r%d, %#lx\n", rd, u16);
		else {
			if (0 == SH)
				printf("mov      r%d, %#lx\n", rd, u16);
			else
				printf("mov      r%d, %#lx << %d\n", rd, u16, 16 << SH);
		}
	}

	return 0;
}

static int __naja_fadd(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;

	printf("fadd   r%d, r%d, r%d\n", rd, rs0, rs1);
	return 0;
}

static int __naja_fsub(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rs0 =  inst        & 0x1f;
	int rs1 = (inst >>  5) & 0x1f;
	int rd  = (inst >> 21) & 0x1f;

	if (31 == rd)
		printf("fcmp   d%d, d%d\n", rs0, rs1);
	else
		printf("fsub   r%d, r%d, r%d\n", rd, rs0, rs1);

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

	if (0 == opt)
		printf("fmadd   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	else if (1 == opt)
		printf("fmsub   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	else
		printf("fmul    d%d, d%d, d%d", rd, rs0, rs1);

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

	if (0 == opt)
		printf("fdadd   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	else if (1 == opt)
		printf("fdsub   d%d, d%d, d%d, d%d", rd, rs2, rs0, rs1);
	else
		printf("fdiv    d%d, d%d, d%d", rd, rs0, rs1);

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

	switch (SH) {
		case 2:
			printf("fstr     f%d, [r%d, %d]\n", rd, rb, s13 << 2);
			break;

		case 3:
			printf("fstr     d%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	return 0;
}

static int __naja_fpush(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	switch (SH) {
		case 2:
			printf("fpush    f%d, [r%d]\n", rd, rb);
			break;

		case 3:
			printf("fpush    d%d, [r%d]\n", rd, rb);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

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

	switch (SH) {
		case 2:
			printf("fldr     f%d, [r%d, %d]\n", rd, rb, s13 << 2);
			break;

		case 3:
			printf("fldr     d%d, [r%d, %d]\n", rd, rb, s13 << 3);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

	return 0;
}

static int __naja_fpop(vm_t* vm, uint32_t inst)
{
	vm_naja_t* naja = vm->priv;

	int rb  =  inst        & 0x1f;
	int rd  = (inst >> 21) & 0x1f;
	int SH  = (inst >> 19) & 0x3;

	switch (SH) {
		case 2:
			printf("fldr    f%d, [r%d]\n", rd, rb);
			break;
		case 3:
			printf("fldr    d%d, [r%d]\n", rd, rb);
			break;
		default:
			loge("SH: %d\n", SH);
			return -1;
			break;
	};

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

	switch (SH) {
		case 2:
			printf("fldr  f%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
		case 3:
			printf("fldr  d%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
		default:
			loge("\n");
			return -1;
			break;
	};

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

	switch (SH) {
		case 2:
			printf("fstr  f%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
		case 3:
			printf("fstr  d%d, [r%d, r%d, %d]\n", rd, rb, ri, u8);
			break;
		default:
			loge("\n");
			return -1;
			break;
	};

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
		if (2 == SH)
			printf("fss2sd   d%d, f%d\n", rd, rs);
		else if (3 == SH)
			printf("fsd2ss   f%d, d%d\n", rd, rs);
		else {
			loge("\n");
			return -EINVAL;
		}

	} else if (1 == opt) {
		if (s)
			printf("cvtss2si r%d, f%d\n", rd, rs);
		else
			printf("cvtss2ui r%d, f%d\n", rd, rs);

	} else if (2 == opt) {
		printf("cvtsi2ss f%d, r%d\n", rd, rs);

	} else {
		if (s)
			printf("fneg     d%d, d%d\n", rd, rs);
		else
			printf("fmov     d%d, d%d\n", rd, rs);
	}

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

static int __naja_vm_run(vm_t* vm, const char* path, const char* sys)
{
	elf_sym_t* s;
	vm_naja_t* naja = vm->priv;
	vector_t*  syms = vector_alloc();
	if (!syms)
		return -ENOMEM;

	int ret = elf_read_syms(vm->elf, syms, ".symtab");
	if (ret < 0)
		return ret;

	int i;
	for (i = 0; i < syms->size; i++) {
		s  =        syms->data[i];

		if (ELF64_ST_TYPE(s->st_info) != STT_FUNC)
			continue;

		int64_t offset = s->st_value - vm->text->addr;

		if (offset >= vm->text->len)
			return -1;

		printf("\n%s: \n", s->name);
		int j;
		for (j = 0; j < s->st_size; j+= 4) {

			uint32_t inst = *(uint32_t*)(vm->text->data + offset + j);

			naja_opcode_pt pt = naja_opcodes[(inst >> 26) & 0x3f];

			if (!pt) {
				loge("inst: %d, %#x\n", (inst >> 26) & 0x3f, inst);
				continue;
			}

			naja->ip = vm->text->addr + offset + j;

			printf("%4d, %#lx: ", j, naja->ip);

			ret = pt(vm, inst);
			if (ret < 0) {
				loge("\n");
				return ret;
			}
		}
	}

	return 0;
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

vm_ops_t  vm_ops_naja_asm =
{
	.name  = "naja_asm",
	.open  = naja_vm_open,
	.close = naja_vm_close,
	.run   = naja_vm_run,
};
