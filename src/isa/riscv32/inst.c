/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>
#include <ftrace/ftrace.h>

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
  TYPE_I, TYPE_U, TYPE_S, TYPE_J, TYPE_B, TYPE_R,
  TYPE_N, // none
};

/*
Instruction Encoding Format:
RISC-V instructions are 32-bit fixed-length and feature a highly structured format.
For the primary instruction types (I, U, S),
the positions of the operand fields within the instruction word are fixed:

rd (destination register): Always at bits [11:7].
rs1 (source register 1): Always at bits [19:15].
rs2 (source register 2): Always at bits [24:20].
The immediate field varies by type, which is why the switch-case is necessary:

I-type: Immediate value is in bits [31:20] (a contiguous field).
U-type: Immediate value is in bits [31:12] (a contiguous field).
S-type: Immediate value is split into two parts,
bits [31:25] (high 7 bits) and bits [11:7] (low 5 bits).

Sign Extension:
Immediate values in RISC-V are typically signed (with exceptions for instructions like AUIPC).
Therefore, a shorter immediate value extracted from the instruction (e.g., 12 bits) 
must be sign-extended (SEXT) to the target machine's word size (e.g., 32 or 64 bits) 
to generate the correct numerical value. 
This is a rule all RISC-V simulators/processors must adhere to.

U-type Immediate Shifting:
U-type instructions (e.g., LUI - Load Upper Immediate)
are designed to load the immediate value into the upper bits of a register.
Consequently, the specification mandates that the 20-bit immediate value
extracted from the instruction must be left-shifted by 12 bits.
The << 12 in the code implements this specification.
*/

#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immJ() do { *imm = SEXT((BITS(i, 31, 31) << 19 | (BITS(i, 19, 12) << 11) | (BITS(i, 20, 20) << 10) | BITS(i, 30, 21)) << 1, 21); } while(0)
// #define immB() do { *imm = SEXT((BITS(i, 31, 31) << 12) | (BITS(i, 7, 7) << 11) | (BITS(i, 30, 25) << 5) | BITS(i, 11, 8), 13) << 1;} while(0)
#define immB() do { *imm = SEXT(BITS(i, 31, 31), 1) << 12 | (BITS(i, 7, 7) << 11) | (BITS(i, 30, 25) << 5) | BITS(i, 11, 8) << 1;} while(0)

int rs1, rs2;

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  rs1 = BITS(i, 19, 15); // rs1 (source register 1): Always at bits [19:15].
  rs2 = BITS(i, 24, 20); // rs2 (source register 2): Always at bits [24:20].
  *rd     = BITS(i, 11, 7);  // rd (destination register): Always at bits [11:7].
  switch (type) {
    case TYPE_I: src1R();          immI(); break; // I-type: Immediate value is in bits [31:20] (a contiguous field).
    case TYPE_U:                   immU(); break; // U-type: Immediate value is in bits [31:12] (a contiguous field).
    case TYPE_S: src1R(); src2R(); immS(); break; // S-type: Immediate value is split into two parts, bits [31:25] (high 7 bits) and bits [11:7] (low 5 bits).
    case TYPE_J:                   immJ(); break; // J-type: Immediate value is split into four parts.
    case TYPE_B: src1R(); src2R(); immB(); break; // B-type: Immediate value is split into four parts.
    case TYPE_R: src1R(); src2R();         break; // R-type: No immediate value.

    case TYPE_N: break;  
    default: panic("unsupported type = %d", type);
  }
}

static int decode_exec(Decode *s) {
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START();
  
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2);
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm);
  INSTPAT("??????? ????? ????? 000 ????? 00110 11", addiw  , I, R(rd) = SEXT(src1 + imm, 32));
  INSTPAT("0000000 ????? ????? 000 ????? 01110 11", addw   , R, R(rd) = SEXT(src1 + src2, 32));
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01110 11", subw   , R, R(rd) = SEXT(src1 - src2, 32));

  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1 & imm);
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1 | src2);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1 ^ src2);
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm);

  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1 << BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   , I, R(rd) = src1 << BITS(imm, 4, 0));
  INSTPAT("0000000 ????? ????? 001 ????? 00110 11", slliw  , I, R(rd) = SEXT(src1 << BITS(imm, 4, 0), 32));
  INSTPAT("0000000 ????? ????? 001 ????? 01110 11", sllw   , R, R(rd) = SEXT(src1 << BITS(src2, 4, 0), 32));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (sword_t)src1 >> BITS(src2, 4, 0));
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, R(rd) = (sword_t)src1 >> BITS(imm, 4, 0));
  INSTPAT("0100000 ????? ????? 101 ????? 00110 11", sraiw  , I, R(rd) = (sword_t)SEXT(src1, 32) >> BITS(imm, 4, 0));
  INSTPAT("0100000 ????? ????? 101 ????? 01110 11", sraw   , R, R(rd) = (sword_t)SEXT(src1, 32) >> BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = src1 >> BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, R(rd) = src1 >> BITS(imm, 4, 0));
  INSTPAT("0000000 ????? ????? 101 ????? 01110 11", srlw   , R, R(rd) = SEXT(BITS(src1, 31, 0) >> BITS(src2, 4, 0), 32));

  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = (sword_t)src1 < (sword_t)src2);
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = src1 < (word_t)imm);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = src1 < src2);

  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = src1 * src2);
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, R(rd) = R(rd) = (word_t)(((int64_t)(sword_t)src1 * (int64_t)(sword_t)src2) >> 32));
  INSTPAT("0000001 ????? ????? 000 ????? 01110 11", mulw   , R, R(rd) = SEXT(src1 * src2, 32));

  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = (sword_t)src1 / (sword_t)src2);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu   , R, R(rd) = src1 / src2);
  INSTPAT("0000001 ????? ????? 100 ????? 01110 11", divw   , R, R(rd) = SEXT(src1, 32) / SEXT(src2, 32));
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = (src2 == 0) ? src1 : (sword_t)src1 % (sword_t)src2);
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu   , R, R(rd) = (src2 == 0) ? src1 : src1 % src2);
  INSTPAT("0000001 ????? ????? 110 ????? 01110 11", remw   , R, R(rd) = (src2 == 0) ? SEXT(src1, 32) : SEXT(src1, 32) % SEXT(src2, 32));

  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, if (src1 == src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, if ((sword_t)src1 >= (sword_t)src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, if (src1 >= src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, if ((sword_t)src1 < (sword_t)src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, if (src1 < src2) s->dnpc = s->pc + imm);
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, if (src1 != src2) s->dnpc = s->pc + imm);

  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, {
    s->dnpc = s->pc;
    s->dnpc += imm;
    if (rd == 1) {
      ftrace_record_call(s->pc, s->dnpc);
    }
    R(rd) = s->pc + 4;
  });

  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, {
    s->dnpc = (src1 + imm) & ~(word_t)1;
    if (rd == 1) {
      ftrace_record_call(s->pc, s->dnpc);
    } else if (rd == 0 && rs1 == 1) {
      ftrace_record_ret(s->pc, s->dnpc);
    }
    R(rd) = s->pc + 4;
  });


  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = SEXT(Mr(src1 + imm, 1), 8));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = SEXT(Mr(src1 + imm, 2), 16));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = SEXT(Mr(src1 + imm, 4), 32));
  INSTPAT("??????? ????? ????? 110 ????? 00000 11", lwu    , I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? 011 ????? 00000 11", ld     , I, R(rd) = Mr(src1 + imm, 8));

  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 011 ????? 01000 11", sd     , S, Mw(src1 + imm, 8, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2));

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc)); // Used to handle Invalid instructions

  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
