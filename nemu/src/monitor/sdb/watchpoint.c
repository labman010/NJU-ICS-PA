  /***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
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

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp() {
  if (free_ == NULL) {
    Assert(0, "Error: No free watchpoints available");
  }

  WP *new_watchpoint = free_;
  free_ = free_->next;
  new_watchpoint->next = head;
  head = new_watchpoint;
  return new_watchpoint;
}

void free_wp(WP *wp) {
  if (wp == NULL) {
    fprintf(stderr, "Error: Trying to free a NULL watchpoint\n");
    return;
  }
  
  WP *current = head;
  WP *prev = NULL;
  while (current != NULL && current != wp) {
      prev = current;
      current = current->next;
  }

  if (current == NULL) {
      fprintf(stderr, "Error: Watchpoint not found in active list\n");
      return;
  }

  if (prev == NULL) {
      head = head->next;
  } else {
      prev->next = current->next;
  }

  wp->next = free_;
  free_ = wp;
}

bool add_watchpoint(const char *args, uint32_t value) {
  if (strlen(args) >= MAX_EXPR_LEN) {
    printf("watchpoint string too long!\n");
    return false;
  }
  WP* p = new_wp();
  strcpy(p->expr, args);
  p->value = value;
  return true;
}

void delete_watchpoint(int NO) {  // 这里其实是O(NR_WP^2)复杂度,可优化
  WP *p = head;
  while (p != NULL && p->NO != NO) {
    p = p->next;
  }
  if (p == NULL) {
      printf("Error: Watchpoint not found in active list\n");
      return;
  }
  free_wp(p);
}

bool check_watchpoint() {
  WP *p = head;
  bool success = true;
  bool changed = false;
  while (p){
    uint32_t value = expr(p->expr, &success);
    if (value != p->value || !success){
      changed = true;
      printf("nemu watchpoint %d: %s\n", p->NO, p->expr);
      printf("\tOld value = %u\n\tNew value = %u\n", p->value, value);
      p->value = value;
    }
    p = p->next;
  }
  return changed;
}

void watchpoint_display() {
  WP* cur = head;
  if (cur == NULL) {
    printf("No watchpoint found.\n");
    return;
  }
  printf("NO.\tExpr\n");
  while (cur){
    printf("\e[1;36m%d\e[0m\t\e[0;32m%s\e[0m\n", cur->NO, cur->expr);
    cur = cur->next;
  }
}