#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

// get a pointer to a CSR
static uint32_t *csr_get_ptr(struct riscv_t *rv, uint32_t csr) {
  switch (csr) {
  case CSR_CYCLE:
    return (uint32_t*)(&rv->csr_cycle) + 0;
  case CSR_CYCLEH:
    return (uint32_t*)(&rv->csr_cycle) + 1;
  case CSR_MSTATUS:
    return (uint32_t*)(&rv->csr_mstatus);
  case CSR_MTVEC:
    return (uint32_t*)(&rv->csr_mtvec);
  case CSR_MISA:
    return (uint32_t*)(&rv->csr_misa);
  case CSR_MSCRATCH:
    return (uint32_t*)(&rv->csr_mscratch);
  case CSR_MEPC:
    return (uint32_t*)(&rv->csr_mepc);
  case CSR_MCAUSE:
    return (uint32_t*)(&rv->csr_mcause);
  case CSR_MTVAL:
    return (uint32_t*)(&rv->csr_mtval);
  case CSR_MIP:
    return (uint32_t*)(&rv->csr_mip);
#if RISCV_VM_SUPPORT_RV32F
  case CSR_FCSR:
    return (uint32_t*)(&rv->csr_fcsr);
#endif
  default:
    return NULL;
  }
}

static bool csr_is_writable(uint32_t csr) {
  return csr < 0xc00;
}

// perform csrrw
uint32_t csr_csrrw(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c = val;
  }
  return out;
}

// perform csrrs (atomic read and set)
uint32_t csr_csrrs(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c |= val;
  }
  return out;
}

// perform csrrc (atomic read and clear)
uint32_t csr_csrrc(struct riscv_t *rv, uint32_t csr, uint32_t val) {
  uint32_t *c = csr_get_ptr(rv, csr);
  if (!c) {
    return 0;
  }
  const uint32_t out = *c;
  if (csr_is_writable(csr)) {
    *c &= ~val;
  }
  return out;
}

struct riscv_t *rv_create(const struct riscv_io_t *io, riscv_user_t userdata) {
  assert(io);
  riscv_t *rv = new riscv_t;
  // copy over the IO interface
  memcpy(&rv->io, io, sizeof(struct riscv_io_t));
  // copy over the userdata
  rv->userdata = userdata;
  // reset
  rv_reset(rv, 0u);
  // initalize jit engine
  rv_jit_init(rv);
  // return the rv structure
  return rv;
}

void rv_halt(struct riscv_t *rv) {
  rv->halt = true;
}

bool rv_has_halted(struct riscv_t *rv) {
  return rv->halt;
}

void rv_delete(struct riscv_t *rv) {
  assert(rv);
  // free any jit state
  rv_jit_free(rv);
  // free the rv structure
  delete rv;
}

void rv_reset(struct riscv_t *rv, riscv_word_t pc) {
  assert(rv);
  memset(rv->X, 0, sizeof(uint32_t) * RV_NUM_REGS);
  // set the reset address
  rv->PC = pc;
  // set the default stack pointer
  rv->X[rv_reg_sp] = DEFAULT_STACK_ADDR;
  // reset the csrs
  rv->csr_cycle = 0;
  rv->csr_mstatus = 0;
  // reset float registers
#if RISCV_VM_SUPPORT_RV32F
  memset(rv->F, 0, sizeof(float) * RV_NUM_REGS);
  rv->csr_fcsr = 0;
#endif
  rv->halt = false;
}
