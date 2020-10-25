#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#if __linux__
//#include <asm/cachectl.h>
#include <sys/mman.h>
#endif

#include "riscv.h"
#include "riscv_private.h"

#include "decode.h"


// note: one idea would be to keep track of how often a block is hit and then
//       when inserting it into the hash map, we swap the hot block closer to
//       its ideal hash location.  it will then have a faster lookup.


// callback for unhandled op_op instructions
static void handle_op_op(struct riscv_t *rv, uint32_t inst) {
  // r-type decode
  const uint32_t rd     = dec_rd(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1    = dec_rs1(inst);
  const uint32_t rs2    = dec_rs2(inst);
  const uint32_t funct7 = dec_funct7(inst);

  switch (funct7) {
  case 0b0000001:
    // RV32M instructions
    switch (funct3) {
    case 0b010: // MULHSU
      {
        const int64_t a = (int32_t)rv->X[rs1];
        const uint64_t b = rv->X[rs2];
        rv->X[rd] = ((uint64_t)(a * b)) >> 32;
      }
      break;
    case 0b100: // DIV
      {
        const int32_t dividend = (int32_t)rv->X[rs1];
        const int32_t divisor = (int32_t)rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = ~0u;
        }
        else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
          rv->X[rd] = rv->X[rs1];
        }
        else {
          rv->X[rd] = dividend / divisor;
        }
      }
      break;
    case 0b101: // DIVU
      {
        const uint32_t dividend = rv->X[rs1];
        const uint32_t divisor = rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = ~0u;
        }
        else {
          rv->X[rd] = dividend / divisor;
        }
      }
      break;
    case 0b110: // REM
      {
        const int32_t dividend = rv->X[rs1];
        const int32_t divisor = rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = dividend;
        }
        else if (divisor == -1 && rv->X[rs1] == 0x80000000u) {
          rv->X[rd] = 0;
        }
        else {
          rv->X[rd] = dividend % divisor;
        }
      }
      break;
    case 0b111: // REMU
      {
        const uint32_t dividend = rv->X[rs1];
        const uint32_t divisor = rv->X[rs2];
        if (divisor == 0) {
          rv->X[rd] = dividend;
        }
        else {
          rv->X[rd] = dividend % divisor;
        }
      }
      break;
    default:
      assert(!"unreachable");
    }
    break;
  default:
    assert(!"unreachable");
  }
}

