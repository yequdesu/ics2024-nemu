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
#include <common.h>
#include <sdb/sdb.h>
#include <memory/vaddr.h>
#include <elf.h>

typedef struct {
    uint32_t address;
    int size;
    char *fun_name;
} FunMap;

extern FunMap *func_map;
extern int func_count;

int parse_elf(const char *elf_file, FunMap **func_map, int *func_count);

void init_ftrace();

void ftrace_record_call(paddr_t pc, paddr_t target);

void ftrace_record_ret(paddr_t pc, paddr_t target);

char* get_func_name(paddr_t dnpc);