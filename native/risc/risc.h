#ifndef RISC_H
#define RISC_H

#include"native.h"
#include"risc_util.h"
#include"risc_reg.h"
#include"risc_opcode.h"
#include"graph.h"
#include"ghr_elf.h"

#define RISC_INST_ADD_CHECK(vec, inst) \
			do { \
				if (!(inst)) { \
					loge("\n"); \
					return -ENOMEM; \
				} \
				int ret = vector_add((vec), (inst)); \
				if (ret < 0) { \
					loge("\n"); \
					free(inst); \
					return ret; \
				} \
			} while (0)

#define RISC_RELA_ADD_CHECK(vec, rela, c, v, f) \
	do { \
		rela = calloc(1, sizeof(rela_t)); \
		if (!rela) \
			return -ENOMEM; \
		\
		(rela)->code = (c); \
		(rela)->var  = (v); \
		(rela)->func = (f); \
		(rela)->inst = (c)->instructions->data[(c)->instructions->size - 1]; \
		(rela)->addend = 0; \
		\
		int ret = vector_add((vec), (rela)); \
		if (ret < 0) { \
			free(rela); \
			rela = NULL; \
			return ret; \
		} \
	} while (0)

#define RISC_PEEPHOLE_DEL 1
#define RISC_PEEPHOLE_OK  0

typedef struct {

	function_t*     f;

} risc_context_t;

typedef struct {
	dag_node_t*      dag_node;

	register_t*  reg;

	risc_OpCode_t*    OpCode;

} risc_rcg_node_t;

typedef int (*risc_rcg_handler_pt )(native_t* ctx, _3ac_code_t* c, graph_t* g);
typedef int (*risc_inst_handler_pt)(native_t* ctx, _3ac_code_t* c);

risc_rcg_handler_pt   risc_find_rcg_handler(const int op_type);
risc_inst_handler_pt  risc_find_inst_handler(const int op_type);

int  risc_rcg_find_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg);
int _risc_rcg_make_node(graph_node_t** pp, graph_t* g, dag_node_t* dn, register_t* reg);

int risc_open  (native_t* ctx, const char* arch);
int risc_close (native_t* ctx);
int risc_select(native_t* ctx);

int risc_optimize_peephole(native_t* ctx, function_t* f);

int risc_graph_kcolor(graph_t* graph, int k, vector_t* colors, function_t* f);


intptr_t risc_bb_find_color (vector_t* dn_colors, dag_node_t* dn);
int      risc_save_bb_colors(vector_t* dn_colors, bb_group_t* bbg, basic_block_t* bb);

int risc_bb_load_dn (intptr_t color, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, function_t* f);
int risc_bb_save_dn (intptr_t color, dag_node_t* dn, _3ac_code_t* c, basic_block_t* bb, function_t* f);
int risc_bb_load_dn2(intptr_t color, dag_node_t* dn, basic_block_t* bb, function_t* f);
int risc_bb_save_dn2(intptr_t color, dag_node_t* dn, basic_block_t* bb, function_t* f);

int  risc_fix_bb_colors  (basic_block_t* bb, bb_group_t* bbg, function_t* f);
int  risc_load_bb_colors (basic_block_t* bb, bb_group_t* bbg, function_t* f);
int  risc_load_bb_colors2(basic_block_t* bb, bb_group_t* bbg, function_t* f);
void risc_init_bb_colors (basic_block_t* bb, function_t* f);


