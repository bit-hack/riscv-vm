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

uint64_t rv_get_csr_cycles(struct riscv_t *rv) {
  return rv->csr_cycle;
}

void rv_except_inst_misaligned(struct riscv_t *rv, uint32_t old_pc) {
  const uint32_t base = rv->csr_mtvec & ~0x3;
  const uint32_t mode = rv->csr_mtvec & 0x3;

  const uint32_t code = 0;  // instruction address misaligned

  rv->csr_mepc = old_pc;
  rv->csr_mtval = rv->PC;

  switch (mode) {
  case 0: // DIRECT
    rv->PC = base;
    break;
  case 1: // VECTORED
    rv->PC = base + 4 * code;
    break;
  }

  rv->csr_mcause = code;
}

void rv_except_load_misaligned(struct riscv_t *rv, uint32_t addr) {
  const uint32_t base = rv->csr_mtvec & ~0x3;
  const uint32_t mode = rv->csr_mtvec & 0x3;

  const uint32_t code = 4;  // load address misaligned

  rv->csr_mepc = rv->PC;
  rv->csr_mtval = addr;

  switch (mode) {
  case 0: // DIRECT
    rv->PC = base;
    break;
  case 1: // VECTORED
    rv->PC = base + 4 * code;
    break;
  }

  rv->csr_mcause = code;
}

void rv_except_store_misaligned(struct riscv_t *rv, uint32_t addr) {
  const uint32_t base = rv->csr_mtvec & ~0x3;
  const uint32_t mode = rv->csr_mtvec & 0x3;

  const uint32_t code = 6;  // store address misaligned

  rv->csr_mepc = rv->PC;
  rv->csr_mtval = addr;

  switch (mode) {
  case 0: // DIRECT
    rv->PC = base;
    break;
  case 1: // VECTORED
    rv->PC = base + 4 * code;
    break;
  }

  rv->csr_mcause = code;
}

void rv_except_illegal_inst(struct riscv_t *rv) {
  assert(!"illegal instruction");
}
