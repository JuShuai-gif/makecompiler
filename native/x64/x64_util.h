#ifndef X64_UTIL_H
#define X64_UTIL_H

#include "utils_def.h"

enum x64_REX_values {
    X64_REX_INIT = 0x40,
    X64_REX_B = 0x1,
    X64_REX_X = 0x2,
    X64_REX_R = 0x4,
    X64_REX_W = 0x8,
};

enum x64_OpCode_types {
    X64_MOV = 0,
    X64_MOVSX,
    X64_MOVZX,
    X64_MOVS,
    X64_STOS,

    X64_LEA,
    X64_PUSH,
    X64_POP,
    X64_INC,
    X64_DEC,

    X64_XOR,
    X64_AND,
    X64_OR,
    X64_NOT,

    X64_NEG,

    X64_CALL,
    X64_RET,

    X64_ADD,
    X64_SUB,

    X64_MUL,
    X64_DIV,
    X64_IMUL,
    X64_IDIV,

    X64_CBW,
    X64_CWD = X64_CBW,
    X64_CDQ = X64_CBW,
    X64_CQO = X64_CBW,

    X64_SAR,
    X64_SHR,
    X64_SHL,

    X64_CMP,
    X64_TEST,

    X64_SETZ,
    X64_SETNZ,

    X64_SETG,
    X64_SETGE,

    X64_SETL,
    X64_SETLE,

    X64_ADDSS,
    X64_ADDSD,

    X64_SUBSS,
    X64_SUBSD,

    X64_MULSS,
    X64_MULSD,

    X64_DIVSS,
    X64_DIVSD,

    X64_MOVSS,
    X64_MOVSD,

    X64_UCOMISS,
    X64_UCOMISD,

    X64_CVTSI2SD,
    X64_CVTSI2SS,

    X64_CVTSS2SD,
    X64_CVTSD2SS,

    X64_CVTTSD2SI,
    X64_CVTTSS2SI,

    X64_PXOR,

    X64_JZ,
    X64_JNZ,

    X64_JG,
    X64_JGE,

    X64_JL,
    X64_JLE,

    X64_JA,
    X64_JAE,

    X64_JB,
    X64_JBE,

    X64_JMP,

    X64_NB

};

enum x64_Mods {
	X64_MOD_BASE		= 0x0, // 00, [base]
	X64_MOD_BASE_DISP8	= 0x1, // 01, [base] + disp8
	X64_MOD_BASE_DISP32	= 0x2, // 10, [base] + disp32
	X64_MOD_REGISTER	= 0x3, // 11, register
};

enum x64_SIBs {
	X64_SIB_SCALE1      = 0x0, // 00, index * 1 + base
	X64_SIB_SCALE2      = 0x1, // 01, index * 2 + base
	X64_SIB_SCALE4      = 0x2, // 10, index * 4 + base
	X64_SIB_SCALE8      = 0x3, // 11, index * 8 + base
};


enum x64_RMs {
	// others same to scf_x64_REGs

	// when Mod = 11
	X64_RM_ESP		= 0x4, // 100
	X64_RM_SP		= 0x4, // 100
	X64_RM_AH		= 0x4, // 100
	X64_RM_MM4		= 0x4, // 100
	X64_RM_XMM4		= 0x4, // 100

	X64_RM_SIB			= 0x4, // 100, when Mod = 00
	X64_RM_SIB_DISP8	= 0x4, // 100, when Mod = 01
	X64_RM_SIB_DISP32	= 0x4, // 100, when Mod = 10

	// when Mod = 11
	X64_RM_EBP		= 0x5, // 101
	X64_RM_BP		= 0x5, // 101
	X64_RM_CH		= 0x5, // 101
	X64_RM_MM5		= 0x5, // 101
	X64_RM_XMM5		= 0x5, // 101

	X64_RM_R12      = 0xc,
	X64_RM_R13      = 0xd,


	X64_RM_DISP32		= 0x5, // 101, when Mod = 00
	X64_RM_EBP_DISP8	= 0x5, // 101, when Mod = 01
	X64_RM_EBP_DISP32	= 0x5, // 101, when Mod = 10
};

enum x64_REGs {
	X64_REG_AL		= 0x0,
	X64_REG_AX		= 0x0,
	X64_REG_EAX		= 0x0,
	X64_REG_RAX		= 0x0,
	X64_REG_MM0		= 0x0,
	X64_REG_XMM0	= 0x0,

