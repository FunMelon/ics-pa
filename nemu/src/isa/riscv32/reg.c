#include "local-include/reg.h"
#include <isa.h>
#include <stdio.h>

const char *regs[] = {"$0", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                      "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                      "a6", "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                      "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

void isa_reg_display() {
  printf("\033[32m");
  for (size_t i = 0; i < 32; ++i) {
    printf("%3s: %#010X %12d ", regs[i], cpu.gpr[i]._32, cpu.gpr[i]._32);
    if (!((i + 1) % 4))
      putchar('\n');
  }
  printf("%3s: %#010X %12d \n", "pc", cpu.pc, cpu.pc);
  printf("\033[0m");
}

word_t isa_reg_str2val(const char *s, bool *success) {
  *success = false;
  size_t i = 0;
  while (i < sizeof(regs) / sizeof(char *)) {
    if (strcmp(s, regs[i]) == 0) {
      *success = true;
      // Log("匹配到寄存器%s，值为%d", s, cpu.gpr[i]._32);
      return cpu.gpr[i]._32;
    }
    ++i;
  }
  if (strcmp(s, "pc") == 0) {
    *success = true;
    // Log("匹配到寄存器%s，值为%d", s, pc);
    return cpu.pc;
  }
  return 0;
}
