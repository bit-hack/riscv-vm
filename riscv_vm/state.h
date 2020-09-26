#pragma once

#include "../riscv_core/riscv.h"

#include "memory.h"

// state structure passed to the VM
struct state_t {
  memory_t mem;
  bool done;
  // the data segment break address
  riscv_word_t break_addr;
};
