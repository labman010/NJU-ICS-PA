#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    /* Event distribution */
    // printf("c->mepc : 0x%08x\n", c->mepc);
    // printf("c->mcause=%d\n", c->mcause);
    switch (c->mcause) {
      case 0:  ev.event = EVENT_NULL; break;
      // case 1:/* sys_yield */
      // case 2:/* sys_open */
      // case 3:/* sys_read */
      // case 4:/* sys_write */
      // case 7:/* sys_close */
      // case 8:/* sys_lseek */
      // case 9:/* sys_brk */
      // case 13:/* sys_execve */
      // case 19:/* sys_gettimeofday */
      case 0x0b:/* nemu will always be machine mode call */
        if(c->gpr[17] == -1) {  // 寄存器a7
          ev.event = EVENT_YIELD;
          c->mepc += 4; // trap是一种特殊的异常类型,在这种情况下,恢复时会运行下一条指令,所以要+4 (参考pa说明)
          break;
        }
        ev.event = EVENT_SYSCALL; break;
      default:
        printf("c->mcause=%d\n", c->mcause);
        ev.event = EVENT_ERROR; break;
    }
    c = user_handler(ev, c);
    assert(c != NULL);
  }

  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  return NULL;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}
