#include <stdbool.h>
#include <assert.h>

#include "riscv.h"
#include "riscv_private.h"


riscv_user_t rv_userdata(struct riscv_t *rv) {
  assert(rv);
  return rv->userdata;
}

bool rv_set_pc(struct riscv_t *rv, riscv_word_t pc) {
  assert(rv);
  if (pc & 3) {
    return false;
  }
  rv->PC = pc;
  return true;
}

riscv_word_t rv_get_pc(struct riscv_t *rv) {
  assert(rv);
  return rv->PC;
}

void rv_set_reg(struct riscv_t *rv, uint32_t reg, riscv_word_t in) {
  assert(rv);
  if (reg < RV_NUM_REGS && reg != rv_reg_zero) {
    rv->X[reg] = in;
  }
}

riscv_word_t rv_get_reg(struct riscv_t *rv, uint32_t reg) {
  assert(rv);
  if (reg < RV_NUM_REGS) {
    return rv->X[reg];
  }
  return ~0u;
}

riscv_exception_t rv_get_exception(struct riscv_t *rv) {
  return rv->exception;
}

void rv_set_exception(struct riscv_t *rv, uint32_t except) {
  rv->exception = except;
}

uint64_t rv_get_csr_cycles(struct riscv_t *rv) {
  return rv->csr_cycle;
}
