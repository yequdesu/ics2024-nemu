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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sdb/sdb.h>
#include <memory/paddr.h>

#define TEST_LENS 1000

#ifdef CONFIG_BATCHMODE
static int is_batch_mode = true;
#else
static int is_batch_mode = false;
#endif

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args);
static int cmd_q(char *args);
static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_test(char *args);
static int cmd_w(char *agrs);
static int cmd_d(char *args);

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT; // Set NEMU_QUIT when using q to quit mannualy.
  return -1;
}

static bool is_nemu_ended() {
    return nemu_state.state == NEMU_END || 
           nemu_state.state == NEMU_ABORT || 
           nemu_state.state == NEMU_QUIT;
}

static void set_nemu_running() {
    if (!is_nemu_ended()) {
        nemu_state.state = NEMU_RUNNING;
    }
}

static int execute_steps(int step_count) {
    if (is_nemu_ended()) {
        printf("Program execution has ended. Cannot execute more instructions.\n");
        return -1;
    }
    
    int executed_count = 0;
    for (int i = 0; i < step_count; i++) {
        if (is_nemu_ended()) {
            printf("Program execution completed after %d instruction%s.\n", 
                   executed_count, executed_count != 1 ? "s" : "");
            return executed_count;
        }
        
        cpu_exec(1);
        executed_count++;
        
        if (is_nemu_ended()) {
            printf("Program execution completed after %d instruction%s.\n", 
                   executed_count, executed_count != 1 ? "s" : "");
            return executed_count;
        }
    }
    
    printf("Executed %d instruction%s\n", executed_count, executed_count != 1 ? "s" : "");
    return executed_count;
}

static int cmd_si(char *args) {
    if (is_nemu_ended()) {
        printf("Program execution has ended.\n");
        return 0;
    }
    
    int step_count = 1;
    char *arg = strtok(NULL, " ");
    
    if (arg != NULL) {
        char *endptr;
        int count = strtol(arg, &endptr, 10);
        if (endptr != arg && *endptr == '\0' && count > 0) {
            step_count = count;
            printf("Executing %d instruction%s\n", step_count, step_count > 1 ? "s" : "");
            int result = execute_steps(step_count);
            if (result < step_count) {
                return 0;
            }
            return 0;
        }
        else if (*endptr != '\0') {
            printf("Error: Invalid step count '%s'\n", arg);
            return 0;
        }
    }
    
    printf("Entering single-step mode.\n");
    printf("Type 'help' for available commands, 'q' to quit.\n");
    
    char *line_read = NULL;
    
    while (1) {
        if (is_nemu_ended()) {
            printf("Program execution has ended in single-step mode.\n");
            break;
        }
        
        set_nemu_running();
        
        line_read = readline("(si) ");
        if (line_read == NULL) {
            printf("\nExiting single-step mode\n");
            break;
        }
        
        if (line_read[0] == '\0') {
            free(line_read);
            int result = execute_steps(step_count);
            if (result < step_count) {
                break;
            }
            continue;
        }
        
        char *cmd = strtok(line_read, " \t");
        if (cmd == NULL) {
            free(line_read);
            continue;
        }
        
        char *arg_str = strtok(NULL, " \t");
        
        if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            printf("Exiting single-step mode\n");
            break;
        }
        else if (strcmp(cmd, "si") == 0) {
            if (arg_str != NULL) {
                char *endptr;
                int new_count = strtol(arg_str, &endptr, 10);
                if (endptr != arg_str && *endptr == '\0' && new_count > 0) {
                    step_count = new_count;
                    printf("Step count set to: %d\n", step_count);
                    int result = execute_steps(step_count);
                    if (result < step_count) {
                        free(line_read);
                        break;
                    }
                }
                else {
                    printf("Error: Invalid step count '%s'\n", arg_str);
                }
            }
            else {
                printf("Current step count: %d\n", step_count);
            }
        }
        else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            printf("Single-step mode commands:\n");
            printf("  [Enter]    - Execute %d step%s\n", step_count, step_count > 1 ? "s" : "");
            printf("  si [n]     - Set step count to n (default %d)\n", step_count);
            printf("  q/quit     - Exit single-step mode\n");
            printf("  h/help     - Show this help\n");
        }
        else {
            printf("Unknown command: '%s', type 'help' for available commands\n", cmd);
        }
        
        free(line_read);
        line_read = NULL;
    }
    
    if (line_read != NULL) {
        free(line_read);
    }
    
    return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if(arg != NULL) {
    if(strcmp(arg, "r") == 0) {
        isa_reg_display();
      } else if(strcmp(arg, "w") == 0) {
        scan_wp("print");
      }
  } else {
    printf("Please enter the correct parameters.\n");
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg = strtok(NULL, " ");
  char *step_s = NULL;
  char *base_s = NULL;
  
  if(arg != NULL) {
    char *cmd_end = arg + strlen(arg);
    step_s = strtok(arg, " ");
    base_s = step_s + strlen(step_s) + 1;

    if(step_s >= cmd_end || base_s >= cmd_end) {
      printf("Please enter the correct parameters.\n");
      return 0;
    }

    int step = atoi(step_s);
    word_t base = atoi(base_s);

    for(int i = 0; i < step; i++) {
      printf("%d:%d\n", base + 4 * i, paddr_read(base + 4 * i, 4));
    }
  } else {
    printf("Please enter the correct parameters.\n");
  }
  return 0;
}

