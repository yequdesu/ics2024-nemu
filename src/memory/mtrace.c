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
#include <memory/mtrace.h>
#include <memory/paddr.h>
#include <memory/host.h>
#include <stdarg.h>
#include <isa.h>

#define MTRACE_LOG_BUF_SIZE 256
static FILE *mtrace_log_fp = NULL;
vaddr_t temp_pc = 0x00000000;
extern CPU_state cpu;

// Nemu's log records call terminal instructions through makefile, passing in the path. 
// mtrace does not change the architecture to affect stability,
// and chooses to directly specify the path to the same location.

#ifdef CONFIG_MTRACE

void init_mtrace() {
    mtrace_log_fp = fopen("/home/yequdesu/repos/ics2024/nemu/build/mtrace-log.txt", "w");
}

static char* generate_mtrace_log(const char *op_type, paddr_t addr, int len, ...) {
    static __thread char log_buf[MTRACE_LOG_BUF_SIZE];
    
    va_list args;
    va_start(args, len);
    
    if (strcmp(op_type, "read") == 0) {
        word_t data = host_read(guest_to_host(addr), len);
        if(cpu.pc != temp_pc) {
            snprintf(log_buf, sizeof(log_buf), 
                 "[READ]   pc:0x%08x, addr: 0x%08x, len: %d bytes, data: 0x%08x", cpu.pc, addr, len, data);
            temp_pc = cpu.pc;
        } else {
            snprintf(log_buf, sizeof(log_buf), 
                 "[READ]   pc:0x%08x, addr: 0x%08x, len: %d bytes, data: 0x%08x, data(dec): %d", cpu.pc, addr, len, data, data);
        }
        
    } 
    else if (strcmp(op_type, "write") == 0) {
        word_t data = va_arg(args, word_t);
        if(cpu.pc != temp_pc) {
            snprintf(log_buf, sizeof(log_buf), 
                 "[WRITE]  pc:0x%08x, addr: 0x%08x, len: %d bytes, data: 0x%08x", cpu.pc, addr, len, data);
            temp_pc = cpu.pc;
        } else {
            snprintf(log_buf, sizeof(log_buf), 
                 "[WRITE]  pc:0x%08x, addr: 0x%08x, len: %d bytes, data: 0x%08x, data(dec): %d", cpu.pc, addr, len, data, data);
        }
        
    }
    else {
        snprintf(log_buf, sizeof(log_buf), 
                 "[UNKNOWN]  addr: 0x%08x, len: %d bytes", addr, len);
    }
    va_end(args);
    return log_buf;
}
#endif

void mtrace_read_record(paddr_t addr, int len) {
    mtrace_log_write("%s\n", generate_mtrace_log("read", addr, len));
}

void mtrace_write_record(paddr_t addr, int len, word_t data) {
    mtrace_log_write("%s\n", generate_mtrace_log("write", addr, len, data));
}