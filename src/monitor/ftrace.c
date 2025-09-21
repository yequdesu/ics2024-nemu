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
#include <ftrace/ftrace.h>
#define FTRACE_LOG_BUF_SIZE 256
#define CALL_STACK_SIZE 256

typedef struct callstack_frame {
    paddr_t addr;
    char* func_name;
} CS_frame;

extern char *elf_file;
static FILE *ftrace_log_fp = NULL;
static CS_frame call_stack[CALL_STACK_SIZE] = {};
static int call_stack_top = 0;
static char blanks[CALL_STACK_SIZE] = {};

static void callstack_push(paddr_t addr, char* func_name) {
    if (call_stack_top < CALL_STACK_SIZE) {
        call_stack[call_stack_top].addr = addr;
        call_stack[call_stack_top].func_name = func_name;
        call_stack_top++;
    } else {
        printf("ftrace: call stack overflow at %08x\n", addr);
    }
}

static void callstack_pop() {
    if (call_stack_top > 0) {
        call_stack_top--;
    } else {
        printf("ftrace: call stack underflow\n");
    }
}

void init_ftrace() {
    ftrace_log_fp = fopen("/home/yequdesu/repos/ics2024/nemu/build/ftrace-log.txt", "w");
}

void ftrace_record_call(paddr_t pc, paddr_t target) {
    char* func_name = get_func_name(target);
    for (int i = 0; i < call_stack_top; i++) {
        blanks[i] = ' ';
    }
    blanks[call_stack_top] = '\0';
    callstack_push(pc + 4, func_name);
    ftrace_log_write("%08x: %scall [%s@%08x]\n", pc, blanks, func_name, target);
}

void ftrace_record_ret(paddr_t pc, paddr_t target) {
    char* func_name = "???";
    if (call_stack_top > 0) {
        func_name = call_stack[call_stack_top - 1].func_name;
        callstack_pop();
    } else {
     printf("ftrace: call stack underflow at %08x\n", pc);
    }
    for (int i = 0; i < call_stack_top; i++) {
        blanks[i] = ' ';
    }
    blanks[call_stack_top] = '\0';
    ftrace_log_write("%08x: %sret [%s@%08x]\n", pc, blanks, func_name, target);
}


int read_at(FILE* file, long offset, void* buffer, size_t size) {
    if (fseek(file, offset, SEEK_SET) != 0) {
        return -1;
    }
    return fread(buffer, 1, size, file) == size ? 0 : -1;
}

char* get_section_name(FILE* file, Elf32_Ehdr* ehdr, Elf32_Shdr* shdrs, uint32_t index) {
    Elf32_Shdr shstrtab;
    if (read_at(file, ehdr->e_shoff + ehdr->e_shstrndx * sizeof(Elf32_Shdr), 
                &shstrtab, sizeof(Elf32_Shdr)) != 0) {
        return NULL;
    }
    
    static char name[256];
    if (read_at(file, shstrtab.sh_offset + shdrs[index].sh_name, name, sizeof(name)) != 0) {
        return NULL;
    }
    
    return name;
}

int parse_elf(const char *elf_file, FunMap **func_map, int *func_count) {
    FILE* file = fopen(elf_file, "rb");
    Assert(file, "Can not open '%s'", elf_file);
    Log("Elf file is %s", elf_file);

    Elf32_Ehdr ehdr;
    if (read_at(file, 0, &ehdr, sizeof(Elf32_Ehdr)) != 0) return -1;

    Elf32_Shdr* shdrs = malloc(ehdr.e_shnum * sizeof(Elf32_Shdr));
    if (read_at(file, ehdr.e_shoff, shdrs, ehdr.e_shnum * sizeof(Elf32_Shdr)) != 0) return -1;

    Elf32_Shdr* strtab_shdr = NULL;
    Elf32_Shdr* symtab_shdr = NULL;
    
    for (int i = 0; i < ehdr.e_shnum; i++) {
        char* name = get_section_name(file, &ehdr, shdrs, i);
        if (!name) continue;
        
        if (strcmp(name, ".strtab") == 0) {
            strtab_shdr = &shdrs[i];
        } else if (strcmp(name, ".symtab") == 0) {
            symtab_shdr = &shdrs[i];
        }
    }

    if (!strtab_shdr || !symtab_shdr) return -1;

    char* strtab = malloc(strtab_shdr->sh_size);
    if (read_at(file, strtab_shdr->sh_offset, strtab, strtab_shdr->sh_size) != 0) return -1;
    int sym_count = symtab_shdr->sh_size / symtab_shdr->sh_entsize;
    Elf32_Sym* symtab = malloc(symtab_shdr->sh_size);
    if (read_at(file, symtab_shdr->sh_offset, symtab, symtab_shdr->sh_size) != 0) return -1;

    int count = 0;
    for (int i = 0; i < sym_count; i++) {
        unsigned char type = ELF32_ST_TYPE(symtab[i].st_info);
        if (type == STT_FUNC && symtab[i].st_name != 0 && symtab[i].st_value != 0) {
            count++;
        }
    }
    
    *func_map = malloc(count * sizeof(FunMap));

    int idx = 0;
    for (int i = 0; i < sym_count; i++) {
        unsigned char type = ELF32_ST_TYPE(symtab[i].st_info);
        if (type == STT_FUNC && symtab[i].st_name != 0 && symtab[i].st_value != 0) {
            (*func_map)[idx].address = symtab[i].st_value;
            (*func_map)[idx].fun_name = strdup(strtab + symtab[i].st_name);
            idx++;
        }
    }
    
    *func_count = count;

    for(int i = 0; i < count; i++) {
        printf("address:%08x  ---  funcname:%s \n", (*func_map)[i].address, (*func_map)[i].fun_name);
    }

    return 0;
}

char* get_func_name(paddr_t dnpc) {
    int count = func_count;
    for(int i = 0; i < count; i++) {
        if(func_map[i].address == dnpc) return func_map[i].fun_name;
    }
    return "???";
}
