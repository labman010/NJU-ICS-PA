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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

#define MAX_BUF_LEN 5000  // change max to 5000
// this should be enough (NO, this is too much...)
static char buf[65536] = {};
static int buf_len = 0;
static int invalid_expr = 0;
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static int choose(int n) {
  return rand() % n;
}

static void buf_write(char c) {
  if (buf_len >= MAX_BUF_LEN - 1) {
    invalid_expr = 1;
  }
  else {
    buf[buf_len ++] = c;
  }
}

static void gen(char c) {
  buf_write(c);
}

static void gen_num() {
  int times = rand() % 3 + 1; // number value max is thousands
  char c = '1' + choose(9);
  buf_write(c);
  for (int i = 0; i < times; i++) {
    char c = '0' + choose(10);
    buf_write(c);
  }
}

static void gen_rand_op() {
  switch (choose(4)) {
    case 0: gen('+'); break;
    case 1: gen('-'); break;
    case 2: gen('*'); break;
    case 3: gen('/'); break;
  }
}


static void gen_rand_blank(){
  switch (choose(7))
  {
  case 0:
  case 1:
  case 2:
  case 3:
    break;

  case 4:
  case 5:
    buf_write(' ');
    break;

  case 6:
    buf_write(' ');
    buf_write(' ');
    break;
  }
}

static void gen_rand_expr() {
  if(invalid_expr) {
    return;
  }
  switch (choose(3)) {
    case 0: {
      gen_rand_blank();
      gen_num();
      gen_rand_blank();
      break;
    }
    case 1: {
      gen_rand_blank();
      gen('(');
      gen_rand_blank();
      gen_rand_expr();
      gen_rand_blank();
      gen(')');
      gen_rand_blank();
      break;
    }
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
}

// 我无法判断是否出现除号之后的表达式存在值为0的情况...
// 如果存在上面情况,运行时不会异常退出,而是warning并记录整个表达式结果值为0到文件
int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    memset(buf, 0, sizeof(buf));
    buf_len = 0;
    invalid_expr = 0;
    gen_rand_expr();

    if (invalid_expr) {
      i --;
      // printf("========== invalid expr skipped ==========\n");
      continue;
    }

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
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

