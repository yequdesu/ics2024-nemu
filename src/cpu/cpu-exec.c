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
* See the Mulan PSL v2 for more deir_tails.
***************************************************************************************/

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>
#include <sdb/sdb.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10
#define RINGBUF_SIZE 10

CPU_state cpu = {}; // isa-def.h
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

#ifndef CONFIG_TARGET_AM
static int ir_head = 0, ir_tail = 0, ir_size = 0;
static char buf[RINGBUF_SIZE][500] = {};
#endif

void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);


#ifndef CONFIG_TARGET_AM
static void insert_iringbuf(const char i[], int *ir_size, int *ir_head, int *ir_tail) {
    strncpy(buf[*ir_tail], i, sizeof(buf[*ir_tail]) - 1);
    buf[*ir_tail][sizeof(buf[*ir_tail]) - 1] = '\0';
    
    (*ir_size)++;
    *ir_tail = (*ir_tail + 1) % RINGBUF_SIZE;
    
    if (*ir_size > RINGBUF_SIZE) {
        *ir_size = RINGBUF_SIZE;
        *ir_head = (*ir_head + 1) % RINGBUF_SIZE;
    }
    
    // printf("size:%d ir_tail:%d ir_head:%d, ins:%s \n", *ir_size, *ir_tail, *ir_head, i);
}

static void display_iringbuf(const int ir_size, const int ir_head, const int ir_tail) {
    if (ir_size == 0) {
        printf("Buffer is empty\n");
        return;
    }
    
    int current = ir_head;
    for (int i = 0; i < ir_size; i++) {
        printf("%s\n", buf[current]);
        current = (current + 1) % RINGBUF_SIZE;
    }
}

static char* generate_inst_info(const Decode s, bool trap) {
  static char output_buf[128] = {};
  static char err_pre_buf[5] = "--> ";
  static char normal_pre_buf[5] = "    ";
  static char disasm_buf[64] = {};
  static char code_buf[59] = {};

  output_buf[0] = '\0';
  disasm_buf[0] = '\0';
  code_buf[0] = '\0';

  char *p = disasm_buf;
  int pc_len = snprintf(p, sizeof(disasm_buf), FMT_WORD ":", s.pc);
  p += pc_len;
  int ilen = s.snpc - s.pc;
  char temp_buf[64] = {};
  disassemble(temp_buf, sizeof(temp_buf),
             MUXDEF(CONFIG_ISA_x86, s.snpc, s.pc), 
             (uint8_t *)&s.isa.inst, ilen);
  
  char *src = temp_buf;
  while (*src && p < disasm_buf + sizeof(disasm_buf) - 1) {
    if (*src == '\t') {
      int spaces = 4 - ((p - disasm_buf) % 4);
      memset(p, ' ', spaces);
      p += spaces;
      src++;
    } else {
      *p++ = *src++;
    }
  }
  *p = '\0';
  
  int c_len = 0;
  for (char *c = disasm_buf; *c && *c != '\0'; c++) {
    if (*c != '\0') c_len++;
  }
  int t_len = 36;
  if (c_len < t_len) {
    int spaces = t_len - c_len;
    if (p + spaces < disasm_buf + sizeof(disasm_buf)) {
      memset(p, ' ', spaces);
      p += spaces;
    }
  }
  *p = '\0';
  
  p = code_buf;
  uint8_t *inst = (uint8_t *)&s.isa.inst;
  for (int i = ilen - 1; i >= 0; i--) {
    p += snprintf(p, code_buf + sizeof(code_buf) - p, "%02x ", inst[i]);
  }

  if(trap) {
    strncpy(output_buf, err_pre_buf, sizeof(output_buf) - 1);
  } else {
    strncpy(output_buf, normal_pre_buf, sizeof(output_buf) - 1);
  }
  strncat(output_buf, disasm_buf, sizeof(output_buf) - strlen(output_buf) - 1);
  strncat(output_buf, code_buf, sizeof(output_buf) - strlen(output_buf) - 1);
  
  return output_buf;
}
#endif




void device_update();

static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) { log_write("%s\n", _this->logbuf); }
#endif
  if (g_print_step) { IFDEF(CONFIG_ITRACE, puts(_this->logbuf)); }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));

  IFDEF(CONFIG_WATCHPOINT, scan_wp("check"));
  // scan_wp("check");

}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  // 0x800000f4: 01 41 2a 83 lw      s5, 0x14(sp)
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  // 0x800000f4:
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i ++) {
#else
  for (i = ilen - 1; i >= 0; i --) {
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  // 0x800000f4: 01 41 2a 83
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;
  // 0x800000f4: 01 41 2a 83 
  // void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst, ilen);
  insert_iringbuf(generate_inst_info(*s, nemu_state.state != NEMU_RUNNING), &ir_size, &ir_head, &ir_tail);
#endif
}

static void execute(uint64_t n) {
  Decode s;
  for (;n > 0; n --) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst ++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING) break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0) Log("simulation frequency = " NUMBERIC_FMT " inst/s", g_nr_guest_inst * 1000000 / g_timer);
  else Log("Finish running in less than 1 us and can not calculate the simulation frequency");
}

void assert_fail_msg() {
  #ifndef CONFIG_TARGET_AM
  isa_reg_display();
  display_iringbuf(ir_size, ir_head, ir_tail);
  #endif
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
    case NEMU_END: case NEMU_ABORT: case NEMU_QUIT:
      printf("Program execution has ended. To restart the program, exit NEMU and run again.\n");
      return;
    default: nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
    case NEMU_RUNNING: nemu_state.state = NEMU_STOP; break;

    case NEMU_END: case NEMU_ABORT:
      Log("nemu: %s at pc = " FMT_WORD,
          (nemu_state.state == NEMU_ABORT ? ANSI_FMT("ABORT", ANSI_FG_RED) :
           (nemu_state.halt_ret == 0 ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN) :
            ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
          nemu_state.halt_pc);
      // fall through
    case NEMU_QUIT: statistic();
  }
  // display_iringbuf(ir_size, ir_head, ir_tail);
}
