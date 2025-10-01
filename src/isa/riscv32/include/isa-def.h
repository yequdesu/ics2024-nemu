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

#ifndef __ISA_RISCV_H__
#define __ISA_RISCV_H__

#include <common.h>

enum {
  CSR_MSTATUS = 0x300,
  CSR_MISA    = 0x301,
  CSR_MIE     = 0x304,
  CSR_MTVEC   = 0x305,
  CSR_MSCRATCH= 0x340,
  CSR_MEPC    = 0x341,
  CSR_MCAUSE  = 0x342,
  CSR_MTVAL   = 0x343,
  CSR_MIP     = 0x344
};

typedef struct {
  word_t mtvec;
  vaddr_t mepc;
  word_t mstatus;
  word_t mcause;
} riscv32_CSR;

typedef struct {
  word_t gpr[MUXDEF(CONFIG_RVE, 16, 32)];
  riscv32_CSR csr;
  vaddr_t pc;
} MUXDEF(CONFIG_RV64, riscv64_CPU_state, riscv32_CPU_state);

// decode
typedef struct {
  uint32_t inst;
} MUXDEF(CONFIG_RV64, riscv64_ISADecodeInfo, riscv32_ISADecodeInfo);

#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)

#endif
