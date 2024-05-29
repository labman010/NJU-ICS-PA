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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write
#define CSR *csr_register

// Kconfig TRACE, 记录所有异常处理的踪迹
#ifdef CONFIG_ETRACE
uint32_t etrace_no = 0;
#define ETRACE_PRINT printf("\33[1;31metrace %d:\n\tepc:0x%x  mstatus:%x  mcause:%d  mtvec:0x%x\n\33[0m",\
           etrace_no++, cpu.csr.mepc, cpu.csr.mstatus, cpu.csr.mcause, cpu.csr.mtvec);
#endif


// 按照手册 mcause=0xb 表示的是environment call from M-mode
// 由于我们全程都在M模式下跑，因此ecall对应的mcause就是0xb
#define ECALL(dnpc) { \
  dnpc = (isa_raise_intr(0xb, s->pc)); \
  IFDEF(CONFIG_ETRACE, ETRACE_PRINT); \
  }

// clear MIE(4th bit) flag
// set MPIE to MIE
// set MPIE to 1
// clear MPP flag to consistent with spike
#define MRET { \
  s->dnpc = cpu.csr.mepc; \
  cpu.csr.mstatus &= ~(1<<3); \
  cpu.csr.mstatus |= ((cpu.csr.mstatus&(1<<7))>>4); \
  cpu.csr.mstatus |= (1<<7); \
  cpu.csr.mstatus &= ~((1<<11)+(1<<12)); \
}


enum {
  TYPE_I, TYPE_U, TYPE_S, TYPE_J, TYPE_R, TYPE_B,
  TYPE_N, // none
};

#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
// 对于jal,解析拼接后的值只表示最终offset位数范围[20:1],还少一个最低位,需要左移1位补全,实际是个21位有符号数扩展到32位
#define immJ() do { *imm = ((SEXT(BITS(i, 31, 31), 1) << 20) | (BITS(i, 19, 12) << 12) | (BITS(i, 20, 20) << 11) | (BITS(i, 30, 21) << 1)) ; } while(0)
#define immB() do { *imm = ((SEXT(BITS(i, 31, 31), 1) << 12) | (BITS(i, 7, 7) << 11) | (BITS(i, 30, 25) << 5) | (BITS(i, 11, 8) << 1)) ; } while(0)

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst.val;
  // 在riscv中，source/target寄存器的位置是固定的，所以可直接提取。和手册匹配。
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7); // 直接对应寄存器数组下标
  // 框架代码定义了src1R()和src2R()两个辅助宏, 用于寄存器的读取结果记录到相应的操作数变量中
  // 框架代码还定义了immI等辅助宏, 用于从指令中抽取出立即数
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_J:                   immJ(); break;
    case TYPE_R: src1R(); src2R();         break;
    case TYPE_B: src1R(); src2R(); immB(); break;
  }
}

