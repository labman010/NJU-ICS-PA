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
#include <memory/paddr.h>
#include "sdb.h"
/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,
  TK_DECIMAL,
  TK_HEXNUM,
  TK_REG,
  TK_NOTEQ,
  TK_LGAND,
  TK_DEREF,
  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */
  {" +", TK_NOTYPE},      // spaces
  {"0x[0-9A-Fa-f]+", TK_HEXNUM}, // hex number
  {"\\$[0-9a-z]+", TK_REG},     // register
  {"[0-9]+", TK_DECIMAL},// decimal integers

  {"\\(", '('},          // left parenthesis
  {"\\)", ')'},          // right parenthesis
  {"\\*", '*'},          // multiply
  {"/", '/'},            // divide
  {"\\+", '+'},          // plus
  {"-", '-'},            // minus

  {"==", TK_EQ},         // equal
  {"!=", TK_NOTEQ},      // not equal

  {"&&", TK_LGAND},      // logical and
  // 若后续添加其他，需要考虑识别解引用的框架代码是否也要修改
};

#define INVALID_OP_INDEX  -1
#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[MAX_TOKENS] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

#define NR_TOKEN ARRLEN(tokens)  // 控制token不越界

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        Assert(nr_token < NR_TOKEN, "exp with too much tokens!");
        switch (rules[i].token_type) {
          case TK_NOTYPE: break;
          case TK_HEXNUM:
          case TK_REG: 
          case TK_DECIMAL: { // 十进制, 十六进制, 寄存器名 解析方式相同
            tokens[nr_token].type = rules[i].token_type;
            if (substr_len >= 32) {
              printf("The length of the decimal integer is too long.\n");
              return false;
            }
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            nr_token++;
            break;
          }
          case '+':
          case '-':
          case '*':
          case '/':
          case '(':
          case ')':
          case TK_EQ:
          case TK_NOTEQ:
          case TK_LGAND: {
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
          }
          default: printf("Unrecognized token type.\n");
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool check_parentheses(int p, int q) {

  if (!(tokens[p].type == '(' && tokens[q - 1].type == ')')) {
    return false;
  }
  ++p;
  --q;
  int count = 0;
  for (int i = p; i < q; i++) {
      if (tokens[i].type == '(') {
          count++;
      } else if (tokens[i].type == ')') {
          count--;
          if (count < 0) {
              return false; // 右括号比左括号多
          }
      }
  }
  return count == 0;
}

// 获取候选主运算符优先级,数值越小,优先级越高,越接近主运算符。(与运算符优先级相反)
static inline uint32_t get_priority(int type) {
    switch(type) {
      case TK_LGAND:
        return 0;
      case TK_EQ:
      case TK_NOTEQ:
        return 1;
      case '+':
      case '-':
        return 2; 
      case '*':
      case '/':
        return 3;
      default: Assert(0, "[Unexpected] Inside function '%s' at line %d\n", __FUNCTION__, __LINE__);
    }
}

// 获取表达式中的主运算符
int get_main_operator(int p, int q) {
  int main_operator = INVALID_OP_INDEX;
  uint32_t current_priority = 0xffffffff;
  int in_parentheses = 0;
  int i;
  // 从左向右遍历表达式
  for (i = p; i < q; i++) {
    switch (tokens[i].type) {
      case '(':
        in_parentheses++;
        break;
      case ')':
        in_parentheses--;
        break;
      case '+':
      case '-':
      case '*':
      case '/': 
      case TK_EQ:
      case TK_NOTEQ:
      case TK_LGAND: {
        if (in_parentheses == 0) {
          int priority = get_priority(tokens[i].type);
          if (main_operator == INVALID_OP_INDEX || priority <= current_priority) {
              main_operator = i;
              current_priority = priority;
          }
        }
        break;
      }
      Assert(in_parentheses >= 0, "Unexpected: maybe parentheses not match!");  // TODO: add *success for error handling
      default:break;
    }
  }
  Assert(in_parentheses == 0, "Unexpected parentheses: it's impossible...");

  return main_operator;
}

// p , q are range of index of tokens, which is [p , q)
int eval(int p, int q, bool *success) {
  if (p >= q) {
    /* Bad expression or no tokens found */
    Assert(0, "[TMP] Bad expression!");
  }
  else if (p + 1 == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */

    uint32_t buffer = 0;
    switch (tokens[p].type){
    case TK_HEXNUM:
      sscanf(tokens[p].str, "%x", &buffer);
      break;
    case TK_DECIMAL:
      sscanf(tokens[p].str, "%u", &buffer);
      break;
    case TK_REG: {
      if (strcmp(tokens[p].str, "$pc") == 0){
        buffer = cpu.pc;
      }else {
        buffer = isa_reg_str2val(tokens[p].str, success);
      }
      if (!*success){
        printf("isa_reg_str2val() failed!\n");
        return 0;
      }
      break;
    }
    default:
      Assert(0, "Invalid expression!");
    }
    return buffer;
  }
  else if (p + 2 == q || check_parentheses(p + 1, q) == true){//长度为2的子表达式呈型于解引用 "*" <expr>  或 负数(暂未实现)
    switch (tokens[p].type) {
    case TK_DEREF:
      return *((uint32_t *)guest_to_host(eval(p + 1, q, success)));
    default:
      Assert(0, "Invalid expression! expr length2");
    }
  } 
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1, success);
  }
  else {
    int op = get_main_operator(p, q);
    Assert(op != INVALID_OP_INDEX, "Unexpected: it's impossible...1");

    int val1 = eval(p, op, success);
    int val2 = eval(op + 1, q, success);

    switch (tokens[op].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': {
        if (val2 == 0) {
          printf("Divided by zero, Invalid expression!\n");
          *success = false;
          return 0;
        }
        return val1 / val2;
      }
      case TK_EQ: return val1 == val2;
      case TK_NOTEQ: return val1 != val2;
      case TK_LGAND: return val1 && val2;
      default: assert(0);
    }
  }
}

word_t expr(char *e, bool *success) {
  *success = true;
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */

  // 框架代码++，识别解引用
  int i;
  for (i = 0; i < nr_token; i ++) {
    if (tokens[i].type == '*' && (i == 0 || tokens[i - 1].type == '(' ||
        tokens[i - 1].type == '+' || tokens[i - 1].type == '-' ||
        tokens[i - 1].type == '*' || tokens[i - 1].type == '/' ||
        tokens[i - 1].type == TK_EQ || tokens[i - 1].type == TK_NOTEQ ||
        tokens[i - 1].type == TK_LGAND ) ) { 
      tokens[i].type = TK_DEREF;
    }
  }

  int output = eval(0, nr_token, success);
  if (*success == false) {
    return 0;
  }
  *success = true;
  nr_token = 0;    // clear record
  return (word_t)output;
}
