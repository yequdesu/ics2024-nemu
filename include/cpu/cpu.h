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

#ifndef __CPU_CPU_H__
#define __CPU_CPU_H__

#include <common.h>

void cpu_exec(uint64_t n);

void set_nemu_state(int state, vaddr_t pc, int halt_ret);
void invalid_inst(vaddr_t thispc);

#define IS_FAULT_EXCEPTION(cause) \
  ((cause) == 1  || (cause) == 2  || (cause) == 3  || (cause) == 4  || \
   (cause) == 5  || (cause) == 6  || (cause) == 7  || (cause) == 12 || \
   (cause) == 13 || (cause) == 15)


#define NEMUTRAP(thispc, code) set_nemu_state(NEMU_END, thispc, code)
#define INV(thispc) invalid_inst(thispc)

#define ECALL(thispc) \
    bool success = true; \
    raise_intr(&(thispc), s->pc, &success); \
    assert(success); \

#define MRET(thispc, mepc, mcause) thispc = IS_FAULT_EXCEPTION(mcause) ? mepc : mepc + 4;


#endif
