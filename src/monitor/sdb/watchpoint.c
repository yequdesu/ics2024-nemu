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

#include <sdb/sdb.h>
#include <string.h>

#define NR_WP 32
#define EXPR_MAX_LEN 64

typedef struct watchpoint {
  int NO;
  char watchexpr[EXPR_MAX_LEN];
  word_t value;
  struct watchpoint *next;
} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

static WP* new_wp(char *watchexpr);
static void free_wp(WP* wp);
static WP* tail_del(WP** list_head);
static void tail_insert(WP** list_head, WP* wp);
static WP* find_wp(int no);
static void remove_wp(WP** list_head, WP* wp);

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].watchexpr[0] = '\0';
  }

  head = NULL;
  free_ = wp_pool;
}

static WP* tail_del(WP** list_head) {
  if (*list_head == NULL) {
    return NULL;
  }
  
  WP *prev = NULL;
  WP *current = *list_head;
  
  while (current->next != NULL) {
    prev = current;
    current = current->next;
  }
  
  if (prev == NULL) {
    *list_head = NULL;
  } else {
    prev->next = NULL;
  }
  
  return current;
}

static void tail_insert(WP** list_head, WP* wp) {
  if (wp == NULL) return;
  
  wp->next = NULL;
  
  if (*list_head == NULL) {
    *list_head = wp;
    return;
  }
  
  WP *current = *list_head;
  while (current->next != NULL) {
    current = current->next;
  }
  
  current->next = wp;
}

static void remove_wp(WP** list_head, WP* wp) {
  if (*list_head == NULL || wp == NULL) return;
  
  if (*list_head == wp) {
    *list_head = wp->next;
    wp->next = NULL;
    return;
  }
  
  WP *prev = *list_head;
  while (prev != NULL && prev->next != wp) {
    prev = prev->next;
  }
  
  if (prev != NULL) {
    prev->next = wp->next;
    wp->next = NULL;
  }
}

static WP* find_wp(int no) {
  WP *current = head;
  while (current != NULL) {
    if (current->NO == no) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

static WP* new_wp(char *watchexpr) {
  WP* wp = tail_del(&free_);
  if (wp == NULL) {
    printf("No free watchpoints available.\n");
    return NULL;
  }
  
  bool success;
  strncpy(wp->watchexpr, watchexpr, EXPR_MAX_LEN - 1);
  wp->watchexpr[EXPR_MAX_LEN - 1] = '\0';
  wp->value = expr(watchexpr, &success);
  
  if (!success) {
    tail_insert(&free_, wp);
    printf("Invalid expression: %s\n", watchexpr);
    return NULL;
  }
  
  tail_insert(&head, wp);
  return wp;
}

static void free_wp(WP* wp) {
  if (wp == NULL) return;
  
  remove_wp(&head, wp);
  tail_insert(&free_, wp);
  wp->watchexpr[0] = '\0';
}

void scan_wp(char *print) {
  if (strcmp(print, "print") == 0) {
    if (head == NULL) {
      printf("No watchpoints set.\n");
      return;
    }
    
    WP* current = head;
    while (current != NULL) {
      printf("wp %d: %s, value: %u\n", current->NO, current->watchexpr, current->value);
      current = current->next;
    }
    return;
  }
  
  if (head == NULL) {
    return;
  }
  
  WP* current = head;
  while (current != NULL) {
    bool success;
    word_t new_value = expr(current->watchexpr, &success);
    
    if (success && new_value != current->value) {
      printf("Watchpoint %d: %s changed from %u to %u\n", 
             current->NO, current->watchexpr, current->value, new_value);
      current->value = new_value;
      nemu_state.state = NEMU_STOP;
      return;
    }
    current = current->next;
  }
}

void set_watchpoint(char *expr) {
  WP* wp = new_wp(expr);
  if (wp != NULL) {
    printf("Set watchpoint #%d: %s = %u\n", wp->NO, wp->watchexpr, wp->value);
  }
}

void delete_watchpoint(int no) {
  WP* wp = find_wp(no);
  if (wp != NULL) {
    free_wp(wp);
    printf("Deleted watchpoint #%d\n", no);
  } else {
    printf("Watchpoint #%d not found\n", no);
  }
}
