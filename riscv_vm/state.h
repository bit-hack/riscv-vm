#pragma once

#include "../riscv_core/riscv.h"

#include "memory.h"

// define the valid memory region for sbrk
enum {
  sbrk_start = 0x10000000,
  sbrk_end = 0x1fffffff,
};

// state structure passed to the VM
struct state_t {
  memory_t mem;
  bool done;
  // the data segment break address
  riscv_word_t break_addr;
};