instruction_t* risc_make_inst         (_3ac_code_t* c, uint32_t opcode);
instruction_t* risc_make_inst_BL      (_3ac_code_t* c);
instruction_t* risc_make_inst_BLR     (_3ac_code_t* c, register_t* r);
instruction_t* risc_make_inst_PUSH    (_3ac_code_t* c, register_t* r);
instruction_t* risc_make_inst_POP     (_3ac_code_t* c, register_t* r);
instruction_t* risc_make_inst_TEQ     (_3ac_code_t* c, register_t* rs);
instruction_t* risc_make_inst_NEG     (_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_MSUB    (_3ac_code_t* c, register_t* rd, register_t* rm, register_t* rn, register_t* ra);
instruction_t* risc_make_inst_MUL     (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_SDIV    (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_DIV     (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_FDIV    (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_FMUL    (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_FSUB    (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_FADD    (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_SUB_G   (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_ADD_G   (_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_ADD_IMM (_3ac_code_t* c, register_t* rd, register_t* rs, uint64_t imm);
instruction_t* risc_make_inst_SUB_IMM (_3ac_code_t* c, register_t* rd, register_t* rs, uint64_t imm);
instruction_t* risc_make_inst_CMP_IMM(_3ac_code_t* c, register_t* rs, uint64_t imm);
instruction_t* risc_make_inst_CVTSS2SD(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_MOVZX   (_3ac_code_t* c, register_t* rd, register_t* rs, int size);
instruction_t* risc_make_inst_MOVSX   (_3ac_code_t* c, register_t* rd, register_t* rs, int size);
instruction_t* risc_make_inst_FMOV_G  (_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_MVN     (_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_MOV_G   (_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_MOV_SP  (_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_AND_G(_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_OR_G(_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_CMP_G(_3ac_code_t* c, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_SHL(_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_SHR(_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);
instruction_t* risc_make_inst_ASR(_3ac_code_t* c, register_t* rd, register_t* rs0, register_t* rs1);

instruction_t* risc_make_inst_CVTSS2SD(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_CVTSD2SS(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_CVTF2SI(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_CVTF2UI(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_CVTSI2F(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_CVTUI2F(_3ac_code_t* c, register_t* rd, register_t* rs);
instruction_t* risc_make_inst_FCMP(_3ac_code_t* c, register_t* rs0, register_t* rs1);

instruction_t* risc_make_inst_JA (_3ac_code_t* c);
instruction_t* risc_make_inst_JB (_3ac_code_t* c);
instruction_t* risc_make_inst_JZ (_3ac_code_t* c);
instruction_t* risc_make_inst_JNZ(_3ac_code_t* c);
instruction_t* risc_make_inst_JGT(_3ac_code_t* c);
instruction_t* risc_make_inst_JGE(_3ac_code_t* c);
instruction_t* risc_make_inst_JLT(_3ac_code_t* c);
instruction_t* risc_make_inst_JLE(_3ac_code_t* c);
instruction_t* risc_make_inst_JMP(_3ac_code_t* c);
instruction_t* risc_make_inst_JAE(_3ac_code_t* c);
instruction_t* risc_make_inst_JBE(_3ac_code_t* c);
instruction_t* risc_make_inst_RET(_3ac_code_t* c);

instruction_t* risc_make_inst_SETZ (_3ac_code_t* c, register_t* rd);
instruction_t* risc_make_inst_SETNZ(_3ac_code_t* c, register_t* rd);
instruction_t* risc_make_inst_SETGT(_3ac_code_t* c, register_t* rd);
instruction_t* risc_make_inst_SETGE(_3ac_code_t* c, register_t* rd);
instruction_t* risc_make_inst_SETLT(_3ac_code_t* c, register_t* rd);
instruction_t* risc_make_inst_SETLE(_3ac_code_t* c, register_t* rd);

void risc_set_jmp_offset(instruction_t* inst, int32_t bytes);

int risc_make_inst_I2G   (_3ac_code_t* c, register_t* rd, uint64_t imm, int bytes);
int risc_make_inst_M2G   (_3ac_code_t* c, function_t* f, register_t* rd, register_t* rb, variable_t* vs);
int risc_make_inst_M2GF  (_3ac_code_t* c, function_t* f, register_t* rd, register_t* rb, variable_t* vs);
int risc_make_inst_G2M   (_3ac_code_t* c, function_t* f, register_t* rs, register_t* rb, variable_t* vs);
int risc_make_inst_G2P   (_3ac_code_t* c, function_t* f, register_t* rs, register_t* rb, int32_t offset, int size);
int risc_make_inst_P2G   (_3ac_code_t* c, function_t* f, register_t* rd, register_t* rb, int32_t offset, int size);
int risc_make_inst_ISTR2G(_3ac_code_t* c, function_t* f, register_t* rd, variable_t* vs);
int risc_make_inst_SIB2G (_3ac_code_t* c, function_t* f, register_t* rd, sib_t* sib);
int risc_make_inst_G2SIB (_3ac_code_t* c, function_t* f, register_t* rd, sib_t* sib);
int risc_make_inst_ADR2G (_3ac_code_t* c, function_t* f, register_t* rd, variable_t* vs);
int risc_make_inst_ADRP2G(_3ac_code_t* c, function_t* f, register_t* rd, register_t* rb, int32_t offset);
int risc_make_inst_ADRSIB2G(_3ac_code_t* c, function_t* f, register_t* rd, sib_t* sib);

int risc_rcg_make(_3ac_code_t* c, graph_t* g, dag_node_t* dn, register_t* reg);

#endif

