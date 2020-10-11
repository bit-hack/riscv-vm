#pragma once
#include <stdint.h>

#include "../riscv_emu/riscv_private.h"

#include "ir.h"

struct ir_builder_t {
  struct ir_block_t ir;
  int32_t pc;
  int32_t instructions;
};