static void handle_op_system(struct riscv_t *rv, uint32_t inst) {
  // i-type decode
  const int32_t  imm = dec_itype_imm(inst);
  const int32_t  csr = dec_csr(inst);
  const uint32_t funct3 = dec_funct3(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rd = dec_rd(inst);

  uint32_t tmp;

  // dispatch by func3 field
  switch (funct3) {
#if RISCV_VM_SUPPORT_Zicsr
  case 1: // CSRRW    (Atomic Read/Write CSR)
    tmp = csr_csrrw(rv, csr, rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 2: // CSRRS    (Atomic Read and Set Bits in CSR)
    tmp = csr_csrrs(rv, csr, (rs1 == rv_reg_zero) ? 0u : rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 3: // CSRRC    (Atomic Read and Clear Bits in CSR)
    tmp = csr_csrrc(rv, csr, (rs1 == rv_reg_zero) ? ~0u : rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 5: // CSRRWI
    tmp = csr_csrrc(rv, csr, rv->X[rs1]);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 6: // CSRRSI
    tmp = csr_csrrs(rv, csr, rs1);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
  case 7: // CSRRCI
    tmp = csr_csrrc(rv, csr, rs1);
    rv->X[rd] = rd ? tmp : rv->X[rd];
    break;
#endif  // RISCV_VM_SUPPORT_Zicsr
  default:
    assert(!"unreachable");
  }
}

// callback for unhandled op_fp instructions
static void handle_op_fp(struct riscv_t *rv, uint32_t inst) {
  const uint32_t rd = dec_rd(inst);
  const uint32_t rs1 = dec_rs1(inst);
  const uint32_t rs2 = dec_rs2(inst);
  const uint32_t rm = dec_funct3(inst); // TODO: rounding!
  const uint32_t funct7 = dec_funct7(inst);
  // dispatch based on func7 (low 2 bits are width)
  switch (funct7) {
  case 0b0010000:
  {
    uint32_t f1, f2, res;
    memcpy(&f1, rv->F + rs1, 4);
    memcpy(&f2, rv->F + rs2, 4);
    switch (rm) {
    case 0b000:  // FSGNJ.S
      res = (f1 & ~FMASK_SIGN) | (f2 & FMASK_SIGN);
      break;
    case 0b001:  // FSGNJN.S
      res = (f1 & ~FMASK_SIGN) | (~f2 & FMASK_SIGN);
      break;
    case 0b010:  // FSGNJX.S
      res = f1 ^ (f2 & FMASK_SIGN);
      break;
    default:
      assert(!"unreachable");
    }
    memcpy(rv->F + rd, &res, 4);
    break;
  }
  case 0b0010100:
    switch (rm) {
    case 0b000:  // FMIN
      rv->F[rd] = fminf(rv->F[rs1], rv->F[rs2]);
      break;
    case 0b001:  // FMAX
      rv->F[rd] = fmaxf(rv->F[rs1], rv->F[rs2]);
      break;
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1110000:
    switch (rm) {
    case 0b001:  // FCLASS.S
    {
      uint32_t bits;
      memcpy(&bits, rv->F + rs1, 4);
      rv->X[rd] = calc_fclass(bits);
      break;
    }
    default:
      assert(!"unreachable");
    }
    break;
  case 0b1010000:
    switch (rm) {
    case 0b010:  // FEQ.S
      rv->X[rd] = (rv->F[rs1] == rv->F[rs2]) ? 1 : 0;
      break;
    case 0b001:  // FLT.S
      rv->X[rd] = (rv->F[rs1] < rv->F[rs2]) ? 1 : 0;
      break;
    case 0b000:  // FLE.S
      rv->X[rd] = (rv->F[rs1] <= rv->F[rs2]) ? 1 : 0;
      break;
    default:
      assert(!"unreachable");
    }
    break;
  default:
    assert(!"unreachable");
  }
}

static void rv_translate_block(struct riscv_t *rv, struct block_t *block) {
  assert(rv && block);

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;
  block->predict = nullptr;

  // decompose the basic block
  for (;;) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, block->pc_end);
    // decode
    block->inst.emplace_back();
    rv_inst_t &dec = block->inst.back();
    uint32_t pc = block->pc_end;
    if (!decode(inst, &dec, &pc)) {
      assert(!"unreachable");
    }
    ++block->instructions;
    block->pc_end = pc;
    // stop on branch
    if (inst_is_branch(&dec)) {
      break;
    }
  }
}

static struct block_t *block_find_or_translate(struct riscv_t *rv, struct block_t *prev) {
  block_map_t &map = rv->jit.block_map;

  // check the block prediction first
  if (prev && prev->predict && prev->predict->pc_start == rv->PC) {
    return prev->predict;
  }

  // lookup the next block in the block map
  block_t *next = map.find(rv->PC);

  // translate if we didnt find one
  if (!next) {

    // allocate a new block
    next = map.alloc(rv->PC);
    assert(next);

    // translate the basic block
    rv_translate_block(rv, next);

    // update the block predictor
    // note: if the block predictor gives us a win when we
    //       translate a new block but gives us a huge penalty when
    //       updated after we find a new block.  didnt expect that.
    if (prev) {
      prev->predict = next;
    }
  }

  // return the next block
  assert(next);
  return next;
}

void rv_step(struct riscv_t *rv, int32_t cycles) {

  // find or translate a block for our starting PC
  struct block_t *block = block_find_or_translate(rv, NULL);

  const uint64_t cycles_start = rv->csr_cycle;
  const uint64_t cycles_target = rv->csr_cycle + cycles;

  // loop until we hit out cycle target
  while (rv->csr_cycle < cycles_target && !rv->halt) {

    // if this block has no instructions we cant make forward progress so
    // must fallback to instruction emulation
    if (!block->instructions) {
      assert(!"unable to execute empty block");
    }

    // call the translated block
    assert(block);
    emulate_block(rv, *block);

    // increment the cycles csr
    rv->csr_cycle += block->instructions;

    // try to predict the next block
    // note: block predition gives us ~100 MIPS boost.
    if (block->predict && block->predict->pc_start == rv->PC) {
      block = block->predict;
    }
    else {
      // lookup the next block in the block map or translate a new block
      struct block_t *next = block_find_or_translate(rv, block);
      // move onto the next block
      block = next;
    }
  }
}

bool rv_jit_init(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;

  // setup nonjit instruction callbacks
  // XXX: we can replace this with just instruction emulation
  jit->handle_op_op     = handle_op_op;
  jit->handle_op_fp     = handle_op_fp;
  jit->handle_op_system = handle_op_system;

  return true;
}

void rv_jit_free(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;
}