	X64_REG_CL		= 0x1,
	X64_REG_CX		= 0x1,
	X64_REG_ECX		= 0x1,
	X64_REG_RCX		= 0x1,
	X64_REG_MM1		= 0x1,
	X64_REG_XMM1	= 0x1,

	X64_REG_DL		= 0x2,
	X64_REG_DX		= 0x2,
	X64_REG_EDX		= 0x2,
	X64_REG_RDX		= 0x2,
	X64_REG_MM2		= 0x2,
	X64_REG_XMM2	= 0x2,

	X64_REG_BL		= 0x3,
	X64_REG_BX		= 0x3,
	X64_REG_EBX		= 0x3,
	X64_REG_RBX		= 0x3,
	X64_REG_MM3		= 0x3,
	X64_REG_XMM3	= 0x3,

	X64_REG_AH		= 0x4,
	X64_REG_SP		= 0x4,
	X64_REG_ESP		= 0x4,
	X64_REG_RSP		= 0x4,
	X64_REG_MM4		= 0x4,
	X64_REG_XMM4	= 0x4,

	X64_REG_CH		= 0x5,
	X64_REG_BP		= 0x5,
	X64_REG_EBP		= 0x5,
	X64_REG_RBP		= 0x5,
	X64_REG_MM5		= 0x5,
	X64_REG_XMM5	= 0x5,

	X64_REG_DH		= 0x6,
	X64_REG_SIL     = 0x6,
	X64_REG_SI		= 0x6,
	X64_REG_ESI		= 0x6,
	X64_REG_RSI		= 0x6,
	X64_REG_MM6		= 0x6,
	X64_REG_XMM6	= 0x6,

	X64_REG_BH		= 0x7,
	X64_REG_DIL     = 0x7,
	X64_REG_DI		= 0x7,
	X64_REG_EDI		= 0x7,
	X64_REG_RDI		= 0x7,
	X64_REG_MM7		= 0x7,
	X64_REG_XMM7	= 0x7,

	X64_REG_R8D     = 0x8,
	X64_REG_R8      = 0x8,

	X64_REG_R9D     = 0x9,
	X64_REG_R9      = 0x9,

	X64_REG_R10D    = 0xa,
	X64_REG_R10     = 0xa,

	X64_REG_R11D    = 0xb,
	X64_REG_R11     = 0xb,

	X64_REG_R12D    = 0xc,
	X64_REG_R12     = 0xc,

	X64_REG_R13D    = 0xd,
	X64_REG_R13     = 0xd,

	X64_REG_R14D    = 0xe,
	X64_REG_R14     = 0xe,

	X64_REG_R15D    = 0xf,
	X64_REG_R15     = 0xf,
};

enum x64_EG_types {
	X64_G	= 0,
	X64_I	= 1,
	X64_G2E	= 2,
	X64_E2G = 3,
	X64_I2E = 4,
	X64_I2G = 5,
	X64_E	= 6,
};

static inline uint8_t ModRM_getMod(uint8_t ModRM)
{
	return ModRM >> 6;
}

static inline uint8_t ModRM_getReg(uint8_t ModRM)
{
	return (ModRM >> 3) & 0x7;
}

static inline uint8_t ModRM_getRM(uint8_t ModRM)
{
	return ModRM & 0x7;
}

static inline void ModRM_setMod(uint8_t* ModRM, uint8_t v)
{
	*ModRM |= (v << 6);
}

static inline void ModRM_setReg(uint8_t* ModRM, uint8_t v)
{
	*ModRM |= (v & 0x7) << 3;
}

static inline void ModRM_setRM(uint8_t* ModRM, uint8_t v)
{
	*ModRM |= v & 0x7;
}

static inline uint8_t SIB_getScale(uint8_t SIB)
{
	return (SIB >> 6) & 0x3;
}
static inline void SIB_setScale(uint8_t* SIB, uint8_t scale)
{
	*SIB |= scale << 6;
}

static inline uint8_t SIB_getIndex(uint8_t SIB)
{
	return (SIB >> 3) & 0x7;
}
static inline void SIB_setIndex(uint8_t* SIB, uint8_t index)
{
	*SIB |= (index & 0x7) << 3;
}

static inline uint8_t SIB_getBase(uint8_t SIB)
{
	return SIB & 0x7;
}
static inline void SIB_setBase(uint8_t* SIB, uint8_t base)
{
	*SIB |= base & 0x7;
}






#endif
