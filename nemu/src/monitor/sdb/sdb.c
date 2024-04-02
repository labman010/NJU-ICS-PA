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

#include <isa.h>
#include <cpu/cpu.h>
#include <memory/vaddr.h>  // should be included for "Scan memory"???
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

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

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_help(char *args);
static int cmd_si(char *args);
static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Step through 1 or N instructions", cmd_si },
  { "info", "Display status of register or watch point", cmd_info },
  { "x", "Scan memory", cmd_x },
  { "p", "Calculate expression", cmd_p },
  { "w", "Set watch point", cmd_w },
  { "d", "Delete watch point", cmd_d },
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
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args) {
  char *arg = strtok(NULL, " ");

  if (arg == NULL) {
    /* no argument given */
    // execute one instruction
    cpu_exec(1);
  }
  else {
    // execute N instructions
    int n = atoi(arg);
    cpu_exec(n);
  }
  return 0;
}

static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");

  if (arg == NULL) {
    printf("Wrong usage of info, did you mean\n    info r\n    info w\n");
  }
  else {
    if (strcmp(arg, "r") == 0) {
      isa_reg_display();
    }
    else if (strcmp(arg, "w") == 0) {
      watchpoint_display();
    }
    else {
      printf("Unknown argument '%s'\n", arg);
    }
  }
  return 0;
}

static int cmd_x(char *args) {
  char *arg = strtok(NULL, " ");

  if (arg == NULL) {
    printf("Wrong usage of x, missing argument\n");
  }
  else {
    int n = atoi(arg);
    char *hex_part = strtok(NULL, " ");
    if (hex_part == NULL) {
        printf("Wrong format of x! expect 2 arguments.\n");
        return 0;
    }
    // 解析hex_part，转化为16进制数作为地址
    word_t addr = strtoul(hex_part, NULL, 16);;
    // 循环打印从addr开始的n组内存数据，每组4字节，内存在pmem中。
    for (int i = 0; i < n; i++) {
      printf("0x%08x: ", addr);
      for (int j = 0; j < 4; j++) {
        /* 
          注意是否合理: len设置为1，我严格按照字节顺序打印，结果与cpu_exec()打印的不同，因为是大端。
        */
        word_t value = vaddr_read(addr, 1);
        printf("%02x ", value);
        addr += 1;
      }
      printf("\n");
    }
  }
  return 0;
}

static int cmd_p(char *args) {
  bool success;
  uint32_t v = expr(args, &success);
  if (success)
    printf("%s = \e[1;36m%u\e[0m\n", args, v);
  else {
    printf("cmd_p failed\n");
  }
  return 0;
}

static int cmd_w(char *args) {
  bool success;
  uint32_t v = expr(args, &success);
  if (!success) {
    printf("cmd_w failed,Something wrong with the watchpoint expression\n");
    return 0;
  }
  if (!add_watchpoint(args, v)) {
    printf("cmd_w failed\n");
  }
  return 0;
}

static int cmd_d(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Wrong usage of d, missing argument\n");
  }
  else {
    char *endptr;
    int n = strtol(args, &endptr, 10);
    if (endptr == args || *endptr != '\0') {
        printf("Invalid watchpoint number: %s\n", args);
    } else {
      delete_watchpoint(n);
    }
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
#ifdef TEST_EXPR
  char line[65536 + 128] = {0};
  FILE *fp = fopen("./tools/gen-expr/input", "r"); 
  if (fp == NULL) {
    printf("open file failed\n");
    assert(0);
  }
  int correct = 0;
  int line_num = 0;

  while (fgets(line, sizeof(line), fp)) {
    size_t len = strlen(line); // 获取字符串的长度
    if (len > 0 && line[len - 1] == '\n') { // 检查末尾是否是换行符
        line[len - 1] = '\0'; // 将换行符替换为字符串结束符
    }
    char *space_position = strchr(line, ' ');
    *space_position = '\0';
    unsigned int val = (unsigned int)atoi(line);
    printf("val: %u\n", val);
    printf("expression: %s\n", space_position + 1);

    bool success = true;
    unsigned int outcome = expr(space_position + 1, &success);
    if (success) {
      if (val == outcome) {
        correct++;
      }
      else {
        volatile static int tmp_val = 0;
        printf("wrong answer, please debug! val:%u outcome:%u\n", val, outcome);
        tmp_val++;
      }
    }
    else {
      // 这里表示expr认为这个表达式不合法, 可能是输入的测试用例存在除数0的问题,所以跳过。
      continue;
    }
    line_num++;
    if (line_num > 100) break;
  }
  printf("score: %f\n", (float)correct / line_num);
  assert(0);
#endif

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