static int decode_exec(Decode *s) {
  int rd = 0;
  word_t src1 = 0, src2 = 0, imm = 0;
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START();
  /*
    INSTPAT(模式字符串, 指令名称, 指令类型, 指令执行操作);
      指令名称：在代码中仅当注释使用, 不参与宏展开
      指令类型：用于后续译码过程type参数的传递
      指令执行操作：则是通过C代码来模拟指令执行的真正行为
  */
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm);

  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret   , J, MRET;);

  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall  , I, ECALL(s->dnpc)); 
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw  , I, R(rd) = CSR(imm), CSR(imm) = src1);  // 在汇编代码中显示为 csrw
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs  , I, R(rd) = CSR(imm), CSR(imm) |= src1); // 在汇编代码中显示为 csrr

  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb     , I, R(rd) = SEXT(Mr(src1 + imm, 1), 8));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh     , I, R(rd) = SEXT(Mr(src1 + imm, 2), 16));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu    , I, R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti   , I, R(rd) = ((sword_t)src1 < (sword_t)imm));
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = (src1 < imm));
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm); // 立即数异或
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori    , I, R(rd) = src1 | imm);
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1 & imm);
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, R(rd) = (sword_t)src1 >> (imm & 0x1f)); // 算术右移。imm有5位
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, R(rd) = src1 >> (imm & 0x1f)); // 逻辑右移
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   , I, R(rd) = src1 << (imm & 0x1f)); // 逻辑左移
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm); // 伪指令li的一种实现
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, R(rd) = s->snpc; s->dnpc = src1 + imm;
                                                                IFDEF(CONFIG_FTRACE, 
                                                                  if(s->isa.inst.val == 0x78067){
                                                                    // [TODO]不知道是什么,为什么跳过
                                                                    // 000000000000 01111 000 00000 1100111
                                                                    // 参考某个pa的说法:
                                                                      /*
                                                                        jr  <-> jalr x0, 0(rs1)
                                                                        0x00078067 | 00000000 00000111 10000000 01100111
                                                                        t =pc+4; pc=(x[15](a5)+offset(0))&∼1; x[0]=t
                                                                        here return address aborted due to `tail-recursive`
                                                                      */
                                                                  }
                                                                  else if(s->isa.inst.val == 0x00008067) {
                                                                    // 理解为调用返回指令
                                                                    // [riscv标准]即 ret -> jalr x0, 0(x1)
                                                                    // 000000000000 00001 000 00000 1100111
                                                                    ftrace_ret_print(s->pc, s->dnpc);
                                                                  }
                                                                  else if(rd == 1) {
                                                                    // (rd == 1 || rd == 5) && (src1 != 1 && src1 != 5)
                                                                    // [riscv标准] 函数调用指令
                                                                    ftrace_call_print(s->pc, s->dnpc, false);
                                                                  }
                                                                  else if(rd == 0 && imm == 0) {
                                                                    // 据说是尾调用的特征
                                                                    // [riscv标准]即 jr -> jalr x0, 0(rs1)
                                                                    // 区别于ret,这里rs1可以为其他的寄存器,是一种间接跳转。
                                                                    ftrace_call_print(s->pc, s->dnpc, true);
                                                                  }
                                                                ));

  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, R(rd) = s->snpc; s->dnpc = s->pc + imm; 
                                                                IFDEF(CONFIG_FTRACE, 
                                                                  if(rd == 1) { // [riscv标准]目标寄存器为x1或x5时, 判定在做函数调用
                                                                    ftrace_call_print(s->pc, s->dnpc, false);
                                                                  }
                                                                ));

  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, src2));

  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = src1 * src2); // [TODO] 是否需要转换为有符号数来运算？
  INSTPAT("0000001 ????? ????? 001 ????? 01100 11", mulh   , R, R(rd) =(long long)(sword_t)src1 * (long long)(sword_t)src2 >> 32);
  INSTPAT("0000001 ????? ????? 011 ????? 01100 11", mulhu  , R, R(rd) =(unsigned long long)src1 * (unsigned long long)src2 >> 32);
  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = (sword_t)src1 / (sword_t)src2);
  INSTPAT("0000001 ????? ????? 101 ????? 01100 11", divu   , R, R(rd) = src1 / src2);
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = (sword_t)src1 % (sword_t)src2); // 有符号数的余数
  INSTPAT("0000001 ????? ????? 111 ????? 01100 11", remu   , R, R(rd) = src1 % src2); // 无符号数的余数
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2);
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt    , R, R(rd) = (sword_t)src1 < (sword_t)src2);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = (src1 < src2));
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1 ^ src2);
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1 | src2);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2);
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1 << src2);
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (sword_t)src1 >> (src2 & 0x1f)); // 立即数算术右移
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = src1 >> (src2 & 0x1f)); // 立即数逻辑右移

  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, if(src1 == src2) {s->dnpc = s->pc + imm;} );
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, if(src1 != src2) {s->dnpc = s->pc + imm;} );
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, if((sword_t)src1 >= (sword_t)src2) {s->dnpc = s->pc + imm;}); // 有符号二进对比(二进制补码) >=
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, if(src1 >= src2) {s->dnpc = s->pc + imm;}); // 无符号数对比 >=
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, if((sword_t)src1 < (sword_t)src2) {s->dnpc = s->pc + imm;});
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu   , B, if(src1 < src2) {s->dnpc = s->pc + imm;});

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
