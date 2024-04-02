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

#ifndef __SDB_H__
#define __SDB_H__

#include <common.h>

/*
 ############ for expr calculation ##############
*/
#define MAX_TOKENS 32               // 框架代码最多支持32个toekn,测试expr时可调整扩容。
#define MAX_EXPR_LEN  MAX_TOKENS*4  // 假设每个token对应原表达式的4个字符。
word_t expr(char *e, bool *success);

/*
 ############ for watchpoint ##############
*/
typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  uint32_t value;
  char expr[MAX_EXPR_LEN];
} WP;

bool add_watchpoint(const char *args, uint32_t value);
void delete_watchpoint(int NO);
bool check_watchpoint(); // 同gdb，打印所有表达式值出现变动的watchpoint
void watchpoint_display();



#endif