static int cmd_test(char *args) {
  char *arg = strtok(NULL, " ");

  if (arg != NULL) {
    if (strcmp(arg, "make_token") == 0) {
      char *exp = readline("(nemu) The expression for testing:");
      bool success = false;
      word_t value = expr(exp, &success);
      if (!success) {
        printf("Failed to evaluate expression!\n");
      } else {
        printf("%d\n", value);
      }
      return 0;
    }
    else if (strcmp(arg, "test_exprs") == 0) {
      FILE *file = fopen("/home/yequdesu/repos/ics2024/nemu/tools/gen-expr/input", "r");
      if (file == NULL) {
        printf("Failed to open test file\n");
        return 0;
      }

      char line[TEST_LENS] = {};
      int total_tests = 0;
      int failed_tests = 0;
      int passed_tests = 0;
      while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) {
          continue;
        }
        total_tests += 1;
        char *space_pos = strchr(line, ' ');
        if (space_pos == NULL) {
          printf("Invalid test case format: %s\n", line);
          failed_tests++;
          continue;
        }
        *space_pos = '\0';
        word_t expected_result = (word_t)atoi(line);
        char *expression = space_pos + 1;
        bool success;
        word_t actual_result = expr(expression, &success);
        if (!success) {
          printf("Test failed: failed to evaluate %s\n", expression);
          failed_tests++;
        } else if (actual_result == expected_result) {
          printf("Test passed: %s = %u\n", expression, actual_result);
          passed_tests++;
        } else {
          printf("Test failed: %s = %u (expected %u)\n",
                 expression, actual_result, expected_result);
          failed_tests++;
        }
      }
      fclose(file);
      printf("Test results:\n");
      printf("Total tests: %d\n", total_tests);
      printf("Passed tests: %d\n", passed_tests);
      printf("Failed tests: %d\n", failed_tests);
      return 0;
    } else {
      printf("Unknown test subcommand '%s'\n", arg);
    }
  } else {
    printf("Usage: test [make_token|test_exprs]\n");
  }
  return 0;
}

#ifdef CONFIG_WATCHPOINT

static int cmd_w(char *args) {
  char *watchexpr = strtok(NULL, " ");
  if (watchexpr == NULL) {
    printf("Usage: w <expression>\n");
    return 0;
  }
  set_watchpoint(watchexpr);
  return 0;
}

static int cmd_d(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Usage: d <watchpoint_number>\n");
    return 0;
  }
  
  int no = atoi(arg);
  if (no < 0) {
    printf("Invalid watchpoint number: %d\n", no);
    return 0;
  }
  
  delete_watchpoint(no);
  return 0;
}

#else

static int cmd_w(char *args) {
  printf("Watchpoint support is disabled. Enable CONFIG_WATCHPOINT to use this feature.\n");
  return 0;
}

static int cmd_d(char *args) {
  printf("Watchpoint support is disabled. Enable CONFIG_WATCHPOINT to use this feature.\n");
  return 0;
}

#endif /* CONFIG_WATCHPOINT */


static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Single step execution", cmd_si},
  { "info", "Print regs", cmd_info},
  { "x", "Scan memory", cmd_x},
  { "test", "Test functions", cmd_test},
  { "w", "Set watchpoint", cmd_w},
  { "d", "Delete watchpoint", cmd_d},
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown commands '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();
  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
