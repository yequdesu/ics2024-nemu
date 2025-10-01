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

#include <isa.h>
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  for(int i = 0; i < 32; i += 4) {
    printf("%-4s : 0x%08x    ", regs[i], gpr(i));
    printf("%-4s : 0x%08x    ", regs[i + 1], gpr(i + 1));
    printf("%-4s : 0x%08x    ", regs[i + 2], gpr(i + 2));
    printf("%-4s : 0x%08x    \n", regs[i + 3], gpr(i + 3));
  }
}

word_t isa_reg_str2val(const char *s, bool *success) {
  for(int i = 0; i < 32; i++) {
    if(strcmp(s, regs[i]) == 0) {
      return (word_t)gpr(i);
    }
  }
  *success = false;
  return 0;
}

word_t read_csr(int idx) {
  idx = check_csr_idx(idx);
  switch (idx) {
    case 0x300: return cpu.csr.mstatus;
    case 0x305: return cpu.csr.mtvec;
    case 0x341: return cpu.csr.mepc;
    case 0x342: return cpu.csr.mcause;
    default: panic("Unknown csr idx in reading");
  }
}

void write_csr(int idx, word_t value) {
  idx = check_csr_idx(idx);
  switch (idx) {
    case 0x300: cpu.csr.mstatus = value; break;
    case 0x305: cpu.csr.mtvec = value; break;
    case 0x341: cpu.csr.mepc = value; break;
    case 0x342: cpu.csr.mcause = value; break;
    default: panic("Unknown csr idx in writing");
  }
}

word_t csr2idx(int csr) {
  switch (csr) {
    case 0x300: return 0; // mstatus
    case 0x301: return 1; // misa (如果不需要可以忽略)
    case 0x305: return 2; // mtvec
    case 0x341: return 3; // mepc
    case 0x342: return 4; // mcause
    default:
      printf("[ERROR] 未知的 CSR 寄存器: 0x%x\n", csr);
      assert(0);
      return -1;
  }
}