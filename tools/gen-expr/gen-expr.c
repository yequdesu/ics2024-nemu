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
#define BLANK_NUM_MAX 2
#define RECURSIVE_DEPTH 5
#define NUM_MIN -100
#define NUM_MAX 100

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int choose();
static void gen_num();
static void gen(char c);
static void gen_rand_expr();
static void gen_rand_op();
static void gen_blank();
static int expr_complex_cal();

static int choose(int n) {
  return (int)(rand() % n);
}

static void gen_num() {
    int num = NUM_MIN + (rand() % (NUM_MAX + 1));
    
    char num_str[8];
    sprintf(num_str, "%d", num);
    strcat(buf, num_str);
}

static void gen(char c) {
    char temp[2] = {c, '\0'};
    strcat(buf, temp);
}

static void gen_blank() {
  int num = rand() % BLANK_NUM_MAX;
  for(int i = 0; i < num; i++) {
    strcat(buf, " ");
  }
}

static int expr_complex_cal() {
  int len = strlen(buf);
  int op_count = 0;
  for (int i = 0; i < len; i++) {
      if (buf[i] == '+' || buf[i] == '-' || buf[i] == '*' || buf[i] == '/') {
          op_count++;
          continue;
      }
  }
  return op_count;
}

static void gen_rand_op() {
  switch (choose(4)) {
    case 0: strcat(buf, "+"); break;
    case 1: strcat(buf, "-"); break;
    case 2: strcat(buf, "*"); break;
    case 3: strcat(buf, "/"); break;
  }
}

static void gen_rand_expr(int d) {
  if(d <= 0) {gen_num(); return;}
  switch (choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_blank(); gen_rand_expr(d - 1); gen_blank(); gen(')'); break;
    default: gen_rand_expr(d - 1); gen_blank(); gen_rand_op(); gen_blank(); gen_rand_expr(d - 1); break;
  }
}

static void solve_sub_sub() {
  for(int i = 0; i < strlen(buf) - 1; i++) {
    if(buf[i] == '-' && buf[i + 1] == '-') {
      buf[i] = ' ';
      buf[i + 1] = '+';
    }
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf[0] = '\0';
    gen_rand_expr(RECURSIVE_DEPTH);

    while(expr_complex_cal() <= 10) {buf[0] = '\0'; gen_rand_expr(RECURSIVE_DEPTH);}

    solve_sub_sub();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    // filter div-by-zero expressions by adding -Wall -Werror
    int ret = system("gcc /tmp/.code.c -Wall -Werror -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
