#ifndef X64_OPCODE_H
#define X64_OPCODE_H

#include "native.h"
#include "x64_util.h"

typedef struct 
{
    int type;

    char* name;

    int len;

    uint8_t OpCOdes[3];
    int nb_OpCodes;

	// RegBytes only valid for immediate
	// same to OpBytes for E2G or G2E
    int OpBytes;
    int RegBytes;
    int EG;

    uint8_t ModRM_OpCode;
    int ModRM_OpCode_used;

    int nb_regs;
    uint32_t regs[2];
}x64_OpCode_t;

x64_OpCode_t* x64_find_OpCode_by_type(const int type);

x64_OpCode_t* x64_find_OpCode(const int type,const int OpBytes,const int RegBytes,const int EG);

int x64_find_OpCodes(vector_t* results,const int type,const int OpBytes,const int RegBytes,const int EG);




#endif