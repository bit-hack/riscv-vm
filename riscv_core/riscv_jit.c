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

// calling convention
//
//        windows     linux
//  arg1  RCX         RDI
//  arg2  RDX         RSI
//  arg3  R8          RDX
//  arg4  R9          RCX
//  arg5              R8
//  arg6              R9
//
//  callee save
//    windows   RBX, RBP, RDI, RSI, R12, R13, R14, R15
//    linux     RBX, RBP,           R12, R13, R14, R15
//
//  caller save
//    windows   RAX, RCX, RDX,           R8, R9, R10, R11
//    linux     RAX, RCX, RDX, RDI, RSI, R8, R9, R10, R11
//
//  erata:
//    windows - caller must allocate 32bytes of shadow space.
//    windows - stack must be 16 byte aligned.
//    linux   - no shadow space needed.


enum {
  reg_rv = cg_rsi
};


// total size of the code block
static const uint32_t code_size = 1024 * 1024 * 8;

// total number of block map entries
static const uint32_t map_size = 1024 * 64;


// flush the instruction cache for a region
static void sys_flush_icache(const void *start, size_t size) {
#ifdef _WIN32
  if (!FlushInstructionCache(GetCurrentProcess(), start, size)) {
    // failed
  }
#endif
#ifdef __linux__
//  if (cacheflush(start, size, ICACHE) == -1) {
//    // failed
//  }
#endif
}

// allocate system executable memory
static void *sys_alloc_exec_mem(uint32_t size) {
#ifdef _WIN32
  return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#endif
#ifdef __linux__
  const int prot = PROT_READ | PROT_WRITE | PROT_EXEC;
  // mmap(addr, length, prot, flags, fd, offset)
  return mmap(NULL, size, prot, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
}

// this hash function is used when mapping addresses to indexes in the block map
static uint32_t wang_hash(uint32_t a) {
  a = (a ^ 61) ^ (a >> 16);
  a = a + (a << 3);
  a = a ^ (a >> 4);
  a = a * 0x27d4eb2d;
  a = a ^ (a >> 15);
  return a;
}

// allocate a new code block
static struct block_t *block_alloc(struct riscv_jit_t *jit) {
  // place a new block
  struct block_t *block = (struct block_t *)jit->head;
  struct cg_state_t *cg = &block->cg;
  // set the initial codegen write head
  cg_init(cg, block->code, jit->end);
  block->predict = NULL;
#if RISCV_JIT_PROFILE
  block->hit_count = 0;
#endif
  return block;
}

// dump the code from a block
static void block_dump(struct block_t *block, FILE *fd) {
  fprintf(fd, "// %08x\n", block->pc_start);
  const uint8_t *ptr = block->cg.start;
  for (int i = 0; ptr + i < block->cg.head; ++i) {
    fprintf(fd, (i && i % 16 == 0) ? "\n%02x " : "%02x ", (int)ptr[i]);
  }
  fprintf(fd, "\n");
}

// finialize a code block and insert into the block map
static void block_finish(struct riscv_jit_t *jit, struct block_t *block) {
  assert(jit && block && jit->head && jit->block_map);
  struct cg_state_t *cg = &block->cg;
  // advance the block head ready for the next alloc
  jit->head = block->code + cg_size(cg);
  // insert into the block map
  uint32_t index = wang_hash(block->pc_start);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    if (jit->block_map[index & mask] == NULL) {
      jit->block_map[index & mask] = block;
      break;
    }
  }
#if RISCV_DUMP_JIT_TRACE
  block_dump(block, stdout);
#endif
  // flush the instructon cache for this block
  sys_flush_icache(block->code, cg_size(cg));
}

