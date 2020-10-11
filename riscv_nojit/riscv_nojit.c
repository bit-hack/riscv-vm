#include <stdbool.h>
#include <stdint.h>

#include "../riscv_emu/riscv.h"
#include "../riscv_emu/riscv_private.h"

bool rv_step_jit(struct riscv_t *rv, const uint64_t cycles_target) {
  return false;
}

bool rv_init_jit(struct riscv_t *rv) {
  return false;
}