// try to locate an already translated block in the block map
static struct block_t *block_find(struct riscv_jit_t *jit, uint32_t addr) {
  assert(jit && jit->block_map);
  uint32_t index = wang_hash(addr);
  const uint32_t mask = jit->block_map_size - 1;
  for (;; ++index) {
    struct block_t *block = jit->block_map[index & mask];
    if (block == NULL) {
      return NULL;
    }
    if (block->pc_start == addr) {
      return block;
    }
  }
}

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

  struct cg_state_t *cg = &block->cg;

  // setup the basic block
  block->instructions = 0;
  block->pc_start = rv->PC;
  block->pc_end = rv->PC;

  // prologue
  codegen_prologue(cg);

  // translate the basic block
  for (;;) {
    // fetch the next instruction
    const uint32_t inst = rv->io.mem_ifetch(rv, block->pc_end);

    struct rv_inst_t dec;
    uint32_t pc = block->pc_end;
    if (!decode(inst, &dec, &pc)) {
      assert(!"unreachable");
    }

    if (!codegen(&dec, cg, block->pc_end, inst)) {
      assert(!"unreachable");
    }
    ++block->instructions;
    block->pc_end = pc;
    if (inst_is_branch(&dec)) {
      break;
    }
  }

  // epilogue
  codegen_epilogue(cg);
}

struct block_t *block_find_or_translate(struct riscv_t *rv, struct block_t *prev) {
  // lookup the next block in the block map
  struct block_t *next = block_find(&rv->jit, rv->PC);
  // translate if we didnt find one
  if (!next) {
    next = block_alloc(&rv->jit);
    assert(next);
    rv_translate_block(rv, next);
    block_finish(&rv->jit, next);
    // update the block predictor
    // note: if the block predictor gives us a win when we
    //       translate a new block but gives us a huge penalty when
    //       updated after we find a new block.  didnt expect that.
    if (prev) {
      prev->predict = next;
    }
  }
  assert(next);
  return next;
}

void rv_step(struct riscv_t *rv, int32_t cycles) {

  // find or translate a block for our starting PC
  struct block_t *block = block_find_or_translate(rv, NULL);
  assert(block);

  const uint64_t cycles_start = rv->csr_cycle;
  const uint64_t cycles_target = rv->csr_cycle + cycles;

  // loop until we hit out cycle target
  while (rv->csr_cycle < cycles_target && !rv->halt) {

    const uint32_t pc = rv->PC;

    // try to predict the next block
    // note: block predition gives us ~100 MIPS boost.
    if (block->predict && block->predict->pc_start == pc) {
      block = block->predict;
    }
    else {
      // lookup the next block in the block map or translate a new block
      struct block_t *next = block_find_or_translate(rv, block);
      // move onto the next block
      block = next;
    }

    // we should have a block by now
    assert(block);

    // call the translated block
    typedef void(*call_block_t)(struct riscv_t *);
#if RISCV_JIT_PROFILE
    block->hit_count++;
#endif
    call_block_t c = (call_block_t)block->code;
    c(rv);

    // increment the cycles csr
    rv->csr_cycle += block->instructions;

    // if this block has no instructions we cant make forward progress so
    // must fallback to instruction emulation
    if (!block->instructions) {
      assert(!"unable to execute empty block");
    }
  }
}

bool rv_jit_init(struct riscv_t *rv) {

  struct riscv_jit_t *jit = &rv->jit;

  // allocate the block map which maps address to blocks
  if (jit->block_map == NULL) {
    jit->block_map_size = map_size;
    jit->block_map = malloc(map_size * sizeof(struct block_t*));
    memset(jit->block_map, 0, map_size * sizeof(struct block_t*));
  }

  // allocate block/code storage space
  if (jit->start == NULL) {
    void *ptr = sys_alloc_exec_mem(code_size);
    memset(ptr, 0xcc, code_size);
    jit->start = ptr;
    jit->head = ptr;
    jit->end = jit->start + code_size;
  }

  // setup nonjit instruction callbacks
  jit->handle_op_op     = handle_op_op;
  jit->handle_op_fp     = handle_op_fp;
  jit->handle_op_system = handle_op_system;

  return true;
}

void rv_jit_dump_stats(struct riscv_t *rv) {
  struct riscv_jit_t *jit = &rv->jit;

  uint32_t num_blocks = 0;
  uint32_t code_size = (uint32_t)(jit->head - jit->start);

  for (uint32_t i = 0; i < jit->block_map_size; ++i) {
    struct block_t *block = jit->block_map[i];
    if (!block) {
      continue;
    }
    ++num_blocks;

#if RISCV_JIT_PROFILE
    if (block->hit_count > 1000) {
      block_dump(block, stdout);
      fprintf(stdout, "Hit count: %u\n", block->hit_count);
    }
#endif
}

  fprintf(stdout, "Number of blocks: %u\n", num_blocks);
  fprintf(stdout, "Code size: %u\n", code_size);
}
